# F24G — SOS closed-session seal **contract** (build-only, no physical authority)

**Required accepted local parent:** `e6733883496333f6d849333b3ca98bf433f0b593` on `feature/firmware-multihop`, clean. Date: 2026-11-23. **Status:** assistant Linux GCC/Clang ASan+UBSan synthetic checks PASS; Windows MSYS2 UCRT64 g++ 16.2.0 F24B/F24F regressions and F24G 79-check suite PASS; Windows PlatformIO Core 6.2.0 ESP32-S3 build-only PASS (operator-run, 2026-11-23). Local Git commit is validated by the final commit SHA. Original V002 evidence must remain unchanged.

## Confirmed scope

- Normal LoRa packets are for peer communication/distance and are not operational DTN uploads.
- SOS is the only operational DTN source: `/THESIS/F06TEST/V%03lu/SXXXXXXX/sos_events.jsonl`.
- `telemetry.jsonl` under continuous F07 SD mode is for research/trends and is **not** enabled for operational DTN. `diagnostics.jsonl` is excluded.
- SOS journal records are *evidence*. In particular, `sos_rx`, LoRa acknowledgments, TX attempts and relay observations are **not** automatically cloud emergency activations. Stable cloud IDs, origin authentication and operator resolution are separate implementation gates.

## What the new source actually adds

`firmware/shared/storage/dtn_f24g_sos_seal.h` implements a small, portable, deterministic seal *model* with no Arduino/SD/network operations:

1. A `CloseFacts` barrier requiring an independently trustworthy sole SD logger to retire the session, drain pending appends, successfully flush and close **all** source file handles, verify complete final JSONL newline, protect the closed source against further writes, and bind positive media tag, mount epoch, source epoch, close serial and exclusive owner serial. The current firmware **does not yet create these reliable facts**.
2. `prepare(...)` accepts only a previously completed F24D whole-file SHA-256 scan returning `DTNF24D::Result::Verified` under the same owner and exact file-byte count. An `F24D Verified` result proves only the bytes seen during its scan; it is NOT permanent immutability or provenance.
3. A fixed 106-byte, explicitly little-endian `Seal` with version/magic, uploader UUID format, vessel, session, SOS journal, whole-file SHA-256, byte length, media identity tag, source epoch, close serial and CRC32. CRC32 detects accidental/torn bytes; it is not a MAC, digital signature, trusted media ID or physical write lock.
4. An exact sidecar path `/THESIS/DTNSYNC/V###/SXXXXXXX/sos.close` that is **outside** the original F06 source journal tree. The abstract `Store` requires a separately supplied `Gate` and `Port` to load a seal or perform one-time *write-once + readback*; it rejects existing invalid/conflicting seals and never repairs, overwrites, deletes, creates directories or modifies an F06 source. `NoPhysicalSealPermission` always rejects I/O. **There is no real SD Port, grant source or caller.**
5. `recheckAtScan(...)` requires current, independently re-established owner/closure facts and a **fresh** whole-file SHA-256 scan matching the sealed digest, length and media/epoch/serial values. Same-length edits detected by the fresh digest check are denied. A modification after that scan can still escape unless a separate immutable-source enforcement mechanism is proven.
6. `thesis_sd_logger.h` and a new `tbeam-supreme-f24g-sos-seal-deny-build-only` profile **only typecheck** the header. They do not mount SD, write a seal, switch Wi-Fi on, issue an upload, authorize F24F, or change radio behavior. All flags remain OFF by default.

## Critical distinction: software contract vs. authoritative closure

This unit **does not yet create authoritative real-world session closure**. An application can trivially invent the `CloseFacts` booleans or reuse a stale `DTNF24D::Result::Verified`; do not do that. To make an actual closure authority, a separate firmware integration must:

- Create unique per-boot session identity and retire the session within the **actual** `ThesisSDLogger` writer; do not infer closure merely because it belongs to a previous boot.
- Stop appends, drain SOS write queues, successfully flush/close all handles, and verify a final newline under exclusive SD ownership before any seal candidate can exist. In an interrupted-power boot with no proven close record, fail closed and do not retroactively seal the source.
- Derive stable trustworthy media identity and persistent source/change generations from the real card/owner, and increment them on *every possible write*, mount or media replacement. A guessed CID, timestamp, volatile epoch, path, or recycled boolean is not enough.
- Establish exclusive file ownership or a genuinely write-locked immutable snapshot across the hash/read/upload boundary. A single SHA-256 scan (even repeated immediately) cannot guarantee permanence on an editable FAT SD card. No physical media tamper-proof or power-loss durability claim is made.
- Separately authorize exact sidecar **writes** on a disposable microSD, qualify flush/close/readback and torn-power behavior, and later restore fresh read permission after reboot. Actual card readers, `SD.begin`, `SD.open`, CID handling and F24C persistent ACK updates remain unimplemented by F24G.
- Keep `DTNF24F::ClosureProof.independentlyApproved` and `.fullFileSealed` **false** until independent authority is actually verified. F24G intentionally has **no API** that constructs a permissive `DTNF24F::ClosureProof`; even a valid seal is not a replay grant.

## Synthetic test matrix

`tests/dtn_f24g_sos_seal_host.cpp` executes bounded native tests with a synthetic in-memory SOS journal and mock sidecar only: default deny (no SD operations); telemetry excluded; retirement/append/flush/close/newline/write-lock prerequisites; owner/media/mount/source/lease checks; incomplete hash and byte mismatch; exact little-endian wire encode/decode; torn/truncated/corrupted seal; write-once, identical retry, conflict and refusal to overwrite; SHA-256 recheck on a **same-size mutation**; removed/exchanged card or mount change; and inability to convert a prepared candidate into F24F permission.

On the assistant's Linux environment, native GCC and Clang ASan+UBSan completed 79 checks using the exact F24F and F24D headers from the previously supplied patch archives and **temporary compiler shims** for two deeper F24B dependencies. The Windows staging script will repeat the test **against actual committed repository dependencies**, including the real F24B headers. No physical SD test has been run.

## Acceptance/holds

| Gate | Current result |
| --- | --- |
| F24A–F24F accepted local HEAD | `e6733883496333f6d849333b3ca98bf433f0b593` (operator reported) |
| Assistant Linux host tests | PASS: 79 synthetic checks; sanitizer PASS |
| User Windows repo native tests | PASS: F24B and F24F regressions + F24G 79 checks (2026-11-23) |
| User PlatformIO firmware build-only | PASS: Core 6.2.0 ESP32-S3 compile only (2026-11-23) |
| F24G local Git commit | Verify exact parent, scoped commit and clean tree in operator output |
| Authoritative logger closure, real media/lease, sidecar durability | PENDING |
| Original V002 journal physical replay / firmware flash | NOT AUTHORIZED |
| Wi-Fi provisioning, GPS geofence, HTTPS SOS upload, cloud ACK | PENDING; production RLS/security hold |

**DO NOT FLASH:** the F24G build-only profile inherits normal operational radio transmissions if installed; it is not globally RF-silent. No production cloud schema or GitHub push is changed.
