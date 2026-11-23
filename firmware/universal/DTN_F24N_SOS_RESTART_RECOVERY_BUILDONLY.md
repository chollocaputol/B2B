# F24N — SOS-only recovery from F24M packet journals (BUILD ONLY / SD DENIED)

**Accepted parent required:** `bcb20ce44dcf5132a3e21075c2dff06351945903` on `feature/firmware-multihop`, with a clean Git worktree. F24N changes *only* `thesis_sd_logger.h`, `platformio.ini`, and adds this note, `dtn_f24n_sos_restart_recovery.h`, and `tests/dtn_f24n_sos_restart_e2e.cpp`.

## Real data contract and scope

Only `PACKET_SOS=2`, with exact 62-byte `VesselPacket` payload, is used as a DTN record. It already contains all populated GPS, weather, origin, timestamp/uptime, battery and message fields. Ordinary 25-byte POSITION v2 broadcasts and continuous weather/GPS/system/peer research logging are **not** separate DTN records, delivery jobs or network uploads. A LoRa `PACKET_SOS_ACK=3` is *direct-neighbor* receipt only; never final delivery.

## What is implemented

1. **Portable `DTNF24N::Replay<32,12>`** recognizes prior closed F06 sandbox session paths, reads the exact F24M `dtn_sos_packets.f24m` frames without modifying them, and reconstructs in-memory `(origin,event)` SOS records. It reads at most one 88-byte frame per `poll`; file handles close each time. A conflicting same-ID payload, invalid CRC/version/type, torn or growing closed file, too many sessions/packets or I/O error halts replay and makes pending records unavailable.
2. **Separate 40-byte F24N receipt journal format** (`dtn_shore_receipts.f24n`) records a purported shore-server response without mutating F24M source journals. The `ReceiptWriter` writes to a *new* session only, flushes/closes, reads the bytes back, and does not mark delivered unless an externally supplied verifier independently confirms the exact `(origin,event,payload CRC,server receipt ID)` response. CRC detects accidental corruption, **not authentication**. A missing verifier returns `Unverified` and leaves the SOS pending.
3. **Actual firmware hook in `ThesisSDLogger::poll()`** prepares prior closed session names under the vessel's F06 sandbox and uses the incremental reader after the existing urgent storage poll. The build is hard-locked to F24M + F24K's **permanent SD DENIAL**, F23D independent writer OFF, and F06 sandbox. `SD.begin` cannot grant `ready`, and `DTNF24N::DenyReadAuthority` and `DenyReceiptVerifier` *always* deny source access and final receipt credit. The type-checked hook is not physical authorization or an operational replay path.
4. **End-to-end host workflow uses the same F24M 88-byte frames and F24N 40-byte receipts** with real disposable host files in separate process invocations: SOS capture → offline retention → simulated server acceptance and lost response → idempotent retry → durable receipt persistence → restart recovery → corruption/conflict and untrusted receipt rejection. The fake shore server is a LOCAL FILE, not an HTTP endpoint or cryptographically authenticated service.

## Key restrictions / remaining work

- Build-only profile: `tbeam-supreme-f24n-sos-recovery-deny-build-only` extends the existing F24M/F24K permanent-denial chain. **DO NOT FLASH**; inherited routine RF transmission exists and this image is not globally radio-silent.
- F24N **does not** establish physical SD ownership, SD media identity, lease renewal, FAT `fsync`/power-loss atomicity, actual durability, F23D writer coordination, end-to-end TLS, Supabase acknowledgments, actual LoRa multihop RF or rescue receipt authenticity. The original Vessel-002 historical SD must remain untouched. No physical operations are authorized.
- Past session discovery is bounded (up to 12 prior sessions, no more than 64 directory entries examined); any excess/unknown directory blocks rather than silently omitting source. This is a preview, not a validated worst-case loop timing proof.
- A CRC-only receipt is **never** evidence of final shore delivery; the firmware's default receipt verifier denies. Authentication and production idempotency must be provided separately, then verified after cold restart.
- The source `timestamp` field is currently uptime, not a verified UTC clock; do not mislabel it in research analysis.
- The stage script runs 391 portable host assertions across eight independent process launches, F24M 93 checks, and F24L 36 assertions, then copies exactly five files. **Windows host and ESP32 compile-only still require validation before a commit.**

## Acceptance order

(1) Run the pinned archive's guarded PowerShell staging script on a clean `bcb20ce` worktree; (2) run PlatformIO compile-only for `tbeam-supreme-f24n-sos-recovery-deny-build-only`; (3) inspect **five** intentional file differences (2 modified + 3 new) and supply both logs; (4) perform a separately prepared guarded **local-only** commit. No push/flash/media/network/cloud actions.
