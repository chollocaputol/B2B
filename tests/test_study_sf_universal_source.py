from pathlib import Path
import configparser, re, shlex
r=Path(__file__).resolve().parents[1]
read=lambda p:(r/p).read_text(encoding="utf-8")
c=configparser.ConfigParser(interpolation=None); c.read_string(read("firmware/universal/platformio.ini"))
name="env:tbeam-supreme-universal-study"
flag="THESIS_STUDY_SF_UNIVERSAL_OFFLINE"
def flags(n):
    text=c[n]["build_flags"]
    return re.sub(r"\$\{([^}]+)\.build_flags\}",lambda m:flags(m[1]),text)
# PlatformIO's attached -U flags cancel inherited definitions; check resolved intent.
def effective(n):
    tokens=shlex.split(flags(n)); definitions={}; undef=set(); i=0
    while i<len(tokens):
        t=tokens[i]; i+=1
        if t=="-D": t=t+tokens[i]; i+=1
        if t.startswith("-D"):
            k,_,v=t[2:].partition("="); definitions[k]=v or "1"
        elif t.startswith("-U"): undef.add(t[2:])
    for k in undef: definitions.pop(k,None)
    return definitions
f=effective(name); m=read("firmware/universal/src/main.cpp"); t=read("firmware/universal/src/study_sf_target.inc"); e=read("firmware/shared/hardware/fleet_usb_enrollment.h"); ident=read("firmware/shared/hardware/vessel_identity.h")
checks=[c[name]["extends"]=="env:tbeam-supreme-study-sf-build-only", f.get(flag)=="1", all(f.get(k)=="1" for k in ("THESIS_STUDY_SF_BUILD_ONLY","THESIS_DTN_F25A_BUILD_ONLY","THESIS_DTN_F25C_BUILD_ONLY","THESIS_DTN_F24Y_BUILD_ONLY","THESIS_V003_F09_SHARED_SENSOR_BUS","THESIS_V003_F10_MICRO_NOWCAST","THESIS_V003_F11_MACRO_FIVE_MEDIAN","THESIS_V003_F19_DUAL_DISPLAY")), f.get("WEATHER_FAST_TEST")=="0", all(f.get(k,"0")=="0" for k in ("THESIS_STUDY_SF_OFFLINE_V003","THESIS_STUDY_SF_SYNC_V003","THESIS_FLEET_USB_ENROLLMENT","THESIS_F22_FLEET_OPERATIONAL","THESIS_V003_F15_SD_PILOT_ARMED","THESIS_V003_F13_LIVE_OUTPUTS","THESIS_LAB_RELAY_TX","THESIS_MULTIHOP_DEV_TEST","THESIS_MULTIHOP_BENCH_RF","THESIS_SD_PROBE")), all(flag not in effective(n) for n in c.sections() if n!=name), "f09SensorAuthorized = F22UniversalCapability::validIdentity(" in m, "#elif THESIS_STUDY_SF_UNIVERSAL_OFFLINE == 1\n     f09SensorAuthorized &&" in m, "#if THESIS_STUDY_SF_OFFLINE_V003 == 1 || THESIS_STUDY_SF_SYNC_V003 == 1\n if(node!=3U)return false;" in t, "node<1||node>9999" in t, "#elif THESIS_STUDY_SF_UNIVERSAL_OFFLINE == 1\n return true;" in t, "#if THESIS_STUDY_SF_OFFLINE_V003 != 1 && THESIS_STUDY_SF_UNIVERSAL_OFFLINE != 1" in t, "#if THESIS_STUDY_SF_SYNC_V003 == 1\n#include \"study_sf_local_config.generated.h\"" in t, "std::atomic<bool> studyContinuous{false};" in t, "F25A] LEGACY_SD_INIT_BYPASSED" in m, "THESIS_DTN_F25A_BUILD_ONLY == 1 && THESIS_STUDY_SF_BUILD_ONLY != 1" in m, "ALREADY_PROVISIONED" in e and "EXISTING_KEYS_REFUSE_OVERWRITE" in e and "MAC_MISMATCH" in e, "prefs.begin(\"vessel-ident\", true)" in ident, "REBOOT_REQUIRED=1" in e]
for i,ok in enumerate(checks,1): assert ok, f"SF7 source check {i}"
print(f"SF7 universal source PASS: {len(checks)} checks; no hardware or compile proof")
