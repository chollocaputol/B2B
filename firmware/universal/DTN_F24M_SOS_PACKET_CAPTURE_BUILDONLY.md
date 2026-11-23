# F24M — Complete SOS-packet capture wired into firmware (build-only, SD denied)

**Accepted parent:** `7503bca289a64c359934f72338cc48a6d6913dca` on `feature/firmware-multihop`.

## Verified current packet format

`firmware/shared/communication/packet_protocol.h`: packed `VesselPacket` is exactly 62 bytes. `PACKET_SOS=2`; weather, position, vessel origin, SOS event/sequence, flags, hop count, and uptime-based `timestamp` are in the same frame. Normal compact POSITION v2 is 25 bytes and **never enters F24M capture**. Existing weather/GPS/system/peer continuous recording is research-only and does not enter operational DTN.

Caution: `timestamp` is currently device uptime seconds (not verified GNSS UTC); `batteryLevel=100` and `shoreDistance_m=0` are assigned by current `createPacket` (not measured). Legacy v1 SOS frames are unauthenticated. F24M does **not** solve those source limitations.

## What the patch actually wires

1. `lora_manager.cpp`: after final SOS bytes are assembled and **before** scheduling first radio transmission, pass the *unaltered* 62 bytes to the F24M RAM ingress. On receive, only after the existing v1 header/origin gate and `MultiPeerSOS::Result::New`, pass the original received packet to the same ingress. This path is bounded RAM copying; **no SD, SPI, network, or new relay call occurs in RX/TX**. Existing SOS/ACK/CANCEL behavior, RF cadence, retry caps and GUI remain unchanged.
2. `main.cpp`: an F24M bridge calls an 8-entry packet queue; diagnostics print once per 30 seconds and clearly say `owner=DENIED durable=NO`.
3. `dtn_f24m_sos_packet_store.h`: C++17 portable queue and explicit 88-byte binary journal frame (`F24M` magic, version, source, SOS origin and event, payload length, CRC32, exact 62-byte payload, outer CRC32). The CRC32 is an accidental-corruption/torn-record check **not cryptographic authentication**. Duplicate in-RAM `(origin,event)` with same bytes is rejected as duplicate; changed bytes is conflict. Full queue counts losses; no silent overwrite. Only type `PACKET_SOS=2` is admitted; routine telemetry and SOS control/ACK types are not enqueued as independent DTN payloads.
4. `thesis_sd_logger.h`: **only the existing main-loop logger poll** may call `SessionWriter::appendChecked`. It constructs a new `/THESIS/F06TEST/VNNN/SXXXXXXX/dtn_sos_packets.f24m` file, checks nonexistence, uses a short-lived append handle, flushes/closes, opens read-only and verifies exact appended bytes, closes the verification handle, then removes the RAM queue entry. A failed/ambiguous append latches failure and retains the RAM item; no append retry, truncation, or false durability credit. A new writer refuses to reopen an existing session file for writing.
5. `platformio.ini`: new `tbeam-supreme-f24m-sos-fullpacket-deny-build-only` environment extends **F24K's always-denied SD facade**. Compile guards **refuse F24M with F24K disabled, F23D writer enabled, or logger disabled**. The build is *not* RF silent if flashed; **DO NOT FLASH**.

## Important limits — not an operational store-and-forward release

- F24M now connects the **actual SOS packet generation and accepted receive call sites** to a complete-packet RAM queue and a typechecked binary journal write path. The only permitted F24M build is the F24K deny-only profile: `ThesisSDLogger::ready` cannot become true through the denied facade and **no physical SD file can be written**. The OS-temp fake-SD host test validates the portable writer logic; it is not physical card proof.
- F24K interposes logger calls, but F23D's separate writer and additional SD handle lifetimes are **not centrally interposed**. F24M excludes F23D with a compile guard rather than claiming resolution; there is no approved writer-exclusion witness, physical media lease, or handoff/replay permission.
- A cold restart discards the in-RAM ingress queue. The writer creates new per-boot session files; the host test demonstrates reopening a previously written file **read-only and rejecting torn frames**, but firmware startup recovery across previous sessions is **not** wired. F24L's separate Python experiment demonstrates host-side ACK/retry/restart, not hardware storage durability. Do not claim physical restart recovery.
- This phase does not introduce LoRa relay TX, distant receiver acknowledgments, Wi-Fi/HTTPS, backend upload, SOS cancellation propagation, cloud idempotency, credential provisioning, or deployment. Direct-neighbor ACK is **not shore/far-end delivery**. Preserve pending/unconfirmed semantics.
- The original Vessel-002 microSD evidence and all previous F06 SD journals are protected. No physical card, board, serial, GitHub push or cloud action is authorized. No existing PlatformIO environment is changed; the new environment is for compile-only tests.

## Tested and pending

- This package's portable C++ writer + wire frame: **93 checks PASS** on Linux GCC 14 and Clang 17 with ASan+UBSan against the unchanged protocol definition. In-memory duplicate/full handling, full 62-byte payload readback, file handles closed, new session file only, rejected existing session, corrupted/torn frame rejection, and denied writer were exercised using **synthetic OS-temp files**.
- F24L existing Python 36-assertion host test against the same packet protocol: PASS.
- **Windows UCRT64 host tests, exact local Git staging, and ESP32-S3 PlatformIO compile-only build: PENDING**. Separate guarded local commit only after all Windows checks pass. Flashing and cloud actions remain forbidden.

## Next authorized implementation gate after F24M compile-only acceptance

Complete *physical owner arbitration and write/readback timing* with disposable approved media and explicit local authorization, then scan closed prior-boot SOS packet journals to recover pending originals with a strict seal/ACK scheme; separately implement genuinely authenticated end-to-end delivery and LoRa multihop verification. Do not promote this build-only profile to a deployment image.
