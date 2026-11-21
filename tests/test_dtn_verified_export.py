#!/usr/bin/env python3
"""Pure offline fixture tests: real exports are validated separately on Windows."""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1] / "scripts" / "dtn" / "audit_verified_f06_exports.py"
spec = importlib.util.spec_from_file_location("dtn_export_audit", MODULE)
assert spec and spec.loader
import sys
import types
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


def create_export(directory: Path, session: str, *, sos: bytes = b"", telemetry: bytes = b"",
                  diagnostics: bytes = b'{"kind":"boot","vessel_id":2,"uptime_ms":0}\n') -> None:
    directory.mkdir()
    files = []
    for name, raw in zip(module.NAMES, (sos, telemetry, diagnostics)):
        (directory / name).write_bytes(raw)
        files.append({"name": name, "bytes": len(raw),
                      "sha256": hashlib.sha256(raw).hexdigest().upper(), "fnv1a32": "ignored"})
    (directory / "sha256_manifest.json").write_text(
        json.dumps({"target": session, "files": files}), encoding="utf8")


def event(vessel: int = 2, boot: int = 3, index: int = 1) -> bytes:
    return json.dumps({"kind": "telemetry_rx", "vessel_id": vessel,
                       "boot_token": boot, "event_index": index,
                       "uptime_ms": 100, "rssi": -78}, separators=(",", ":")).encode() + b"\n"


class ClosedExportAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def make(self, name="S123ABCD", **kwargs) -> Path:
        dest = self.root / name
        create_export(dest, name, **kwargs)
        return dest

    def test_normal_and_empty_sos(self):
        dest = self.make(telemetry=event())
        result = module.audit_export(dest, 2)
        self.assertEqual(result.counts, {"sos_events.jsonl": 0, "telemetry.jsonl": 1,
                                        "diagnostics.jsonl": 1})
        self.assertEqual(result.identities, frozenset({(2, 3, 1)}))
        self.assertEqual(module.main(["--vessel", "2", "--export", str(dest)]), 0)

    def test_sha_mismatch_and_missing_file(self):
        dest = self.make(telemetry=event())
        (dest / "telemetry.jsonl").write_bytes(event(boot=9))
        with self.assertRaisesRegex(module.AuditError, "SHA-256 mismatch"):
            module.audit_export(dest, 2)
        (dest / "telemetry.jsonl").unlink()
        with self.assertRaises(module.AuditError):
            module.audit_export(dest, 2)

    def test_incomplete_tail_rejected(self):
        dest = self.make(telemetry=event()[:-1])
        with self.assertRaisesRegex(module.AuditError, "incomplete final"):
            module.audit_export(dest, 2)

    def test_duplicate_keys_and_identity_rejected(self):
        raw = b'{"kind":"sos_rx","vessel_id":2,"vessel_id":2,"boot_token":3,"event_index":1,"uptime_ms":5}\n'
        dest = self.make(sos=raw)
        with self.assertRaises(module.AuditError):
            module.audit_export(dest, 2)
        other = self.make(name="S0000001", sos=event(), telemetry=event())
        with self.assertRaisesRegex(module.AuditError, "duplicate F06 identity"):
            module.audit_export(other, 2)

    def test_wrong_vessel_and_nested_json_rejected(self):
        dest = self.make(telemetry=event(vessel=3))
        with self.assertRaisesRegex(module.AuditError, "wrong or invalid vessel"):
            module.audit_export(dest, 2)
        bad = event()[:-2] + b',"extra":{"nested":1}}\n'
        other = self.make(name="S0000002", telemetry=bad)
        with self.assertRaisesRegex(module.AuditError, "nested JSON"):
            module.audit_export(other, 2)

    def test_no_legacy_and_cross_session_collision(self):
        dest = self.make(name="LEGACY", telemetry=event())
        with self.assertRaisesRegex(module.AuditError, "LEGACY excluded"):
            module.audit_export(dest, 2)
        a = self.make(name="S0000003", telemetry=event())
        b = self.make(name="S0000004", sos=event())
        self.assertEqual(module.audit_export(a, 2).counts["telemetry.jsonl"], 1)
        self.assertEqual(module.main(["--vessel", "2", "--export", str(a), "--export", str(b)]), 1)

    def test_bad_number_and_line_length(self):
        nan_line = event()[:-2] + b',"extra":NaN}\n'
        dest = self.make(telemetry=nan_line)
        with self.assertRaises(module.AuditError):
            module.audit_export(dest, 2)
        other = self.make(name="S0000005", telemetry=b'{' + b'A' * 1023 + b'}\n')
        with self.assertRaises(module.AuditError):
            module.audit_export(other, 2)


if __name__ == "__main__":
    unittest.main()
