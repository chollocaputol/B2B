# Phase 6 Additive Schema Change Proposal

**Status:** Proposal for review only. This is not a migration script and must not be applied without approval.

This proposal distinguishes **current SQL facts**, **project requirements**, and **design proposals**. It preserves the applied initial migration and the existing five tables.

## 1. Current SQL baseline

The inspected local migration is `supabase/migrations/20261024081011_initial_banca_monitoring_schema.sql`; the reference is `backend/schema-reference/banca_monitoring_schema.sql`. The migration contains five tables and six indexes. Its constraints match the reference semantically.

| Table | Current schema facts relevant to ingestion |
|---|---|
| `vessels` | UUID PK defaulting to `gen_random_uuid()`; required `registration_serial_no`, `vessel_name`, `node_id`; unique constraints on registration serial and node ID. `node_id` is a LoRa identity, not a device credential. |
| `telemetry` | UUID PK; required vessel FK (`ON DELETE RESTRICT`), `origin_node_id bigint`, `sequence_number bigint`, `device_timestamp_s bigint`, `gps_fix`, `weather_state`; nullable `received_at`, `synced_at DEFAULT now()`; optional coordinates/sensor/link data; GPS, battery, hop-count, and weather checks. There is a non-unique index on `(origin_node_id, sequence_number)`. |
| `emergency_alerts` | UUID PK; required vessel FK (`ON DELETE RESTRICT`), `origin_node_id`, `weather_state`, `triggered_at`; `received_at DEFAULT now() NOT NULL`; optional `resolved_at`; `resolved DEFAULT false`; GPS, battery, and weather checks. No stable SOS event or action identity exists. |
| `communication_tests` | UUID PK; distance, received-packet-count, and PDR checks; no vessel FK. It is not an ingestion receipt table. |
| `sync_logs` | UUID PK; optional vessel FK (`ON DELETE RESTRICT`); status check; nonnegative `records_failed`; aggregate start/completion and counts. No batch ID or authenticated uploading device is recorded. |

The SQL files contain no RLS enablement/policies, grants, or revokes. `supabase/config.toml` exposes `public` in its API schema list, but local files do not establish effective hosted grants or RLS state. Verify those separately before release.

## 2. Design goals and compatibility rules

The new schema should support unique/revocable device authentication; distinguish authenticated uploader from LoRa origin; deduplicate offline retries across reboots; retain source-authentication status; record SOS activation, acknowledgment, sender cancellation, and authorized rescue as distinct actions; and preserve server receipt time separately from source uptime/UTC.

**Team decision:** retain the existing Vite + React + TypeScript dashboard and use a separate backend API service for ingestion and privileged database operations. The repository contains the Vite frontend but no backend API service. The API service must authenticate devices and dashboard users separately, validate and authorize requests, classify LoRa source trust, implement idempotent ingestion and SOS actions, and perform server-side Supabase operations. The dashboard calls that service through authenticated requests. The backend framework/runtime, hosting platform, and deployment arrangement are open; this proposal does not select them or add dependencies.

Compatibility rules:

- Do not edit or rename the initial migration.
- Add a new migration only after approval.
- Leave historical rows valid; add new fields as nullable or with safe defaults first, then require them in the API for new records.
- Do not reinterpret existing `resolved`/`resolved_at` values as cancellation or rescue.
- Do not treat current `batteryLevel=100` or `shoreDistance_m=0` placeholders as measurements.
- Do not use `sequence_number` alone as a unique key.

## 3. Mandatory additions: device registry and credentials

### `ingestion_devices` (proposed)

Suggested fields: `id uuid primary key`, `vessel_id uuid not null`, `label`, `is_active`, `created_at`, `disabled_at`, `last_seen_at`; FK `vessel_id REFERENCES vessels(id) ON DELETE RESTRICT`.

The registered vessel row supplies its `node_id`; keep that separate from the device UUID. Do not add `UNIQUE(vessel_id)` until the team confirms a vessel can only ever have one registered uploading device. The current schema does not define this cardinality.

### `ingestion_device_credentials` (proposed)

Suggested fields: UUID ID, `device_id` FK with `ON DELETE RESTRICT`, unique credential verifier/hash, issued/expiry/revocation timestamps, and optional `rotated_from` self-reference. Use high-entropy random HTTP credentials so a fast hash is suitable; do not store the bearer secret in plaintext. Permit a controlled rotation overlap rather than overwriting the device identity.

