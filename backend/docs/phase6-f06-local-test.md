# F06 local-only PostgreSQL integration gate (2026-11-22)

**Status: prepared in GitHub; requires a successful Windows/Docker run.**
Team selected **local testing**, not a billable Supabase development branch.
No production Supabase writes, user provisioning, Edge Function deployment,
firmware flashing, or original microSD access are part of this test.

## What the test proves

The PowerShell script `scripts/dtn/test_f06_archive_local.ps1` launches
a **disposable PostgreSQL 17 Docker container**. It publishes no ports and
sets `--network none`. It runs (in order):

1. Previously committed 11 Node/TypeScript flat-F06-SOS validator fixtures.
2. `tests/sql/f06_archive_mock_supabase.sql`, defining only the essential
   local Supabase roles and a single fictional `auth.users` record.
3. The repository's original 2026-10-24 SQL migration (5 operational tables).
4. The UNDEPLOYED F06 archive SQL proposal in `backend/sql/proposals/`.
5. Seven database assertions in `tests/sql/f06_archive_local_assertions.sql`.

These assertions verify RLS/grants; unapproved digest denial; one approved
historical file accepted; identical retry treated as duplicate; modified
unapproved file rejected; cross-session `(vessel,boot_token,event_index)`
collision rolled back atomically; and unchanged operational SOS/telemetry.
Test data use synthetic IDs and checksums. The test does NOT prove the
actual original V002 manifest, Edge Function user authentication, HTTPS,
cryptographic firmware identity, on-device replay, or live DTN acceptance.

## Windows prerequisites and execution

- A clean feature worktree at `C:\Users\Public\Thesis\SYSTEM_multihop`
  with the latest `feature/firmware-multihop` test commit checked out.
- Node.js 22+, Git, Docker Desktop with the Linux engine running.
- Docker can obtain `postgres:17-bookworm` if the image is not cached.
- Run from Windows PowerShell (no administrator elevation required if your
  Docker Desktop account is configured to access its local engine).

Preflight, without starting a container:

```powershell
& 'C:\Users\Public\Thesis\SYSTEM_multihop\scripts\dtn\test_f06_archive_local.ps1' -PreflightOnly
```

Run full integration test:

```powershell
& 'C:\Users\Public\Thesis\SYSTEM_multihop\scripts\dtn\test_f06_archive_local.ps1'
```

The script stops on any failed check. It removes **only the unique test
container it created**, even when SQL tests fail. It does not remove
existing containers, volumes, local Supabase databases, or Git files.
If a Docker image pull or permission check fails, correct the local
prerequisite before retrying. Do not substitute the live Supabase project.

## Still blocked after a local test PASS

Even a complete 11/11 host + 7/7 database PASS is a **simulation**, not
proof of historical V002 upload or production DTN. The next milestones
are isolated Edge Function request tests, independent original-manifest
approval by an operator, correct production RLS design, and then a
controlled historical archive import. No offline logs become live SOS.

## 2026-11-22 first real Windows Docker execution

User verified Windows PowerShell 5.1, Node 24.14.1, Docker Engine 29.9.3,
WSL2, clean feature worktree at commit 8aabb74 and successful Docker
postgres:17-bookworm image pull. F06 JSONL host test: **11/11 PASS**.
The original five-table schema and proposed three archive tables applied
in disposable local Postgres. SQL assertions **PASS 1/7 through 3/7**.
Assertion 4 stopped on an identical-file retry with PostgreSQL
`permission denied for table dtn_f06_history_files` from
`SELECT ... FOR UPDATE`. The importer is intentionally restricted to
SELECT/INSERT; FOR UPDATE needs UPDATE privilege. This is a SQL defect,
not a Windows/Docker/Node error. The temporary container was removed.

A follow-up `fix(database)` removes that unnecessary locking clause,
retains immutable-table privileges, and expands the privilege regression
check. The unique `INSERT ... ON CONFLICT DO NOTHING` already protects
competing attempts at the immutable file key. **The new seven database
tests are NOT YET VERIFIED** until run by the user on the corrected commit.
The separately entered interactive `LOCAL DTN VALIDATION COMPLETED`
message after a failed command is not test acceptance.
