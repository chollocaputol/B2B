"""Validate exported F23D journals. No device/SD/NVS access; no RF proof from hops."""
import argparse
import hashlib
import json
from pathlib import Path
import re

class Invalid(ValueError):
    pass

def require(ok, message):
    if not ok:
        raise Invalid(message)

NUMBERS = ("v local i ms run origin event type hop bytes wire canonical rc rx_rc "
           "rssi snr_x10 used sos_left cancel_left lost health pending written urgent_lost").split()
TEXT = ("session record validation state").split()

def read_journal(path, local, session=None):
    rows = []
    with Path(path).open("rb") as stream:
        while True:
            raw=stream.readline(768)
            if not raw:
                break
            require(len(rows)<50,"too many records for one bounded capture")
            require(0 < len(raw) < 768 and raw.endswith(b"\n"), "partial/oversized JSONL record")
            try:
                row = json.loads(raw)
            except (ValueError, UnicodeError) as error:
                raise Invalid("invalid JSONL") from error
            require(type(row) is dict, "record is not an object")
            require(set(row) == set(NUMBERS + TEXT + ["permit", "called"]), "unknown/missing schema fields")
            for name in NUMBERS:
                require(type(row[name]) is int, "non-integer " + name)
                if name not in ("rc", "rx_rc", "rssi", "snr_x10"):
                    require(0 <= row[name] <= 0xFFFFFFFF, "numeric range " + name)
            for name in ("permit", "called"):
                require(type(row[name]) is bool, "non-boolean " + name)
            for name in TEXT:
                require(type(row[name]) is str, "non-string " + name)
            require(re.fullmatch(r"S[0-9A-F]{7}", row["session"]), "invalid session")
            require(row["v"] == 1 and row["local"] == local, "schema/vessel mismatch")
            require(row["i"] == len(rows) + 1 and row["written"] == len(rows), "sequence gap/reordering")
            require(not (row["lost"] or row["health"] or row["urgent_lost"]), "loss/storage-health failure")
            require(row["hop"] <= 255 and row["type"] <= 255 and row["bytes"] <= 99, "packet field range")
            for name, limit in (("record", 24), ("validation", 24), ("state", 20)):
                require(len(row[name]) < limit and re.fullmatch(r"[A-Z0-9_]*", row[name]), "invalid record token")
            if local != 2:
                require(row["run"] == 0, "endpoint invented a relay run ID")
            if rows:
                require(row["session"] == rows[0]["session"], "reboot/session mixing")
                require((row["ms"] - rows[-1]["ms"]) & 0xFFFFFFFF < 0x80000000, "uptime ordering")
            rows.append(row)
    require(len(rows) >= 3, "empty/incomplete journal")
    require(rows[0]["record"] == "BOOT" and rows[1]["record"] == "START", "missing BOOT/START")
    require(rows[-1]["record"] == "END" and rows[-1]["pending"] == 0, "missing clean END/drain")
    require(sum(r["record"] == "BOOT" for r in rows) == 1 and
            sum(r["record"] == "START" for r in rows) == 1 and
            sum(r["record"] == "END" for r in rows) == 1, "reboot/reused capture")
    if session is not None:
        require(rows[0]["session"] == session, "manifest session mismatch")
    return rows

def selected(rows, stage, event, kind=None):
    packet_stages={"RX","TX_BEGIN","TX_RESULT","ALLOCATED","LOCAL_SOS","LOCAL_CANCEL","ACCEPTED","RESERVED","RADIO_INVOKE","RADIO_RESULT"}
    return [r for r in rows if r["record"] == stage and r["event"] == event and
            (stage not in packet_stages or r["origin"] == 1) and (kind is None or r["type"] == kind)]

