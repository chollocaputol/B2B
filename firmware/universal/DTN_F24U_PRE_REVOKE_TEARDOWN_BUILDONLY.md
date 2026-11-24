# F24U — Pre-revocation owner-verified SD handle retirement (BUILD ONLY)

**Accepted local parent:** F24T `7911e2abb3b4ef447000482a9620fafb080bc26f`, branch `feature/firmware-multihop`. This is a **DTN-only** phase: V001 is unavailable and multihop is not being implemented. The F24U profile is for PlatformIO **compile only**, never upload or flash. It remains NOT globally RF silent if accidentally flashed, because ordinary firmware radio scheduling is inherited.

## Purpose and narrow implementation

- Adds `DTNF24Q::Arbiter::beginRetirement` / `finishRetirement`, leaving the pre-existing F24Q/F23D/history arbitration semantics intact when retirement is inactive. A verified, matching lease must start retirement before any owner/media permission is withdrawn. Retirement is monotonic for that lease and rejects subsequent new open, child enumeration, metadata/exists, content reads, seeks and writes. An existing append handle may flush, then close, while the **same validated owner and mount epoch** still exist. F23D's verification handle must be closed before its append handle; a history child must close before its directory parent.
- Adds `DTNF24U::Teardown` with `NotStarted`, `DrainRequired`, `ReadyToRelease`, `Released`, `Denied`, and `UnsafeQuarantined` outcomes. The caller (not the coordinator) must explicitly close every outstanding File while authorized. Finishing rechecks the witness and refuses release if any slot remains. A changed owner/media epoch or externally revoked permission permanently quarantines that arbiter instance; there is no forced close or automatic regrant.
- The only firmware instance uses `DTNF24Q::DenyWitness` plus a zero lease on F24R's existing denied SD facade. No hardware-readable grant, real SD operation, runtime owner setup, key, credential or retained operational permission exists in this profile. Ordinary profiles are unchanged by default build flags.

## Critical unresolved physical boundary

This does **not** make Arduino `File` destruction/move/close safe after unexpected SD ownership loss, media removal, or a mount-epoch race. In the host test, `FileHandle::close()` correctly refuses a revoked lease, yet the raw mock `File` destructor still closes without guard permission. Real Arduino libraries may behave similarly. The model also cannot prove that raw close, flush, FAT metadata updates, power-loss durability, or another task's SD call remain atomic with a witness recheck. `File::close()` returning void provides no durability attestation. It cannot force old handles to close after abrupt removal. Report such a case as **UNSAFE_QUARANTINED**, not 'successfully closed' or 'durable'.

The actual device requires an independently verified media identity, a global owner covering ALL file operations including file destruction, and an approved critical section/quiescence policy around real SD driver lifetimes. A disposable card bench fault matrix is then needed; the original V002 evidence card must remain untouched. The accepted work in F24O–F24T already models SOS packet capture, recovery, audit and guarded file paths but does not prove physical durability or final authenticated shore delivery.

## Safety and acceptance

- Host-only: `tests/dtn_f24u_retirement_host.cpp`, source contract test, plus complete F24T/S/R/Q/P/O and F23D regressions. Synthetic positive witnesses are test inputs only and SHALL NOT appear in firmware.
- Windows g++ and PlatformIO ESP32-S3 compile-only verification are required before a local commit. No Git push, physical SD access, RF relay, flashing, Supabase or Docker modifications are included.
- No new DTN packet format or full-62-byte SOS/CANCEL semantic changes. Neighbor radio ACK is not proof of authenticated final shore delivery.
