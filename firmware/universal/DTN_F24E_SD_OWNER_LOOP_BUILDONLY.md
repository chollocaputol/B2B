# F24E — Cooperative SD-owner scheduling preview (BUILD ONLY)

**Baseline:** locally accepted `feature/firmware-multihop` at `eab7cc5c0abc33b1ddb7ad64dab857ba34b00345` (F24D Windows GCC and PlatformIO ESP32-S3 build-only PASS, 2026-11-23).

**Status:** Windows MSYS2 UCRT64 g++ 16.2.0 native host test PASS and PlatformIO Core 6.2.0 ESP32-S3 build-only PASS (user-run, 2026-11-23). Physical SD permission, device validation, immutable-session proof, runtime replay and backend synchronization remain PENDING. No firmware uploaded or real microSD authorized.

## Added behavior, strictly bounded

- `dtn_f24e_sd_owner_scheduler.h` is a portable (Arduino-free) main-loop state machine wrapping the existing F24D SHA-256 job. A caller provides an externally approved F06 closed-session Stream and a **RAM-only** `Gate::allowClosedJournal` decision; `queue()` never accesses SD. The gate must verify hardware/media continuity, vessel and uploader ownership, closed immutable session and consent. **No gate implementation or grant is supplied.**
- `poll(gate, sdOwnerGranted, urgentOrRadio, budget)` returns without ANY File/Volume I/O when the SD owner is unavailable or SOS/radio work has priority. Once permitted and unpreempted, it opens the exact allowlisted F06 file using F24D's read-only transport. Each polling invocation hashes at most 512 bytes, rechecking access and length during scanning. A previously verified file may NOT be assumed immutable later without a separate seal/closure proof.
- A revoked gate is latched fail-closed. When another task owns the SPI bus, even closing the open file is deferred until SD ownership returns. `reset(sdOwnerGranted)` refuses to close/reuse an open handle without owner permission. Device use requires a static scheduler lifetime under a real SD-owner lock.
- `thesis_sd_logger.h` receives a **default-OFF** compile flag and a `poll()` hook. In the F24E preview it instantiates an **IDLE** scheduler with the existing F24D **deny-all permission** and calls `poll()` while explicitly setting `sdOwnerGranted=false` and `urgentOrRadio=true`. It never queues a request; no new SD API call, mount, SPI access, HTTP, RF relay permission, NVS update, sidecar write, or file export occurs.
- The `tbeam-supreme-f24e-owner-loop-deny-preview-build-only` PlatformIO environment inherits F24D's build-only profile. The inherited operational RF remains present if flashed: **DO NOT FLASH.** The only accepted run is `platformio run -d firmware/universal -e tbeam-supreme-f24e-owner-loop-deny-preview-build-only` without `-t upload`.

## Acceptance tests and known limitations

The host test exercises denial before open, malformed stream, queued/preempted states, absent SD-owner lease, per-step bounded hashing, verified SHA digest, media permission revocation while another task owns SD, state reset, changed file size, changed complete-file digest, missing read-only source, and source read errors.

**Important test demonstrates a remaining TOCTOU gap:** same-size source mutation **after** an already-hashed segment may evade a single scan. A matched digest is NOT a trusted, enduring upload approval. The immutable closed-session proof, media fingerprint/continuity and replay-time seal binding must be implemented and validated separately. F24B `Reader::begin()` is still synchronous and does not automatically inherit F24D's hash result; do NOT substitute a digest-bypass adapter. F24A ACK ledger, F24C sidecar and F24B replay are **not** yet connected to this scheduling hook. No real card access, test-card power-loss characterization or authenticated shore Wi-Fi/HTTPS exists in this preview.

All original `/THESIS/F06TEST` journals including the historical V002 evidence remain untouched. Sidecar writes, diagnostics uploads, journal repairs and source-file edits are prohibited. Source file integrity is not proof of LoRa-origin authenticity, weather measurement provenance, or server acceptance.

## Next implementation gate

1. Independent review of a real `Gate` implementation: sole-SD-owner arbitration, current/live-session exclusion, immutable closure manifest, hardware/media ID, identity authorization, emergency priority, explicit user consent and safe removable-media handling.
2. Disposable test-card-only validation of SHA-256 and restart/power-loss behavior, with no real V002 historical media.
3. Nonblocking verified replay/candidate mapper bound to the same immutable snapshot, separate per-journal F24A/F24C checkpoint, then a fully specified authenticated backend protocol and production RLS fix. Only a verified per-record server reply can advance the ACK ledger; generic HTTP 200 and LoRa ACK are insufficient.

**Source status:** Windows acceptance and Git commit **pending** until independently verified after staging. This build-only code is not a physical permission candidate.
