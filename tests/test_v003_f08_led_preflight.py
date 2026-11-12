#!/usr/bin/env python3
"""Host-only tests for Vessel-003 LED preflight; no hardware or serial ports."""
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'firmware/universal/include/v003_f08_led_bench.h'
MAIN = ROOT / 'firmware/universal/src/main.cpp'
MANAGER = ROOT / 'firmware/universal/src/weather_manager.cpp'
INI = ROOT / 'firmware/universal/platformio.ini'
PHYSICAL = ROOT / 'firmware/universal/include/physical_indicators.h'


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    for file in (HEADER, MAIN, MANAGER, INI, PHYSICAL):
        require(file.is_file(), f'missing: {file}')
    hdr = HEADER.read_text(encoding='utf-8')
    main_cpp = MAIN.read_text(encoding='utf-8')
    manager = MANAGER.read_text(encoding='utf-8')
    ini = INI.read_text(encoding='utf-8')
    physical = PHYSICAL.read_text(encoding='utf-8')
    require('static constexpr int kBuzzerPin = -1;' in hdr, 'buzzer must remain unassigned')
    require('digitalWrite(kYellowPin' in hdr and 'digitalWrite(kRedPin' in hdr, 'future LED adapter missing')
    require('digitalWrite(kBuzzer' not in hdr and 'pinMode(kBuzzer' not in hdr,
            'buzzer must never be driven by LED-only harness')
    require('static_assert(!V003F08LedBench::kOutputsEnabled' in main_cpp,
            'LED build-only profile must reject physical activation')
    require('static constexpr bool kOutputsEnabled = false;' in physical,
            'production indicator adapter unexpectedly enabled')
    require('V003F08LedBench::instance().tick' in main_cpp, 'SOS/timeout servicing missing')
    require('V003F08LedBench::instance().command' in manager, 'serial command handling missing')
    require('THESIS_V003_F08_LED_OUTPUTS_ENABLE=0' in ini and
            'THESIS_V003_F08_LED_BENCH=1' in ini, 'output-disabled profile missing')
    require('THESIS_V003_F08_INDICATOR_BENCH=1' in ini, 'weather-disabled foundation missing')
    require('cancelActiveSOS();' in manager and 'V003F08DryRun::parse(testLine)' in manager,
            'existing serial handlers missing')

    compiler = (os.environ.get('CXX') or shutil.which('g++') or shutil.which('clang++') or
                (r'C:\msys64\ucrt64\bin\g++.exe' if Path(r'C:\msys64\ucrt64\bin\g++.exe').is_file() else None))
    require(compiler, 'host C++ compiler unavailable: set CXX or add MSYS2 g++ to PATH')

    cpp = r"""
#include <cassert>
#include <cstring>
#include "v003_f08_led_bench.h"
using namespace V003F08LedBench;
int writes=0, modes=0, wrongPins=0, yellowLevel=0, redLevel=0;
void digitalWrite(int pin,int value) {
  ++writes;
  if(pin==kYellowPin) yellowLevel=value;
  else if(pin==kRedPin) redLevel=value;
  else ++wrongPins;
}
void pinMode(int pin,int mode) { ++modes; if ((pin!=39 && pin!=44) || mode!=OUTPUT) ++wrongPins; }
int main() {
  assert(parse("F08 LED OFF")==Request::Off);
  assert(parse("F08 LED YELLOW")==Request::Yellow);
  assert(parse("F08 LED RED")==Request::Red);
  assert(parse("F08 LED STATUS")==Request::Status);
  assert(parse("F08 LED HELP")==Request::Help);
  assert(parse("F08 LED ALL")==Request::Invalid);
  assert(parse("F08 TEST RED")==Request::NotLed);
  assert(parse("CANCEL SOS")==Request::NotLed);
  assert(parse(nullptr)==Request::NotLed);
  assert(std::strcmp(requestName(Request::Red),"RED")==0);
  Controller c;
  c.begin(true);
  assert(!c.yellow() && !c.red());
#if THESIS_V003_F08_LED_OUTPUTS_ENABLE == 1
  assert(modes==2 && writes==2 && yellowLevel==LOW && redLevel==LOW);
#else
  assert(modes==0 && writes==0);
#endif
  assert(c.command(Request::Yellow,100U,false));
  c.tick(599U,false); assert(c.yellow() && !c.red());
  c.tick(600U,false); assert(!c.yellow() && !c.red());
  assert(c.command(Request::Red,700U,false));
  assert(!c.yellow() && c.red());
  c.tick(800U,true); assert(!c.yellow() && !c.red()); // SOS immediate preemption
  assert(!c.command(Request::Yellow,900U,true));
  assert(!c.yellow() && !c.red());
  assert(c.command(Request::Red,UINT32_MAX-50U,false));
  c.tick(100U,false); assert(c.red()); // millis rollover
  c.tick(449U,false); assert(!c.red());
  assert(c.command(Request::Yellow,2000U,false));
  assert(c.command(Request::Off,2100U,false));
  assert(!c.yellow() && !c.red());
  assert(!c.command(Request::Invalid,2200U,false));
  assert(!c.yellow() && !c.red());
#if THESIS_V003_F08_LED_OUTPUTS_ENABLE == 1
  assert(yellowLevel==LOW && redLevel==LOW);
#else
  assert(writes==0 && modes==0);
#endif
  assert(wrongPins==0);
}
"""
    stub = '#pragma once\n#define HIGH 1\n#define LOW 0\n#define OUTPUT 1\nvoid digitalWrite(int,int);\nvoid pinMode(int,int);\n'
    with tempfile.TemporaryDirectory(prefix='thesis_v003_f08_led_') as td:
        tmp = Path(td)
        (tmp / 'Arduino.h').write_text(stub, encoding='utf-8')
        src = tmp / 'led_test.cpp'
        src.write_text(cpp, encoding='utf-8')
        exe = tmp / ('led_test.exe' if os.name == 'nt' else 'led_test')
        cmd = [compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
               '-I', str(tmp), '-I', str(HEADER.parent), str(src), '-o', str(exe)]
        subprocess.run(cmd, check=True)
        subprocess.run([str(exe)], check=True)
        # The future driver, when electrically reviewed, is individually exercised
        # with mocked Arduino APIs; this does not enable any firmware build.
        reviewed = ['-DTHESIS_V003_F08_LED_OUTPUTS_ENABLE=1',
                    '-DTHESIS_V003_F08_LED_HARDWARE_REVIEWED=1',
                    '-DTHESIS_V003_F08_LED_BENCH=1',
                    '-DTHESIS_V003_F08_INDICATOR_BENCH=1',
                    '-DTHESIS_V003_F08_LED_LIVE_PROFILE=1']
        subprocess.run(cmd[:1]+reviewed+cmd[1:], check=True)
        subprocess.run([str(exe)], check=True)
        unauthorized = subprocess.run(cmd[:1]+['-DTHESIS_V003_F08_LED_OUTPUTS_ENABLE=1']+cmd[1:],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        require(unauthorized.returncode != 0 and
                'LED output requires separately reviewed' in unauthorized.stderr,
                'unreviewed LED actuation must fail compilation')
    print('PASS: OFF/YELLOW/RED/STATUS/HELP parser; no ALL or buzzer request')
    print('PASS: 500ms automatic OFF, SOS preemption, millis rollover')
    print('PASS: disabled build performs ZERO GPIO calls')
    print('PASS: mock future LED driver only uses GPIO39/44; buzzer never driven')
    print('PASS: unreviewed actuation compilation rejected')

if __name__ == '__main__':
    main()
