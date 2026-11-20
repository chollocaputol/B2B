"""F23C actual-header host matrix and source isolation checks. No device access."""
from pathlib import Path
import configparser
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASELINE = "d7dcd4cc7394e18305b79b4022d92918e6fb5c22"
BEGIN = "// F23C Phase 1 begin"
END = "// F23C Phase 1 end"
REQUIRED = {
    "THESIS_F23C_BUILD_ONLY": 1,
    "THESIS_F22_FLEET_OPERATIONAL": 1,
    "THESIS_SD_LOGGER": 1,
    "THESIS_F05_RECOVERY": 1,
    "THESIS_F06_SD_SANDBOX": 1,
    "THESIS_V003_F19_DUAL_DISPLAY": 1,
    "THESIS_V003_F15_SD_PILOT_ARMED": 1,
}

def execute(command):
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout

def remove_readiness(text):
    result=[];inside=False
    for line in text.splitlines(keepends=True):
        if line.strip()=="// F23D readiness begin":
            assert not inside
            inside=True
        elif line.strip()=="// F23D readiness end":
            assert inside
            inside=False
        elif not inside: result.append(line)
    assert not inside
    return "".join(result)

def remove_f23d(text):
    result=[];inside=False
    for line in text.splitlines(keepends=True):
        if line.strip()=="// F23D begin":
            assert not inside
            inside=True
        elif line.strip()=="// F23D end":
            assert inside
            inside=False
        elif not inside: result.append(line)
    assert not inside
    return "".join(result)

def remove_additions(text):
    lines = text.splitlines(keepends=True)
    result = []
    inside = False
    for line in lines:
        if line.strip() == BEGIN:
            assert not inside
            inside = True
        elif line.strip() == END:
            assert inside
            inside = False
        elif not inside:
            result.append(line)
    assert not inside
    return "".join(result)

def run():
    compiler = shutil.which("g++")
    assert compiler, "g++ required"
    # Exact original source survives beneath independently removable additions.
    for name in ("main.cpp", "lora_manager.cpp"):
        path = "firmware/universal/src/" + name
        before = execute(["git", "show", BASELINE + ":" + path])
        after = (ROOT / path).read_text(encoding="utf-8")
        accepted = execute(["git","show","1fc20613ffb8457c2b1552fdcc62379c9176b638:"+path])
        assert remove_readiness(after)==accepted, path+": unguarded readiness edits"
        after=remove_readiness(after)
        f23c = execute(["git", "show", "0463ebc158938909c84d2991e6e3435dbb6dfbf6:" + path])
        assert remove_f23d(after) == f23c, path + ": unguarded F23D edits"
        assert remove_additions(remove_f23d(after)) == before, path + ": existing source changed"
    ini_path = "firmware/universal/platformio.ini"
    before = execute(["git", "show", BASELINE + ":" + ini_path])
    ini = (ROOT / ini_path).read_text(encoding="utf-8")
    assert ini.split("; F23C Phase 1 appended profile", 1)[0] == before
    # Neither safety contracts, packet/state semantics, nor storage changed.
    protected = [
        "firmware/shared/relay/lab_relay_gate.h",
        "firmware/shared/relay/relay_authorization_contract.h",
        "firmware/shared/relay/relay_tx_readiness.h",
        "firmware/shared/relay/relay_preflight.h",
        "firmware/shared/relay/relay_policy.h",
        "firmware/shared/relay/relay_queue.h",
        "firmware/shared/relay/relay_dryrun_scheduler.h",
        "firmware/shared/relay/relay_receipt_state.h",
        "firmware/shared/communication/packet_protocol.h",
        "firmware/shared/communication/multi_peer_sos.h",
        "firmware/shared/communication/device_verification_contract.h",
        "firmware/shared/storage/thesis_sd_logger.h",
        "firmware/shared/storage/f05_sos_recovery.h",
        "firmware/shared/hardware/vessel_identity.h",
        "firmware/shared/hardware/fleet_usb_enrollment.h",
    ]
    for path in protected:
        assert (ROOT / path).read_text(encoding="utf-8") == execute(
            ["git", "show", BASELINE + ":" + path]), path
    parser = configparser.ConfigParser(interpolation=None)
    parser.read_string(ini)
    profile = parser["env:tbeam-supreme-f23c-lab-model-build-only"]
    assert profile["extends"] == "env:tbeam-supreme-fleet-universal-operational-build-only"
    assert profile["build_flags"].splitlines() == [
        "", "${env:tbeam-supreme-fleet-universal-operational-build-only.build_flags}",
        "-D THESIS_F23C_BUILD_ONLY=1"]
    for name in ("main.cpp", "lora_manager.cpp"):
        text = (ROOT / "firmware/universal/src" / name).read_text(encoding="utf-8")
        additions = []
        inside = False
        for line in text.splitlines():
            if line == BEGIN:
                inside = True
            elif line == END:
                inside = False
            elif inside:
                additions.append(line)
        block = "\n".join(additions)
        for forbidden in (".arm(", "radio.", "transmitWithIdentity(", "SD_EVENT(",
                          "Preferences", "SD.", "FleetUSBEnrollment::"):
            assert forbidden not in block, forbidden
        assert "#if THESIS_F23C_BUILD_ONLY == 1" in block
    print("PASS: F22 sources/guards intact; F23C adapter disarmed, no radio/storage/arming calls")

    with tempfile.TemporaryDirectory(prefix="thesis_f23c_") as directory:
        temp = Path(directory)
        fixture = ROOT / "tests/f23c_lab_transaction_host.cpp"
        executable = temp / ("f23c.exe" if os.name == "nt" else "f23c")
        execute([compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT), str(fixture), "-o", str(executable)])
        print(execute([str(executable)]).strip())
        guard = temp / "guard.cpp"
        guard.write_text('#include "firmware/shared/relay/f23c_lab_contract.h"\nint main(){}\n')
        matrix = [({}, True), (REQUIRED, True)]
        matrix += [({**REQUIRED, key: 0}, False) for key in REQUIRED if key != "THESIS_F23C_BUILD_ONLY"]
        matrix += [
            ({"THESIS_F23C_BUILD_ONLY": 2}, False),
            ({"THESIS_F23C_BUILD_ONLY": -1}, False),
            ({"THESIS_F23C_RELAY_RF_TX": 1}, False),
            ({**REQUIRED, "THESIS_F23C_RELAY_RF_TX": 1}, False),
            ({**REQUIRED, "THESIS_LAB_RELAY_TX": 1}, False),
            ({**REQUIRED, "THESIS_V003_F13_LIVE_OUTPUTS": 1}, False),
        ]
        for defines, success in matrix:
            args = [compiler, "-std=c++17", "-fsyntax-only", "-I", str(ROOT)]
            args += ["-D" + key + "=" + str(value) for key, value in defines.items()]
            result = subprocess.run(args + [str(guard)], capture_output=True, text=True)
            assert (result.returncode == 0) == success, str(defines) + result.stderr
            if not success:
                assert "error:" in result.stderr
        print(f"PASS: {len(matrix)} real-header positive/negative compile gates")

if __name__ == "__main__":
    run()
