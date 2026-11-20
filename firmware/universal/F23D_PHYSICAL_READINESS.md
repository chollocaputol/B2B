# F23D physical-readiness software qualification

Accepted parent: 1fc20613ffb8457c2b1552fdcc62379c9176b638.
This milestone permits source and offline checks only. No flashing, physical
RF transmission, physical SD/NVS access, provisioning or device changes occurred.

## Isolated profiles

| Role | Profile suffix after tbeam-supreme-f23d- | Behavior |
|---|---|---|
| V001 | v001-event-query-build-only | R23D NEXT reads cached state, no allocation |
| V003 | v003-rx-observer-build-only | Read-only packet-specific RX evidence |
| V002 | v002-relay-evidence-build-only | Offline controller with dynamic queued evidence |

Profiles have exclusive compile gates, inherit complete F22 integration and
reject legacy relay, GPIO actuation and F23C model mixing. Runtime query READY
requires cached valid V001 identity. RX observation requires cached valid V003.
F22, F23C and the accepted F23D offline profile keep their original behavior.

## Cached next-event query

R23D NEXT reports local numeric identity/provisioning, cached nextEmergencyId,
counter availability/exhaustion, active/pending SOS, pending cancellation,
storage safety, recovery hold and READY/BLOCKED. It performs no Preferences,
NVS read/write, SD operation, RF call, reservation or SOS action.
Zero, UINT32_MAX, unavailable/unsafe storage, wrong identity or unresolved work
is BLOCKED. This is a snapshot, not a reservation: another SOS start can consume
the ID, even if checkpointing or transmission subsequently fails.

R23D EVIDENCE reports the diagnostic queue, sticky loss and maximum chunk-write
time. Commands use the existing bounded complete-line decoder; R prefixes
preserve FLEET and SD command ownership. Normal CANCEL SOS remains unchanged.

## Receiver and relay evidence

RX observer records local ID, uptime, claimed origin/event/type, incoming hop,
wire length, RSSI, SNR in tenths of dB, validation and actual state outcome.
It records format/ID/self-origin/payload rejection and NEW, DUPLICATE, STALE,
FULL, CANCELLED, CANCEL_DUPLICATE and CANCEL_MISMATCH without changing state or ACKs.
Unparseable frames have origin/event zero as unknown; no untrusted partial struct
is dereferenced. Radio CRC/I/O failures without a decoded packet retain the
existing RX error diagnostics, not fabricated packet identities.

FNV-1a wire and canonical fingerprints aid comparison. Canonical form substitutes
hop zero; neither fingerprint authenticates origin or proves delivery.
No packet payloads, private keys, credentials or unrelated stored data are dumped.

The relay-evidence profile records actual permission/arm state, run/event,
selection/run deadlines, remaining lifetime, admission, duplicate/cancellation
outcomes, consumed/per-kind budget, reservation, radio invocation/result and
RX-restoration result. The adapter budget remains reserved before invocation.
RX-restoration failure revokes the evidence-profile controller. Deferral logs
are throttled to one per second. Expiry/revocation is recorded from actual state.
Old fixed permission/TX literals remain only in the unchanged offline baseline;
the new evidence profile uses actual context values.

Added time-critical hooks enqueue into a fixed 12-row, 512-byte RAM queue.
The loop drains at most 64 bytes, only when Serial reports available space,
after SOS/CANCEL retry handling. Partial writes preserve FIFO line framing.
Sticky loss, peak depth, maximum enqueue and drain-call microseconds are exposed;
any loss, formatting overflow or missing required record invalidates a trial.
Host backpressure/chunking tests prove bounded operations, not USB timing.
Existing operational Serial statements are retained; total timing requires
physical measurement after separate authorization.

## Physical permission qualification, not installation

f23d_permission_qualification.h is explicitly host-only and rejected by Arduino
compilation. It wraps the real RunControl rather than duplicating packet/attempt
logic. Tests require provisioned V002, matched board, present local operator,
reviewed physical contract, recovery safety, run/event, challenge, finite time
and revalidation before the mock adapter. Revocation, expiry, replay/rearming
and failed-call accounting are checked. No firmware provider consumes the model.
F23DBench::kPhysicalPermissionInstalled remains immutable FALSE; its compile-time
physical-enable rejection is unchanged. IDs/board checks/challenges are not
cryptographic origin proof. Installing permission later requires an explicitly
reviewed new physical contract and image, not renaming or overriding old guards.
No persistent permission, auto-arm, reboot restore, DTN or new wire format exists.

