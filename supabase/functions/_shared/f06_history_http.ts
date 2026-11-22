// Pure HTTP boundary used by the Deno Supabase Edge Function and local Node tests.
// ONLY historical, independently approved F06 SOS evidence is archived.
// It never writes an operational SOS or counts radio ACKs as cloud ACKs.
import { parseF06SosJournal } from "./f06_history_validator.ts";

export type ArchiveOperator = { id: string; archiveImporter: boolean };
export type ArchiveRecord = ReturnType<typeof parseF06SosJournal>["records"];
export type ArchiveRpcArguments = {
  p_actor: string;
  p_vessel: number;
  p_session: string;
  p_sha256: string;
  p_size: number;
  p_records: ArchiveRecord;
};
export type ArchiveRpcResult = {
  data: unknown;
  error?: { code?: string } | null;
};
export type F06HistoryDependencies = {
  getOperator: (token: string) => Promise<ArchiveOperator | null>;
  archiveFile: (args: ArchiveRpcArguments) => Promise<ArchiveRpcResult>;
};
const MAX_HTTP_BYTES = 100000;
const MAX_SOS_BYTES = 65536;
const base64 = /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/;
const sha = /^[0-9A-F]{64}$/;

function response(status: number, data: unknown): Response {
  return new Response(JSON.stringify(data), { status, headers: {
    "content-type": "application/json", "cache-control": "no-store",
  }});
}

async function boundedBody(req: Request): Promise<Uint8Array | null> {
  const length = req.headers.get("content-length");
  if (length && (!/^\d+$/.test(length) || Number(length) > MAX_HTTP_BYTES)) return null;
  if (!req.body) return null;
  const reader = req.body.getReader();
  const chunks: Uint8Array[] = [];
  let total = 0;
  while (true) {
    const part = await reader.read();
    if (part.done) break;
    total += part.value.byteLength;
    if (total > MAX_HTTP_BYTES) { await reader.cancel(); return null; }
    chunks.push(part.value);
  }
  if (total === 0) return null;
  const out = new Uint8Array(total);
  let off = 0;
  for (const chunk of chunks) { out.set(chunk, off); off += chunk.length; }
  return out;
}

export async function handleF06HistoryRequest(req: Request, deps: F06HistoryDependencies): Promise<Response> {
  if (req.method !== "POST") return response(405, { error: "method_not_allowed" });
  const token = /^Bearer ([A-Za-z0-9._-]+)$/.exec(req.headers.get("authorization") ?? "");
  if (!token) return response(401, { error: "supabase_user_auth_required" });
  if (!/^application\/json(?:\s*;|$)/i.test(req.headers.get("content-type") ?? "")) {
    return response(415, { error: "json_content_type_required" });
  }

  let operator: ArchiveOperator | null;
  try { operator = await deps.getOperator(token[1]); }
  catch { return response(503, { error: "auth_provider_unavailable" }); }
  if (!operator) return response(401, { error: "invalid_or_expired_user_token" });
  if (!operator.archiveImporter) return response(403, { error: "archive_import_role_required" });
  if (!/^[0-9a-f-]{36}$/i.test(operator.id)) return response(503, { error: "invalid_operator_context" });

  let input: Record<string, unknown>;
  try {
    const raw = await boundedBody(req);
    if (!raw) return response(413, { error: "missing_or_oversized_request" });
    const value = JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(raw));
    if (!value || typeof value !== "object" || Array.isArray(value)) throw Error("invalid envelope");
    input = value as Record<string, unknown>;
  } catch { return response(400, { error: "invalid_json_envelope" }); }
  if (input.schema_version !== 1 || input.journal !== "sos_events.jsonl" ||
      typeof input.content_base64 !== "string" || input.content_base64.length > 90000 ||
      !base64.test(input.content_base64) || typeof input.file_sha256 !== "string" ||
      !sha.test(input.file_sha256)) {
    return response(400, { error: "invalid_historical_archive_envelope" });
  }

  let bytes: Uint8Array;
  try {
    const decoded = atob(input.content_base64);
    if (decoded.length > MAX_SOS_BYTES) return response(413, { error: "sos_file_too_large" });
    bytes = Uint8Array.from(decoded, char => char.charCodeAt(0));
  } catch { return response(400, { error: "invalid_file_base64" }); }
  const expectedDigest = input.file_sha256 as string;
  const digest = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)),
    byte => byte.toString(16).padStart(2, "0")).join("").toUpperCase();
  if (digest !== expectedDigest) return response(422, { error: "f06_file_sha256_mismatch" });

  let parsed: ReturnType<typeof parseF06SosJournal>;
  try {
    // The source journal must be ASCII and immutable; the validator checks all characters.
    parsed = parseF06SosJournal(new TextDecoder("utf-8", { fatal: true }).decode(bytes),
      input.vessel_id, input.session);
  } catch { return response(422, { error: "invalid_closed_f06_sos_journal" }); }

  let saved: ArchiveRpcResult;
  try {
    saved = await deps.archiveFile({
      p_actor: operator.id,
      p_vessel: input.vessel_id as number,
      p_session: input.session as string,
      p_sha256: expectedDigest,
      p_size: bytes.length,
      p_records: parsed.records,
    });
  } catch { return response(503, { error: "archive_storage_unavailable" }); }
  if (saved.error) {
    if (saved.error.code === "23505" || saved.error.code === "P0001") {
      return response(409, { error: "archive_not_approved_or_immutable_conflict" });
    }
    return response(503, { error: "archive_storage_unavailable" });
  }
  const record = saved.data as Record<string, unknown> | null;
  if (!record || typeof record !== "object" ||
      (record.status !== "accepted" && record.status !== "duplicate") ||
      record.scope !== "historical_sos_archive_only" ||
      record.emergency_alerts_changed !== false ||
      record.record_count !== parsed.record_count) {
    return response(503, { error: "unexpected_archive_response" });
  }
  return response(200, {
    schema_version: 1, result: record,
    file_sha256: expectedDigest, upload_authorization: "history_only",
  });
}
