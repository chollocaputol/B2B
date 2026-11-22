// Historical-only importer. NOT deployable until authorization/RLS review.
import { createClient } from "npm:@supabase/supabase-js@2.95.0";
import { handleF06HistoryRequest } from "../_shared/f06_history_http.ts";

function readKey(legacy: string, dictionary: string): string {
  const old = Deno.env.get(legacy);
  if (old) return old;
  const raw = Deno.env.get(dictionary);
  if (!raw) throw new Error("missing server configuration");
  const values = JSON.parse(raw);
  const candidate = values?.default;
  const resolved = typeof candidate === "string" ? (Deno.env.get(candidate) ?? candidate) : "";
  if (!resolved) throw new Error("missing server configuration");
  return resolved;
}

Deno.serve(async req => {
  const url = Deno.env.get("SUPABASE_URL");
  if (!url) return new Response(JSON.stringify({ error: "server_not_configured" }), { status: 503 });
  return await handleF06HistoryRequest(req, {
    getOperator: async token => {
      const client = createClient(url, readKey("SUPABASE_ANON_KEY", "SUPABASE_PUBLISHABLE_KEYS"), {
        auth: { autoRefreshToken: false, persistSession: false },
      });
      const { data, error } = await client.auth.getUser(token);
      if (error || !data.user) return null;
      return { id: data.user.id, archiveImporter: data.user.app_metadata?.dtn_archive_importer === true };
    },
    archiveFile: async args => {
      // A protected DB RPC enforces an independently approved file manifest;
      // no caller supplied checksum or operator role can self-approve.
      const admin = createClient(url, readKey("SUPABASE_SERVICE_ROLE_KEY", "SUPABASE_SECRET_KEYS"), {
        auth: { autoRefreshToken: false, persistSession: false },
      });
      const { data, error } = await admin.rpc("dtn_archive_f06_sos_file", args);
      return { data, error };
    },
  });
});
