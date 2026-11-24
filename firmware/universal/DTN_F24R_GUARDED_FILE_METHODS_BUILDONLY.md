# THESIS F24R — Actual retained-File method adapter (BUILD ONLY)

**Parent required:** F24Q `40c86f72bc0f43c33023a5385e1943f28ec45920` on `feature/firmware-multihop`, clean. F24R is a *candidate* until verified on the operator's Windows repository. **DO NOT FLASH**: the inherited operational LoRa radio schedule remains present; this is not RF-silent.

## Changed source and exact scope

- `dtn_f24r_guarded_file_port.h` wraps actual `File` operations (`read`, `write`, `flush`, `size`, `seek`, `getWriteError`, `close`, `isDirectory`) in F24Q's already fail-closed lease arbiter. The new F24Q `probe(Lease)` checks its independently sampled witness **before** unbound volume `exists()` or `open()`, closing the F24Q entrypoint hole without bypassing fault latching. A lease must already be acquired independently. No witness or physical permission provider is supplied.
- `ThesisSDLogger` F24R-only profile uses that adapter for the **two real retained read handles** `exportFile` and `historyFile`, including both calls that acquire them. It binds an always-invalid `DenyWitness` and empty lease to a permanently denied F24K volume. The `poll()` preview also attempts only a denied open; **no physical `File` can be opened**.
- `f23d_sd_file_writer.h` allows the same guarded `File` type at the actual persistent append, readback, and final verification call sites. In ordinary builds `F23D_SD_FILE_TYPE` defaults to the original Arduino `File`. Host tests compile and execute the **actual unchanged F23D writer control flow** with the guarded volume, synthetic witness, and two File slots. The firmware F24R profile forbids F23D storage, so the host-positive witness is not deployed.
- `platformio.ini` adds `tbeam-supreme-f24r-file-handle-deny-build-only`, extending the F24Q/F24P/F24O/F24N/F24M/F24K permanent denial chain. All earlier profiles retain their original flags and behavior.

## Evidence and limitations

Host evidence planned: F24R actual writer / retained reader positive mock and revocation tests (66 assertions); F24Q cross-writer (307), actual F23D default/named (20+20), F24P (133), F24O (266), totaling **812 native C++ assertions**, plus 21 source-contract checks. **Windows acceptance and ESP32-S3 compile-only are pending** until the operator runs `TEST_F24R.ps1`.

- **Not global interposition.** Other temporary logger Files, startup verification, diagnostics, directory iterators, `appendRecord` and legacy audit still use raw methods. F23D's guarded type is exercised **host-only**, not run as an enabled SD writer in the F24R firmware profile.
- **No safe physical teardown.** Even if `close()` is refused on revocation, the destructor of an underlying Arduino `File` may still perform file-system cleanup on lost media/lease. Mock behavior, RAII and F24O's mandatory-close contract cannot yet establish an exclusive hardware owner. **No real authority, card, mount, or media continuity is implemented**, and no valid physical `allPathsInterposed` assertion can be made.
- `bool` conversion/metadata probes and each guarded method check the lease. Host-positive facts are synthetic. A card's true CID/identity, FAT flush/fsync, crash consistency, session sealing, post-reboot recovery, trusted HTTP per-item server receipt, SOS RF relay and user authorization remain pending.
- No original V002 card or historical evidence may be modified; no flash, RF relay activation, Supabase/RLS deployment, cloud credentials, Wi-Fi provisioning or GitHub push.

## Operator acceptance

Guarded staging against exactly the F24Q full SHA; verify all four modified files and four added files. Run native F24R, prior F24Q/F24P/F24O and strict source-contract tests, then `platformio run -d firmware/universal -e tbeam-supreme-f24r-file-handle-deny-build-only` **without any upload target**. Review Windows report and diff before a separately prepared *local-only* commit. The physical integration/teardown milestone must remain OPEN after this candidate.
