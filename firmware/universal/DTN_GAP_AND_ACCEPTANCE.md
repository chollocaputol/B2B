# DTN firmware gate — delayed shore synchronization (not LoRa multi-hop)

**Status (Nov 2026): required in the manuscript; not yet implemented or accepted.**
This guide makes no changes to radio, Wi-Fi, SD, NVS, SOS, credentials or identities.

## Thesis source and intended roles

- Sections 3.5.1 and 4.8.7: near-real-time LoRa forwarding for SOS and
  controlled emergency mesh; three-node RF proof remains blocked by V001.
- Sections 3.5.2 and 4.8.7: routine weather/GPS telemetry and unresolved SOS
  records cached on microSD during offshore disruption, physically carried
  ashore and bulk-uploaded over Wi-Fi to the backend.
- Section 4.9.2 item 9: asynchronous shore upload with explicit acceptance
  before a record is marked synchronized. Server arrival is not source UTC.

**Do not equate RAM-only LoRa relay queuing with durable DTN storage.**

## Verified current repository boundary

- Firmware has per-boot F06 microSD journals and export functionality.
- Default SD mode is SOS_ARMED: routine recording is OFF until CONTINUOUS
  is chosen on the OLED. A mounted card alone is NOT proof of complete
  routine DTN capture. Historical journals must remain untouched.
- No running firmware nearshore Wi-Fi reattachment + authenticated REST
  batch uploader, accepted-record cursor or reboot-safe retry process has
  been demonstrated. DTN bulk synchronization remains a work item.
- backend/docs/phase6-ingestion-api-contract.md is explicitly **proposed**
  and not yet an implemented service; its identity/auth and partial
  acceptance semantics must be decided before real uploads.
- No existing GPS/PHT HOME display change should be misrepresented as a
  trustworthy UTC timestamp in stored records or LoRa packets.

## Implementation gates before physical DTN acceptance

1. Decide whether/how routine logging becomes automatic at sea without
   silently overriding user-selected SOS_ARMED and emergency priorities.
2. Read immutable complete journal records, detect partial writes and
   preserve every previous per-vessel/per-session directory.
3. Assign stable record and batch identities; distinguish authenticated
   uploader hardware from unauthenticated LoRa origin.
4. Add a separately durable, crash-consistent sync checkpoint. Wi-Fi
   presence and generic HTTP 200 alone are not record acceptance;
   check per-record accepted/duplicate/rejected results and handle
   partial batches without marking rejected records synchronized.
5. Require secure device credentials, TLS validation, shore Wi-Fi
   connection gating, timeouts and SOS/LoRa preemption.
6. Host tests: duplicate replay; delayed acknowledgment; HTTP timeout
   after backend commit; corrupt last SD line; missing SD; reboot mid
   checkpoint; full card; disconnected shore Wi-Fi; rejected items.
7. Integrate only after backend API and schema are reviewed, then test
   real shore upload and deduplication separately from LoRa multihop.

The manuscript's absolute data-survival language is a desired research
outcome, NOT a measured guarantee under SD failure or lost equipment.

Next development unit: pure offline sync-cursor contract + host tests,
then read-only SD adapter; no live HTTP/Wi-Fi change in this commit.

## 2026-11-22 offline checkpoint model (host-only)

A pure C++17 cursor model now exists at
`firmware/shared/storage/dtn_sync_checkpoint.h` with matching
`tests/dtn_sync_checkpoint_host.cpp`. It is **not linked into the device
firmware** and does not read or write any SD journals.

The model accepts explicit backend per-record `Accepted` or `Duplicate`
results only, keeps any rejected or uncertain record pending, checkpoints
only a contiguous prefix, and rejects a wrong session or unauthenticated
or mismatched batch response. Its two-slot storage interface requires a
future crash-consistent SD sidecar adapter; no source SD record is modified.
A local native `g++ -std=c++17 -Wall -Wextra -Werror` test passed for
partial acceptance, duplicate retry, wrong session, failed write, corrupt
slot recovery and reboot. **This is a model PASS only.**

Do not infer that `FLEET` enrollment credentials authorize HTTP uploads,
or that a receiver's LoRa ACK proves shore ingestion. Firmware-side capture
in CONTINUOUS mode, real journal readback, secure Wi-Fi/REST upload,
backend idempotent acceptance, and durable sidecar implementation remain
pending. The current fleet `main` branch is not modified.

## 2026-11-22 read-only F06 JSONL snapshot framer (host-only)

