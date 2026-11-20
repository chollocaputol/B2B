# Physical permission candidate: NOT AUTHORIZED OR DEPLOYABLE

Accepted parent: 53815809722074215588cd5c89e988996c8ea2ad.
New profile: tbeam-supreme-f23d-v002-physical-permission-candidate-build-only.
It extends F22, not an offline candidate. All accepted offline flags and immutable
permission remain unchanged. Legacy relay and GPIO outputs remain OFF. Normal
operational transmissions are inherited; this is not a globally RF-silent image.

The actual firmware Permission instance defaults to unavailableEvidence, which
returns no binding, operator or timing evidence. It has no serial override, MAC,
GPIO, credential, NVS storage or default grant. SELECT may record one run/event;
missing evidence then latches closed. ARM cannot authorize relay transmission.
A reboot discards permission and controller history; it does not authorize a run.
No supplied artifact may be flashed under this milestone.

## Implemented boundary

Reuses RunControl, Transaction, packet route validation, radio adapter and bounded
readiness evidence queue. One origin-1 selected event, hop 0 to 1; one SOS and one
matching CANCEL attempt, charged before submission even on failure. CANCEL wins
before a pending SOS. Challenges are local sequence confirmation, not cryptography.
Firmware eligibility checks cached valid V002 identity, recovery safety, independent
board/operator/run/event evidence and reviewed nonzero timing bounds. Permission
is evaluated at ARM, observation, dispatch and inside transmitWithIdentity after
the original identity guard and CAD interruption, immediately before radio.transmit.
No formatting or serial output follows the final gate before that radio call.
Normal operational traffic without the synchronous relay lease is unchanged.

The provider times successful ARM from the same timestamp as RunControl.
Unsigned elapsed arithmetic keeps the 15000ms window rollover-safe. The final gate
requires remaining time strictly greater than call-entry plus radio-to-RX bounds;
zero, unbounded or unapproved bounds deny. Those bounds MUST include worst-case
preemption, final-check overhead, RadioLib internal setup/start latency, TX timeout
paths and RX restoration. This task supplies no guessed measurements.
Without finite justified bounds, no assurance about physical start/completion is
claimed and no grant is possible. Radio or restoration failure revokes the run.

Evidence includes existing selection/arm/expiry/reservation/invocation/RX outcome
rows plus authorization command, final rejection, radio return and revocation.
RADIO_INVOKE records entry to the guarded wrapper, not proof RF started; AUTH final
rejection can occur afterward. Local return success still is not V003 receipt.
Queue loss is sticky and prevents further relay submission. Existing operational
Serial traffic still needs measurement; host tests do not establish USB timing.

## Missing platform dependencies and separate authorization

A separately reviewed implementation must supply Evidence through the firmware
reader, freshly sampled for the requested run/event:

* Independent board binding: actual stable board identifier, trusted expected
  association, verification source/procedure and mismatch handling. Fleet ID alone
  is insufficient. No board identifier has been invented here.
* Deliberate local operator authorization: actual approved mechanism, presence,
  revocation/fault behavior, and explicit scope for this run/event. No pin or input
  mechanism is selected here; SELECT/ARM alone are not physical permission.
* Approved finite timing bounds from the exact board, radio, toolchain and PHY,
  including call-entry/preemption and TX/RX failure timeouts. If these cannot be
  bounded under the current driver, physical activation stays blocked pending a
  narrowly reviewed driver boundary correction, not a longer run lifetime.

Reader evidence is a trusted local interface, not cryptographic packet proof.
A future implementation must not import the host qualification model, override
THESIS_F23D_PHYSICAL_PERMISSION, or change immutable offline contracts. Review must
confirm actual hardware checks, immediate revalidation, no grant caching, source
isolation and all offline profile equivalence. Device installation, normal SOS NVS
writes, storage arrangement, isolation equipment and RF operations each remain
outside this software-only authorization. Preserve application backups and live
monotonic counters on eventual restoration. No DTN/store-and-forward is added.