HTTP credentials do not verify LoRa source authenticity. The current repository has no MAC implementation. Do not infer a MAC algorithm, key provisioning method, or device security capability here. The final agreed source-authentication mechanism must be independently verified by the backend before a source is labeled `VERIFIED`.

## 4. Mandatory additions: telemetry identity and provenance

Add to `telemetry`:

- `record_id uuid`: stable ID created once and retained in JSONL; unique index. Keep existing `id` as the database row UUID.
- `boot_id uuid`: source boot/session identity.
- `uploading_device_id uuid`: FK to `ingestion_devices(id) ON DELETE RESTRICT`; server-derived, nullable for legacy records.
- `event_time_utc timestamptz`: nullable trusted source time; distinct from `device_timestamp_s` and server receipt time.
- `origin_auth_status text`: backend-assigned status. Ordinary telemetry may store `VERIFIED` or `UNVERIFIED`; never accept a submitted status. `VERIFIED` requires backend verification of the agreed source-authentication mechanism. `UNVERIFIED` means source authenticity has not been established. `INVALID` means verification failed or a required security rule was violated; reject it or retain it only in a separate restricted security audit according to team policy, not as ordinary telemetry.

Indexes/constraints:

- Unique index on `record_id`.
- Unique index on `(origin_node_id, boot_id, sequence_number)` for source sequence identity across boot sessions. Legacy `boot_id` nulls remain possible while the API requires a value for new records; PostgreSQL unique indexes allow multiple null-containing legacy tuples unless a separate partial strategy is chosen.
- Check stored `origin_auth_status` against `VERIFIED`/`UNVERIFIED`; validate non-null production identity and value ranges at the API boundary. Consider database checks for stable database invariants.
- Keep the existing non-unique `(origin_node_id, sequence_number)` index unless query-plan review proves it unnecessary. The proposed unique index orders `(origin_node_id, boot_id, sequence_number)`, so it is not a direct replacement for queries on origin plus sequence without boot ID. Avoid adding another index with the same column order as an existing/unique index.

**Compatibility:** `origin_node_id` is currently a bigint while firmware uses `uint32_t`; current IDs fit. Preserve the integer representation and document JSON decimal encoding. Do not change existing GPS/weather/battery checks. Current firmware has `sensorFusionState`, but SQL does not; omit in v1 unless the team approves a separate field. New stable IDs and boot identity are mandatory for production ingestion; nullable legacy rows may remain during staged development, but production requests must not omit them.

## 5. Mandatory additions: SOS event identity and action history

### Extend `emergency_alerts`

Suggested additions: stable `event_id uuid` with unique index; `boot_id`; source sequence; `uploading_device_id` FK; nullable `event_time_utc`; backend-assigned `origin_auth_status`; and a current lifecycle projection only if it can be kept transactionally consistent with the action history.

### Add `sos_event_actions`

Use an append-only action history for stable `action_id uuid` (unique), `event_id` FK to a unique event key with `ON DELETE RESTRICT`, action type check (`ACTIVATED`, `ACKNOWLEDGED`, `CANCELLED`, `RESCUE_CONFIRMED`), actor type (`DEVICE`/`USER`), optional `actor_device_id`, optional `actor_user_id`, event/source time, and server `received_at`.

An acknowledgment is an action on an active alert; it does not clear it. Cancellation requires the originating sender under the policy approved by the team and does not imply rescue. Rescue confirmation requires an administrator/rescue-operator user identity. The action log should be append-only and authoritative. If a denormalized current status is stored on `emergency_alerts`, update it in the **same transaction** as the action insert. If atomic updates cannot be guaranteed, omit the projection and derive current state from the action history.

Indexes: unique `action_id`; index `(event_id, received_at)` for history; index on current status and receipt time for active dashboard alerts only if a projection is used. Foreign keys to devices use `ON DELETE RESTRICT`. For user IDs, use a Supabase Auth user FK only after confirming the desired deletion/retention behavior; alternatively retain an immutable actor UUID in the audit row and enforce role authorization in the server.

### Atomic write approach comparison and recommendation

The action insert and current-status update must not be separate independent HTTP writes.

