# THESIS F24P — SOS prior-session directory discovery through F24O owner (BUILD ONLY)

**Basis:** locally committed F24O `493f575cbc57a19a794dc5deac861b565cfdc319`, on `feature/firmware-multihop`, clean worktree before staging. F24P is a *candidate patch*, not accepted hardware integration, a firmware release or a Git commit.

## Narrow actual change

- `DTNF24O::Port` gains an owner-bound, read-only `scanClosedSessions(root, "r", sink)` that restricts the root to the exact F06 vessel path. It holds the port's single logical owner slot while using up to two internal directory handles (root and one child), checks the injected independent witness before/after sensitive calls, ignores ordinary files, rejects invalid or foreign session directories, excludes the active session, refuses unresolved overflow at 64 entries, and latches failure. No journal is changed.
- Only the new F24P firmware profile routes F24N's old-session directory discovery through that same F24O port. Older firmware profiles preserve their prior source path. If discovery fails, F24N does not acquire approval for any source replay.
- The new profile extends F24O/F24K's **permanent denied-SD facade** and `DenyWitness`. It cannot call real `SD.begin`, acquire a file handle, issue shore receipts or replay SOS. The independent F23D writer is compile-time OFF in this profile.
- The portable `tests/dtn_f24p_owner_discovery_host.cpp` exercises simulated directory names, owner lease change, urgency, competing readers/writers, duplicate isolation, bad paths and overflow. Its permissive witness is a **mock**, NOT firmware authority.

## Evidence and bounded guarantees

Linux host review against the uploaded 52-file bundle plus an external 62-byte protocol-layout test dependency: F24P 133 PASS; F24O regression in seven host processes 266 PASS. This **does not** substitute for a Windows native compile against the local `firmware/shared/communication/packet_protocol.h`, nor ESP32-S3 PlatformIO compilation. Await actual user-run output.

F24O still performs mandatory handle `close` after revocation. Neither F24O nor F24P proves that a physical owner may safely clean up a file after its lease is lost. No real card or power-loss/durability behavior was tested. F24N discovery is bounded but may traverse up to 64 entries in one call; on-board scheduling latency must be measured before any physical grant.

## Still unresolved — not "global SD ownership"

1. The legacy `ThesisSDLogger` still uses raw `File` objects returned by F24K-interposed **entrypoints**, including append, directory/export/history, audit, initialization, and persistent history handles. F24K DENY mode makes entrypoints inaccessible in F24P, but this is not a real global reader/writer lock.
2. `firmware/shared/storage/f23d_sd_file_writer.h` holds independent `File file_` and `File verification_` and calls `SD.open/exists` directly. F24P forbids this profile, it does not solve concurrent writer interposition for future physical use.
3. Optional adapter headers `dtn_ack_sidecar_arduino_port.h`, `dtn_f06_arduino_sd_read_port.h`, `dtn_f24d_arduino_read_port.h`, and standalone `sd_probe.h` contain direct SD calls. The audit does not prove that all call sites, helper includes, or future profiles route through a single physical owner.
4. Trusted media/CID identity, source mutation epochs, immutable closure, reliable sidecar persistence, exclusive physical SD lock and handle revocation cleanup, authenticated backend receipts and RLS, HTTPS/Wi-Fi, physical multi-hop RF and fleet approval are **not implemented/authorized** here.

**Preserve:** normal POS v2 25 bytes and `VesselPacket` 62 bytes; SOS priority and on-air ACK semantics; routine weather/GPS/research journals are not operational SOS DTN; no V002 historical SD access; no flashing, pushing, Docker, Supabase changes or field use.

## Windows acceptance before a local-only commit

Use `STAGE_F24P.ps1` from the patch package against the exact clean local F24O HEAD. Then execute `TEST_F24P.ps1`, using `C:\PlatformIO\penv\Scripts\platformio.exe`, MSYS2 UCRT64 g++ if available. Inspect detailed `git diff` and require a clean `git diff --check`; do not commit without reviewing the complete Windows report. The next milestone should separately tackle the **legacy logger and F23D all-handle owner boundary** without granting physical I/O.
