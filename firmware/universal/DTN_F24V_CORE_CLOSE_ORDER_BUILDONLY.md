# F24V — Arduino-ESP32 3.3.12 core-aware close order (build-only)

**Accepted parent:** F24U `4dfc65688a6f39a0f44cca2070f25538daf3c9f5`; branch `feature/firmware-multihop` is only a historical name. **Active scope: DTN SOS/CANCEL. Multihop paused; V001 unavailable.** No new packet layout, SOS relay logic, hardware grant, or cloud behavior.

**Installed framework evidence:** 2026-11-24 read-only audit of `C:\PlatformIO\packages\framework-arduinoespressif32`, package version 3.3.12; evidence ZIP SHA-256 `9EBCF27BD983ADA6A5A0B3251DE90BEF025DFE88327172F0312215F85D1DBA0F`. F24V `test_dtn_f24v_source_contract.py` requires hashes of all **11** captured framework source/provenance files before passing. `tests/dtn_f24v_core_fs_host.cpp` compiles **that installed `FS.cpp`**, with a host-only Arduino `Stream` shim and an instrumented `FileImpl` for negative tests. This does *not* substitute for compiling the full VFS implementation or physical behavior.

## Narrow source change

- F24Q `Arbiter::permitClose()` performs a fresh lease/media witness check and verifies parent-child order **without releasing the handle slot**.
- Only in the new `THESIS_DTN_F24V_BUILD_ONLY=1` profile, F24R `FileHandle::close()` attempts backend `RawFile::close()` **after** permitClose, then calls the existing arbiter close operation with a **second witness check** before clearing the slot. A witness lost during raw close leaves the slot quarantined, not incorrectly recorded as drained. Old profiles retain their original close path.
- `THESIS_DTN_F24V_BUILD_ONLY=1` requires the existing permanent F24K interposition deny, zero F24Q lease, F24U/T/S/R/Q enabled, F23D physical evidence/fleet/relay/outputs disabled. The header declares no physical SD permission, destructor certification or durability claim.

## Confirmed, still-open hazards

1. `fs::File` holds `std::shared_ptr<FileImpl>`; copies alias the same backend. Last-reference destruction of `VFSFileImpl` calls its `close()` and then potentially `fclose`/`closedir` **outside the F24R guard**. This persists after F24V.
2. Arduino `File::close()` returns `void`; `VFSFileImpl::flush()` and `close()` do not provide dependable `fsync`/`fclose` success back to this API. The two-phase F24V close is **attempt order/accounting**, not proof of successful close or committed data. Physical media could disappear *during* raw close.
3. `SD.end()` is independent of F24Q's slot inventory. The global mount owner, task serialization, raw alias prevention, and unmount synchronization are unimplemented. A synthetic witness is not physical owner evidence.
4. Failed post-open witness and unexpected lease loss remain unsafe: an automatic `File` destructor can still call raw close after owner loss. Host tests **reproduce** the hazard; they do not cure it.
5. No hardware or on-card durability/retention, restart/power loss, media swapping, SOS/CANCEL replay, or authenticated shore ACK was tested. These require separately accepted protocols and a disposable card.

**Disposition:** F24V build-only software correction. Windows g++ (including actual installed `FS.cpp`) and PlatformIO `run` compile-only must PASS before local commit. No flash, original V002 SD access, remote GitHub push, cloud deployment, RF-relay enable, or hardware owner grant. Inherited firmware has routine scheduled radio transmissions if flashed accidentally, so it is NOT globally RF silent.