| Approach | Advantages | Costs/risks |
|---|---|---|
| Narrow PostgreSQL function invoked by the backend API through the Supabase API | Fits Supabase-hosted PostgreSQL and can be called by a server-side Supabase client; one database transaction can append an action, check transition/idempotency rules, and update the projection; does not require a separate direct database driver. | Function must have narrow inputs/behavior, reviewed execute grants and a safe fixed search path; service-role callers bypass RLS, so caller authorization remains essential. |
| Transaction-capable server-side PostgreSQL connection | Familiar explicit transaction boundary and direct control over multi-statement operations. | Adds a runtime PostgreSQL driver and connection configuration not present in this repository; requires secure pooled connection handling and deployment network/TLS configuration. |

**Recommendation:** use a narrowly scoped PostgreSQL function called by the separate backend API service, with one action per transaction (or a carefully specified per-item batch function). This fits the Supabase-hosted database and avoids requiring a direct database driver solely for transaction support. Do not add the function or dependencies yet. The implementation owner and database reviewer must approve function permissions, transaction boundaries, and behavior before coding. If the team chooses a direct connection instead, re-evaluate dependency and deployment requirements.

Transition decisions still requiring approval: cancellation after acknowledgment; ordering when cancellation and rescue confirmation race; behavior for delayed/reordered activation and cancellation; whether repeated acknowledgments by the same receiver are duplicates or distinct audit actions; whether acknowledgment is allowed for an `UNVERIFIED` origin; and whether cancellation/rescue are terminal or may be corrected. Same `action_id` plus same content should be idempotent; same ID plus different content is a conflict.

**Compatibility:** Keep existing `resolved` and `resolved_at` unchanged for legacy readers. Do not backfill them into `CANCELLED` or `RESCUE_CONFIRMED` without evidence of their former semantics. Recommended approach for `triggered_at NOT NULL`: add nullable `event_time_utc`, preserve all existing values, and relax `triggered_at` to nullable in the new additive migration after checking consumers. For new rows, set `triggered_at` only when source UTC is trustworthy; otherwise leave it null and set server `received_at` separately. Never write upload time into `triggered_at` as if it were activation time. If a consumer requires a non-null value, migrate it to use `received_at` for server arrival and `event_time_utc` for source event time before relaxing the constraint. The local frontend files are empty, but external consumers have not been audited.

## 6. Mandatory security requirements and recommended access controls

The database has no current user-role table or policies. Supabase Auth is enabled in local config, but that does not itself authorize rescue actions.

Choose one server-managed authorization source:

- `monitoring_user_roles` keyed by Supabase Auth user UUID with constrained roles such as `viewer`, `rescue_operator`, `administrator`; or
- trusted server-managed app metadata/custom claims with equivalent role and vessel-scope checks.

Never use user-editable metadata. Dashboard browser clients should receive only the public/anon client key and be constrained by RLS. Device ingestion uses server-side credentials after explicit device authentication. A service-role client bypasses RLS, so the backend API must authorize each operation before privileged writes. Enable RLS/restrict direct writes on sensitive application tables and create narrowly scoped dashboard read policies. Credential verifiers and any source-authentication references must never be readable by browser roles. Verify actual hosted grants/RLS before release; the checked-in SQL does not show them.

The role source (server-managed `monitoring_user_roles` table or trusted server-managed claims), vessel scoping, and rescue-operator assignment process are mandatory decisions. Adding a role table is conditional on choosing that approach; no user roles currently exist in the inspected SQL.

## 7. Recommended synchronization audit and optional packet history

The API must return a result for each record in a response. Recommended schema addition: extend `sync_logs` with `batch_id uuid` and `uploading_device_id uuid` FK. Define count semantics for existing `records_sent`, `records_successful`, `records_failed`, and `duplicates_detected`. Add nonnegative checks for counts where appropriate and a unique key on `(uploading_device_id, batch_id)` if a logical batch is retried unchanged.

The API response is the primary per-record acknowledgment. A `sync_log_items` child table containing batch/log ID, record/action ID, outcome, error code, and receipt time is an optional enhancement if durable per-record audit beyond the response and device checkpoint is required. It adds storage volume but supports investigation of partial-batch and retry histories. The current aggregate `sync_logs` row alone cannot identify each record outcome.

