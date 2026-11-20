# F23C Phase 1: build-only laboratory transaction model

F22 baseline: d7dcd4cc7394e18305b79b4022d92918e6fb5c22.
Parent baseline: 250c8807bc441c3b5d5981e8282db53dc329f2fb.
All seven supplied raw-file SHA-256 values matched before checkpointing.
The corrected external F23A gate passed 39/39 before that checkpoint.

## Scope and safety

The appended environment inherits the complete accepted F22 operational image.
THESIS_LAB_RELAY_TX remains zero. All original F06, F15-F19, F22, v3,
identity, recovery, GPIO and SD guards remain unchanged.
THESIS_F23C_RELAY_RF_TX rejects every nonzero value, even outside the model
profile. Existing firmware source is preserved beneath marked additions.
This is not a deployment image and has not been flashed.

Firmware owns a default-disarmed transaction and a DisabledRadio adapter.
There is no firmware arming command or call, radio callback, new SD journal,
NVS write, ACK change, packet-format change, or authenticated permission.
Normal operational radio behavior still exists in the inherited image.
"Relay RF disabled" does not mean the inherited firmware is RF-silent.

## Model contract

Host tests alone arm one expected V001 event, bound to a nonzero run number
and valid V002 identity. Only structurally valid hop-zero v1 SOS/CANCEL can
be offered with independently accepted operational state.
Unknown CANCEL, other origins/events/types, malformed payloads and forwarded
input are rejected. The output is a byte-preserving copy except hop becomes 1.
The run number is an admission input, not an authenticated nonce on the wire.

One transaction holds separate SOS and CANCEL records. A validated CANCEL
suppresses an unsent SOS and survives replacement of the operational peer
record by a newer SOS. No terminal record is evicted and no second event can
consume capacity. Disarming cannot re-arm that object or replenish budgets.
The model emits at most one SOS and one CANCEL preparation, consuming an
attempt before any mock submission, including a simulated failure.
ACKs cannot enter the model and do not grant further attempts.

Preparation waits 250 ms; modeled outputs are separated by at least 1000 ms.
These are simulation settings, not a validated RF turnaround interval.
Local control work defers output. A run expires after 15 seconds, including
while busy. Identity loss latches disarmed. Unsigned elapsed-time arithmetic
handles millis rollover. Reset constructs a disarmed object with no recovery.

## Limitations

Legacy v1 numeric IDs, labels, magic and matching payloads are unauthenticated.
The model does not repair replay at operational receivers, authenticate an
immediate transmitter, or provide durable replay history. New objects can
accept old events if an external controller reauthorizes them; do not claim
reboot-safe attempt limits. An external run ledger is needed for future labs.

Existing ACKs remain neighbor claims. Radio success and V002 ACK do not prove
V003 receipt. Unknown CANCEL at unchanged V003 stores no tombstone: delayed
SOS can still become active. Loss of either modeled output is unconfirmed.
No arbitrary-loss, end-to-end cancellation guarantee is provided.

A future RF phase requires a separately reviewed adapter and authorization.
Do not rename flags to evade guards or enable the legacy lab environment.
F23B FIX1 must never be flashed. A future RF demonstration needs a reviewed
shielded/conducted topology, blocked V001-V003 links in both directions,
relay-disabled negative controls, and correlated event/hop logs from all
three vessels. Software hop filtering alone does not prove physical routing.

## Validation

Run tests/test_f23c_lab_transaction.py with MSYS2 UCRT64 g++ on PATH.
It compiles the real C++ transaction header and uses MockRadio only,
exercises cancellation races, budgets, invalid input, priority, rollover,
expiry, reboot and byte preservation, and checks actual compile guards.
It also verifies protected sources against the F22 commit and removes only
marked additions to establish exact original main/lora source preservation.

After the F22 commit, compile and execute the unchanged external
f23_relay_host.cpp directly with firmware/shared on the include path.
Do not rerun the full F23A inventory gate against a newer commit: its required
old HEAD and seven-file dirty inventory intentionally no longer apply.
Run all repository host scripts and build only the F22 operational and
tbeam-supreme-f23c-lab-model-build-only environments. Never select upload.
Build artifacts and temporary host programs are not committed.