def successful_tx(rows, event, kind, hop):
    begin = selected(rows, "TX_BEGIN", event, kind)
    result = selected(rows, "TX_RESULT", event, kind)
    require(1 <= len(begin) <= 4 and len(begin) == len(result), "origin attempt count/pairing")
    for number, (b, r) in enumerate(zip(begin, result), 1):
        require(b["i"] < r["i"] and b["used"] == r["used"] == number, "origin attempt ordering")
        require(b["origin"] == r["origin"] == 1 and b["hop"] == r["hop"] == hop, "origin route")
        require(b["canonical"] == r["canonical"] != 0 and b["bytes"] == r["bytes"] == 62, "origin packet identity")
    good = [r for r in result if r["called"] and r["rc"] == 0 and r["rx_rc"] == 0]
    require(good, "no successful actual origin radio call")
    return good

def delivery(journals, manifest):
    event, run = manifest["event"], manifest["run"]
    require(type(event) is int and 0 < event < 0xFFFFFFFF and
            type(run) is int and 0 < run <= 0xFFFFFFFF, "invalid run/event")
    a, b, c = (journals[x] for x in (1, 2, 3))
    require(len(selected(a, "ALLOCATED", event, 2)) == 1, "missing/duplicate V001 event allocation")
    require(len(selected(a, "LOCAL_SOS", event, 2)) == 1, "missing V001 local SOS")
    require(len(selected(a, "LOCAL_CANCEL", event, 4)) == 1, "missing matching V001 local CANCEL")
    origin = {kind: successful_tx(a, event, kind, 0) for kind in (2, 4)}
    require(selected(a, "TX_BEGIN", event, 2)[0]["i"] < selected(a, "LOCAL_CANCEL", event, 4)[0]["i"],
            "CANCEL precedes originating SOS")
    selection = selected(b, "SELECTED", event)
    arms = selected(b, "ARMED", event)
    require(len(selection) == len(arms) == 1 and selection[0]["i"] < arms[0]["i"], "missing SELECT/ARM sequence")
    require(selection[0]["run"] == arms[0]["run"] == run and arms[0]["permit"], "wrong/unauthorized run")
    for stage in ("RESERVED", "RADIO_INVOKE", "RADIO_RESULT"):
        attempts = [r for r in b if r["record"] == stage]
        require(len(attempts) == 2 and {r["type"] for r in attempts} == {2, 4} and
                all(r["run"] == run and r["event"] == event for r in attempts), "extra/out-of-run relay attempt")
    previous_result = None
    for kind, state in ((2, "NEW"), (4, "CANCELLED")):
        rx = [r for r in selected(b, "RX", event, kind) if r["state"] == state]
        admission = selected(b, "ACCEPTED", event, kind)
        reserved = selected(b, "RESERVED", event, kind)
        invoke = selected(b, "RADIO_INVOKE", event, kind)
        result = selected(b, "RADIO_RESULT", event, kind)
        require(rx and len(admission) == len(reserved) == len(invoke) == len(result) == 1,
                "missing/extra relay admission or attempt")
        x, accept, reserve, call, done = rx[0], admission[0], reserved[0], invoke[0], result[0]
        require(arms[0]["i"] < x["i"] < accept["i"] < reserve["i"] < call["i"] < done["i"], "relay ordering")
        for r in (x, accept, reserve, call, done):
            require(r["origin"] == 1 and r["event"] == event and r["run"] == run and r["bytes"] == 62,
                    "relay event/run/origin mismatch")
        require(x["hop"] == accept["hop"] == 0 and reserve["hop"] == call["hop"] == done["hop"] == 1, "relay hop transition")
        require(done["used"] == reserve["used"] == (1 if kind == 2 else 2), "relay budget accounting")
        require(reserve["sos_left"] == 0 and reserve["cancel_left"] == (1 if kind == 2 else 0),
                "remaining relay budget mismatch")
        require(done["permit"] and done["called"] and done["rc"] == done["rx_rc"] == 0, "relay local failure/rejection")
        require((done["ms"] - arms[0]["ms"]) & 0xFFFFFFFF < 15000, "relay lifetime exceeded")
        require(done["canonical"] == x["canonical"] != 0 and
                any(r["canonical"] == done["canonical"] for r in origin[kind]), "origin/relay fingerprint mismatch")
        far = [r for r in selected(c, "RX", event, kind) if r["state"] == state]
        require(len(far) == 1, "missing/duplicate V003 accepted " + state)
        far = far[0]
        require(far["origin"] == 1 and far["hop"] == 1 and far["bytes"] == 62 and
                far["validation"] == "LEGACY_UNAUTHENTICATED" and
                far["canonical"] == done["canonical"] and far["wire"] == done["wire"], "V003 packet mismatch")
        if previous_result:
            require(previous_result["i"] < far["i"], "V003 cancellation ordering")
        previous_result = far
    for rows in journals.values():
        require(not any(r["origin"] == 1 and r["event"] == event and r["record"] == "RX" and
                        r["state"] not in ("NEW","DUPLICATE","CANCELLED","CANCEL_DUPLICATE") for r in rows),
                "selected-event stale replay")
    require(not any(r["event"] == event and r["record"] in
                    ("AUTH_FINAL_REJECTED", "ADAPTER_REJECTED", "EXPIRED", "REVOKED") and
                    r["i"] < selected(b,"RADIO_RESULT",event,4)[0]["i"] for r in b),
            "selected run rejected/expired")
    return "PASS"