The next pure component is `firmware/shared/storage/dtn_jsonl_snapshot.h`
with `tests/dtn_jsonl_snapshot_host.cpp`. It models *closed F06 sessions*,
uses a caller-supplied fixed byte snapshot, preserves complete JSONL
record bytes plus original file offset/ordinal, accepts only the existing
three file names, and holds the cursor on truncated tail / malformed
framing / oversize / I/O failure. A trailing partial line is never
treated as accepted or automatically skipped.

This component checks only the JSONL **framing** (a printable `{...}`
line ending in LF or CRLF), not complete JSON syntax, source identity,
field validity, end-to-end uploadable content, or cryptographic proof.
An invalid object such as `{broken}` can be framed but MUST be rejected
by the later strict JSON and backend mapping stage. The built-in FNV
digest is non-cryptographic and diagnostic only.

Existing F06 event journals use `vessel_id`, `boot_token`,
`event_index`, and `uptime_ms`. They do not provide server-approved
per-record UUIDs, stable origin-authentication proof, or a final
session-wide replay manifest. The host framer's per-file ordinal
and byte offset are not a backend record ID; cross-file ordering and
stable replay identity still need review. Never upload `diagnostics.jsonl`
as ordinary telemetry or assume an SD journal automatically means the
`CONTINUOUS` recording mode was active.

The mock-reader host test passes under native g++ locally; **device SD
adapter, strict JSON validation, backend ingestion, shore Wi-Fi,
sidecar storage, and real DTN upload remain NOT IMPLEMENTED**.
No existing device/runtime source was modified.

## 2026-11-22 F06 strict flat-record gate (host-only)

New pure code: `firmware/shared/storage/dtn_f06_record_gate.h`,
with a native mock/fixture test `tests/dtn_f06_record_gate_host.cpp`.
It runs after the existing newline framer, with no Arduino, SD, Wi-Fi,
HTTP, LoRa, NVS or runtime linking. Original journal bytes stay immutable.

The validator requires bounded flat JSON syntax; rejects duplicate keys,
invalid escapes, malformed numbers, wrong-vessel records, missing `kind`
and `vessel_id`, and for SOS/telemetry journals requires unsigned 32-bit
`boot_token`, positive `event_index` and `uptime_ms`. It does NOT verify
all event-specific/weather value semantics or origin authenticity.
Diagnostics are classified `DiagnosticsOnly`, not ordinary cloud telemetry.
Other records can be only `CandidateUnverified`, **never approved uploads**.

The `localLocator()` helper combines vessel, allowlisted F06 session,
allowlisted journal name, immutable file ordinal/byte-range and nonsecure
FNV-1a checksum. This is a **local diagnostic reference**, not a UUID,
MAC, uploader credential, cryptographically unique ID or backend
idempotency key. Do not use it as a server `record_id` without a
separate collision-safe identity and backend review.

The exact new header and test pass native g++ C++17 -Wall -Wextra -Werror
against the exact earlier JSONL header. PlatformIO builds are unaffected
because no firmware translation unit includes the new model.

**Still blocked:** strict per-kind schema mapping, stable cloud record ID,
source proof, durable SD sidecar, real on-board SD read adapter,
authenticated shore Wi-Fi/REST ingestion, backend acceptance and reboot
replay against a real service. Physical multihop remains blocked by V001.

## 2026-11-22 host audit against verified closed F06 exports

Read-only host CLI: `scripts/dtn/audit_verified_f06_exports.py`.
Offline fixture tests: `tests/test_dtn_verified_export.py` (seven PASS
in local Python standard-library unittest). The CLI opens only known
`sos_events.jsonl`, `telemetry.jsonl`, `diagnostics.jsonl` and a matching
`sha256_manifest.json` from each supplied *closed* F06 Sxxxxxxx session.

It verifies exact manifest filenames, SHA-256 digests and lengths; rejects
oversized files/lines, incomplete tail, invalid flat JSON, duplicate keys,
wrong vessel/invalid event identity and duplicate
`(vessel_id,boot_token,event_index)` keys both within sessions and
across all supplied isolated exports. Diagnostics are counted only and
are never authorized as telemetry. It prints summary counts only,
not GPS, secret payloads, or whole records.

The legacy mixed-boot export is intentionally refused by this tool;
its separately audited 55 event boot tokens are not proof of one
immutable session. A SHA-256 manifest checks local copies against their
saved manifest, not their authorship, source authentication or backend
acceptance. Original V002 audit showed zero duplicate F06 event tuples
within each of four local exports (1,534 SOS+telemetry records),
but **the original bytes have not been audited by this new CLI here**.

