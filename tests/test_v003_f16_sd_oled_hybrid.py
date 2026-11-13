"""F16 host-only integration checks: previous F06 OLED+hybrid and F15 weather."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
MAIN=(ROOT/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
INI=(ROOT/'firmware/universal/platformio.ini').read_text(encoding='utf-8')
LOGGER=(ROOT/'firmware/shared/storage/thesis_sd_logger.h').read_text(encoding='utf-8')
POLICY=ROOT/'firmware/shared/storage/sd_hybrid_policy.h'

for phrase in (
    '#define THESIS_V003_F16_SD_OLED_HYBRID 0',
    '#define THESIS_V003_F16_EXPLICIT_OPT_IN 0',
    'F16 requires isolated V003 F15 macro SD, F06 hybrid OLED, relay/indicators OFF',
    'THESIS_V003_F15_MACRO_SD_SANDBOX != 1',
    'THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1',
    'THESIS_SD_OLED_UI != 1',
    'THESIS_F06_SD_HYBRID != 1',
    'THESIS_LAB_RELAY_TX != 0',
    'THESIS_V003_F13_SUPERVISED_DRIVER != 0',
):
    assert phrase in MAIN, phrase
assert '#define OLED_PAGE_TOTAL "8"' in MAIN
assert 'static constexpr uint8_t kOLEDPageCount = 8;' in MAIN
assert 'case 7: oledSDStoragePage(); break;' in MAIN
assert '"SD STORAGE MENU 8/8"' in MAIN
assert '"RECORDING MODE 8/8"' in MAIN
assert '"SOS ARMED", "CONTINUOUS", "BACK"' in MAIN
assert '"SD JOURNALS", "SD USB READBACK", "RECORDING MODE", "BACK"' in MAIN
assert 'ThesisSDLogger::hybridSetContinuous(sdHybridMenuCursor == 1);' in MAIN
assert 'ThesisSDLogger::hybridObserveSOSPresence(' in MAIN
assert 'hasActiveLocalSOS() || getActiveSOSCount() > 0' in MAIN
assert 'sdOLEDSubview = 0;' in MAIN

# V003 active-session USB export, while old V002 historical UI remains unchanged.
s=MAIN[MAIN.index('    u8g2.drawStr(0, 9, "SD USB READBACK 8/8");'):]
assert s.count('#if THESIS_V003_F16_SD_OLED_HYBRID == 1') == 1
v003=s.split('#if THESIS_V003_F16_SD_OLED_HYBRID == 1',1)[1].split('#elif THESIS_F06_SD_SANDBOX == 1',1)[0]
assert 'PC: SD EXPORT' in v003 and 'ACTIVE SESSION ONLY' in v003
v002=s.split('#elif THESIS_F06_SD_SANDBOX == 1',1)[1].split('#else',1)[0]
assert 'PC COMMAND: SD LIST' in v002 and 'SD READ Sxxxxxxx' in v002
assert 'if (!ready || vesselId != 2)' in LOGGER  # V002 history access stays restricted
assert 'if (!ready || vesselId != 2 || historyActive || exportActive)' in LOGGER
assert 'inline bool queueWeatherSample(const WeatherMeasurement& s)' in LOGGER
assert 'const bool queued = ThesisSDLogger::queueWeatherSample(weatherMeasurement);' in MAIN
assert 'THESIS_V003_F15_SD_PILOT_ARMED == 1' in MAIN

pre=INI.split('[env:tbeam-supreme-v003-f16-sd-oled-hybrid-build-only]',1)[1].split('[env:',1)[0]
live=INI.split('[env:tbeam-supreme-v003-f16-sd-oled-hybrid-live-v003-only]',1)[1]
for flag in ('THESIS_SD_OLED_UI=1','THESIS_F06_SD_HYBRID=1',
             'THESIS_V003_F16_SD_OLED_HYBRID=1','THESIS_V003_F16_EXPLICIT_OPT_IN=1'):
    assert flag in pre, flag
assert 'extends = env:tbeam-supreme-v003-f15-macro-sd-sandbox-build-only' in pre
assert 'THESIS_V003_F15_SD_PILOT_ARMED=1' not in pre
assert 'extends = env:tbeam-supreme-v003-f16-sd-oled-hybrid-build-only' in live
assert '-D THESIS_V003_F15_SD_PILOT_ARMED=1' in live
for flag in ('THESIS_LAB_RELAY_TX=1','THESIS_V003_F13_LIVE_OUTPUTS=1',
             'THESIS_V003_F14_INDICATOR_PROBE=1'):
    assert flag not in pre+live,flag

CPP=r"""
#include <cassert>
#include <cstdint>
#include "sd_hybrid_policy.h"
int main() {
  using namespace SDHybrid;
  State p{};
  assert(!p.continuous && !p.activeSOS);
  assert(!p.routineEnabled(100));
  assert(!p.shouldRecord(false,100));
  assert(p.skippedRoutine==1);
  assert(p.shouldRecord(true,101)); // urgent bypass even in SOS_ARMED
  assert(p.routineEnabled(102)); // tail active after urgent
  assert(p.routineEnabled(600100));
  assert(!p.routineEnabled(600101)); // 10min expiry
  p.observeSOS(true,610000);
  assert(p.sosOverride(620000));
  p.observeSOS(false,620000);
  assert(p.sosOverride(1219999));
  assert(!p.sosOverride(1220000));
  p.continuous=true;
  assert(p.routineEnabled(3000000));
  assert(p.shouldRecord(false,3000001));
  // Unsigned wraparound must retain the post-SOS window.
  State w{};
  w.observeSOS(true, UINT32_MAX-10U);
  w.observeSOS(false, UINT32_MAX-10U);
  assert(w.sosOverride(20U));
  assert(!w.sosOverride(600000U+20U));
}
"""
CXX=os.environ.get('CXX') or ('C:/msys64/ucrt64/bin/g++.exe' if os.name=='nt' else 'g++')
with tempfile.TemporaryDirectory(prefix='thesis_f16_') as d:
    tmp=Path(d)
    src=tmp/'policy.cpp'; src.write_text(CPP,encoding='utf-8')
    exe=tmp/('policy.exe' if os.name=='nt' else 'policy')
    p=subprocess.run([CXX,'-std=c++17','-Wall','-Wextra','-Werror',
                      '-I',str(POLICY.parent),str(src),'-o',str(exe)],
                     text=True,capture_output=True)
    assert p.returncode==0,p.stderr
    subprocess.run([str(exe)],check=True)
print('PASS F16 host: F06 8th SD OLED page, mode/menu, V003 active-session export')
print('PASS F16 host: SOS_ARMED default, continuous selection, urgent priority/tail/wrap')
print('PASS F16 host: F15 five-median research weather remains independent of routine-mode filter')
