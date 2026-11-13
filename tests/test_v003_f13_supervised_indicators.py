"""F13 host-only preflight: real weather/SOS priority, physical GPIO isolation.

Builds the actual F13 adapter against stub Arduino pins (never touches hardware).
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / "firmware/universal/include"
MAIN = ROOT / "firmware/universal/src/main.cpp"
INI = ROOT / "firmware/universal/platformio.ini"
HDR = INC / "v003_f13_supervised_indicators.h"
ARDUINO = r'''#pragma once
#include <stdint.h>
#define OUTPUT 1
#define HIGH 1
#define LOW 0
extern int modes, writes, pinState[49];
inline void pinMode(int pin,int mode) { if (pin < 0 || pin >= 49 || mode != OUTPUT) __builtin_trap(); ++modes; }
inline void digitalWrite(int pin,int value) { if (pin < 0 || pin >= 49) __builtin_trap(); ++writes; pinState[pin] = value; }
'''
CPP = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include "v003_f13_supervised_indicators.h"
using namespace V003F13Indicators;
using WeatherIndicators::Mode;
int modes=0, writes=0, pinState[49] = {};
static void expect(Controller& c, Mode mode, bool y,bool r,bool b) {
  assert(c.mode() == mode);
  auto x=c.outputs();
  assert(x.yellow == y && x.red == r && x.buzzer == b);
  if constexpr (kPhysicalOutputsEnabled) {
    assert(pinState[39] == int(y) && pinState[44] == int(r) && pinState[38] == int(b));
  } else { assert(modes == 0 && writes == 0); }
}
int main(){
  Controller denied;
  denied.begin(false,true);
  denied.tick(0,WEATHER_SQUALL,true,true,true,true);
  assert(!denied.authorized() && modes == 0 && writes == 0);
  Controller noRail;
  noRail.begin(true,false);
  noRail.tick(0,WEATHER_SQUALL,true,true,true,true);
  assert(!noRail.authorized() && modes == 0 && writes == 0);
  std::cout << "PASS: wrong-ID and disabled PMU enable bit make zero GPIO calls\n";

  Controller c;
  c.begin(true,true);
  assert(c.authorized());
  if constexpr (kPhysicalOutputsEnabled) {
    assert(modes == 3 && writes == 3);
    assert(pinState[39] == 0 && pinState[44] == 0 && pinState[38] == 0);
  } else { assert(modes == 0 && writes == 0); }
  c.tick(0,WEATHER_NORMAL,true,false,false,true);
  expect(c,Mode::Off,false,false,false);
  const int writesBeforeIdle = writes;
  c.tick(50,WEATHER_NORMAL,true,false,false,true);
  expect(c,Mode::Off,false,false,false);
  assert(writes == writesBeforeIdle); // suppress redundant GPIO writes
  c.tick(100,WEATHER_FRONT,true,false,false,true);
  expect(c,Mode::Front,true,false,false);
  c.tick(250,WEATHER_FRONT,true,false,false,true);
  expect(c,Mode::Front,false,false,false);
  c.tick(400,WEATHER_FRONT,true,false,false,true);
  expect(c,Mode::Front,true,false,false);
  c.tick(450,WEATHER_SQUALL,true,false,false,true);
  expect(c,Mode::Squall,false,true,true);
  c.tick(950,WEATHER_SQUALL,true,false,false,true);
  expect(c,Mode::Squall,false,false,false);
  c.tick(1000,WEATHER_FRONT,false,false,false,true);
  expect(c,Mode::Off,false,false,false);
  std::cout << "PASS: macro weather, validity gating and cadences follow tested production policy\n";
  // Not receiving micro state is deliberate: experimental watch stays virtual.
  c.tick(1100,WEATHER_NORMAL,true,false,false,true);
  expect(c,Mode::Off,false,false,false);
  c.tick(1200,WEATHER_CALIBRATING,false,false,true,true);
  expect(c,Mode::SOS,false,true,true);
  c.tick(1300,WEATHER_CALIBRATING,false,false,true,true);
  expect(c,Mode::SOS,false,false,false);
  c.tick(1400,WEATHER_SQUALL,true,true,false,true);
  expect(c,Mode::SOS,false,true,true);
  c.tick(1500,WEATHER_FRONT,true,false,false,true);
  expect(c,Mode::Front,true,false,false);
  std::cout << "PASS: authoritative local/received SOS preempts all macro states, even invalid sensors\n";
  c.tick(1550,WEATHER_SQUALL,true,false,false,false);
  assert(c.railLost());
  assert(!c.outputs().yellow && !c.outputs().red && !c.outputs().buzzer);
  if constexpr (kPhysicalOutputsEnabled) {
    assert(pinState[39] == 0 && pinState[44] == 0 && pinState[38] == 0);
  }
  int before = writes;
  c.tick(2000,WEATHER_SQUALL,true,true,false,true);
  assert(c.railLost() && writes == before);
  std::cout << "PASS: PMU-enable dropout forces OFF and latches until reboot\n";

  Controller wrap;
  wrap.begin(true,true);
  wrap.tick(UINT32_MAX-40U,WEATHER_FRONT,true,false,false,true);
  expect(wrap,Mode::Front,true,false,false);
  wrap.tick(109U,WEATHER_FRONT,true,false,false,true);
  expect(wrap,Mode::Front,false,false,false);
  wrap.tick(259U,WEATHER_FRONT,true,false,false,true);
  expect(wrap,Mode::Front,true,false,false);
  std::cout << "PASS: millisecond rollover-safe warning cadence\n";
}
'''
GOOD = [
 '-DTHESIS_V003_F09_SHARED_SENSOR_BUS=1',
 '-DTHESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN=1',
 '-DTHESIS_V003_F10_MICRO_NOWCAST=1',
 '-DTHESIS_V003_F10_MICRO_NOWCAST_OPT_IN=1',
 '-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN=1',
 '-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN_OPT_IN=1',
 '-DTHESIS_V003_F12_INDICATOR_PREVIEW=1',
 '-DTHESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN=1',
 '-DTHESIS_V003_F13_SUPERVISED_DRIVER=1',
 '-DTHESIS_V003_F13_LIVE_OUTPUTS=0',
 '-DTHESIS_V003_F13_EXPLICIT_OPT_IN=0',
 '-DTHESIS_V003_F13_HARDWARE_REVIEWED=0',
 '-DWEATHER_FAST_TEST=0',
]

def build(compiler, temp, flags, good=True, clue=None):
    src=temp/'f13.cpp'
    src.write_text(CPP)
    cmd=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(temp),'-I',str(INC),str(src),'-o',str(temp/'f13'),*flags]
    r=subprocess.run(cmd,encoding='utf-8',capture_output=True)
    if good:
        assert r.returncode == 0, r.stderr
        run=subprocess.run([str(temp/'f13')],capture_output=True,text=True)
        assert run.returncode == 0,run.stderr
        print(run.stdout,end='')
    else:
        assert r.returncode != 0 and (not clue or clue in r.stderr),r.stderr

def run():
    main=MAIN.read_text()
    ini=INI.read_text()
    header=HDR.read_text()
    assert main.count('f13Indicators.tick(') == 1
    call=main[main.index('f13Indicators.tick('):].split(';',1)[0]
    # F14 supplies an optional driver-only selection. In all other builds,
    # these arguments must still originate from the actual F11 weather state.
    if 'weatherState' not in call or 'weatherInputsValid' not in call:
        assert 'f13HardwareState' in call and 'f13HardwareValid' in call
        baseline = '  uint8_t f13HardwareState = weatherState;\n  bool f13HardwareValid = weatherInputsValid;'
        assert baseline in main, 'F13 input defaults must be real macro weather'
        begin = main.index(baseline)
        end = main.index('  f13Indicators.tick(', begin)
        route = main[begin:end]
        gate = '#if THESIS_V003_F14_INDICATOR_PROBE == 1'
        assert route.count(gate) == 1 and route.count('#endif') == 1
        guarded = route.split(gate, 1)[1].split('#endif', 1)[0]
        assert 'f14Probe.select(' in guarded
        assert 'weatherState, weatherInputsValid' in guarded
        assert 'hasActiveLocalSOS() || getActiveSOSCount() > 0' in guarded
        assert 'f13HardwareState = f14Selection.state;' in guarded
        assert 'f13HardwareValid = f14Selection.valid;' in guarded
        assert route.strip().endswith('#endif'), 'F14 override must be compile-time gated'
    assert 'hasActiveLocalSOS()' in call and 'getActiveSOSCount() > 0' in call
    assert 'f10Forecaster' not in call and 'alertDemoMode' not in call
    assert 'f09SensorAuthorized && VesselIdentity::ready()' in main
    assert 'VesselIdentity::nodeId() == 3U' in main
    assert 'pmu.isEnableBLDO2()' in main
    assert 'F13 must not share GPIO ownership' in main
    assert 'tbeam-supreme-v003-f13-indicator-preflight-v003-only' in ini
    assert 'tbeam-supreme-v003-f13-indicator-live-v003-only' in ini
    assert 'THESIS_V003_F13_LIVE_OUTPUTS=0' in ini and 'THESIS_V003_F13_LIVE_OUTPUTS=1' in ini
    assert 'kPhysicalOutputsEnabled' in header
    for forbidden in ('transmitSOS(', 'transmitPacket(', 'readMicroWeatherSample(', 'processWeather(', 'Preferences', 'SDLogger'):
        assert forbidden not in header, forbidden
    print('PASS: F13 separately owns only GPIO38/39/44; no micro, radio, SD, I2C or OLED ownership')
    compiler=os.environ.get('CXX') or ('C:/msys64/ucrt64/bin/g++.exe' if os.name=='nt' else 'g++')
    with tempfile.TemporaryDirectory(prefix='thesis_f13_') as d:
        temp=Path(d)
        (temp/'Arduino.h').write_text(ARDUINO)
        build(compiler,temp,GOOD)
        live=[f for f in GOOD if not f.startswith(('-DTHESIS_V003_F13_LIVE_OUTPUTS=', '-DTHESIS_V003_F13_EXPLICIT_OPT_IN=', '-DTHESIS_V003_F13_HARDWARE_REVIEWED='))]
        live+=['-DTHESIS_V003_F13_LIVE_OUTPUTS=1','-DTHESIS_V003_F13_EXPLICIT_OPT_IN=1','-DTHESIS_V003_F13_HARDWARE_REVIEWED=1']
        build(compiler,temp,live)
        build(compiler,temp,[f for f in live if f != '-DTHESIS_V003_F13_EXPLICIT_OPT_IN=1'],False,'F13 physical GPIO needs')
        build(compiler,temp,[f for f in live if f != '-DTHESIS_V003_F13_HARDWARE_REVIEWED=1'],False,'F13 physical GPIO needs')
        build(compiler,temp,[f for f in GOOD if f != '-DTHESIS_V003_F12_INDICATOR_PREVIEW=1'],False,'F13 requires real-time')
        build(compiler,temp,[f for f in GOOD if f != '-DWEATHER_FAST_TEST=0']+['-DWEATHER_FAST_TEST=1'],False,'F13 requires real-time')
        build(compiler,temp,[f for f in GOOD if f != '-DTHESIS_V003_F13_LIVE_OUTPUTS=0']+['-DTHESIS_V003_F13_LIVE_OUTPUTS=2'],False,'Invalid F13')
        print('PASS: F13 live-output build gates reject missing opt-ins, unsafe timing, and invalid values')

if __name__ == '__main__':
    run()