## Complete timing budget

Current PHY: 915 MHz, SF9, BW125 kHz, coding 4/7, preamble 8, explicit header/CRC;
62-byte airtime is approximately 484.352 ms from the installed RadioLib formula.
V001 SOS uses 45 ms debounce, a 1.5 s hold, then 3290-3438 ms first slot. CANCEL uses 3240-3388 ms
first slot after either the existing serial CANCEL SOS or a 45 ms debounce plus five-second hold.

The 250 ms preparation delay starts before V002's immediate ACK. Since that ACK
takes about 484 ms, the delay does not establish 250 ms of post-ACK quiet time.
The 1000 ms output separation is start-to-start; it is not a measured RX margin.
Do not infer reliable half-duplex turnaround from these constants.

The host timing test enumerates all five event slot residues. With first-attempt
success, no CAD/channel/retry deferral, 100 ms processing allowances at V002,
and 400 ms host reaction after V003 SOS reception, matching CANCEL reaches V003
in about 11.58-11.77 s after ARM. Substituting a five-second CANCEL hold yields
about 16.23-16.42 s: it exceeds the unchanged 15-second run window.
These are explicit modeling assumptions, not measured upper bounds.
Keep the 15-second window for a separately authorized serial-assisted trial.
Abort on delays beyond the declared margin; never silently extend permission.
A button-only trial would need a separately reviewed finite window, for example
a justified bound computed from measured processing/turnaround plus the 5 s hold;
no increased limit is implemented here. Measure USB/logging, ACK completion,
TX duration and RX-ready times before authorizing physical delivery claims.

## Future physical readiness matrix (test plan only)

| Requirement | Required evidence/action after separate approval |
|---|---|
| V001 origin | Diagnostic image if live next-ID query needed; capture NEXT snapshot then actual selected SOS |
| V002 relay | Separately authorized physical image; current evidence image cannot arm RF |
| V003 receiver | Observer image or equivalent external packet capture; match received hop-one SOS/CANCEL |
| Backups | Per-device verified deployed app, board identity, active slot and partition map; archive full-flash recovery evidence |
| SD protection | Original cards unmounted and protected; any spare card use separately authorized |
| RF containment | Reviewed shielded/conducted 915 MHz network, rated attenuation, terminations, three captured USB streams |
| Isolation | Measure assembled network/leakage; prove both adjacent links, block both V001-V003 directions |
| Negative control | Relay disabled, separate event, no V003 direct frame; cancel/close control before selecting next event |
| Positive SOS | V001 origin 1/event E/hop 0; V002 one reserved hop-1 call/result; V003 accepted E/hop 1 |
| Positive CANCEL | Matching origin/event payload, one V002 cancellation attempt, V003 matching cancelled state |
| Stop | Leakage, mismatch, reboot, expiry, RX/TX error, extra attempt, evidence loss or missing endpoint receipt |
| Restore | Restore verified application only; preserve identity and monotonically advanced SOS counter; verify readback |

Determine next E with the cached query, then SELECT and ARM within their bounded
deadlines only after a future permission contract is installed and approved.
Trigger SOS, require V003 receipt, issue the existing serial cancellation promptly,
then require V003 cancellation. Count at most one relay call per type, including
errors. Normal V001 retries have their own budgets and do not refill V002.
Record packet identities/digests, RSSI/SNR, host timestamps and device uptimes.
A neighbor ACK or local TX success is not verified end-to-end receipt.

CANCEL can suppress unsent SOS. Unknown CANCEL at V003 creates no tombstone;
delayed SOS may subsequently activate. An old matching cancellation is retained
by the controller independently of a newer operational peer event. Volatile
replay state is lost on reboot; abort and account externally, never restore old
NVS counters to replay a run. An actual operational SOS/CANCEL necessarily writes
its normal NVS checkpoints: future authorization must explicitly permit those
normal writes or use spare laboratory devices. This task performs none.
