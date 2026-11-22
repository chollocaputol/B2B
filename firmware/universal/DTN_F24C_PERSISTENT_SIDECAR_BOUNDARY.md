# F24C — independent F06 ACK sidecar: two-slot persistence contract

**Status 2026-11-23:** portable source + host-only disposable-file persistence
and torn-write/reboot fixtures PASS on Linux GCC and Clang sanitizer checks.
**Windows MSYS2 UCRT64 g++ 16.2.0 host build and disposable-file reboot tests: PASS (user-run, 2026-11-23).** Physical SD acceptance, runtime integration and GitHub push remain pending.
F24B parent must be local `d64381c7789c15619ff0123ed91fe1ac8c59fc99`.
No files are included by an operational firmware translation unit, and no
real on-device mount, flash, Wi-Fi or backend upload was performed.

## Exact scope

- `firmware/shared/storage/dtn_ack_sidecar.h` serializes F24A's immutable
  `DTNAckLedger::Snapshot` to **exactly 94 little-endian bytes** without C++
  padding, using the existing CRC32 of the complete logical snapshot.
- Two independent `slot0`/`slot1` files are resolved from a validated stream
  only: `/THESIS/DTNSYNC/V###/SXXXXXXX/sos_0.ack`, `sos_1.ack`,
  `tlm_0.ack`, `tlm_1.ack`. The F06 source namespace remains
  `/THESIS/F06TEST/V###/SXXXXXXX/` and is never opened for writing here.
- Per-stream identity includes the registered uploader UUID, vessel number,
  validated closed session, SOS/telemetry journal type and SHA-256 of the
  complete source journal. The sidecar header does **not** compute SHA-256,
  invent stable production record UUIDs or establish backend source trust.
- `DTNAckSidecar::Store<Port,Gate>` implements the F24A `read()`/`write()`
  interface, refuses unauthorized I/O, rejects active-slot overwrites,
  rejects wrong streams or nonmonotonic generation transitions, and
  requires full reopen/readback of a sidecar write. It does **not** mount
  an SD card or create directories. The `Gate` must reverify media,
  current single-SD-owner permission, registered identity, immutable
  closed-session evidence and current physical permission on *every* I/O.
- `dtn_ack_sidecar_arduino_port.h` optionally maps `Port` to Arduino `SD`
  `FILE_READ` and `FILE_WRITE` calls. This header is **not imported** by
  the existing runtime or included in any PlatformIO build. It never
  mounts a card, initializes SPI/pins, creates directories or writes any
  F06 source journals. It replaces only the PRECHECKED inactive sidecar
  file, flushes/closes and relies on Store readback; physical durability
  is not yet demonstrated.

## Important recovery rules

If the inactive slot receives an incomplete/torn file, F24A recovers the
latest valid remaining slot. It must NEVER treat an invalid existing file
as an empty store and recreate a new ledger. If the storage write returns
false **after** it reached disk, the current ledger fail-stops: the caller
must reload before retrying; the reloaded ledger can see either valid
state. Never regard HTTP status, Wi-Fi availability or LoRa ACK as evidence
of cloud acceptance. Backend per-record `accepted` / identical `duplicate`
results must be authenticated and exactly matched to the sent record UUIDs.

CRC32 checks incidental corruption, not malicious tampering. The sidecar
path does not cryptographically authenticate its own owner. A FAT filesystem
may lose data or directory metadata during sudden power loss even after
`File.flush()` and readback. This is why **the Arduino adapter is NOT yet
approved for physical use**. A true on-device durability claim requires
board-specific media identity, single-writer locking, closed-file proof,
write/flush semantics review, physical power-cut/removal tests, and failure
logging with no F06 original modifications.

The standalone host test uses disposable temporary files with file flush
and OS file sync (`fsync` or `_commit`); it reopens fresh Store instances
and also uses mocks for torn writes, denied permissions, wrong uploader,
changed source binding, corrupt slots and uncertain writes. On filesystems,
parent-directory metadata durability also needs separate qualification.
The tests do **not** simulate loss of the device, SD controller failures,
physical FAT metadata corruption or an authenticated production backend.

## Dependency on F24B and next implementation gates

F24B has a closed-session read-only journal adapter and candidate parser,
but its physical approval gate and real full-file SHA-256 verification
remain missing. F24C adds a *storage port* for F24A acknowledgments; it
is not an uploader and is not automatic SD access.

The next work should bring these models under one authorized main-loop
SD owner in an isolated, build-only integration with read scheduling and
SOS/LoRa priority; add a **real** complete-file SHA-256 verifier, closed
session/media permission proof, and hardware-specific sidecar behavior;
then define and implement registered device authentication, stable cloud
UUID/idempotency, operational backend per-item results, and shore Wi-Fi/TLS.
No F06 historical importer response can be silently substituted for an
operational per-record DTN ACK.

Never flash the current images or touch original V002 SD evidence based on
these host tests. Never push to GitHub or change `main` from a staging step.
