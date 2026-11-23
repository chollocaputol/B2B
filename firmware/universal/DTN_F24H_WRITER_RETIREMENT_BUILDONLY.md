# F24H — Writer-side SOS session retirement protocol (BUILD-ONLY)

**Required local parent:** `bafb8ea9cf70a1c3ff767b746d732e17327b3a01` (F24G) on clean `feature/firmware-multihop`.
**Scope:** SOS operational DTN only. Normal peer LoRa packets are not uploaded. Continuous telemetry/weather research logging is separately retained, not operational DTN input.

## What is implemented

- `dtn_f24h_writer_retirement.h` uses a pre-close `SessionKey` without SHA-256 (the digest is calculated only after the writer has retired); `bindPostHash()` binds candidate facts to the later complete journal digest only after a separate SHA-256 scan. The header defines a portable, explicit state machine: `Untrusted` -> `Draining` -> `Closing` -> `Checking` -> `RetiredCandidate`, or permanently `Failed`. It provides a `Candidate` containing F24G `CloseFacts`, NOT a verified F24G seal or F24F replay permission.
- A future sole SD-owner `Port` must quiesce all producers, preserve queued SOS and research records, drain one urgent record per poll (SOS first), then research records to close the *whole* current F06 session, complete flush and handle closure, verify the final SOS JSONL newline, and protect the source against future writes. Nothing may drop queued records merely to pass closure.
- Before/after every potential port operation, the code checks vessel, stream, lease, media tag, mount epoch, close serial, and monotonic source epoch, and fails if SOS, radio, export/history activity, writer in-flight, lost owner, or a newly pending append appears. Every append must advance the source epoch and remove exactly one queue element after a successful durable append.
- A new `Controller` after power loss has **no retained closure evidence**; existing historical sessions are never retroactively sealed merely because they are no longer the active session. Any failure clears candidate facts.
- Firmware `ThesisSDLogger` build-only branch reads **only the actual in-RAM queue counts** into a deny-only preview. It never calls a Port, never quiesces producers, flushes, closes or writes a file, mounts an SD card, reads a journal, or grants a lease. All compile guards are off by default.
- New PlatformIO profile `tbeam-supreme-f24h-writer-retirement-deny-build-only` inherits F24G and is **DO NOT FLASH**. It is not globally radio-silent if installed.

## What is NOT implemented (hard hold)

The code is a **software model of the writer's retirement sequence**, not a physical implementation. There is **no real SD Port**, no trusted device/card identity reader, no durable monotonic source/close serial, no actual source write lock or immutable snapshot, no power-loss verified FAT flush/close, and no real F24G sidecar writes. The in-memory example's positive evidence is synthetic, not proof. The current logger's append/close file pattern and queues must be integrated behind a reviewed SD-owner adapter before any actual retirement happens.

No F24F `ClosureProof.independentlyApproved` or `fullFileSealed` is set by F24H. No production backend auth, Wi-Fi provisioning, geofence, HTTPS upload, record ACK, cloud deployment or dashboard action is enabled.

## Negative validation matrix

`tests/dtn_f24h_writer_retirement_host.cpp`: 121 GCC/Clang host checks against a synthetic Port, including two-FIFO drain order, valid candidate fields, no rewrite after retirement, restart without proof, new SOS/radio preemption, export/history conflicts, unproven media, owner/lease/epoch changes, failed/incorrect append, queue loss, a write still in flight, failed flush, missing final newline, failed readback/inspection/protection, same-lease unexpected writes, and revoked candidate after additional append. F24B/F24F/F24G regression suites should also pass on the operator's Windows repository.

## Acceptance checkpoints

1. Guard exact F24G SHA parent and clean local worktree. Windows host tests against actual locally committed F24A–G files.
2. Stage exactly FIVE changed files (2 modified, 3 added), with no commit or push.
3. PlatformIO ESP32-S3 **compile only** for new build-only environment.
4. Only after reviewing logs and exact staged changes: separately guarded local commit. No flash or real card operations.
5. Separate later project gate: real writer owner/port design and disposable sacrificial microSD durability test, only after **explicit physical permission**. Original V002 card and recorded F06 evidence are excluded.

**Never classify this host mock as authoritative immutable source closure.**
