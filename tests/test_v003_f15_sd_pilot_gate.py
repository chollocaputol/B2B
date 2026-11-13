"""F15 read-only source review: SD pilot arming is explicit and isolated.

No hardware use, no firmware upload, and no writes to the SD card.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
ini=(root/'firmware/universal/platformio.ini').read_text(encoding='utf-8')
assert '#define THESIS_V003_F15_SD_PILOT_ARMED 0' in main
assert '#error "Invalid F15 supervised SD pilot flag"' in main
assert '#error "F15 supervised SD pilot requires the complete F15 sandbox profile"' in main
start=main.index('static void f15QueuePhysicalMacroMedian()')
end=main.index('\nvoid setup() {', start)
helper=main[start:end]
assert helper.count('#if THESIS_V003_F15_SD_PILOT_ARMED == 1')==1
assert 'ThesisSDLogger::queueWeatherSample(weatherMeasurement)' in helper.split('#if THESIS_V003_F15_SD_PILOT_ARMED == 1')[1].split('#else')[0]
assert '[F15-WX] BUILD_ONLY_NO_SD_QUEUE' in helper.split('#else')[1]
sd=main[main.index('#if THESIS_SD_LOGGER == 1\n#if THESIS_V003_F15_MACRO_SD_SANDBOX == 1'):]
sd=sd.split('\n#if THESIS_F07_WEATHER_LOG',1)[0]
assert sd.count('#if THESIS_V003_F15_SD_PILOT_ARMED == 1')==1
assert 'ThesisSDLogger::begin(pmu, VesselIdentity::nodeId())' in sd.split('#if THESIS_V003_F15_SD_PILOT_ARMED == 1')[1].split('#else')[0]
assert '[F15-SD] BUILD_ONLY_NO_SD_MOUNT' in sd.split('#else')[1]
assert 'const bool f15SdReady = ThesisSDLogger::begin(pmu, VesselIdentity::nodeId());' in sd
assert 'sandbox_mount_ok=%u' in sd
base=ini.split('[env:tbeam-supreme-v003-f15-macro-sd-sandbox-build-only]',1)[1].split('[env:',1)[0]
assert 'THESIS_V003_F15_SD_PILOT_ARMED=1' not in base
pilot=ini.split('[env:tbeam-supreme-v003-f15-macro-sd-sandbox-live-v003-only]',1)[1]
assert 'extends = env:tbeam-supreme-v003-f15-macro-sd-sandbox-build-only' in pilot
assert '-D THESIS_V003_F15_SD_PILOT_ARMED=1' in pilot
for forbidden in ('THESIS_V003_F13_LIVE_OUTPUTS=1','THESIS_V003_F14_INDICATOR_PROBE=1','THESIS_LAB_RELAY_TX=1'):
    assert forbidden not in pilot, forbidden
print('PASS: F15 build-only SD mount and queue disabled; pilot requires explicit opt-in')
print('PASS: F15 pilot extends isolated F11/F06 SD sandbox, no indicators/relay')
