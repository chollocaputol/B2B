# Three-vessel multi-hop DEV preview — no RF forwarding
This is part of the existing feature/firmware-multihop branch, not a new project phase.

## Design
- Final intended network behavior: automatic V001→V002→V003 forwarding for routine
  position telemetry and urgent SOS/CANCEL, with duplicate suppression and
  emergency priority. Fishermen should not have to enable relay through a menu.
- The current **build-only** DEV preview is NOT that deployed behavior.
  The current F23D physical permission provider denies relay RF; this profile
  keeps the old relay OFF, does not install a physical grant, and does not flash devices.
- V002 accepts only valid original 25-byte POSITION_V2 packets from V001 into
  a bounded four-entry RAM queue. It preserves origin, sequence and original
  wire bytes while queued; recently seen duplicates are suppressed.
- When a frame leaves the queue, the software constructs an **existing 62-byte
  v1 telemetry frame with hopCount=1**. This is the preview of a future relay,
  NOT a transmission. V003 already supports receiving 62-byte telemetry with
  origin/sequence preserved. Weather/battery/name fields absent from POSITION_V2
  are never invented or flagged as valid.
- Routine preview preparation defers while an own-vessel emergency or a
  relayed SOS/CANCEL is pending. A remote SOS remaining active in the peer
  table does NOT indefinitely block routine forwarding once the urgent
  relay queue has drained. The preview does not retransmit packets.
- GPIO0 long-hold on Development page 4 (PDR) toggles V002's PREVIEW OFF/PREP;
  the last line reports `RLY:OFF` or `RLY:PREP`. V001/V003 cannot enable it.
  Turning it OFF clears queued entries; the toggle is volatile and NOT an RF grant.
  Existing PDR counters remain visible. SD recording remains independent.
- POSITION_V2 carries no hop field; a future physical relay should emit the
  existing 62-byte legacy telemetry with hopCount=1, not rebroadcast unchanged
  25-byte POSITION_V2. V003 can then distinguish a relayed packet from direct.
  This preview does not authenticate origin or prove direct-path RF isolation.
- Incoming SOS and its CANCEL still use the unchanged 62-byte wire format and
  F23D guarded experimental path. Existing neighbor ACK is not far-end receipt.

## Verification and next steps
1. Host-test the pure compact-position queue and DEV switch separately.
2. Build all affected profile(s) and run the existing F22/F23 regression set.
3. Source-review actual board binding, deliberate operator permission, and measured
   radio-to-RX timing. No fabricated timing grants or compile-flag overrides.
4. Complete a reviewed supervised physical relay adapter, then validate one-hop
   direct isolation + negative control + positive SOS/CANCEL + routine telemetry.
5. Preserve SD journals and original PDR logs separately; never claim a physical
   multi-hop PASS from host-only tests.

No serial/GPIO operator control is installed for radio permission. This preview
remains build-only; GPIO0 is display/test-preview control, not an SOS trigger.

## Supervised V002 physical bench integration
The separate opt-in profile `tbeam-supreme-multihop-v002-supervised-bench`
adds a constrained V002 physical radio path. It is not the accepted F23D
permission candidate and does not change that candidate's default-deny
behavior. No sea deployment or acceptance claim is authorized.

- Boot output identifies the supervised profile with
  `[MULTIHOP-BENCH-BOOT] PROFILE=V002_SUPERVISED_R1`; the older
  `[LAB-RELAY] BUILD=DISABLED` banner refers only to the legacy relay.
- DEV PDR Page 4, GPIO0 HOLD: **OFF -> PREP -> LIVE -> OFF**.
  V002 (NVS vessel ID 2) alone can proceed. PREP records queues without
  forwarding. LIVE requires the second deliberate hold. V001 and V003
  cannot activate forwarding. The operator must visually verify V002's label.
- LIVE automatically expires after **10 minutes** or **128 attempted
  relay frames**; loss of radio TX, RX restoration, or a measured synchronous
  radio-to-RX interval above **5,000 ms** disarms it. These are conservative
  software stop bounds, not measured hardware assurances yet.
- Normal operation is not changed by this test profile. While LIVE, V002
  forwards admitted V001 SOS/CANCEL (62 bytes, hop=1) and accepted V001
  position telemetry converted to existing 62-byte legacy v1 telemetry,
  hop=1, original origin/sequence preserved. Routine traffic cannot
  preempt local SOS/CANCEL or queued relayed SOS/CANCEL.
- V002's SD logger boots `SOS_ARMED` with **routine recording OFF**.
  For the complete telemetry relay journal the operator must explicitly
  select `CONTINUOUS` in the SD recording menu, without erasing the card.
  SOS event writes are marked urgent independently. The radio sender
  also prints attempt details on serial for immediate inspection.
- The existing SD writer is asked to record both radio result and measured
  TX-to-RX interval; research acceptance still requires verifying journal
  integrity/readback and V003 reception, not just a local radio-success code.
- Frames remain unauthenticated; only fixed-route supervised laboratory
  tests are contemplated. This is not safe to deploy at sea.
- The six-profile PlatformIO build at `bc5bf68` passed on
  2026-11-21; the corrected V002 profile at `63da2b0` also passed
  locally on 2026-11-21. Neither verifies this priority/banner change
  or actual on-device delivery. Rebuild this revision before flash review.
- If a received/local SOS forces the OLED into emergency browsing while
  V002's relay is PREP/LIVE, a GPIO0 **long hold stops relay immediately**
  without cancelling or acknowledging the SOS. This is an emergency bench
  STOP, not an SOS operation. Normal SOS browsing resumes after stop.
- The existing F22 and F23D physical candidate profiles do not define
  `THESIS_MULTIHOP_BENCH_RF` and remain unchanged.

## Fleet HOME identity label correction (build verification pending)
Operational HOME headers now format the existing provisioned
`VesselIdentity::nodeId()` as `V001`, `V002`, `V003`, etc.,
rather than displaying a hardcoded V003 label on every common-fleet board.
Portrait HOME retains the previous V003 x3 title, battery icon x26
and battery percentage x38 layout for IDs 1–999. For four-digit
vessel IDs the battery **icon only** is omitted to prevent overlap;
the battery percentage is always shown. Landscape HOME shows
`V002 HOME` on Vessel-002 and `V003 HOME` on Vessel-003.
No identity/NVS writes, radio protocol, relay permission, SD path,
SOS handling or other OLED page flows are changed.
A standalone C++ host test passes for IDs 1, 2, 3, 999, 1000,
9999 and missing identities. The new PlatformIO image and SHA256
must be built and verified again before flashing V002; do NOT reuse
the previous `d488a96` firmware hash for the corrected display.
