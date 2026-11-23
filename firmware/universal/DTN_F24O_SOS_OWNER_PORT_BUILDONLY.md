# F24O: SOS-only shared owned-port integration (build-only / physical SD denied)

Parent local commit: `828c2818bdde4c3381de59fea4b3b4cd114f9b9e` on `feature/firmware-multihop`. F24O replaces no accepted SOS on-air fields: `VesselPacket` remains exactly 62 bytes; routine POSITION v2 remains exactly 25 bytes. Research-only weather/GPS/peer logs are not DTN records.

## Concrete connected implementation

A single `DTNF24O::Port` is passed to the *actual* F24M `SessionWriter::appendChecked` call and the *actual* F24N `Replay::poll` call inside `ThesisSDLogger`. It exposes only the F24M packet and F24N receipt journal paths under the selected `/THESIS/F06TEST/Vxxx/Sxxxxxxx` session tree; append mode is allowed only for that session. It tracks one in-flight handle; rechecks the injected witness on every file operation; denies stale media/mount/owner generation, concurrent handle, F23D or other writers, live radio, exports, histories, bad paths and unverified closed-session reads. It always closes an acquired handle even after revocation.

The same F24M packet journal and F24N recovery/receipt components run in seven **independent host processes**, with actual disposable filesystem files, two prior sessions, third restart and simulated shore acceptances. The source SOS frames remain unchanged; `PACKET_SOS_ACK` is never final shore receipt. Only a trusted host-injected fake shore verifier can mark delivered in the test. Restart and duplicate/conflict handling stay in F24N. The host witness and fake shore are only test doubles; they are NOT physical board/card authentication.

## Deliberate limits and unresolved integration

- The only F24O PlatformIO profile strictly extends F24N/F24M/F24K DENY and wires `DenyWitness`. Thus `bind` always fails. The F24K SD facade always denies mount/open. F23D is compile-time OFF; no real card access, replay or receipt acknowledgement is possible. **DO NOT FLASH.** It inherits routine LoRa behavior and is not RF-silent.
- The F24N root directory discovery still uses the preexisting logger File interface. F24O fails closed before discovery if the owner is unavailable; root directory handles, historical exporter/history/audit, the legacy logger, and the independent F23D `File` writer have **NOT** been fully interposed under a physical global SD lease. They must be audited and coordinated before any physical grant.
- The F24O facade is a narrow adapter, not independent evidence of media identity, exclusive card ownership, trust, filesystem durability, seal/closure, file pinning, power-loss fsync or actual backend receipt. Host `fflush` + readback is not an SD power-loss test.
- Existing F24M SessionWriter and F24N Reader policies remain unchanged, including fail-closed on ambiguous appends, torn frames, same `(origin,event)` conflicts, bounded history and no final receipt without independent verification.
- Normal telemetry transmissions and continuous research-only logs are unchanged and never enqueued in operational DTN. The timestamp in existing SOS `VesselPacket` is device uptime, not authenticated UTC.

## Acceptance gates

Run the bundled guarded Windows staging script against exact clean F24N parent, then run PlatformIO environment `tbeam-supreme-f24o-sos-owned-port-deny-build-only` compile-only. Inspect Windows output and exact source deltas before considering a guarded local-only commit. No actual SD, COM/USB, RF test, flash, Wi-Fi, HTTP, Supabase or GitHub push is authorized by this phase.
