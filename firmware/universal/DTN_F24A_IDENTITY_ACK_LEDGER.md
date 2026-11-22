# F24A — DTN per-journal ACK ledger and production identity boundary (proposal)

**Status 2026-11-23:** new source and host test **PASS on the assistant's Linux C++17 toolchain and user-run Windows MSYS2 UCRT64 g++ 16.2.0**. Arduino/PlatformIO integration, durable sidecar storage, device upload and production backend **NOT YET DONE**. No changes to existing firmware, LoRa, physical permission, user SD, Auth, Supabase cloud or `main`.

## Why this new unit is needed

`dtn_sync_checkpoint.h` handles only a contiguous prefix. In a partial batch such as `Accepted, Rejected, Accepted`, its later accepted result is discarded from the persisted cursor. The next reboot can needlessly resend it. A safe DTN queue must retain a bounded out-of-order ACK mask for each immutable closed session **and for each journal independently**, without marking rejected data synchronized.

New files: `firmware/shared/storage/dtn_ack_ledger.h` and `tests/dtn_ack_ledger_host.cpp`. The prior checkpoint/framer/validator/export audit remain unchanged.

## Identity and authorization preconditions (NOT satisfied by legacy F06)

The `Stream` key contains the **server-registered HTTP uploader UUID**, vessel ID 1..9999, exact closed `S[0-9A-F]{7}` session, either `sos_events.jsonl` or `telemetry.jsonl`, and full SHA-256 of that immutable complete journal. The ledger rejects diagnostics. A stream's SHA-256 is a snapshot identity and accidental-change guard, **not device-origin authentication**. The caller must verify it against sealed source bytes before allowing replay.

Each *new production record* needs a cryptographically random UUIDv4 (or reviewed UUIDv7) assigned **once at record creation**, durably stored with the JSONL event and reused across retries; an immutable per-boot identity must likewise be persisted. The ledger **does not mint IDs**, extract new IDs from legacy `boot_token`/`event_index`, or call local FNV checksums secure. Historical F06 exports need a separately approved server-side migration/identity and source-trust policy. No legacy `telemetry_tx`/`telemetry_rx` is automatically weather/GPS telemetry, and no SOS journal record is automatically a live cloud emergency action.

Each in-memory submitted `Batch` contains immutable `recordId`s paired with journal ordinals and a stable `batchId`. Current prototype restricts a batch to **one journal, one closed session and at most 32 records**, even if an eventual backend API supports mixed kinds. The `Response` must have the same batch UUID, the authenticated uploader UUID, schema version 1, exactly one result for every submitted UUID and no extra/duplicate IDs. Results can arrive in a different order. **The server must guarantee `accepted` means committed and `duplicate` means identical data already durably committed; same UUID with changed content is rejection/conflict.** The module does not prove these server properties or parse HTTP JSON.

`verifiedBackendResponse=true` is not inferred from HTTP 200, Wi-Fi, LoRa receipt/ACK, user-visible status, or an untrusted response body. The future TLS/HTTP adapter may pass true only after certificate verification, HTTP-level authentication, source/identity checks, response parsing, and exact pending batch matching. The caller must not display an uplink as successful based on the ledger alone.

## Crash/replay invariants

- One independent `Ledger<Store>` and **separate durable sidecar namespace** per `Stream`. Never reuse two slots across SOS and telemetry. Changed journal hash/session/uploader/vessel must fail closed (`WrongStream`). A source journal is immutable and never truncated, edited or deleted.
- Snapshot contains `firstPending` (first ordinal not in contiguous accepted prefix) plus 64-bit `ackMask` indexed by ordinal from `firstPending`. Later accepted/identical-duplicate results are persisted even if earlier items are `rejected`, `retry`, or unacknowledged. A gap is never skipped. Every accepted/replayed ID must have passed exact batch comparison.
- Once the earliest gaps are acknowledged, the mask shifts and the contiguous prefix advances. The ledger limits work to the first 64 ordinals and up to 32 items per reply; ACKed later ordinals are ineligible for repeat upload within this model. No allocation or Arduino dependency.
- Two alternating snapshots use generation, CRC32 of logical fields and read-after-write verification. CRC32 detects ordinary corruption/torn writes but **is not a cryptographic authenticator**. Write error or unverified write faults the in-memory ledger; reload from the most recent valid slot before further work. Older valid slot recovery can resend an already accepted record; server idempotency is therefore mandatory.
- `Store::write` MUST durably write a complete sidecar slot and verify media synchronization (not just return from buffered `File.write`). It must protect against torn overwrites and cannot overwrite both slots in one operation. An Arduino SD/NVS adapter and unplug/power-fail tests do not yet exist. A full SD/failed write must block checkpoint advancement, not silently count as success.
- Generation overflow and the ordinal ceiling stop progress rather than wrap. The ledger's `load()` checks two-slot consistency and a monotonic prefix; mixed sessions or incompatible streams fail closed.

## Test scope and execution

Native command from the repository root, after staging the two new C++ files:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic tests/dtn_ack_ledger_host.cpp -o dtn_ack_ledger_host
./dtn_ack_ledger_host
```

Assertions cover per-journal independence, partial/out-of-order batch statuses, rejected gaps, reboot preservation, duplicate/unknown ID injection, wrong uploader/batch/schema, stream-digest changes, crash or torn alternate slot, full-media simulation, CRC corruption, rollback and 64-window rollover. Expected output: `F24A DTN per-journal ACK persistence / replay / corruption: PASS`.

**Remaining gates:** reviewed production ingestion schema and source trust; securely provisioned/stored device credentials and UUIDs; SD-file reader and crash-safe sidecar adapter; verified backend per-record idempotence; Wi-Fi/HTTPS transport with SOS/LoRa preemption and no unauthorized RF; reboot-and-power-loss hardware tests; firmware builds; separate authorized physical acceptance. F06 historical synthetic importer remains a separate accepted local archive gate, not the operational DTN API.
