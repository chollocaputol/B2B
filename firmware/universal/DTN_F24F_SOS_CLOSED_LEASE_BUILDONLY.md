# THESIS F24F — SOS-only close-evidence lease and replay-time denial (build only)

**Exact local parent:** `9844fe44113b15df101d0fba308ebd815c328f4a`, `feature/firmware-multihop`, clean tree, no GitHub push. **Status: assistant Linux native host tests PASS; Windows MSYS2 UCRT64 g++ 16.2.0 F24B regression and F24F SOS-only lease host tests PASS; Windows PlatformIO Core 6.2.0 ESP32-S3 build-only PASS (user-run, 2026-11-23).** This is a default-deny policy typecheck, not physical DTN, end-to-end upload or an immutable-session proof.

## Operational scope

- Normal LoRa packets provide peer/distance information only and are **not** routine DTN records.
- Only `sos_events.jsonl` is an operational DTN source candidate. F06 `telemetry.jsonl` remains an optional continuous research journal; `diagnostics.jsonl` is never a production SOS source. **No record reaches a cloud API in F24F.**
- `sos_local_start` is an activation *candidate* and `sos_local_cancel` a cancellation *candidate*. `sos_rx`, ACKs, radio relay attempts and other SOS-related entries are **radio evidence**, not automatically authenticated cloud emergency activations. Unknown kinds are unsupported.
- The current F06 files do not supply production UUIDs or verified LoRa-origin authentication. Stable backend identities and the allowed action state machine remain future work.

## What F24F actually adds

1. `dtn_f24f_sos_closed_lease.h` is a **pure in-memory fail-closed contract** that binds the exact SOS path, authenticated-uploader UUID format, vessel, session, complete-file SHA-256, exact byte length, source epoch, media tag, mount epoch, and exclusive owner-lease serial. The closure `ClosureProof` is externally supplied and must attest an independent approval, completed and closed journal, complete JSONL line tail, no remaining logger writes, and read protection. `OwnerNow` must remain continuously current and signal urgent/radio work. Any failed check permanently revokes that `SosClosedLease` instance.
2. F24B `Reader::begin()` now checks `gate.allowClosedJournalBytes(stream,path,size)` after opening the file; it rejects wrong byte counts before a digest scan.
3. F24B `Reader::next()` **now requires the Gate argument**; it checks permission before any File call and for each byte read, and again before publishing the candidate. The candidate is cleared on each invocation, and permission loss is latched. An existing caller of `Reader::next(ledger,...)` must migrate to `Reader::next(gate,ledger,...)`.
4. Host regression tests exercise zero permissions, wrong journal/session/uploader/media/epoch/owner, active logger, pending writes, urgency, byte-count mismatch, lease loss between records and **during JSONL byte framing**, and regrant attempts after revocation.
5. `thesis_sd_logger.h` + new `platformio.ini` profile compile the default-denied contract inside the existing main-loop owner; they DO NOT schedule a file open, install an SD permission source, call `SD.begin`, use Wi-Fi, or change LoRa behavior. All new flags default to OFF.

## Important remaining security and functional holds

**A synthesized `ClosureProof` made from boolean flags does not provide trust.** No current runtime component actually emits a durable independently verified immutable close manifest or a physical card identity proof. A media tag / source epoch is safe only when derived and updated by an independent trustworthy SD owner, not from stale RAM or a guessed file timestamp. A same-size edit that does not increment the externally supplied source epoch can still escape detection. A digest of the source at one time does not guarantee it stays unchanged. Existing V002 original exports must not be marked authorized retroactively or modified.

The F24B `Reader::begin` digest interface remains **synchronous** and must not run inside the live radio/SOS poll. F24D/E's bounded hash is scan-time integrity only. Do not use a mocked digest or force `fullFileSealed=true` to enable an upload. Caller must hold the sole physical SD owner when `begin()`, `close()` or destruction touches a File; after lease loss, postpone handle cleanup until the exclusive owner is restored. No real device Reader is constructed by this preview.

Physical FAT card-removal and power-loss behavior, immutable snapshot creation, authenticated HTTPS device credential verification, per-record stable UUIDs and full Supabase RLS/audit design are **NOT implemented**. F24C sidecar write/reboot durability remains physically unverified. A LoRa ACK and an HTTP 200 without independently verified item results do not advance the cloud ACK ledger. Viewer/operator frontend roles remain separate from these software gates.

## Hardware warning

**DO NOT FLASH.** Profile `tbeam-supreme-f24f-sos-close-lease-deny-build-only` inherits normal operational radio transmissions if flashed. This is compile-only. No live RF relay permission, SD source access, Wi-Fi provisioning, cloud credentials, backend deployment or original SD mutation is authorized.

## Acceptance

- Assistant Linux g++/Clang native tests: PASS (synthetic in-memory fixtures only).
- Windows MSYS2 UCRT64 C++17 host tests: PASS (user-run, 2026-11-23; F24B regression + F24F lease).
- Windows PlatformIO Core 6.2.0 ESP32-S3 **build-only**: PASS (user-run, 2026-11-23).
- Real immutable close evidence, card owner, physical SD, sidecar durability and operational HTTPS: PENDING.