A separate `packet_receptions` history table is an **optional future enhancement, not required for initial ingestion**, if the initial goal is canonical telemetry and at most one receiver observation. Add it only if the team requires durable history for every radio reception, duplicate, relay hop, or acknowledgment. The existing telemetry RSSI/SNR/hop fields do not preserve multiple receptions.

## 8. Idempotency and database behavior

- Unique `record_id` identifies a stable telemetry record through retries.
- `(origin_node_id, boot_id, sequence_number)` catches reuse/duplication in the source sequence namespace. SOS uses `event_id` and unique `action_id` rather than telemetry identity alone.
- On conflict, compare immutable normalized payload contents (or a server-computed canonical content hash). Same ID and same contents returns `duplicate`; same ID and changed contents returns an idempotency conflict. Never overwrite silently.
- Each batch item should commit independently so valid records can be accepted when another item is rejected. Return a per-record result.
- Keep unverified source rows explicitly marked `UNVERIFIED`; do not elevate trust because the uploader authenticated. `VERIFIED` requires backend verification of the agreed mechanism. `INVALID` is rejected or written only to an approved restricted security audit, not ordinary business tables.
- Assign `received_at` on the server. Source uptime remains a duration paired with `boot_id`; optional UTC event time remains nullable.
- SOS action insertion and any current-state projection need atomicity. Recommended: a narrowly scoped PostgreSQL function called by the backend API service. A transaction-capable server-side connection is a valid alternative but adds a database driver and connection/deployment requirements absent from the repository. Separate REST writes can leave action history and current status inconsistent.

## 9. Compatibility review summary

| Existing schema | Proposed change | Potential compatibility concern / mitigation |
|---|---|---|
| `telemetry.id` is generated UUID PK | Add `record_id` | Keep existing PK; new API supplies stable record ID. Backfill/default old rows safely if the new field becomes non-null later. |
| Telemetry sequence index is non-unique; no boot ID | Add boot ID and unique source tuple | Existing rows lack boot ID; nullable legacy values and API-level requirement avoid immediate rewrite. Check duplicate data before enforcing stricter constraints. |
| `vessels.node_id` is unique | Add device UUID registry linked by vessel FK | Do not equate node ID with device credentials; avoid one-device-per-vessel uniqueness until confirmed. |
| No uploader identity | Add uploader device FK | Nullable for legacy rows; server derives for new writes. Historical uploader cannot be reconstructed reliably. |
| `resolved`/`resolved_at` only | Add event/action identity and separate states | Do not reinterpret old data; preserve columns and migrate readers deliberately. |
| `triggered_at` required | Add nullable source UTC field and relax `triggered_at` after consumer review | Current firmware lacks UTC; never map uptime or receipt time to activation time. Existing consumers may rely on non-null `triggered_at`. |
| Aggregate sync counts only | Add batch/device IDs and optionally item rows | Define count meanings and avoid duplicate count updates on retried batches. |
| No RLS policies in local SQL | Add policies/grant restrictions | Service role bypasses RLS; browser access must stay user-scoped and device credentials inaccessible. Hosted effective grants are unknown from local files. |
| `communication_tests` has no vessel FK | No change proposed for ingest v1 | Keep separate; do not use it as per-packet receipt history. |

No SQL migration is included in this proposal.

## 10. Mandatory, recommended, and optional scope

### Mandatory for production

- Registered device identity, active/revocable HTTP credential, and server-derived uploader identity.
- Stable telemetry record ID and source boot ID; unique source identity across sequence resets.
- Stable SOS event/action IDs and append-only action history.
- Separate source identity, uploader identity, source uptime, optional source UTC, and server receipt time.
- Backend-verified source authentication for `VERIFIED`; explicit `UNVERIFIED` for unestablished authenticity; an approved reject-versus-security-audit policy for `INVALID`.
- Server-side user role checks for rescue confirmation; no device credential may confirm rescue.
- RLS/grant review and atomic action/state writes.

### Recommended initial design

- Two device tables (device registry and revocable credential history), additive nullable columns for legacy data, unique record/source indexes, `sos_event_actions`, and a narrowly scoped PostgreSQL action function.
- Extend `sync_logs` with batch and uploader identity and clear count semantics.
- Add nullable source event-time storage and, after consumer review, relax `triggered_at` so unknown UTC stays unknown.

### Optional future enhancements

