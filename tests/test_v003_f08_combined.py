#!/usr/bin/env python3
"""Host-only mock regression for isolated Vessel-003 combined F08 bench outputs."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT/'firmware/universal/include'
MAIN = ROOT/'firmware/universal/src/main.cpp'
WEATHER = ROOT/'firmware/universal/src/weather_manager.cpp'
INI = ROOT/'firmware/universal/platformio.ini'
HEADER = INC/'v003_f08_combined_bench.h'
PHYSICAL = INC/'physical_indicators.h'


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    header = HEADER.read_text(encoding='utf-8')
    code = MAIN.read_text(encoding='utf-8')
    serial = WEATHER.read_text(encoding='utf-8')
    ini = INI.read_text(encoding='utf-8')
    physical = PHYSICAL.read_text(encoding='utf-8')
    require('VesselIdentity::nodeId() == 3U' in code, 'V003 runtime gate absent')
    require('pmu.isEnableBLDO2()' in code, 'PMU authorization absent')
    require('V003F08CombinedBench::instance().begin(' in code, 'combined startup absent')
    require('V003F08CombinedBench::instance().tick(' in code, 'combined cadence absent')
    require('V003F08CombinedBench::parse(testLine)' in serial, 'isolated serial parser absent')
    require('SIMULATED_ONLY_NO_RADIO' in serial, 'simulated bench intent not disclosed')
    require('THESIS_V003_F08_COMBINED_EXPLICIT_OPT_IN=1' in ini, 'explicit opt-in absent')
    require('static constexpr bool kOutputsEnabled = false;' in physical, 'production output unexpectedly enabled')
    require('PhysicalIndicators::kBuzzerDriverPin == -1' in code, 'production buzzer not isolated')
    require('const bool liveSOS = localSOS || receivedSOS;' in header, 'real SOS preemption absent')
    require('WeatherIndicators::Policy policy_' in header, 'existing F08 cadence policy not reused')
    require('kDemoLimitMs = 6000U' in header, 'demo timeout changed')
    require('digitalWrite(kBuzzerPin, current_.buzzer ? HIGH : LOW);' in header, 'GPIO38 does not follow policy')
    compiler = os.getenv('CXX') or shutil.which('g++') or shutil.which('clang++')
    if not compiler and Path(r'C:\msys64\ucrt64\bin\g++.exe').is_file():
        compiler = r'C:\msys64\ucrt64\bin\g++.exe'
    require(compiler, 'C++ host compiler missing')
    stub = '''#pragma once
#include <cmath>
#include <cstdint>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
void pinMode(int, int);
void digitalWrite(int, int);
'''
    cpp = r'''
#include <cassert>
#include <climits>
#include "v003_f08_combined_bench.h"
using namespace V003F08CombinedBench;
static int writes=0,modes=0,unexpected=0;
static bool configured[49]={};
static int levels[49]={};
void pinMode(int pin,int mode) {
  ++modes;
  if (mode!=OUTPUT || (pin!=38 && pin!=39 && pin!=44)) ++unexpected;
  else configured[pin]=true;
}
void digitalWrite(int pin,int value) {
  ++writes;
  if (pin<0 || pin>48 || !configured[pin] || (pin!=38 && pin!=39 && pin!=44)) ++unexpected;
  else levels[pin]=value;
}
void isOff() { assert(levels[38]==LOW && levels[39]==LOW && levels[44]==LOW); }
int main() {
  assert(kOutputsEnabled && kYellowPin==39 && kRedPin==44 && kBuzzerPin==38);
  assert(parse("F08 IND FRONT")==Request::Front);
  assert(parse("F08 IND SQUALL")==Request::Squall);
  assert(parse("F08 IND SOS")==Request::SOS);
  assert(parse("F08 IND STATUS")==Request::Status);
  assert(parse("F08 IND OFF")==Request::Off);
  assert(parse("F08 IND HELP")==Request::Help);
  assert(parse("F08 IND INVALID")==Request::Invalid);
  assert(parse("F08 BUZZER TEST")==Request::NotIndicator);
  Controller wrong; wrong.begin(false,true); wrong.tick(100,false,false);
  assert(!wrong.authorized() && !wrong.command(Request::Front,100,false) && writes==0 && modes==0);
  Controller noRail; noRail.begin(true,false); noRail.tick(100,false,false);
  assert(!noRail.authorized() && writes==0 && modes==0);
  Controller c; c.begin(true,true);
  assert(c.authorized() && modes==3 && writes==3 && !unexpected); isOff();
  c.tick(100,false,false);isOff();
  assert(c.command(Request::Front,1000,false));
  assert(levels[39]==HIGH && levels[44]==LOW && levels[38]==LOW);
  c.tick(1150,false,false);isOff();
  c.tick(1300,false,false);assert(levels[39]==HIGH && levels[44]==LOW && levels[38]==LOW);
  c.tick(1450,false,false);isOff();
  assert(!c.command(Request::Squall,1500,true)); // real SOS gate
  assert(c.command(Request::Off,1500,false));isOff();
  assert(c.command(Request::Squall,2000,false));
  assert(levels[39]==LOW && levels[44]==HIGH && levels[38]==HIGH);
  c.tick(2500,false,false);isOff();
  c.tick(3000,false,false);assert(levels[44]==HIGH && levels[38]==HIGH);
  c.tick(8000,false,false);isOff();assert(c.demo()==Request::Off);
  assert(c.command(Request::SOS,9000,false));
  assert(levels[44]==HIGH && levels[38]==HIGH && levels[39]==LOW);
  c.tick(9100,false,false);isOff();
  c.tick(9200,false,false);assert(levels[44]==HIGH && levels[38]==HIGH);
  c.tick(9600,false,false);assert(levels[44]==HIGH && levels[38]==LOW);
  c.tick(15000,false,false);isOff();assert(c.demo()==Request::Off);
  assert(c.command(Request::Front,16000,false));
  c.tick(16100,false,true); assert(c.demo()==Request::Off);
  assert(levels[44]==HIGH && levels[38]==HIGH && levels[39]==LOW);
  c.tick(16200,false,false);isOff(); // real SOS cleared, demo must not return
  assert(c.command(Request::Squall,UINT32_MAX-2000U,false));
  c.tick(3998U,false,false); // uint32 rollover, 5999 elapsed
  assert(c.demo()==Request::Squall);
  c.tick(3999U,false,false);
  assert(c.demo()==Request::Off);isOff();
  assert(!unexpected);
  return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='f08_combined_host_') as d:
        td=Path(d)
        (td/'Arduino.h').write_text(stub)
        (td/'test.cpp').write_text(cpp)
        exe=td/('test.exe' if os.name=='nt' else 'test')
        cmd=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic',
             '-I',str(td),'-I',str(INC),str(td/'test.cpp'),'-o',str(exe)]
        flags=['-DTHESIS_V003_F08_COMBINED_PROFILE=1',
               '-DTHESIS_V003_F08_COMBINED_EXPLICIT_OPT_IN=1']
        subprocess.run(cmd[:1]+flags+cmd[1:],check=True)
        subprocess.run([str(exe)],check=True)
        fail=subprocess.run(cmd[:1]+flags[:1]+cmd[1:],capture_output=True,text=True)
        require(fail.returncode!=0 and 'require explicit opt-in' in fail.stderr,
                'missing opt-in gate accepted')
        fail=subprocess.run(cmd[:1]+['-DTHESIS_V003_F08_COMBINED_PROFILE=2']+cmd[1:],
                            capture_output=True,text=True)
        require(fail.returncode!=0 and 'Invalid F08 combined bench build gate' in fail.stderr,
                'invalid profile accepted')
    print('PASS: GPIO38/39/44 startup mode-before-write and wrong-ID/PMU zero GPIO calls')
    print('PASS: existing F08 Front/Squall/SOS cadence, real SOS preempts simulations')
    print('PASS: demo 6000ms cutoff, rollover, real SOS cancellation, command isolation')
    print('PASS: compile-time actuation opt-in enforced')


if __name__=='__main__':
    main()
