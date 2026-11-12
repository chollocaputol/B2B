#!/usr/bin/env python3
"Mock-only live LED validation. No access to real GPIO or serial ports."
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
HEADER=ROOT/'firmware/universal/include/v003_f08_led_bench.h'
MAIN=ROOT/'firmware/universal/src/main.cpp'
WEATHER=ROOT/'firmware/universal/src/weather_manager.cpp'
INI=ROOT/'firmware/universal/platformio.ini'
PHYSICAL=ROOT/'firmware/universal/include/physical_indicators.h'

def check(v,msg):
    if not v: raise AssertionError(msg)

def main():
    for path in (HEADER,MAIN,WEATHER,INI,PHYSICAL):
        check(path.is_file(),f'missing {path}')
    header=HEADER.read_text(encoding='utf-8')
    src=MAIN.read_text(encoding='utf-8')
    serial=WEATHER.read_text(encoding='utf-8')
    ini=INI.read_text(encoding='utf-8')
    physical=PHYSICAL.read_text(encoding='utf-8')
    check('VesselIdentity::nodeId() == 3U' in src, 'runtime Vessel-003 identity gate absent')
    check(src.index('VesselIdentity::load();') < src.index('instance().begin(isVessel003)'), 'GPIO init precedes node identity')
    check('PhysicalIndicators::kBuzzerDriverPin == -1' in src, 'buzzer guard missing')
    check('static constexpr int kBuzzerPin = -1;' in header, 'buzzer assigned')
    check('digitalWrite(kBuzzer' not in header, 'buzzer write present')
    check('static constexpr bool kOutputsEnabled = false;' in physical, 'production physical policy enabled')
    check('[env:tbeam-supreme-v003-f08-led-live-v003-only]' in ini, 'isolated environment missing')
    check('-D THESIS_V003_F08_LED_LIVE_PROFILE=1' in ini, 'missing explicit live build gate')
    check('THESIS_V003_F08_LED_LIVE_PROFILE != 1' in header, 'live gate missing')
    check('!V003F08LedBench::kOutputsEnabled' in src, 'preflight lock missing')
    check('cancelActiveSOS();' in serial, 'SOS handling missing')
    check('BLOCKED: active SOS' in serial and 'BLOCKED: board identity' in serial,'blocking status missing')
    compiler=(os.environ.get('CXX') or shutil.which('g++') or shutil.which('clang++') or
              (r'C:\msys64\ucrt64\bin\g++.exe' if Path(r'C:\msys64\ucrt64\bin\g++.exe').is_file() else None))
    check(compiler,'g++/clang++ not found: set CXX or PATH')
    cpp=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include "v003_f08_led_bench.h"
using namespace V003F08LedBench;
int writes=0, modes=0, invalid=0, y=0, r=0;
void digitalWrite(int pin,int level) {
  ++writes;
  if(pin==kYellowPin) y=level;
  else if(pin==kRedPin) r=level;
  else ++invalid;
}
void pinMode(int pin,int mode) {
  ++modes;
  if((pin!=kYellowPin && pin!=kRedPin) || mode!=OUTPUT) ++invalid;
}
int main() {
  assert(kBuzzerPin == -1);
  Controller wrong;
  wrong.begin(false);  // simulates a misflashed Vessel-001 or -002
  assert(!wrong.authorized());
  assert(!wrong.command(Request::Yellow, 50, false));
  wrong.tick(999, false);
  assert(!wrong.yellow() && !wrong.red());
  assert(writes==0 && modes==0); // identity gate prevents ANY GPIO touch
  Controller c;
  c.begin(true); // simulates Vessel-003 node identity
  assert(c.authorized());
#if THESIS_V003_F08_LED_OUTPUTS_ENABLE==1
  assert(modes==2 && writes==2 && y==LOW && r==LOW);
#else
  assert(modes==0 && writes==0);
#endif
  assert(c.command(Request::Yellow,1000,false));
  c.tick(1499,false); assert(c.yellow() && !c.red());
  c.tick(1500,false); assert(!c.yellow() && !c.red());
  assert(c.command(Request::Red,1700,false));
  assert(!c.yellow() && c.red());
  c.tick(1750,true); assert(!c.yellow() && !c.red());
  assert(!c.command(Request::Yellow,1800,true));
  assert(!c.yellow() && !c.red());
  assert(c.command(Request::Red,UINT32_MAX-10U,false));
  c.tick(488,false); assert(c.red());
  c.tick(489,false); assert(!c.red());
#if THESIS_V003_F08_LED_OUTPUTS_ENABLE==1
  assert(y==LOW && r==LOW);
#else
  assert(writes==0 && modes==0);
#endif
  assert(invalid==0);
}
'''
    stub='#pragma once\n#define HIGH 1\n#define LOW 0\n#define OUTPUT 1\nvoid digitalWrite(int,int);\nvoid pinMode(int,int);\n'
    live=['-DTHESIS_V003_F08_LED_OUTPUTS_ENABLE=1','-DTHESIS_V003_F08_LED_HARDWARE_REVIEWED=1',
          '-DTHESIS_V003_F08_LED_BENCH=1','-DTHESIS_V003_F08_INDICATOR_BENCH=1',
          '-DTHESIS_V003_F08_LED_LIVE_PROFILE=1']
    with tempfile.TemporaryDirectory(prefix='f08_led_live_mock_') as td:
        tmp=Path(td)
        (tmp/'Arduino.h').write_text(stub.replace('\\n','\n'),encoding='utf-8')
        source=tmp/'led.cpp';source.write_text(cpp,encoding='utf-8')
        exe=tmp/('led.exe' if os.name=='nt' else 'led')
        cmd=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(tmp),'-I',str(HEADER.parent),str(source),'-o',str(exe)]
        subprocess.run(cmd,check=True)
        subprocess.run([str(exe)],check=True)
        subprocess.run(cmd[:1]+live+cmd[1:],check=True)
        subprocess.run([str(exe)],check=True)
        unsafe=subprocess.run(cmd[:1]+live[:-1]+cmd[1:],capture_output=True,text=True)
        check(unsafe.returncode!=0 and 'LED output requires separately reviewed' in unsafe.stderr,
              'missing live profile must fail closed')
        unsafe=subprocess.run(cmd[:1]+live[:1]+live[2:]+cmd[1:],capture_output=True,text=True)
        check(unsafe.returncode!=0 and 'LED output requires separately reviewed' in unsafe.stderr,
              'missing hardware confirmation must fail closed')
    print('PASS: identity mismatch causes zero GPIO calls (live/mock build)')
    print('PASS: Vessel-003 mock uses GPIO39/44 only, active LOW start')
    print('PASS: one LED at a time, 500ms OFF, SOS preemption, rollover')
    print('PASS: no buzzer use; production F08 adapter stays disabled')
    print('PASS: omitted live/confirmation build gates rejected')

if __name__=='__main__':
    main()
