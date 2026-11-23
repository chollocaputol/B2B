# F24J — single-owner arbitration boundary (HOST / BUILD-ONLY)

**Required parent:** `2741600559f0f676ae895890b1d97033be090beb` on clean `feature/firmware-multihop`.

## Implemented

- `dtn_f24j_owner_coordinator.h` wraps the already-guarded F24I Port and a separately sampled witness. It requires a valid SOS-only F24H session identity and matching nonzero media/mount/source/lease/close generations.
- Exposes a F24H-compatible retirement Port: producer quiescence, urgent-first queue draining, close/flush, final SOS newline inspection, and source protection. Every operation rechecks the independent witness before and after, checks source epoch mutation rules, latches faults, and never silently restores a revoked lease.
- Adds explicitly arbitrated illustrative export/history/read-hash/seal lanes. These are host model callbacks, **not** existing firmware SD reads. A retiring session excludes regular operations; only a write-protected retired source can enter hash/seal lanes.
- Synthetic host tests compose F24I GuardedPort -> F24J Coordinator -> F24H Controller. They check success with fabricated witness and negative cases: conflicting export/history, queued SOS, radio/SOS preemption, owner/media/epoch changes, re-entrancy, source mutation during read, wrong queue bookkeeping, flush failure and reboot. Positive mock results do not prove a real card or a functioning write barrier.
- The only F24J firmware integration is an always-denied RAM-only preview in `ThesisSDLogger::poll()`. It shares F24I's `NoPhysicalPort`/`NoPhysicalObserver`, accepts no session, performs no new SD I/O and cannot grant F24F replay. The previous operational logger remains unchanged.

## Remaining critical gaps

The existing logger still contains **direct SD operations outside the coordinator**, including `appendRecord()`, `pollEvents()`, `pollExport()`, `pollHistory()`, audits and initialization. Some radio/history/diagnostic paths are separate. As long as any path bypasses central interception, the `allWritesInterposed` / independently verified ownership requirements **cannot be truthfully asserted**. F24J does not implement a true physical owner, card CID verification, mount lock, uninterruptible FAT flush, durable write lock, source hash, journal rotation or seal commit.

The operator-run staging script prints an audit of direct SD call sites but intentionally does **not** rewrite them. Actual migration of these paths must be reviewed independently before testing on a **separately approved expendable SD card** and nonproduction board. Never access preserved original Vessel-002 historical evidence.

## Non-authorizations

This stage does not authorize firmware flashing, LoRa relay changes, physical SD reads/writes, SOS cloud replay, authentication credentials, Wi-Fi, HTTPS, Supabase writes/deployment, or GitHub push. The build-only profile inherits normal operational radio TX if flashed; **DO NOT FLASH**. The normal peer-packet and research/weather-record streams do not become operational DTN.

## Acceptance

1. Inspect and verify exact F24I parent, clean feature worktree and three package source SHA256 values.
2. Compile/run native Windows UCRT64 C++17 F24B/F24F/F24G/F24H/F24I regression tests and F24J coordinator host suite using actual local committed headers.
3. Review printed read-only direct-SD call-site audit. It is evidence of **remaining bypass paths**, not a claim that the owner already controls them.
4. Copy precisely five changes into the working tree: three new files, `thesis_sd_logger.h`, `platformio.ini`; perform Git whitespace and scope checks. Do not commit or push yet.
5. Run only `platformio run -e tbeam-supreme-f24j-owner-coordinator-deny-build-only` for ESP32-S3 compilation. Report tests, Git status, and stop for separate local commit review.
