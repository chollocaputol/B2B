# F24B — Read-only closed F06 replay seam and optional Arduino SD read port

**Status 2026-11-23:** additive source passed Windows MSYS2 UCRT64 g++ 16.2.0 host validation against
local F24A commit `8ec8bff539d49cb57dbe88bd61552e367ddfb048`.
No firmware runtime linkage, hardware mount, SD write, flash, Wi-Fi, HTTPS,
backend schema change or GitHub push is included.

## Exact existing source integration boundary

The current F06 logger allocates `/THESIS/F06TEST/V%03lu/S%07lX/` once per
boot and appends `sos_events.jsonl`, `telemetry.jsonl`, `diagnostics.jsonl`.
It has one main-loop SD owner, urgent-priority queues and read-only USB export.
F24B **does not** modify `thesis_sd_logger.h` or any of those files.

`dtn_f06_closed_replay.h` provides a caller-owned, read-only `Reader` that:

- Builds the exact allowlisted F06 path from the F24A uploader, vessel,
  closed-session ID and journal stream; diagnostics, invalid IDs and
  arbitrary caller paths are not accepted.
- Requires an external hardware/identity/SD-consent `Gate` to attest that
  the session is **closed**, immutable, belongs to this vessel and is not
  currently being written. The present firmware does not implement this
  authority for production DTN, so **all real access must remain denied**.
- Opens only through a `Volume::openReadOnly` interface; an optional Arduino
  port maps this to `SD.open(exactPath, FILE_READ)` without mounting a card.
  That Arduino port is **not included in any firmware translation unit**.
- Requires an injected `Digest::verifyCompleteSha256` to independently
  hash every source byte and match the F24A sealed full-file SHA256 before
  any record is returned. The host test uses an obvious mock verifier, NOT
  a cryptographic implementation. No backend origin trust follows from SHA256.
- Freezes the file size (max 1 MiB) and refuses change during reading;
  retains the existing complete-line JSONL framer and strict flat-record
  gate. Truncated tails, malformed JSON, wrong vessel and read failures
  never silently advance the replay position.
- Reuses F24A independent per-journal ledger: previously checkpointed
  records and later out-of-order ACKed records are skipped on reboot,
  pending/rejected records remain candidates, and the 64-record window
  blocks rather than guessing. Stream mismatch rejects immediately;
  persisted ACKs outside the immutable source EOF also fail closed.

`Read::CandidateUnverified` is a **readable local candidate**, not a
cloud-approved payload, source-authenticated event, weather measurement,
SOS action, or successful upload. No stable production UUID can be invented
from legacy `boot_token` / `event_index` / FNV or offsets. Historical
`telemetry_tx`/`telemetry_rx` remain RF observations. Never transform them
into measured GPS/weather or cloud SOS emergency actions.

## Test and missing implementation

From repository root, after staging:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic tests/dtn_f06_closed_replay_host.cpp -o dtn_f06_closed_replay_host
./dtn_f06_closed_replay_host
```

The test uses only an **in-memory fake Volume/Gate/Digest**. The fake digest
is intentionally not real SHA256. Tests cover path allowlisting, denied
session, wrong stream, digest refusal, changed byte length, malformed JSON,
wrong vessel, partial final line, 1MiB bound, per-journal separation,
contiguous+out-of-order ACK skips on simulated reboot, and window limit.

Next **device-facing** gates: implement real SHA256 over immutable closed SD
files, provable closure/authorization and main-loop SD ownership; perform
bounded File read latency/profiling with LoRa and SOS preemption; implement
crash-safe F24A sidecar `Store` in a separately approved writable namespace
(with two-slot readback, torn-write and removable-card testing); define
approved stable uploader/record identity and backend per-record ingestion;
then implement authenticated shore Wi-Fi/HTTPS upload. The optional Arduino
`FILE_READ` port is not evidence of on-device operational integration.

No current profile may flash or mount a card based on this host model.
