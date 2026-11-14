"""F17 host safety: gated F12/F13 + F15/F16 SD/OLED with GPIO OFF."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
MAIN=(ROOT/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
INI=(ROOT/'firmware/universal/platformio.ini').read_text(encoding='utf-8')
F13=(ROOT/'firmware/universal/include/v003_f13_supervised_indicators.h').read_text(encoding='utf-8')
MICRO=(ROOT/'firmware/universal/include/v003_f10_micro_nowcast.h').read_text(encoding='utf-8')

def between(text, a, b):
    assert text.count(a)==1, a
    return text.split(a,1)[1].split(b,1)[0]

assert '#define THESIS_V003_F17_INTEGRATED_PREFLIGHT 0' in MAIN
assert '#define THESIS_V003_F17_EXPLICIT_OPT_IN 0' in MAIN
assert 'F17 requires V003 F09-F16, F12 virtual, F13 GPIO OFF, isolated SD, relay OFF' in MAIN
assert '#error "F17 opt-in requires integrated preflight mode"' in MAIN
assert 'static_assert(!V003F13Indicators::kPhysicalOutputsEnabled' in MAIN
assert 'THESIS_V003_F13_LIVE_OUTPUTS != 0' in MAIN
assert 'THESIS_V003_F13_EXPLICIT_OPT_IN != 0' in MAIN
assert 'THESIS_V003_F13_HARDWARE_REVIEWED != 0' in MAIN
assert 'THESIS_LAB_RELAY_TX != 0' in MAIN
assert 'THESIS_V003_F14_INDICATOR_PROBE != 0' in MAIN
assert '[F17-INTEGRATED] ' in MAIN

# Every original exclusion must still reject SD/F13 coexistence unless F17 flag=1.
for message in ('F15 requires Vessel-003 real F09-F11',
                'F16 requires isolated V003 F15 macro SD',
                'F12 preview must isolate GPIO bench',
                'F13 must not share GPIO ownership'):
    scope=between(MAIN, '#error "'+message, '\n#endif') if False else None
    point=MAIN.index('#error "'+message)
    before=MAIN[max(0,point-650):point]
    assert 'THESIS_V003_F17_INTEGRATED_PREFLIGHT != 1' in before, message

assert 'V003F13Indicators::kPhysicalOutputsEnabled = ' not in MAIN
assert 'static constexpr bool kPhysicalOutputsEnabled = THESIS_V003_F13_SUPERVISED_DRIVER == 1' in F13
assert 'if constexpr (kPhysicalOutputsEnabled)' in F13
assert 'f13Indicators.tick(f13Now, f13HardwareState, f13HardwareValid,' in MAIN
assert 'uint8_t f13HardwareState = weatherState;' in MAIN
assert 'bool f13HardwareValid = weatherInputsValid;' in MAIN
assert 'f10Forecaster.state()' in MAIN
assert 'r.source = Source::MICRO_EXPERIMENTAL_WATCH' in (ROOT/'firmware/universal/include/v003_f12_indicator_preview.h').read_text(encoding='utf-8')
assert 'EXPERIMENTAL_10_30_MIN OUTPUTS=OFF' in MAIN
assert 'kMinPressureChangeHpa = 1.2f' in MICRO
assert 'kMaxTemperatureChangeC = -1.0f' in MICRO
assert 'kMinHumidityChangePoints = 6.0f' in MICRO
assert 'kWindowCount = 11' in MICRO

pre=between(INI,'[env:tbeam-supreme-v003-f17-integrated-preflight-build-only]',
            '[env:tbeam-supreme-v003-f17-integrated-sd-pilot-v003-only]')
live=INI.split('[env:tbeam-supreme-v003-f17-integrated-sd-pilot-v003-only]',1)[1]
assert 'extends = env:tbeam-supreme-v003-f16-sd-oled-hybrid-build-only' in pre
for flag in ('THESIS_V003_F12_INDICATOR_PREVIEW=1',
             'THESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN=1',
             'THESIS_V003_F13_SUPERVISED_DRIVER=1',
             'THESIS_V003_F13_LIVE_OUTPUTS=0',
             'THESIS_V003_F13_EXPLICIT_OPT_IN=0',
             'THESIS_V003_F13_HARDWARE_REVIEWED=0',
             'THESIS_V003_F17_INTEGRATED_PREFLIGHT=1',
             'THESIS_V003_F17_EXPLICIT_OPT_IN=1'):
    assert '-D '+flag in pre, flag
assert 'THESIS_V003_F15_SD_PILOT_ARMED=1' not in pre
assert 'extends = env:tbeam-supreme-v003-f17-integrated-preflight-build-only' in live
assert '-D THESIS_V003_F15_SD_PILOT_ARMED=1' in live
for forbidden in ('THESIS_V003_F13_LIVE_OUTPUTS=1','THESIS_LAB_RELAY_TX=1',
                  'THESIS_V003_F14_INDICATOR_PROBE=1'):
    assert forbidden not in pre+live, forbidden
assert '#if THESIS_V003_F15_SD_PILOT_ARMED == 1' in MAIN
assert '[F15-SD] BUILD_ONLY_NO_SD_MOUNT' in MAIN
assert '[F15-WX] BUILD_ONLY_NO_SD_QUEUE' in MAIN
assert 'THESIS_SD_OLED_UI=1' in INI and 'THESIS_F06_SD_HYBRID=1' in INI

# Extract real main.cpp guard sections and compile a positive/negative matrix.
# This does NOT replace full PlatformIO compilation, which must run on operator PC.
segments=[]
segments.append(between(MAIN,'// F17: the ONLY exception for combining F12/F13 policy with F15/F16 storage.',
                        '// Separate F15 hardware opt-in:'))
def guard_block(error):
    lines=MAIN.splitlines(keepends=True)
    idx=next(i for i,s in enumerate(lines) if '#error "'+error in s)
    start=idx
    while start>0 and not lines[start].startswith('#if '):
        start-=1
    end=idx+1
    while end<len(lines) and not lines[end].startswith('#endif'):
        end+=1
    assert end<len(lines)
    return ''.join(lines[start:end+1])
for marker in ('F15 requires Vessel-003 real F09-F11',
               'F16 requires isolated V003 F15 macro SD',
               'F17 requires V003 F09-F16',
               'F17 opt-in requires integrated preflight mode',
               'F12 preview must isolate GPIO bench',
               'F13 must not share GPIO ownership'):
    segments.append(guard_block(marker))
src='\n'.join(segments)+'\nint main(){return 0;}\n'
flags={
 'THESIS_V003_F17_INTEGRATED_PREFLIGHT':1,'THESIS_V003_F17_EXPLICIT_OPT_IN':1,
 'THESIS_V003_F15_MACRO_SD_SANDBOX':1,'THESIS_V003_F16_SD_OLED_HYBRID':1,
 'THESIS_V003_F16_EXPLICIT_OPT_IN':1,'THESIS_V003_F09_SHARED_SENSOR_BUS':1,
 'THESIS_V003_F10_MICRO_NOWCAST':1,'THESIS_V003_F11_MACRO_FIVE_MEDIAN':1,
 'THESIS_V003_F12_INDICATOR_PREVIEW':1,
 'THESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN':1,
 'THESIS_V003_F13_SUPERVISED_DRIVER':1,
 'THESIS_V003_F13_LIVE_OUTPUTS':0,'THESIS_V003_F13_EXPLICIT_OPT_IN':0,
 'THESIS_V003_F13_HARDWARE_REVIEWED':0,'THESIS_SD_LOGGER':1,
 'THESIS_SD_FLEET':1,'THESIS_F05_RECOVERY':1,'THESIS_F06_SD_SANDBOX':1,
 'THESIS_F07_WEATHER_LOG':1,'THESIS_SD_OLED_UI':1,
 'THESIS_F06_SD_HYBRID':1,'WEATHER_FAST_TEST':0,
 'THESIS_LAB_RELAY_TX':0,'THESIS_V003_F14_INDICATOR_PROBE':0,
 'THESIS_V003_F08_INDICATOR_BENCH':0,'THESIS_V003_F08_LED_BENCH':0,
 'THESIS_V003_F08_BUZZER_PREFLIGHT':0,'THESIS_V003_F08_BUZZER_LIVE_PROFILE':0,
 'THESIS_V003_F08_COMBINED_PROFILE':0,
}
cxx=os.environ.get('CXX') or ('C:/msys64/ucrt64/bin/g++.exe' if os.name=='nt' else 'g++')
with tempfile.TemporaryDirectory(prefix='thesis_f17_guards_') as d:
    f=Path(d)/'guard.cpp'; f.write_text(src,encoding='utf-8')
    def check(change,expected):
        use={**flags,**change}
        run=subprocess.run([cxx,'-std=c++17','-fsyntax-only',str(f)] +
           [f'-D{k}={v}' for k,v in use.items()],capture_output=True,text=True)
        assert (run.returncode==0)==expected, ('guard matrix',change,run.stderr)
    check({},True)
    for missing in ('THESIS_V003_F17_EXPLICIT_OPT_IN',
                    'THESIS_V003_F16_SD_OLED_HYBRID',
                    'THESIS_V003_F12_INDICATOR_PREVIEW',
                    'THESIS_V003_F13_SUPERVISED_DRIVER',
                    'THESIS_F06_SD_HYBRID'):
        check({missing:0},False)
    for bad in ('THESIS_LAB_RELAY_TX','THESIS_V003_F13_LIVE_OUTPUTS',
                'THESIS_V003_F13_EXPLICIT_OPT_IN','THESIS_V003_F14_INDICATOR_PROBE'):
        check({bad:1},False)
    check({'THESIS_V003_F17_INTEGRATED_PREFLIGHT':0},False)
    check({'THESIS_V003_F17_INTEGRATED_PREFLIGHT':2},False)
print('PASS F17 host: output-disabled F13 + F12 virtual micro forecast + F16 hybrid SD/OLED')
print('PASS F17 host: build-only SD mount/queue blocked; separate SD pilot opt-in')
print('PASS F17 host: actual C++ guard compilation rejects missing opt-ins, live GPIO, relay and F14')