def isolation(manifest, directory):
    proof = manifest.get("isolation")
    if not isinstance(proof, dict):
        return "INCONCLUSIVE"
    try:
        require(isinstance(proof.get("arrangement"), str) and proof["arrangement"].strip(), "missing arrangement")
        require(proof.get("reviewed") is True and proof.get("both_direct_directions_blocked") is True and
                proof.get("adjacent_bidirectional_links_verified") is True, "unverified RF arrangement")
        report = directory / proof["measurement_report"]
        require(report.is_file() and report.stat().st_size > 0 and
                hashlib.sha256(report.read_bytes()).hexdigest() == proof["measurement_sha256"], "missing measurement evidence")
        control = proof["negative_control"]
        require(control.get("relay_disabled") is True and type(control["event"]) is int and
                0 < control["event"] != manifest["event"], "invalid negative-control event")
        rows = {i: read_journal(directory / control["journals"][str(i)], i) for i in (1, 2, 3)}
        event = control["event"]
        successful_tx(rows[1], event, 2, 0)
        require(any(r["origin"] == 1 and r["hop"] == 0 and r["state"] == "NEW"
                    for r in selected(rows[2], "RX", event, 2)), "negative control did not reach V002")
        require(not selected(rows[2], "RESERVED", event), "relay attempted during negative control")
        require(not selected(rows[3], "RX", event), "direct frame observed at V003")
    except (Invalid, KeyError, OSError, TypeError, ValueError):
        return "INCONCLUSIVE"
    return "PASS"

def check(paths, manifest, directory=Path(".")):
    try:
        require(manifest.get("schema") == 1, "manifest schema")
        rows = {i: read_journal(paths[i], i, manifest["sessions"][str(i)]) for i in (1, 2, 3)}
        require(all(manifest["sealed_verified"].get(str(i)) is True for i in (1, 2, 3)),
                "shutdown verification not recorded")
        delivery(rows, manifest)
        rf = isolation(manifest, Path(directory))
        return {"status": "PASS" if rf == "PASS" else "INCONCLUSIVE",
                "delivery": "PASS", "isolation": rf,
                "note": "Operator-supplied RF evidence is required; IDs/fingerprints/ACKs are not cryptographic or isolation proof."}
    except (Invalid, KeyError, OSError, TypeError, ValueError) as error:
        return {"status": "FAIL", "reason": str(error)}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for i in (1, 2, 3):
        parser.add_argument("--v00" + str(i), required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    args = parser.parse_args()
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8-sig"))
        result = check({1: args.v001, 2: args.v002, 3: args.v003}, manifest, args.manifest.parent)
    except (OSError, ValueError) as error:
        result = {"status": "FAIL", "reason": str(error)}
    print(json.dumps(result, indent=2))
    return {"PASS": 0, "FAIL": 1, "INCONCLUSIVE": 2}[result["status"]]
if __name__ == "__main__":
    raise SystemExit(main())
