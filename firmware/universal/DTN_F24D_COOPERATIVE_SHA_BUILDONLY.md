# F24D — cooperative closed-F06 SHA-256 SD read bridge (BUILD-ONLY)

**Source parent:** local feature commit `8487f683178d0300f3f45f6eb1e0ccd531e22e8e` (2026-11-23). **Status:** Windows MSYS2 g++ 16.2.0 native host tests PASS and PlatformIO Core 6.2.0 ESP32-S3 build-only PASS (user-run, 2026-11-23). Hardware authorization, physical SD testing, runtime integration and end-to-end DTN acceptance remain PENDING. NOT hardware-authorized, NOT uploaded to a device, and NOT a DTN end-to-end success.

## What is new

- `firmware/shared/storage/dtn_sha256_portable.h`: fixed-memory FIPS 180-4 SHA-256 implementation for file integrity. This is not a signature, source-auth proof, user enrollment or TLS.
- `dtn_f24d_closed_sha256_job.h`: cooperative closed-journal file verifier. Uses F24A stream identity, F24B's exact allowlisted source path, and a **caller-owned external closed-session gate**. Every `step()` processes **at most 512 bytes** through an immutable read-only handle, including a SOS/radio-work pause. Gate and file size are checked before every read. SHA-256 must match the stream's sealed digest. Mismatch, missing, length mutation, I/O failure, or permission revocation fail closed.
- `dtn_f24d_arduino_read_port.h`: optional actual Arduino `SD.open(path, FILE_READ)` transport and bounded file reads. No SD mount, formatting, directory creation, source write, sidecar write, radio transmission or Wi-Fi.
- `thesis_sd_logger.h` is modified in two tiny **compile-gated** areas only. With the new macro default **0**, previously accepted firmware execution is unchanged. Under `THESIS_DTN_F24D_BUILD_ONLY=1`, the parser/port template is included and explicitly instantiated only via a *deny-all* preview that cannot open a file. No function is called from `poll()`, `begin()`, SOS or LoRa.
- `platformio.ini` receives one new **build-only** environment. It inherits the existing fleet build-only profile, which has existing normal radio transmission behavior if flashed. **DO NOT FLASH the profile.**

## What is deliberately not solved

This does NOT prove a session is closed or source-owned, enforce SD-media identity, handle an SD card swap, authenticate an origin/registered HTTP device, or provide a production record UUID. It does not add an actual authorized SD read scheduling path to runtime, integrate F24B `Reader::begin()` with this incremental digest, bind F24C sidecar I/O to a protected SD owner, or implement shore Wi-Fi/HTTPS/upload/ACK mapping. Even if a hash matches, source authenticity is **UNVERIFIED**. A same-size mutation after a successful scan is not detected by this offline object; a stronger immutable-session seal/closure gate and revalidation before use are required. SD/FAT `flush()` and readback are not a power-loss guarantee.

The existing F06 logger stores RF observations (`telemetry_tx`/`telemetry_rx`) separately from genuine measured weather, and all historical SOS must remain archive-only. No F06 source journal, closed V002 export, live operational emergency table or `main` branch may be modified by this patch.

## Next milestone

After Windows host **and PlatformIO build-only** evidence: review real closed-session permission and media continuity, attach a cooperative read schedule to the existing single SD owner with SOS preemption, and validate actual SHA-256 on an explicitly disposable/test SD (not the V002 evidence card). Only then wire replay candidate -> production mapper -> authenticated HTTP batch -> verified per-record backend reply -> F24A/F24C checkpoint. Production RLS and backend API remain gating.
