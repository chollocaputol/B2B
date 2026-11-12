#!/usr/bin/env python3
# Mock-only, no serial/device I/O: guard buzzer live profile and timing.
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
INC=ROOT/'firmware/universal/include'
MAIN=ROOT/'firmware/universal/src/main.cpp'
WEATHER=ROOT/'firmware/universal/src/weather_manager.cpp'
INI=ROOT/'firmware/universal/platformio.ini'
PHYSICAL=INC/'physical_indicators.h'
HEADER=INC/'v003_f08_buzzer_live.h'


def require(x, label):
    if not x: raise AssertionError(label)


def main():
    src=MAIN.read_text(encoding='utf-8')
    weather=WEATHER.read_text(encoding='utf-8')
    ini=INI.read_text(encoding='utf-8')
    header=HEADER.read_text(encoding='utf-8')
    physical=PHYSICAL.read_text(encoding='utf-8')
    require('VesselIdentity::nodeId() == 3U' in src, 'missing identity gate')
    require('V003F08BuzzerLive::instance().begin(' in src, 'missing live driver initialization')
    require('pmu.isEnableBLDO2()' in src, 'missing PMU enable gate')
    require('V003F08BuzzerLive::instance().tick(' in src, 'missing timeout/SOS tick')
    require(src.index('VesselIdentity::load();') < src.index('V003F08BuzzerLive::instance().begin('), 'identity not loaded first')
    require('V003F08BuzzerLive::parse(testLine)' in weather, 'missing serial handler')
    require('cancelActiveSOS();' in weather, 'SOS cancellation disturbed')
    require('[env:tbeam-supreme-v003-f08-buzzer-live-v003-only]' in ini, 'dedicated env missing')
    require('THESIS_V003_F08_BUZZER_LIVE_EXPLICIT_OPT_IN=1' in ini, 'actuation opt-in absent')
    require('static constexpr bool kOutputsEnabled = false;' in physical, 'production driver enabled')
    require('static constexpr int kBuzzerDriverPin = -1;' in physical, 'production buzzer assigned')
    require('static constexpr int kBuzzerPin = 38;' in header, 'wrong buzzer GPIO')
    require('static constexpr uint32_t kPulseMs = 250U;' in header, 'wrong pulse duration')
    require('if (req != Request::Test || sosActive || active_) return false;' in header, 're-trigger/SOS guard absent')
    compiler=os.getenv('CXX') or shutil.which('g++') or shutil.which('clang++')
    if not compiler and Path(r'C:\msys64\ucrt64\bin\g++.exe').is_file(): compiler=r'C:\msys64\ucrt64\bin\g++.exe'
    require(compiler,'host C++ compiler missing (g++/clang++): set PATH or CXX')
    stub='''#pragma once
#define LOW 0
#define HIGH 1
#define OUTPUT 1
void digitalWrite(int, int);
void pinMode(int, int);
'''
    cpp=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include "v003_f08_buzzer_live.h"
using namespace V003F08BuzzerLive;
static int modes=0, writes=0, level=-1, wrongPin=0;
void digitalWrite(int pin, int value) {
  writes++; if (pin != kBuzzerPin) wrongPin++; level=value;
}
void pinMode(int pin, int mode) {
  modes++; if (pin != kBuzzerPin || mode != OUTPUT) wrongPin++;
}
int main() {
  assert(kOutputsEnabled && kBuzzerPin==38 && kPulseMs==250);
  assert(parse("F08 BUZZER TEST") == Request::Test);
  assert(parse("F08 BUZZER OFF") == Request::Off);
  assert(parse("F08 BUZZER STATUS") == Request::Status);
  assert(parse("F08 BUZZER HELP") == Request::Help);
  assert(parse("F08 BUZZER OOPS") == Request::Invalid);
  assert(parse("F08 LED RED") == Request::NotBuzzer);
  assert(parse("CANCEL SOS") == Request::NotBuzzer);
  Controller denied; denied.begin(false,true);
  assert(!denied.authorized() && modes==0 && writes==0);
  assert(!denied.command(Request::Test, 1,false));
  denied.tick(1000,false); assert(modes==0 && writes==0);
  Controller noRail; noRail.begin(true,false);
  assert(!noRail.authorized() && modes==0 && writes==0);
  assert(!noRail.command(Request::Test, 1,false));
  Controller c; c.begin(true,true);
  assert(c.authorized() && modes==1 && writes==1 && level==LOW);
  assert(!c.active());
  assert(!c.command(Request::Test, 1000,true)); assert(level==LOW);
  assert(c.command(Request::Test,1000,false)); assert(c.active() && level==HIGH);
  assert(!c.command(Request::Test,1249,false)); // cannot extend pulse
  c.tick(1249,false); assert(c.active() && level==HIGH);
  c.tick(1250,false); assert(!c.active() && level==LOW);
  assert(c.command(Request::Test,2000,false)); assert(level==HIGH);
  c.tick(2001,true); assert(!c.active() && level==LOW);
  assert(c.command(Request::Test,3000,false));
  assert(c.command(Request::Off,3001,false)); assert(!c.active() && level==LOW);
  assert(c.command(Request::Test,UINT32_MAX-10U,false));
  c.tick(238,false); assert(c.active() && level==HIGH);
  c.tick(239,false); assert(!c.active() && level==LOW);
  assert(!wrongPin);
  return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='thesis_f08_buzzer_live_host_') as tmp:
        temp=Path(tmp)
        (temp/'Arduino.h').write_text(stub)
        (temp/'test.cpp').write_text(cpp)
        exe=temp/('buzzer.exe' if os.name=='nt' else 'buzzer')
        core=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic','-I',str(temp),'-I',str(INC),str(temp/'test.cpp'),'-o',str(exe)]
        flags=['-DTHESIS_V003_F08_BUZZER_LIVE_PROFILE=1','-DTHESIS_V003_F08_BUZZER_LIVE_EXPLICIT_OPT_IN=1']
        subprocess.run(core[:1]+flags+core[1:],check=True)
        subprocess.run([str(exe)],check=True)
        denied=subprocess.run(core[:1]+['-DTHESIS_V003_F08_BUZZER_LIVE_PROFILE=1']+core[1:],capture_output=True,text=True)
        require(denied.returncode!=0 and 'requires explicit actuation opt-in' in denied.stderr,'missing compile-time actuation gate')
        denied=subprocess.run(core[:1]+['-DTHESIS_V003_F08_BUZZER_LIVE_PROFILE=2']+core[1:],capture_output=True,text=True)
        require(denied.returncode!=0 and 'Invalid buzzer-live profile flag' in denied.stderr, 'invalid profile accepted')
    print('PASS: wrong vessel/disabled PMU produce zero GPIO writes; correct startup OFF')
    print('PASS: GPIO38 only, 250ms shutoff, rollover, OFF and SOS preemption')
    print('PASS: no retrigger while active; serial parsing isolated from LED/SOS')
    print('PASS: unreviewed or invalid live configuration rejected at compile time')

if __name__=='__main__':
    main()
