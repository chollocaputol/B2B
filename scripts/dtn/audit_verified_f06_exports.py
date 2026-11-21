#!/usr/bin/env python3
"""Offline, strictly read-only audit of CLOSED, SHA-256-verified F06 exports.

No device access, SD writes, cloud/network activity, or upload authorization.
Does not treat a local identity tuple as a cloud UUID or cryptographic proof.
Only the three isolated Sxxxxxxx session exports are supported; legacy mixed-
boot journals need a separately reviewed compatibility path.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import re
import sys

NAMES = ("sos_events.jsonl", "telemetry.jsonl", "diagnostics.jsonl")
SESSION = re.compile(r"S[0-9A-F]{7}\Z")
KEY = re.compile(r"[A-Za-z0-9_]{1,47}\Z")
KIND = re.compile(r"[a-z0-9_]{1,39}\Z")
SHA256 = re.compile(r"[0-9A-Fa-f]{64}\Z")
MAX_FILE_BYTES = 1_048_576  # existing F06 historical USB export limit
MAX_LINE_BYTES = 1_024     # existing host-only JSONL framer limit
MAX_KEYS = 48
UINT32_MAX = (1 << 32) - 1


class AuditError(Exception):
    """Fail closed: no record from this export is approved for upload."""


@dataclass(frozen=True)
class Summary:
    session: str
    counts: dict[str, int]
    boot_tokens: frozenset[int]
    identities: frozenset[tuple[int, int, int]]


def _no_duplicate_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    obj = {}
    for name, value in pairs:
        if name in obj:
            raise AuditError("duplicate JSON property")
        obj[name] = value
    return obj


def _reject_constant(value: str) -> object:
    raise AuditError("nonstandard JSON number " + value)


def _json_bytes(raw: bytes) -> object:
    try:
        return json.loads(raw.decode("ascii"), object_pairs_hook=_no_duplicate_keys,
                          parse_constant=_reject_constant)
    except (UnicodeError, json.JSONDecodeError, ValueError, OverflowError) as exc:
        raise AuditError("invalid JSON or non-ASCII source") from exc


def _u32(value: object, positive: bool = False) -> bool:
    return type(value) is int and (1 if positive else 0) <= value <= UINT32_MAX


def _parse_record(line: bytes, filename: str, vessel: int, ordinal: int) -> tuple[int, int, int] | None:
    if not 2 <= len(line) <= MAX_LINE_BYTES:
        raise AuditError(f"{filename}: record {ordinal} has invalid byte length")
    if line[0] != ord('{') or line[-1] != ord('}') or any(b < 32 or b > 126 for b in line):
        raise AuditError(f"{filename}: record {ordinal} has invalid flat JSONL framing")
    obj = _json_bytes(line)
    if not isinstance(obj, dict) or len(obj) > MAX_KEYS:
        raise AuditError(f"{filename}: record {ordinal} is not a bounded flat object")
    for key, value in obj.items():
        if KEY.fullmatch(key) is None:
            raise AuditError(f"{filename}: record {ordinal} has unsupported property name")
        if not (value is None or type(value) in (str, int, float, bool)):
            raise AuditError(f"{filename}: record {ordinal} has nested JSON content")
        if type(value) is float and not math.isfinite(value):
            raise AuditError(f"{filename}: record {ordinal} has nonfinite value")
    if type(obj.get("kind")) is not str or KIND.fullmatch(obj["kind"]) is None:
        raise AuditError(f"{filename}: record {ordinal} has invalid kind")
    if not _u32(obj.get("vessel_id")) or obj["vessel_id"] != vessel:
        raise AuditError(f"{filename}: record {ordinal} has wrong or invalid vessel")
    if filename == "diagnostics.jsonl":
        return None  # diagnostics are NEVER ordinary telemetry upload candidates
    if not (_u32(obj.get("boot_token")) and _u32(obj.get("event_index"), positive=True)
            and _u32(obj.get("uptime_ms"))):
        raise AuditError(f"{filename}: record {ordinal} lacks an unsigned F06 event identity")
    return vessel, obj["boot_token"], obj["event_index"]


def audit_export(directory: Path, vessel: int) -> Summary:
    if not _u32(vessel, positive=True) or vessel > 9999:
        raise AuditError("unsupported vessel ID")
    directory = directory.resolve(strict=True)
    if not directory.is_dir():
        raise AuditError("export path is not a folder")
    manifest_path = directory / "sha256_manifest.json"
    if manifest_path.is_symlink() or not manifest_path.is_file():
        raise AuditError("missing or linked manifest")
    if manifest_path.stat().st_size > 65536:
        raise AuditError("manifest exceeds size limit")
    manifest = _json_bytes(manifest_path.read_bytes())
    if not isinstance(manifest, dict):
        raise AuditError("invalid manifest")
    target = manifest.get("target")
    if not isinstance(target, str) or SESSION.fullmatch(target) is None:
        raise AuditError("only closed isolated Sxxxxxxx sessions are accepted; LEGACY excluded")
    files = manifest.get("files")
    if not isinstance(files, list) or len(files) != len(NAMES):
        raise AuditError("expected exactly three manifest entries")
    entries = {}
    for entry in files:
        if not isinstance(entry, dict) or entry.get("name") not in NAMES:
            raise AuditError("unknown journal in manifest")
        name = entry["name"]
        if name in entries:
            raise AuditError("duplicate journal in manifest")
        if type(entry.get("bytes")) is not int or not 0 <= entry["bytes"] <= MAX_FILE_BYTES:
            raise AuditError("invalid journal size in manifest")
        if type(entry.get("sha256")) is not str or SHA256.fullmatch(entry["sha256"]) is None:
            raise AuditError("missing or invalid SHA-256 digest")
        entries[name] = entry
    if set(entries) != set(NAMES):
        raise AuditError("manifest is missing required journals")

    # Stage all files and verify the entire export BEFORE accepting ANY record.
    data = {}
    for name in NAMES:
        path = directory / name
        if path.is_symlink() or not path.is_file():
            raise AuditError(f"missing or linked journal: {name}")
        if path.stat().st_size != entries[name]["bytes"]:
            raise AuditError(f"size mismatch: {name}")
        payload = path.read_bytes()  # no writes; bounded by verified file size
        if len(payload) != entries[name]["bytes"]:
            raise AuditError(f"changed size while reading: {name}")
        if hashlib.sha256(payload).hexdigest().upper() != entries[name]["sha256"].upper():
            raise AuditError(f"SHA-256 mismatch: {name}")
        data[name] = payload

    counts: dict[str, int] = {}
    identities: set[tuple[int, int, int]] = set()
    tokens: set[int] = set()
    for name in NAMES:
        raw = data[name]
        if raw and not raw.endswith(b"\n"):
            raise AuditError(f"{name}: incomplete final JSONL line")
        n = 0
        for physical_line in raw.split(b"\n")[:-1] if raw else ():
            n += 1
            line = physical_line[:-1] if physical_line.endswith(b"\r") else physical_line
            identity = _parse_record(line, name, vessel, n)
            if identity is not None:
                if identity in identities:
                    raise AuditError(f"{name}: duplicate F06 identity across SOS/telemetry")
                identities.add(identity)
                tokens.add(identity[1])
        counts[name] = n
    return Summary(target, counts, frozenset(tokens), frozenset(identities))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--export", type=Path, action="append", required=True,
                        help="Existing isolated F06 session export folder; repeat for multiple")
    parser.add_argument("--vessel", type=int, required=True, help="Expected vessel ID")
    opts = parser.parse_args(argv)
    total = {name: 0 for name in NAMES}
    global_ids: set[tuple[int, int, int]] = set()
    sessions: set[str] = set()
    summaries: list[Summary] = []
    try:
        for directory in opts.export:
            item = audit_export(directory, opts.vessel)
            if item.session in sessions:
                raise AuditError(f"duplicate session export: {item.session}")
            collision = global_ids.intersection(item.identities)
            if collision:
                raise AuditError(f"replayed identity collision across isolated sessions: {item.session}")
            sessions.add(item.session)
            global_ids.update(item.identities)
            summaries.append(item)
        # Print only after all exports pass; never print sensitive journal fields.
        for item in summaries:
            for name in NAMES:
                total[name] += item.counts[name]
            print(f"PASS {item.session}: SOS={item.counts[NAMES[0]]} "
                  f"telemetry={item.counts[NAMES[1]]} "
                  f"diagnostics={item.counts[NAMES[2]]} "
                  f"boot_tokens={len(item.boot_tokens)}")
        print("CLOSED F06 EXPORT READ-ONLY AUDIT PASS: "
              f"sessions={len(sessions)} SOS={total[NAMES[0]]} "
              f"telemetry={total[NAMES[1]]} diagnostics={total[NAMES[2]]} "
              "duplicate_event_keys=0; upload_authorization=NONE")
    except (AuditError, OSError) as exc:
        print(f"STOP: {exc}. No evidence files modified.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