This is an **off-device read-only exported-copy adapter**, not a
microcontroller SD driver. It writes no evidence or sidecar files,
connects to no COM/Wi-Fi service, and cannot grant upload permission.
Cross-file ordering, approved backend UUID/idempotency, crash-safe
checkpoint storage, source authentication, live shore Wi-Fi/TLS/REST
and actual end-to-end DTN remain outstanding. No runtime firmware
source or `main` branch is changed.

## 2026-11-22 actual V002 closed-export acceptance: verified on Windows

User-run offline audit (script `scripts/dtn/audit_verified_f06_exports.py`,
commit `7c17cb9`) **PASS** on each of three previously exported, immutable,
SHA-256-verified V002 F06 session copies:

| Closed session | SOS events | Telemetry journal | Diagnostics | Distinct boot tokens |
|---|---:|---:|---:|---:|
| S58BAFE2 | 4 | 24 | 6 | 1 |
| S41F5724 | 0 | 33 | 7 | 1 |
| SB1F7223 | 6 | 47 | 11 | 1 |
| **Total** | **10** | **104** | **24** | **3 session-level counts** |

The tool reported `duplicate_event_keys=0`, `upload_authorization=NONE`.
The worktree at `7c17cb9` was clean after the run. This acceptance is
for **exported-file syntax, identity, integrity and compatibility ONLY**.
It is not on-board SD replay, backend ingestion, proof of RF delivery,
shore Wi-Fi synchronization, or a hardware acceptance. The separate
multi-boot `LEGACY` V002 export remains excluded from this strict
single-session tool and cannot be called verified by this tool.

### Critical ingestion mapping gate: actual record kinds are not API actions

All three closed V002 sessions contain `telemetry_tx` / `telemetry_rx`
journal entries. Those are **radio transmit/receive observations**, not
proof that source weather/GPS measurements are populated or valid.
The proposed backend v1 `kind:"telemetry"` requires separately validated
measurements, origin identity, boot UUID and stable record UUID. Never
silently transform these historical radio observations into measured
weather or GPS readings. A dedicated operational event/audit schema
would need separate backend approval.

Observed SOS journal kinds include `sos_rx`, `sos_ack_tx`,
`cancel_rx`, `cancel_ack_tx`, and `sos_local_start`,
`sos_local_cancel`, `sos_tx_attempt`, `sos_ack_rx`,
`cancel_ack_rx`, `cancel_tx_attempt` in the historical inventory.
The exact isolated-session data have the reported SOS kinds, not
backend-stable `event_id`/`action_id` or source authentication.
**NONE of these historical journal entries may automatically activate,
acknowledge, cancel, or confirm a cloud SOS.** Retain as offline audit
evidence until the team approves correlation, idempotency, authorization
and source-verification rules. In particular, radio ACK is not cloud
ingestion ACK and rescue confirmation must remain a privileged human
operation.

`boot`/`heartbeat` entries in `diagnostics.jsonl` are local diagnostics,
not automatic telemetry or SOS cloud records. Neither `weather` nor
`micro_outlook` nor `research_system`/`research_peer` was demonstrated
in these V002 accepted exports; their exact DTN compatibility still
requires separate evidence from a suitable device/session.

### Next engineering gates, in dependency order

1. Approve a backend ingest schema and source-trust policy distinguishing
   radio-event observations, actual sensor telemetry, SOS device actions
   and diagnostics, without inventing data or treating unverified origin
   as verified.
2. Define a durable per-(registered uploader, vessel, immutable session,
   allowlisted journal, record) replay identity. Existing `boot_token`
   and `event_index` passed local collision checks, but they are **not**
   globally unique, cryptographically authenticated, or cloud UUIDs.
   The proposed production `record_id` and `boot_id` must be assigned and
   persisted at record creation; historical import needs a separately
   approved, collision-safe migration mapping.
3. Extend the pure checkpoint contract from one contiguous cursor into
   **independent per-journal streams plus a bounded persistent
   acknowledgment ledger**. A rejected early item must not erase later
   accepted results. Only compact the acknowledged *contiguous prefix*
   after storing all explicitly accepted/identical-duplicate outcomes.
4. Implement crash-safe independent sidecar storage, an **on-device**
   read-only closed-session adapter, and verified retry semantics.
   Source JSONL journals must never be truncated, rewritten, or deleted.
5. Only after backend contract/security approval: device-specific TLS
   credentials, Wi-Fi/HTTPS shore availability, non-blocking
   SOS-prioritized upload, backend idempotency and physical DTN test.

The validated native host tests and prior six PlatformIO builds remain
separate software gates. Nothing here grants firmware flashing or changes
the deployed V002/V003 images. V001's hardware failure still blocks
physical three-node multihop acceptance.
