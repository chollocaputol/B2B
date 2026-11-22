# F06 local Auth / Edge / PostgreSQL acceptance — 2026-11-22

**Evidence class:** Operator-run, actual isolated local Supabase stack, synthetic records only.
**Status:** Historical staging path accepted locally 5/5 on the *previous* SQL
proposal plus a manually applied ID-only grant. Not production approved.

## Source and test anchor
- Previous Git feature revision: `9cffea437697c71bc767240d860d9e6c4a893fa0` (`test(backend)`).
- Lab root: `C:\Users\Public\Thesis\THESIS_F06_LOCAL_SUPABASE`.
- Function: `supabase/functions/ingest-f06-history`.
- Windows test script: `F06_Local_Edge_E2E_Synthetic_v3.ps1`, SHA256
  `2BE780C4B8A971529FD95A8E739F5EB6B703B363DC4E4B4ED2A6C710AA969A7A`.
- Prior Windows host tests: 11/11 JSONL plus 26/26 mock HTTP = 37/37 PASS.
- Prior isolated SQL fixture: 7/7 PASS; local lab actual SQL staging PASS.

## Operator observed actual local integration
| Gate | Response | Result |
| --- | --- | --- |
| Importer without approved manifest | HTTP 409, archive_not_approved_or_immutable_conflict | PASS |
| Independently approved synthetic SOS file | HTTP 200, accepted | PASS |
| Idempotent identical replay | HTTP 200, duplicate | PASS |
| Changed file with original digest | HTTP 422, f06_file_sha256_mismatch | PASS |
| Approved second session, colliding event identity | HTTP 409, rollback | PASS |

Synthetic vessel 2 session `S2F1E41B`: 2 approval rows,
1 historical archive file, 1 archived SOS event. Live operational tables
(`vessels`, `telemetry`, `emergency_alerts`, `communication_tests`,
`sync_logs`) remained unchanged. `emergency_alerts` 0 before/after.
These are *historical radio test events*, not cloud emergency activation,
rescue acknowledgment, on-device DTN delivery, or source authentication.

## Operational lessons / limits
1. Launch function with `--network-id thesis-f06-loopback` alongside the
   isolated Supabase lab; otherwise Kong and Edge Runtime were on different
   Docker networks (503 name resolution failed). Manual Docker connect was
   not durable across Edge Runtime recreation.
2. The staging function uses `SECURITY INVOKER` and checks `auth.users.id`.
   Without `service_role` column SELECT on `auth.users.id`, the actual
   RPC returned `42501` (`permission denied for table users`).
   A narrowly scoped `GRANT SELECT (id) ON auth.users TO service_role`
   made it pass in the lab. The revised un-applied source proposal codifies
   this grant; **the source revision has not been rerun end-to-end yet**.
3. The direct PostgREST manifest failure is HTTP 400 with SQLSTATE `P0001`.
   The Edge adapter maps that to HTTP 409. Do not conflate the two.
4. CLI printed a network exposure warning and 0.0.0.0/[::] mappings.
   Windows Firewall blocked TCP 54321 and 54322, and phone could not
   reach 192.168.1.39:54321, but AP client isolation was not excluded.
5. The local Supabase stack was stopped using `supabase stop` without
   `--no-backup`; CLI reported Docker-volume backup of local data.

## Release hold / next checks
- In the new SQL revision, run revised disposable 7/7 SQL tests and 37/37
  Windows Node HTTP/JSONL tests. Then fresh local E2E **without** a manual
  grant, in a reset disposable lab or controlled new local lab.
- Re-audit the full F06 permission model and Supabase production RLS:
  the five existing public tables were previously observed with RLS OFF
  and broad anonymous grants. This remains critical and unresolved.
- Keep historical importer separate from live operational SOS and future
  production DTN device HTTPS ingestion / ack ledger.
- Original V002 closed sessions `S58BAFE2` (4 SOS), `S41F5724` (0),
  `SB1F7223` (6) remain independently sourced, not imported by this test.
- No Supabase cloud deployment, new paid staging environment, main merge,
  firmware flash, SD write, or physical relay authorization.
