#!/usr/bin/env python3
"""F24L offline-only executable: genuine 62-byte SOS wire layout, disposable host storage.

Never opens an SD card, radio, serial port, socket, Wi-Fi, or cloud service.
The fake server is a JSON file in a uniquely created host temp directory.
This is NOT production storage durability, protocol authentication or physical authorization.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from typing import Any

# Verified against GitHub feature/firmware-multihop shared packet header blob.
PROTOCOL_GIT_BLOB = "c5ceccfda2eb86ae6aa1db23aa6461f93670ebac"
# Packed VesselPacket: magic, version, type, origin, event, UPTIME seconds,
# hop, flags, name, lat, lon, distance, battery, 7 weather fields, message.
WIRE = struct.Struct("<HBBIIIBB12siiIBhHHhBBB8s")
FRAME_HEADER = struct.Struct("<4sH32s")
FRAME_MAGIC = b"F24L"
PACKET_MAGIC, PACKET_VERSION, PACKET_SOS = 0x4D45, 1, 2
FLAG_GPS_FIX, FLAG_TEMP_HUMIDITY_VALID, FLAG_PRESSURE_VALID = 1, 2, 4
SYNTHETIC_DIR_PREFIX = "THESIS_F24L_SYNTHETIC_"
assert WIRE.size == 62 and FRAME_HEADER.size == 38


class Denied(RuntimeError):
    pass


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise Denied(reason)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical(obj: Any) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("ascii")


def verify_protocol(repo: Path) -> None:
    file = repo / "firmware/shared/communication/packet_protocol.h"
    require(file.is_file(), "real shared packet_protocol.h missing")
    data = file.read_bytes().replace(b"\r\n", b"\n")
    blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
    require(blob == PROTOCOL_GIT_BLOB, "packet protocol changed; stop and re-audit wire layout")
    require(b"static_assert(sizeof(VesselPacket) == 62" in data, "62-byte ABI assertion missing")


def allowed_workdir(folder: Path) -> Path:
    folder = folder.resolve()
    temp = Path(tempfile.gettempdir()).resolve()
    require(folder != temp and temp in folder.parents, "test folder must be below OS TEMP")
    relative = folder.relative_to(temp)
    require(relative.parts and relative.parts[0].startswith(SYNTHETIC_DIR_PREFIX),
            "outside F24L disposable synthetic host sandbox")
    # Do not recurse through links to unexpected filesystems.
    require(not folder.is_symlink(), "symlink sandbox forbidden")
    return folder


def fsync_directory(folder: Path) -> None:
    if os.name == "nt":
        return  # Windows host tests cannot claim directory metadata power-loss durability.
    fd = os.open(str(folder), os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def durable_write(file: Path, data: bytes, *, create_only: bool = False) -> None:
    require(file.parent.is_dir(), "invalid test data parent")
    if create_only:
        with file.open("xb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
    else:
        tmp = file.with_name(file.name + ".temp-write")
        require(not tmp.exists(), "pending temp-write, refuse unreviewed replacement")
        with tmp.open("xb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(tmp, file)
    fsync_directory(file.parent)


def seal_json(file: Path, value: dict, *, create_only: bool = False) -> None:
    durable_write(file, canonical(value) + b"\n", create_only=create_only)


def read_json(file: Path) -> dict:
    result = json.loads(file.read_bytes().decode("ascii"))
    require(isinstance(result, dict), "JSON object required")
    return result


def make_sos(event: int) -> bytes:
    # Synthetic values only; timestamp field means milliseconds/1000 (UPTIME).
    # Fixed battery=100, shore-distance=0 match current firmware placeholders.
    payload = WIRE.pack(PACKET_MAGIC, PACKET_VERSION, PACKET_SOS,
                        1, event, 3600 + event, 0,
                        FLAG_GPS_FIX | FLAG_TEMP_HUMIDITY_VALID | FLAG_PRESSURE_VALID,
                        b"VESSEL-001\0\0", 84500000, 1246360000, 0, 100,
                        281, 785, 10133, -3, 5, 1, 2, b"SOS\0\0\0\0\0")
    validate_sos(payload)
    return payload


def validate_sos(payload: bytes) -> tuple[int, int]:
    require(len(payload) == 62, "SOS wire length mismatch")
    fields = WIRE.unpack(payload)
    magic, ver, typ, origin, event, uptime, hops, flags = fields[:8]
    require(magic == PACKET_MAGIC and ver == PACKET_VERSION and typ == PACKET_SOS,
            "only genuine-layout legacy v1 SOS may enter this queue")
    require(1 <= origin <= 9999 and event > 0, "invalid SOS event identity")
    require(hops <= 15 and (flags & ~7) == 0, "SOS flags/hops invalid")
    lat, lon = fields[9:11]
    if flags & FLAG_GPS_FIX:
        require(-900000000 <= lat <= 900000000 and -1800000000 <= lon <= 1800000000
                and (lat, lon) != (0, 0), "invalid GPS-fix coordinates")
    return origin, event


def identity(payload: bytes) -> tuple[str, str]:
    origin, event = validate_sos(payload)
    return f"{origin}:{event}", digest(payload)


def init_synthetic(folder: Path) -> dict:
    require(not folder.exists(), "init requires a new disposable folder")
    folder.mkdir(parents=True)
    durable_write(folder / ".F24L_TEST_ONLY", b"synthetic, not firmware or SD\n", create_only=True)
    records = [make_sos(1001), make_sos(1002), make_sos(1003)]
    journal = b"".join(FRAME_HEADER.pack(FRAME_MAGIC, 62, bytes.fromhex(digest(raw))) + raw
                       for raw in records)
    durable_write(folder / "sos_packets.bin", journal, create_only=True)
    seal_json(folder / "sos_packets.seal.json",
              {"version": 1, "file_sha256": digest(journal), "size": len(journal),
               "record_count": 3, "closed": True}, create_only=True)
    durable_write(folder / "research_only.jsonl",
                  b'{"kind":"research_weather","temperature":28.1}\n', create_only=True)
    seal_json(folder / "fake_server.json", {"accepted": {}, "retry_once": False}, create_only=True)
    return {"created": 3, "journal_sha256": digest(journal)}


def read_closed(folder: Path) -> tuple[list[bytes], str]:
    require((folder / ".F24L_TEST_ONLY").is_file(), "missing synthetic-only marker")
    seal = read_json(folder / "sos_packets.seal.json")
    require(seal.get("closed") is True and seal.get("version") == 1, "session not closed")
    blob = (folder / "sos_packets.bin").read_bytes()
    require(len(blob) == seal.get("size") and digest(blob) == seal.get("file_sha256"),
            "source changed since closure: deny all forwarding")
    stride = FRAME_HEADER.size + 62
    require(len(blob) % stride == 0 and len(blob) // stride == seal.get("record_count"),
            "torn SOS packet stream")
    records = []
    unique = set()
    for offset in range(0, len(blob), stride):
        magic, length, frame_digest = FRAME_HEADER.unpack(blob[offset:offset + FRAME_HEADER.size])
        payload = blob[offset + FRAME_HEADER.size: offset + stride]
        require(magic == FRAME_MAGIC and length == len(payload) == 62
                and bytes.fromhex(digest(payload)) == frame_digest, "bad frame digest or length")
        key, _ = identity(payload)
        require(key not in unique, "duplicate SOS identity in source")
        unique.add(key)
        records.append(payload)
    return records, digest(blob)


class AckSidecar:
    """Two generation slots; simulated tear recovery, not FAT/power-cut proof."""
    def __init__(self, folder: Path, source_hash: str):
        self.folder, self.source_hash = folder, source_hash
        self.generation = 0
        self.acks: dict[str, str] = {}
        valid = []
        found = False
        for letter in ("a", "b"):
            file = folder / ("ack_" + letter + ".json")
            if not file.exists():
                continue
            found = True
            try:
                document = read_json(file)
                content = document["body"]
                require(document["checksum"] == digest(canonical(content)), "bad ACK slot checksum")
                require(content["source_hash"] == source_hash, "source/sidecar mismatch")
                require(isinstance(content["generation"], int) and content["generation"] > 0,
                        "ACK generation invalid")
                require(isinstance(content["acks"], dict), "ACK map invalid")
                require(all(isinstance(k, str) and isinstance(v, str) and len(v) == 64
                            for k, v in content["acks"].items()), "ACK content invalid")
                valid.append(content)
            except (ValueError, KeyError, TypeError, OSError, Denied, UnicodeError):
                continue
        require(not found or bool(valid), "no valid ACK generation; fail closed")
        if valid:
            latest = max(valid, key=lambda x: x["generation"])
            self.generation = latest["generation"]
            self.acks = latest["acks"]

    def acknowledge(self, key: str, content_hash: str) -> None:
        if key in self.acks:
            require(self.acks[key] == content_hash, "ACK identity collision")
            return
        updated = dict(self.acks)
        updated[key] = content_hash
        gen = self.generation + 1
        body = {"generation": gen, "source_hash": self.source_hash, "acks": updated}
        slot = self.folder / ("ack_a.json" if gen % 2 else "ack_b.json")
        seal_json(slot, {"body": body, "checksum": digest(canonical(body))})
        self.acks, self.generation = updated, gen


class FakeShoreServer:
    """Persistent in-process adapter; never opens a socket or calls HTTPS."""
    def __init__(self, folder: Path):
        self.file = folder / "fake_server.json"
        self.data = read_json(self.file)
        require(isinstance(self.data.get("accepted"), dict), "fake server corrupt")
        self.calls = 0

    def submit(self, key: str, payload_hash: str, *, temporary_retry: bool = False) -> dict:
        self.calls += 1
        if temporary_retry and not self.data["retry_once"]:
            self.data["retry_once"] = True
            seal_json(self.file, self.data)
            return {"status": "retry", "key": key, "digest": payload_hash}
        saved = self.data["accepted"].get(key)
        if saved is not None:
            return {"status": "duplicate" if saved == payload_hash else "conflict",
                    "key": key, "digest": saved}
        self.data["accepted"][key] = payload_hash
        seal_json(self.file, self.data)
        return {"status": "accepted", "key": key, "digest": payload_hash}


def response_confirmed(response: dict, key: str, payload_hash: str) -> bool:
    # Structural verification only. Real transport must independently authenticate
    # server identity and tie response to the submitted request.
    return (isinstance(response, dict)
            and response.get("status") in ("accepted", "duplicate")
            and response.get("key") == key
            and response.get("digest") == payload_hash)


def sync_pass(folder: Path, *, connected: bool, urgent_busy: bool = False,
              stop_after_accept_without_ack: bool = False) -> dict:
    if not connected or urgent_busy:
        return {"sent": 0, "acked_now": 0, "paused": True}  # no source/media access
    items, source_hash = read_closed(folder)
    ledger = AckSidecar(folder, source_hash)
    server = FakeShoreServer(folder)
    acked_now, pending, retries, duplicates = 0, 0, 0, 0
    for item in items:
        key, payload_hash = identity(item)
        if key in ledger.acks:
            require(ledger.acks[key] == payload_hash, "stale ACK content digest")
            continue
        reply = server.submit(key, payload_hash, temporary_retry=(key == "1:1003"))
        if stop_after_accept_without_ack:
            require(reply["status"] == "accepted", "expected first server acceptance")
            return {"sent": server.calls, "acked_now": 0, "lost_response": True}
        if reply["status"] == "retry":
            retries += 1
            pending += 1
            continue
        if reply["status"] == "duplicate":
            duplicates += 1
        if not response_confirmed(reply, key, payload_hash):
            raise Denied("server reply lacks matching item proof; no ACK written")
        ledger.acknowledge(key, payload_hash)
        acked_now += 1
    return {"sent": server.calls, "acked_now": acked_now,
            "pending": pending, "retry": retries, "duplicates": duplicates}


def phase(name: str, folder: Path) -> dict:
    folder = allowed_workdir(folder)
    if name == "init":
        return init_synthetic(folder)
    if name == "offline":
        result = sync_pass(folder, connected=False)
        require(not (folder / "ack_a.json").exists(), "offline generated ACK")
        return result
    if name == "lost-response":
        return sync_pass(folder, connected=True, stop_after_accept_without_ack=True)
    if name == "online1":
        return sync_pass(folder, connected=True)
    if name == "online2":
        return sync_pass(folder, connected=True)
    if name == "audit":
        records, source_hash = read_closed(folder)
        acks = AckSidecar(folder, source_hash).acks
        server = FakeShoreServer(folder).data["accepted"]
        require(len(records) == len(acks) == len(server) == 3, "source/server/ACK count mismatch")
        require(all(acks.get(identity(r)[0]) == identity(r)[1] for r in records),
                "not all SOS data durably acknowledged")
        return {"source_packets": len(records), "durable_acks": len(acks),
                "mock_server_records": len(server), "source_sha256": source_hash}
    raise Denied("invalid phase")


def self_test(repo: Path) -> None:
    verify_protocol(repo)
    tests = 0
    def check(condition: bool, detail: str) -> None:
        nonlocal tests
        require(condition, "TEST FAIL: " + detail)
        tests += 1

    with tempfile.TemporaryDirectory(prefix=SYNTHETIC_DIR_PREFIX) as tempdir:
        folder = Path(tempdir) / "scenario"  # created only by init phase
        cmd = [sys.executable, str(Path(__file__).resolve()), "--repo", str(repo),
               "--workdir", str(folder), "--phase"]
        def run_step(step: str) -> dict:
            completed = subprocess.run(cmd + [step], capture_output=True, text=True, check=False)
            if completed.returncode:
                raise Denied(f"separate-process {step} FAILED: {completed.stdout} {completed.stderr}")
            result = json.loads(completed.stdout.strip().splitlines()[-1])
            print(f"F24L PROCESS {step}: {json.dumps(result, sort_keys=True)}")
            return result

        created = run_step("init")
        original_bytes = (folder / "sos_packets.bin").read_bytes()
        original_research = (folder / "research_only.jsonl").read_bytes()
        check(created["created"] == 3, "three synthetic full SOS packets")
        check(created["journal_sha256"] == digest(original_bytes), "source hash")
        check(len(original_bytes) == 3 * (FRAME_HEADER.size + 62), "full 62-byte frames persist")
        samples, before_hash = read_closed(folder)
        check(len(samples) == 3, "closed source loaded")
        check(WIRE.unpack(samples[0])[13:20] == (281,785,10133,-3,5,1,2),
              "weather fields remain in stored SOS frame")
        check(WIRE.unpack(samples[0])[9:11] == (84500000,1246360000),
              "GPS fields in stored SOS frame")
        check(WIRE.unpack(samples[0])[5] == 4601, "timestamp is uptime seconds")
        check(WIRE.unpack(samples[0])[12] == 100, "battery placeholder preserved")
        offline = run_step("offline")
        check(offline["sent"] == 0, "offline no server calls")
        check(not list(folder.glob("ack_*.json")), "offline no ACK")
        lost = run_step("lost-response")
        check(lost["sent"] == 1 and lost["lost_response"], "accepted before response lost")
        check(len(FakeShoreServer(folder).data["accepted"]) == 1,
              "fake shore accepted one SOS")
        check(not list(folder.glob("ack_*.json")), "no optimistic ACK from lost response")
        first = run_step("online1")
        check(first["acked_now"] == 2, "two durable ACKs after reboot")
        check(first["duplicates"] == 1, "same-key repeat classified duplicate")
        check(first["retry"] == 1, "transient retry remains pending")
        check(len(AckSidecar(folder, before_hash).acks) == 2, "gap remains unacked")
        check(len(FakeShoreServer(folder).data["accepted"]) == 2,
              "no duplicate event created at server")
        second = run_step("online2")
        check(second["acked_now"] == 1 and second["retry"] == 0,
              "last pending SOS forwarded after another reboot")
        final = run_step("audit")
        check(final["source_packets"] == final["durable_acks"] == 3, "all SOS acknowledged")
        check(len(FakeShoreServer(folder).data["accepted"]) == 3, "fake server three unique SOS")
        check((folder / "sos_packets.bin").read_bytes() == original_bytes,
              "source journal byte-for-byte unchanged")
        check((folder / "research_only.jsonl").read_bytes() == original_research,
              "research-only logging never selected or uploaded")
        check(sync_pass(folder, connected=False)["sent"] == 0,
              "offline after full delivery still no server work")
        check(sync_pass(folder, connected=True, urgent_busy=True)["sent"] == 0,
              "urgent SOS priority suppresses sync")
        # Negative cases all operate on this unique host temp directory, then restore.
        src = folder / "sos_packets.bin"
        changed = bytearray(original_bytes)
        changed[FRAME_HEADER.size + 34] ^= 1
        src.write_bytes(changed)
        try:
            read_closed(folder)
        except Denied:
            tests += 1
        else:
            raise Denied("same-size source mutation was not blocked")
        src.write_bytes(original_bytes + b"\x00")
        try:
            read_closed(folder)
        except Denied:
            tests += 1
        else:
            raise Denied("torn source tail was not blocked")
        src.write_bytes(original_bytes)
        seal_file = folder / "sos_packets.seal.json"
        seal_bytes = seal_file.read_bytes()
        seal_file.unlink()
        try:
            read_closed(folder)
        except (Denied, FileNotFoundError):
            tests += 1
        else:
            raise Denied("missing close seal was not blocked")
        seal_file.write_bytes(seal_bytes)
        last_slot = folder / "ack_a.json"  # generation 3
        require(last_slot.is_file(), "expected 3rd generation ACK slot")
        saved_slot = last_slot.read_bytes()
        last_slot.write_bytes(b'{"torn":')
        recovered = AckSidecar(folder, before_hash)
        check(recovered.generation == 2 and len(recovered.acks) == 2,
              "torn newest ACK slot recovers older valid generation")
        last_slot.write_bytes(saved_slot)
        check(not response_confirmed({"status": "accepted", "key": "1:1001",
                                      "digest": "0"*64}, "1:1001", identity(samples[0])[1]),
              "wrong-digest response rejected")
        check(not response_confirmed({"http": 200}, "1:1001", identity(samples[0])[1]),
              "HTTP 200 cannot act as item ACK")
        check(not response_confirmed({"status": "accepted", "key": "1:2000",
                                      "digest": identity(samples[0])[1]},
                                     "1:1001", identity(samples[0])[1]),
              "wrong-key response rejected")
        legacy = bytearray(samples[0]);legacy[3] = 1
        try:
            validate_sos(bytes(legacy))
        except Denied:
            tests += 1
        else:
            raise Denied("routine telemetry incorrectly admitted to DTN")
        conflict = FakeShoreServer(folder).submit("1:1001", "f"*64)
        check(conflict["status"] == "conflict", "same SOS id changed content blocked")
        check(digest(src.read_bytes()) == before_hash, "all source negative tests restored")
        check((folder / "research_only.jsonl").read_bytes() == original_research,
              "research-only source still preserved")
        print(f"=== F24L SYNTHETIC SOS-ONLY E2E PASS: {tests} ASSERTIONS; "
              f"3 PACKETS, 3 UNIQUE ACCEPTED, 3 ACKS; 0 SEPARATE ROUTINE/RESEARCH DTN RECORDS ===")
        print("Boundaries: isolated host files and fake shore responses ONLY. "
              "No firmware SD owner, real LoRa forwarding, TLS, cloud or hardware validated.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--self-test", action="store_true")
    group.add_argument("--phase", choices=("init", "offline", "lost-response", "online1", "online2", "audit"))
    parser.add_argument("--workdir", type=Path)
    args = parser.parse_args()
    try:
        if args.self_test:
            self_test(args.repo.resolve())
        else:
            require(args.workdir is not None, "phase requires --workdir")
            verify_protocol(args.repo.resolve())
            print(json.dumps(phase(args.phase, args.workdir), sort_keys=True))
        return 0
    except (Denied, ValueError, KeyError, TypeError, OSError, struct.error, json.JSONDecodeError) as exc:
        print("F24L STOP: " + str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
