# F23D guarded V002 laboratory forwarding candidate

Base: 0463ebc158938909c84d2991e6e3435dbb6dfbf6. Source/offline qualification only.
No flashing, radio experiment, NVS write, or physical SD access is authorized.

## Boundaries and actual adapter

F23D reuses the unchanged F23C Transaction for event selection, packet copying,
hop 0->1, separate SOS/CANCEL slots, one attempt per type, 250 ms preparation,
1000 ms modeled separation, 15 s lifetime and no rearming within an object.
RunControl owns the transaction independently of the operational peer table.
Its dispatch reserves the attempt before calling an adapter, including failures.
The firmware adapter calls transmitWithIdentity and restores continuous RX;
the actual adapter body is compiled and executed against a mock radio in host tests.
This is packet forwarding code, not a simulation log used as delivery evidence.

The offline-qualified firmware's physical-permission provider is immutable FALSE.
THESIS_F23D_PHYSICAL_PERMISSION rejects nonzero values. Operator commands cannot
supply that permission. The adapter and controller both require permission.
Thus no physical relay TX is reachable in this milestone, including on valid IDs.
A future physical phase requires independent review and explicit installation
of a bounded permission provider; changing a flag alone is deliberately rejected.

Original THESIS_LAB_RELAY_TX stays zero, F23C stays a disabled-adapter model,
and all F06/F15-F19/F22/v3/identity/recovery/storage/GPIO guards are untouched.
No origin authentication is claimed. Run challenges are local confirmation,
not cryptographic credentials. Legacy ACKs remain neighbor claims, not V003 proof.

## Deliberate local run control

Commands share the existing bounded serial line decoder using an R prefix, so
FLEET and SD command consumers retain ownership of their existing prefixes.
R23D STATUS reports the state. R23D SELECT <nonzero-run> <nonzero-event> reserves
one event slot and emits a fresh nonzero random decimal challenge. A second
selection is refused. R23D ARM <challenge> must arrive within 15 seconds, under
valid cached V002 identity, recovery-safe state, no local control work, and an
independently installed physical permission. It is DENIED in this milestone.
R23D DISARM revokes permanently until restart; restart always boots disarmed.
Wrong challenge, lost identity/permission, recovery failure and expiry fail closed.
No settings, identities, attempts or permissions are written to NVS or SD.

Only accepted V001 SOS may establish the transaction. Its structurally matching
CANCEL is checked against that transaction even if a newer operational event
has replaced the table record. Operational CANCEL_MISMATCH behavior is unchanged.
Cancellation suppresses pending SOS. Replays, other events, types and hops are
rejected. No queue entries are evicted; one selected event is the capacity limit.
Existing diagnostic queue fullness cannot grant or replenish a relay attempt.
Local SOS/retry/CANCEL, immediate ACK handling, CAD and pending RX have priority.
A radio error still consumes the attempt. Exhaustion is unconfirmed delivery.

At unchanged V003, unknown CANCEL creates no tombstone: later SOS can become
active. Neither reliable arbitrary-loss cancellation nor durable replay safety
is implemented. Reboot loses RAM history and requires external run accounting.
The timing constants are unvalidated simulation settings, not measured RF margins.

## Future physical evidence procedure (NOT executed)

1. Independently approve the physical permission provider, app image, recovery
   plan and containment. Never flash F23B FIX1 or use F23C as a relay release.
2. Use a reviewed shielded/conducted attenuated topology. Prove both V001->V003
   and V003->V001 fail while the two adjacent links work. Include relay-disabled
   controls under exactly the same arrangement. Hop values do not prove isolation.
3. Select the known next V001 event explicitly. Record run/challenge/permission,
   valid V002 identity and disarmed/armed transitions without modifying identity.
4. Correlate V001 hop-zero SOS TX, V002 accepted event and single hop-one adapter
   call/result, and V003 hop-one RX with matching origin/event and packet bytes.
5. Repeat matching CANCEL, duplicate, incorrect event/origin/hop, cancellation
   before queued SOS, newer peer event, failed TX, expiry and budget tests.
6. Capture all three serial streams with host timestamps plus device uptime,
   packet identities, RSSI/SNR at each receiver, TX result and attempt counters.
   Existing receive traces provide RSSI/SNR; F23D logs run outcome separately.
   Require V003 reception evidence; V002 radio success or V001 ACK is insufficient.
7. Record failed/unknown delivery explicitly, disarm, and preserve the evidence.
   Any direct RF leakage invalidates the trial. Do not infer rescue resolution.

## Offline validation

Run tests/test_f23d_bench_candidate.py and existing F23A, F23C and fleet regressions.
F23C's driver now strips independently marked F23D additions only after checking
they exactly reduce to the committed F23C source. Its 105 checks and 14 compile
gates and all original fixture/header bytes remain intact.
Build F22 operational, F23C model and the new isolated F23D candidate without
upload targets. Compare F23D-disabled F22 and F23C allocated ELF sections to
raw committed references under identical tools and dependency paths; explain
any BIN metadata differences. Record ELF/BIN/partition/framework-input hashes.