- `sync_log_items` for durable per-record response history.
- `packet_receptions` for each LoRa reception/duplicate/relay observation.
- A denormalized current SOS status projection if maintained atomically; otherwise derive current state from action history.
- A `monitoring_user_roles` table if the team selects a database role table rather than trusted server-managed claims.
- Storage for source-authentication key references only after the mechanism and provisioning model are selected.

## 11. Implementation readiness and decision register

### Existing application and service boundary

The frontend is the existing Vite + React + TypeScript dashboard and remains so by team decision. The repository does not currently contain the separate backend API service. The service should expose the proposed `POST /api/v1/ingest`, authenticate device requests, authorize dashboard-user requests, and perform privileged Supabase operations only on the server. Do not add API code to the Vite browser bundle. Framework/runtime, service hosting/deployment platform, and its deployment relationship to the dashboard remain open. Dependency selection must wait until the team approves those choices; this document does not create a service or install packages.

| Decision | Current recommendation | Reason | Approval owner | Can implementation proceed first? |
|---|---|---|---|---|
| Frontend architecture | **DECIDED: retain Vite + React + TypeScript dashboard; use a separate backend API service.** | Preserves the existing dashboard and keeps privileged operations outside browser code. | Team | Yes; backend framework/platform choices remain separate decisions. |
| Backend framework/runtime | Select a framework/runtime suitable for the separate API service. | No backend service or framework is present in the repository; avoid implying an unapproved stack. | Team + implementation owner | API contract and schema review can continue; service implementation cannot. |
| Backend hosting and deployment | Select hosting, environment configuration, and deployment relationship to the Vite dashboard. | Required for HTTPS, secret storage, allowed dashboard origins, availability, and operational ownership. | Team + implementation owner | Contract and schema review can continue; production deployment cannot. |
| Source-authentication mechanism and verification | Select the final mechanism separately; only backend-verified proof yields `VERIFIED`. | Current firmware has no MAC and uploader auth is not original-source proof. | Team + supervisor | No production relay acceptance; schema/API scaffolding can proceed with statuses. |
| `UNVERIFIED` and `INVALID` handling | Keep unverified records explicitly labeled; reject invalid records or retain them only in a restricted security audit, after approval. | Avoid treating untrusted data as verified or mixing failed proof into business telemetry. | Team + supervisor | No production security policy or final write path. |
| Production required fields | Require stable IDs, boot ID, registered uploader, and SOS IDs; UTC remains optional when unavailable. | Durable retries/reboot dedup and event identity depend on them. | Team | Test-only contract fixtures can proceed; production firmware/API cannot. |
| SOS transitions and acknowledgment trust | Keep activation, acknowledgment, cancellation, and rescue distinct; define races/repeats and ACK eligibility. | Prevent stale/replayed actions from changing lifecycle incorrectly. | Team + supervisor | Action-table design can proceed; transition function cannot be finalized. |
| SOS timestamp compatibility | Add nullable source UTC, preserve history, relax `triggered_at` only after consumer review; never substitute receipt time. | Current firmware uptime is not UTC; existing column is NOT NULL. | Team + implementation owner | Schema review can proceed; migration/API write semantics wait. |
| User role source and scope | Use server-managed roles/claims and vessel-scoped authorization; rescue confirmation by authorized personnel only. | No current role model or RLS policy is in SQL. | Team | Ingestion contract can proceed; rescue route/policies cannot. |
| Device registration/credential lifecycle | Approve who registers devices, rotates credentials, and revokes them. | No registration/provisioning workflow exists. | Team + implementation owner | Table design can proceed; production onboarding cannot. |
| Atomic SOS implementation | Prefer a narrowly scoped PostgreSQL function called from the backend API through the Supabase API. | Fits hosted Supabase and avoids an unneeded direct DB driver. | Implementation owner + DB reviewer | Contract can proceed; state projection writes wait for approval. |
| Per-record sync history | Begin with API per-record results plus aggregate `sync_logs`; add item table only if durable audit is required. | Keeps initial schema smaller while preserving device acknowledgment semantics. | Implementation owner | Core API can proceed once other blockers resolve. |
| Packet reception history | Defer unless every LoRa reception/duplicate must be audited. | Existing metrics capture at most one observation per telemetry row. | Team | Yes, unless operational monitoring requires it. |

No endpoint, migration, dependency, firmware, or database change has been made by this proposal.
