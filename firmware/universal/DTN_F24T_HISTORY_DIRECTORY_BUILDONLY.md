# F24T — Guarded F06 history directory iteration (BUILD ONLY)

**Required accepted local parent:** F24S `00ffd4903a1ecb60cadb578c6cfce81936355d92` on `feature/firmware-multihop`. Despite the branch name, this phase is **DTN storage/recovery only**: there is no multihop implementation, experimental RF authorization or dependence on V001 availability. Use the phase-specific PlatformIO profile for **compile only**, NEVER upload/flash. This image is not globally RF-silent if incorrectly flashed.

## Code-scoped change

- F24Q retains its original generic `Arbiter::open` semantics. A separate `openHistoryChild` allows exactly one read-only history child in slot 1 only while a slot-0 read handle remains open under the same valid media/owner lease. Parent close fails if child is active. Existing F23D append + verification semantics are unchanged.
- F24R's guarded `FileHandle` adds `name()` and `openNextFile()` with owner checks before each metadata/iterator call, and slot-specific `operator bool`. The new iterator reserves slot 1 before asking the backend for its next child. No new mount, raw SD access, cloud call or radio action is introduced.
- `DTNF24T::HistoryDirectory` accepts only the exact vessel history root `/THESIS/F06TEST/V001..003` matching its vessel id; reads at most 64 entries plus one overflow probe, emits no more than 48 uppercase-hex session names, rejects foreign-prefixed or nested paths and records a fault/truncation status. A failed `openNextFile()` can mean EOF **or** a backend error, so its `complete` indication is merely an iterator observation and MUST NEVER establish trusted SOS-session closure or durability.
- The actual `ThesisSDLogger::listHistory()` uses this scanner ONLY with `THESIS_DTN_F24T_BUILD_ONLY=1`. Older/operational profiles retain their previous raw `File` implementation exactly. F24T only compiles with F24S/R/Q/K/F06 sandbox and disabled independent F23D/fleet/legacy relay/physical GPIO. The instantiated F24Q `DenyWitness` and zero lease prevent physical File opens.

## Tests and unavoidable limits

Host tests use synthetic in-RAM `File` objects with mocked positive owner facts to exercise scan bounds, parent-child arbitration, invalid paths, revocation, lack of unauthorized I/O and the original F24Q/F24R/F24S/F24P/F24O regression contracts. Positive mock grants do not authorize hardware. Windows g++ and ESP32-S3 PlatformIO compile-only verification are required before any local commit.

**Not fixed:** `File`'s real destructor/move/close may touch a card after media/lease loss. The host mock demonstrates that implicit destructor cleanup still occurs even when the guard quarantines a revoked handle. No physical SD owner, independent media identity, mount continuity, power-loss durability, full-source all-writer proof, immutable closed SOS session, or authenticated shore final delivery receipt is established. F24T is a diagnostic logger history boundary; F24P owner-bound SOS recovery remains the authoritative *software-only* preview for SOS session discovery. This phase makes no claim of physically durable, complete DTN.

**Next DTN gate:** independently qualify real Arduino filesystem teardown semantics and design a conservative fault/reboot policy with per-media identity and quarantined handles; then disposable, explicitly approved test-media SD durability tests. Keep V002's original card preserved. V001 or a three-board multihop exercise is not required for this storage milestone. No flashing, SD access, GitHub push, Supabase or Docker lab operations are included.
