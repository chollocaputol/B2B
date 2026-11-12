#!/usr/bin/env python3
"Host-only F08 buzzer preflight: cannot touch serial port or device GPIO."
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
INCLUDE=ROOT/'firmware/universal/include'
HEADER=INCLUDE/'v003_f08_buzzer_bench.h'
MAIN=ROOT/'firmware/universal/src/main.cpp'
WEATHER=ROOT/'firmware/universal/src/weather_manager.cpp'
INI=ROOT/'firmware/universal/platformio.ini'
PHYSICAL=INCLUDE/'physical_indicators.h'


def require(ok, explanation):
    if not ok:
        raise AssertionError(explanation)


def main():
    for p in (HEADER,MAIN,WEATHER,INI,PHYSICAL):
        require(p.is_file(), f'missing source: {p}')
    header=HEADER.read_text(encoding='utf-8')
    main_cpp=MAIN.read_text(encoding='utf-8')
    weather=WEATHER.read_text(encoding='utf-8')
    ini=INI.read_text(encoding='utf-8')
    physical=PHYSICAL.read_text(encoding='utf-8')

    for api in ('digitalWrite(', 'pinMode(', 'analogWrite(', 'ledcWrite(', 'tone('):
        require(api not in header, f'physical actuator API found in preflight header: {api}')
    require('static constexpr int kActualOutputPin = -1;' in header, 'actual GPIO pin assigned')
    require('static constexpr bool kOutputsEnabled = false;' in header, 'output gate active')
    require('static constexpr bool kOutputsEnabled = false;' in physical, 'production output gate active')
    require('static constexpr int kBuzzerDriverPin = -1;' in physical, 'production buzzer assigned')
    require('THESIS_V003_F08_BUZZER_OUTPUTS_ENABLE != 0' in header, 'negative compile guard missing')
    require('[env:tbeam-supreme-v003-f08-buzzer-preflight-build-only]' in ini, 'isolated profile absent')
    require('-D THESIS_V003_F08_BUZZER_OUTPUTS_ENABLE=0' in ini, 'output-disable flag absent')
    require('-D THESIS_V003_F08_BUZZER_PREFLIGHT=1' in ini, 'preflight flag absent')
    require('VesselIdentity::nodeId() == 3U' in main_cpp, 'ID-3 check missing')
    require('V003F08BuzzerBench::instance().begin(isBuzzerVessel003);' in main_cpp, 'init gate absent')
    require(main_cpp.index('VesselIdentity::load();') < main_cpp.index('instance().begin(isBuzzerVessel003)'),
            'buzzer authorization before identity loaded')
    require('V003F08BuzzerBench::instance().tick(' in main_cpp, 'periodic timeout absent')
    require('V003F08BuzzerBench::parse(testLine)' in weather, 'serial command handler absent')
    require('cancelActiveSOS();' in weather, 'cancel SOS command missing')
    require('F08 BUZZER TEST | OFF | STATUS | HELP' in weather, 'help missing')
    require('ACTUATION=DISABLED' in weather, 'user-visible status missing')

    compiler=(os.environ.get('CXX') or shutil.which('g++') or shutil.which('clang++') or
              (r'C:\msys64\ucrt64\bin\g++.exe' if Path(r'C:\msys64\ucrt64\bin\g++.exe').exists() else None))
    require(compiler, 'host compiler not found; set CXX or PATH to MSYS2 g++')

    snippet=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include "v003_f08_buzzer_bench.h"
using namespace V003F08BuzzerBench;
int main() {
  assert(kCandidateGpio == 38);
  assert(kActualOutputPin == -1);
  static_assert(!kOutputsEnabled, "buzzer preflight output disabled");
  assert(parse("F08 BUZZER TEST") == Request::Test);
  assert(parse("F08 BUZZER OFF") == Request::Off);
  assert(parse("F08 BUZZER STATUS") == Request::Status);
  assert(parse("F08 BUZZER HELP") == Request::Help);
  assert(parse("F08 BUZZER BOGUS") == Request::Invalid);
  assert(parse("F08 TEST BUZZER") == Request::NotBuzzer);
  assert(parse("F08 LED RED") == Request::NotBuzzer);
  assert(parse("CANCEL SOS") == Request::NotBuzzer);
  assert(parse(nullptr) == Request::NotBuzzer);
  assert(std::strcmp(requestName(Request::Test), "TEST") == 0);

  Controller unauthorized;
  unauthorized.begin(false);
  assert(!unauthorized.authorized());
  assert(!unauthorized.command(Request::Test, 10, false));
  unauthorized.tick(500, false);
  assert(!unauthorized.virtualActive());

  Controller controller;
  controller.begin(true);
  assert(controller.authorized() && !controller.virtualActive());
  assert(controller.command(Request::Test, 1000, false));
  assert(controller.virtualActive());
  controller.tick(1249, false);
  assert(controller.virtualActive());
  controller.tick(1250, false);
  assert(!controller.virtualActive());
  assert(controller.command(Request::Test, 1800, false));
  controller.tick(1820, true);
  assert(!controller.virtualActive());
  assert(!controller.command(Request::Test, 1830, true));
  assert(!controller.virtualActive());
  assert(controller.command(Request::Test, UINT32_MAX - 10U, false));
  controller.tick(238U, false);
  assert(controller.virtualActive());
  controller.tick(239U, false);
  assert(!controller.virtualActive());
  assert(controller.command(Request::Test, 2000, false));
  assert(controller.command(Request::Off, 2010, false));
  assert(!controller.virtualActive());
  controller.begin(false);
  assert(!controller.virtualActive());
}
'''

    with tempfile.TemporaryDirectory(prefix='thesis_v003_buzzer_mock_') as tmp:
        temp=Path(tmp)
        cpp=temp/'buzzer.cpp'
        exe=temp/('buzzer.exe' if os.name=='nt' else 'buzzer')
        cpp.write_text(snippet,encoding='utf-8')
        cmd=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic',
             '-I',str(INCLUDE),str(cpp),'-o',str(exe)]
        subprocess.run(cmd,check=True)
        subprocess.run([str(exe)],check=True)
        rejected=subprocess.run(cmd[:1]+['-DTHESIS_V003_F08_BUZZER_OUTPUTS_ENABLE=1']+cmd[1:],
                                capture_output=True,text=True)
        require(rejected.returncode != 0 and 'must never drive GPIO38' in rejected.stderr,
                'output-enable must be rejected at compile time')
        rejected=subprocess.run(cmd[:1]+['-DTHESIS_V003_F08_BUZZER_PREFLIGHT=2']+cmd[1:],
                                capture_output=True,text=True)
        require(rejected.returncode != 0 and 'Invalid Vessel-003 buzzer preflight' in rejected.stderr,
                'invalid preflight flag not rejected')
    print('PASS: F08 buzzer TEST/OFF/STATUS/HELP parser; existing LED and SOS commands isolated')
    print('PASS: wrong identity cannot arm; nominal 250ms timeout, rollover, OFF, SOS preemption')
    print('PASS: no GPIO API, kActualOutputPin=-1, production indicator driver disabled')
    print('PASS: output-enable and invalid build flags rejected at compile time')

if __name__=='__main__':
    main()
