"""F14 compile-time safety gates, 6s non-extending probe, and F13 physical route."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
HEADER = ROOT.parent / 'firmware/universal/include/v003_f14_indicator_probe.h'
STUB = '''#pragma once
#include <stdint.h>
#define WEATHER_CALIBRATING 0
#define WEATHER_NORMAL 1
#define WEATHER_FRONT 2
#define WEATHER_SQUALL 3
'''
CPP = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include "v003_f14_indicator_probe.h"
using namespace V003F14Probe;
int main() {
  assert(parse("F14 IND FRONT") == Command::Front);
  assert(parse("F14 IND SQUALL") == Command::Squall);
  assert(parse("F14 IND SOS") == Command::Invalid); // Never forge SOS!
  assert(parse("F14 IND OFF") == Command::Off);
  Controller denied;
  assert(!denied.request(Command::Front,0,false,true,false));
  Controller p;
  p.begin(true);
  assert(!p.request(Command::Front,0,true,true,false));
  assert(!p.request(Command::Front,0,false,false,false));
  assert(!p.request(Command::Front,0,false,true,true));
  assert(p.request(Command::Front,10,false,true,false));
  auto v = p.select(10,false,true,WEATHER_CALIBRATING,false);
  assert(v.synthetic && v.valid && v.state == WEATHER_FRONT);
  assert(!p.request(Command::Squall,11,false,true,false)); // no extension
  v = p.select(6009,false,true,WEATHER_NORMAL,true);
  assert(v.synthetic);
  v = p.select(6010,false,true,WEATHER_NORMAL,true);
  assert(!v.synthetic && v.state == WEATHER_NORMAL && v.valid);
  assert(p.request(Command::Squall,7000,false,true,false));
  v = p.select(7010,true,true,WEATHER_NORMAL,true);
  assert(!v.synthetic && !p.active()); // real SOS preempts
  assert(p.request(Command::Front,7100,false,true,false));
  v = p.select(7101,false,true,WEATHER_SQUALL,true);
  assert(!v.synthetic && v.state == WEATHER_SQUALL && !p.active());
  assert(!p.request(Command::Front,7200,false,true,true));
  assert(p.request(Command::Squall,7300,false,true,false));
  v = p.select(7301,false,false,WEATHER_NORMAL,true);
  assert(!v.synthetic && !p.active()); // rail lost cancels
  p.begin(true);
  const uint32_t nearWrap = UINT32_MAX - 100U;
  assert(p.request(Command::Front,nearWrap,false,true,false));
  v = p.select(nearWrap + 500U,false,true,WEATHER_NORMAL,true);
  assert(v.synthetic);
  v = p.select(nearWrap + 6000U,false,true,WEATHER_NORMAL,true);
  assert(!v.synthetic && !p.active());
  assert(p.request(Command::Squall,50000,false,true,false));
  assert(p.request(Command::Off,50001,true,false,true));
  assert(!p.active());
}
'''
GOOD_FLAGS = [
  '-DTHESIS_V003_F14_INDICATOR_PROBE=1',
  '-DTHESIS_V003_F14_EXPLICIT_OPT_IN=1',
  '-DTHESIS_V003_F13_SUPERVISED_DRIVER=1',
  '-DTHESIS_V003_F13_LIVE_OUTPUTS=1',
  '-DTHESIS_V003_F13_EXPLICIT_OPT_IN=1',
  '-DTHESIS_V003_F13_HARDWARE_REVIEWED=1',
  '-DTHESIS_V003_F12_INDICATOR_PREVIEW=1',
  '-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN=1',
  '-DTHESIS_V003_F10_MICRO_NOWCAST=1',
  '-DTHESIS_V003_F09_SHARED_SENSOR_BUS=1',
  '-DWEATHER_FAST_TEST=0',
  '-DTHESIS_SD_LOGGER=0',
  '-DTHESIS_LAB_RELAY_TX=0',
]
def run():
    compiler = os.environ.get('CXX') or ('C:/msys64/ucrt64/bin/g++.exe' if os.name == 'nt' else 'g++')
    with tempfile.TemporaryDirectory(prefix='thesis_f14_') as d:
        tmp = Path(d)
        (tmp/'weather_manager.h').write_text(STUB)
        (tmp/HEADER.name).write_text(HEADER.read_text())
        (tmp/'test.cpp').write_text(CPP)
        def compile_case(flags, expect_ok):
            cmd=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(tmp),str(tmp/'test.cpp'),'-o',str(tmp/'f14')]+flags
            p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            assert (p.returncode == 0) == expect_ok, p.stderr
            if expect_ok:
                subprocess.run([str(tmp/'f14')],check=True)
        compile_case(GOOD_FLAGS,True)
        for name in ('-DTHESIS_V003_F14_EXPLICIT_OPT_IN=1',
                     '-DTHESIS_V003_F13_LIVE_OUTPUTS=1',
                     '-DTHESIS_V003_F13_HARDWARE_REVIEWED=1',
                     '-DTHESIS_V003_F12_INDICATOR_PREVIEW=1'):
            compile_case([x for x in GOOD_FLAGS if x != name],False)
        compile_case(GOOD_FLAGS + ['-DTHESIS_SD_LOGGER=1'],False)
        compile_case(GOOD_FLAGS + ['-DTHESIS_LAB_RELAY_TX=1'],False)
    assert all(k not in HEADER.read_text() for k in ('digitalWrite(', 'pinMode(', 'transmitSOS(', 'processWeather('))
    print('PASS F14 host: 6s limit, wraparound, SOS/weather/rail priority, no re-arm, no history/GPIO side effects')
    print('PASS F14 host: compile-time live-output, explicit opt-in, relay/SD isolation')
if __name__=='__main__':
    run()
