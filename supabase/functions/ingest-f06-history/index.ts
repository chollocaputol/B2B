// Historical SOS EVIDENCE import only. Never writes public.emergency_alerts.
// This function must remain undeployed until the proposed restricted archive
// schema, operator authorization and existing public RLS policies are reviewed.
import { createClient } from "npm:@supabase/supabase-js@2.95.0";
import { parseF06SosJournal } from "../_shared/f06_history_validator.ts";

const json = (status: number, value: unknown) =>
  new Response(JSON.stringify(value), {status, headers: {"content-type": "application/json",
    "cache-control": "no-store"}});

function getKey(legacy: string, group: string): string {
  const previous = Deno.env.get(legacy);
  if (previous) return previous;
  const raw = Deno.env.get(group);
  if (!raw) throw new Error("server is not configured");
  const keys = JSON.parse(raw);
  const selected = keys?.default;
  const result = typeof selected === "string" ?
    (Deno.env.get(selected) ?? selected) : "";
  if (!result) throw new Error("server is not configured");
  return result;
}

Deno.serve(async (req: Request): Promise<Response> => {
  if (req.method !== "POST") return json(405, {error: "method_not_allowed"});
  const bearer = /^Bearer ([A-Za-z0-9._-]+)$/.exec(req.headers.get("authorization") ?? "");
  if (!bearer) return json(401, {error: "supabase_user_auth_required"});
  if (!(req.headers.get("content-type") ?? "").startsWith("application/json")) {
    return json(415, {error: "json_content_type_required"});
  }
  try {
    const url = Deno.env.get("SUPABASE_URL");
    if (!url) return json(503, {error: "server_not_configured"});
    const publicKey = getKey("SUPABASE_ANON_KEY", "SUPABASE_PUBLISHABLE_KEYS");
    const auth = createClient(url, publicKey, {
      auth: {autoRefreshToken: false, persistSession: false}
    });
    const {data: {user}, error: authError} = await auth.auth.getUser(bearer[1]);
    if (authError || !user) return json(401, {error: "invalid_or_expired_user_token"});
    // Server-fetched app_metadata is administrator controlled; user_metadata is not.
    if (user.app_metadata?.dtn_archive_importer !== true) {
      return json(403, {error: "archive_import_role_required"});
    }

    const declaredBytes = Number(req.headers.get("content-length"));
    if (declaredBytes > 100000) return json(413, {error: "payload_too_large"});
    const reader = req.body?.getReader();
    if (!reader) return json(400, {error: "empty_body"});
    const chunks: Uint8Array[] = [];
    let used = 0;
    while (true) {
      const next = await reader.read();
      if (next.done) break;
      used += next.value.byteLength;
      if (used > 100000) {
        await reader.cancel();
        return json(413, {error: "payload_too_large"});
      }
      chunks.push(next.value);
    }
    const bytes = new Uint8Array(used);
    let offset = 0;
    for (const part of chunks) { bytes.set(part, offset); offset += part.length; }
    const request = JSON.parse(new TextDecoder("utf-8", {fatal: true}).decode(bytes));
    if (!request || typeof request !== "object" || Array.isArray(request) ||
      request.schema_version !== 1 || request.journal !== "sos_events.jsonl" ||
      typeof request.file_sha256 !== "string" ||
      !/^[0-9A-F]{64}$/.test(request.file_sha256) ||
      typeof request.content_base64 !== "string" ||
      request.content_base64.length > 90000 ||
      !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(request.content_base64)
    ) return json(400, {error: "invalid_historical_archive_envelope"});

    const binary = atob(request.content_base64);
    if (binary.length > 65536) return json(413, {error: "sos_file_too_large"});
    const buffer = Uint8Array.from(binary, x => x.charCodeAt(0));
    const fingerprint = Array.from(
      new Uint8Array(await crypto.subtle.digest("SHA-256", buffer)),
      b => b.toString(16).padStart(2, "0")).join("").toUpperCase();
    if (fingerprint !== request.file_sha256) {
      return json(422, {error: "f06_file_sha256_mismatch"});
    }
    // The only currently supported content is a CLOSED, ASCII F06 SOS journal.
    const content = new TextDecoder("ascii").decode(buffer);
    const parsed = parseF06SosJournal(content, request.vessel_id, request.session);
    // Historical archival evidence is never sufficient to authenticate a
    // source device, mutate an active alert, or trigger rescue notification.
    const admin = createClient(url, getKey("SUPABASE_SERVICE_ROLE_KEY", "SUPABASE_SECRET_KEYS"), {
      auth: {autoRefreshToken: false, persistSession: false}
    });
    const {data, error} = await admin.rpc("dtn_archive_f06_sos_file", {
      p_actor: user.id, p_vessel: request.vessel_id,
      p_session: request.session, p_sha256: request.file_sha256,
      p_size: buffer.length, p_records: parsed.records
    });
    if (error) {
      // Do not disclose database internals, user identifiers or source records.
      if (error.code === "23505" || error.code === "P0001") {
        return json(409, {error: "immutable_archive_conflict_or_invalid_record"});
      }
      return json(503, {error: "archive_storage_unavailable"});
    }
    return json(200, {schema_version: 1, result: data,
      file_sha256: request.file_sha256, upload_authorization: "history_only"});
  } catch {
    return json(400, {error: "invalid_or_unavailable_historical_archive"});
  }
});
