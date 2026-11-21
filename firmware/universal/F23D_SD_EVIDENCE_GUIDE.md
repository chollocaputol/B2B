# F23D SD-first evidence (build-only; no physical permission)

This software milestone performs no device, RF or physical-card operations.
All accepted profiles remain unchanged. New suffixes after tbeam-supreme-f23d-:

| Role | Build-only suffix |
|---|---|
| V001 | v001-sd-evidence-build-only |
| V002 offline | v002-sd-evidence-build-only |
| V003 | v003-sd-evidence-build-only |
| V002 physical candidate | v002-physical-sd-candidate-build-only |

The physical candidate still has unavailable board/operator/timing evidence.
SD START, SD status and a healthy card do not authorize radio forwarding.
Existing SELECT/challenge/ARM, one event, 15 seconds, one SOS + matching CANCEL,
local priority, startup disarmed and immutable offline permission are preserved.
No DTN, persistent permission, new packet format or remote arming exists.

## Storage and timing boundary

The existing authorized F22 logger creates the unique F06 per-boot directory.
Only then may the dedicated writer create:
  /THESIS/F06TEST/V001/Sxxxxxxx/f23d_multihop.jsonl
using the corresponding V002/V003 root. Session tokens are seven hexadecimal
digits. New paths are checked against a 96-byte buffer; the existing directory
buffer is 64 bytes. Unknown IDs, wrong roles, malformed sessions, mount failure,
existing files and session creation errors deny experimental recording.
No mount formatting, historical write, truncate, repair or append retry occurs.
The file is opened for writing once; verification handles are read-only.

A 48-entry queue stores 120-byte structured records (host Journal measurement: 5832 bytes; target size is build-dependent),
not 512-byte serial lines. JSON is formatted only by the SD owner, into a bounded
768-byte scratch buffer. Callback capture uses bounded copies/fingerprints and
RAM only; no file, flush, SPI, credentials, payload text or unrelated data.
Serial diagnostics have their own queue. The SD run/RX hooks bypass serial
packet mirroring, so USB disconnection/backpressure cannot consume SD records.
Small SELECT/ARM/STATUS replies still use the independent serial queue so the
operator can obtain the challenge. Serial reply loss does not discard SD rows;
the existing physical serial-evidence safety guard is not bypassed.

R23D SD START begins the capture freeze. All logger filesystem servicing and
history/export access are deferred until sealing is requested and radio/control
work is quiet. Original SOS/CANCEL urgent records drain first; experimental
records precede routine records. The same main-loop SD owner writes one queued
record, flushes, checks the API write-error state, and verifies exact readback.
Final full-file verification advances at most 64 bytes per owner iteration.
Capture has finite capacity: nominal negative-control + positive trial fits;
excess duplicates/commands/traffic can overflow and invalidate the experiment.

Queue health/loss and record indices are sticky. Required missing rows, gaps,
format/write/readback errors, wrong identity and absent clean END invalidate
evidence. A missing card cannot store its own error: no journal/END and cached
unready status are the fail-closed evidence, not an invented durable error row.
A storage failure retains unwritten RAM rows and never clears or repairs them.

Enqueue success is NOT durable success. Flush + readback + verified close is
the software completion criterion, not a power-loss guarantee for SD controller
caches. flush()/close() have no independent durability acknowledgement in this
Arduino API. Exported files and operator-recorded sealed status are required.
No hardware SD-latency measurements are claimed. Existing operational Serial and
SD behavior still needs physical qualification; never start another trial while
draining, and do not promise latency bounds for an unqualified card.

## Future supervised procedure (requires separate authorization)

1. Obtain approved device images/backups, operator permission, media arrangement
   and measured RF containment/isolation. Use one labeled test card per vessel;
   protect historical thesis cards. None of these operations is authorized here.
2. Boot each approved role. Wait for R23D SD STATUS: correct local vessel/path,
   ready=1, pending=0, health=0, lost=0. Record each session and image hash in
   the operator manifest. Missing/unready card or wrong role: stop.
3. Issue R23D SD START locally on each vessel before the trial. Verify active=1.
   Collection requires no live USB observer afterward. Existing SOS/CANCEL
   controls and V002 local authorization still require suitable operators or
   the already approved local arrangement; SD does not replace them.
4. Perform relay-disabled negative controls with a distinct event N. Keep V002
   startup-disarmed; do not issue DISARM and then expect same-boot rearming
   (the accepted controller is terminal after DISARM). Record both adjacent
   bidirectional-link checks and direct-link isolation measurements separately.
5. After control N is deliberately closed, query V001 R23D NEXT for actual
   next event E. SELECT run/E on V002, record challenge, ARM, trigger V001 SOS,
   then promptly issue the existing V001 CANCEL SOS under an independently
   approved timing/procedure. Match E throughout. The 15-second window does
   NOT allow walking between separated vessels or waiting to collect cards.
   Without live V003 observation, cancellation timing must be separately
   qualified; offline SD records establish success afterward, not in real time.
6. Stop RF test activity. Request R23D SD SEAL on each vessel. This does not
   discard pending rows or extend relay permission. Wait for controller expiry
   and pending control work to finish. An unresolved local SOS/CANCEL requires
   deliberate approved resolution; there is no force-drain shortcut.
7. Poll cached SD STATUS until sealed=1, pending=0, lost=0, health=0.
   Record this per vessel as sealed_verified in the manifest. On any fault,
   preserve files/RAM state and mark the trial invalid. Do not reset to replay
   an event or roll back NVS counters. One capture per boot, no automatic resume.
8. Only after separately approved shutdown remove the labeled cards and export
   each whole experimental journal read-only. Preserve original bytes and hashes.
   Session ordering/uptime is local, not synchronized wall-clock time.

## Offline checker and manifest

Run:
  python scripts/fleet/check_f23d_sd_evidence.py --v001 V001.jsonl \
    --v002 V002.jsonl --v003 V003.jsonl --manifest trial.json

Manifest schema:
  schema: 1
  run: nonzero V002 run identity
  event: actual V001 allocated positive event E
  sessions: {"1":"S.......","2":"S.......","3":"S......."}
  sealed_verified: {"1":true,"2":true,"3":true}
  isolation:
    arrangement: explicit conducted/shielded test arrangement
    reviewed: true
    both_direct_directions_blocked: true
    adjacent_bidirectional_links_verified: true
    measurement_report: relative path to independently reviewed measurement file
    measurement_sha256: actual SHA-256 of that file
    negative_control:
      relay_disabled: true
      event: distinct originating event N
      journals: {"1":"...jsonl","2":"...jsonl","3":"...jsonl"}

Negative-control journals can be the same complete captures if N preceded E with
V002 disarmed. There must be actual V001 TX evidence, V002 direct reception,
no V002 reservation and no V003 direct frame for N. Additional adjacent-link and
both-direction isolation proof comes from the separately reviewed report.
No hop count, fingerprint, local TX success or missing ACK proves isolation.

PASS means complete matching delivery records plus supplied reviewed isolation
evidence satisfy the checker. It does not authenticate packet origin or establish
physical truth of an operator attestation. FAIL means incomplete/mismatched/lost
delivery evidence. INCONCLUSIVE means delivery records pass but isolation proof
is missing/insufficient. V003 run is always zero/unknown; run correlation comes
from the manifest and origin/event/fingerprints, never an invented wire field.
Normal retries may produce duplicate RX rows; extra relay reservations, stale
selected-event replay, mismatched fingerprints or missing matching CANCEL fail.
Known CANCEL-before-SOS, volatile reboot replay state and neighbor-ACK limitations
remain unchanged.
