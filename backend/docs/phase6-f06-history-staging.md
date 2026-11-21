# F06 historical SOS staging importer — Supabase development gate

Status (2026-11-22): FEATURE BRANCH CODE ONLY. NOT DEPLOYED. NO LIVE DATA WRITES.
Architecture: Existing React + Vite + TypeScript dashboard on Netlify;
Supabase Auth, PostgreSQL, Realtime and Edge Functions are the backend.
No Next.js runtime and no separately hosted backend API.

## Tested real source evidence

Previously verified V002 closed F06 sessions: S58BAFE2 (4 SOS events),
S41F5724 (empty SOS journal), SB1F7223 (6 SOS events). The mixed-boot
LEGACY archive is excluded. These are historical test records, not
an authoritative new emergency action or rescue confirmation.

## Files and contract

- supabase/functions/_shared/f06_history_validator.ts: strict bounded
  flat ASCII JSONL parser, per-file event identity collision detection
- supabase/functions/ingest-f06-history/index.ts: POST authenticated
  with a Supabase user JWT; requires server-fetched user
  app_metadata.dtn_archive_importer===true; no such user granted here
- backend/sql/proposals/f06_archive_staging.sql: proposed restrictive
  archive-only RLS tables, an independently approved export table, and
  a privileged atomic idempotent RPC.
  This is NOT an applied migration; no live Supabase SQL was changed.
- tests/dtn_f06_history_validator.test.mjs: Node 22+ fixtures.

Request JSON fields: schema_version=1, vessel_id=2,
session=S58BAFE2, journal=sos_events.jsonl,
file_sha256=<original file uppercase SHA256>,
content_base64=<base64 encoding of original SOS JSONL file bytes>.

An identical (vessel, session, file) retry returns archive status
duplicate. Different bytes under the same session/file key or a
duplicate cross-session event identity fail without overwriting.
Only the archive tables are written. public.emergency_alerts is never
written; neither SOS alerts nor notifications can be triggered here.
These responses are not production DTN acknowledgments, radio ACKs,
source-authenticated records, cloud SOS actions, or rescue confirmation.

## Deployment prerequisites — NOT SATISFIED

1. Five existing public Supabase tables have RLS disabled and broad
   anon privileges. Review their table privileges and RLS policies
   before production connection. Do not blindly enable RLS.
2. Before any upload, a database administrator must independently
   verify the SAVED F06 SHA256 manifest for the closed session and
   all three original journal files. Explicitly register the expected
   SOS file digest, exact byte size, event count and vessel/session in
   the approval table. The HTTP uploader is not allowed to approve itself.
   A matching digest is evidence of file integrity, not device-origin proof.
3. Generate a real migration using the Supabase CLI's migration-new
   command, review SQL and apply to an isolated test/staging project.
   Verify anon/authenticated access is denied to archive tables/RPC.
4. Review and provision a specifically approved lab importer identity
   via server-controlled app_metadata (not user_metadata). Keep
   all secret/service-role keys inside the hosted Edge Function.
5. Confirm JWT validation and function request limits; run the
   Node test suite and verify a real closed F06 export in Windows.
6. Import one S58BAFE2 historical file: expect 4 archived records;
   identical retry should be duplicate, altered replay conflict.
   Confirm live emergency_alerts count unchanged.
7. For full DTN later: device-specific authenticated ingestion,
   stable production record UUIDs, per-item ack ledger, persistent
   on-device SD replay and Wi-Fi/HTTPS retry/reboot recovery.

The historical archive is NOT device-authorized cloud ingestion.
No hardware changes, SD writes, RF changes, firmware flashes,
live Edge Function deployments, or main branch changes here.

## Staging environment status

Supabase project currently has **zero development branches**. Creating
one requested a billable-project confirmation ID. No branch has been
created, no cost approved, and no production SQL has been applied.
Database transaction tests and real cloud upload are BLOCKED until a
separate permitted staging environment is available and its price is
explicitly accepted. The earlier 11/11 Node validator fixtures are
host-only and do not satisfy DB SQL or live Edge Function acceptance.
