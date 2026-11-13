"""F11 Vessel-003 five-physical-readings median and true 100-minute trend.

Host-only. Uses the actual exported weather_manager.cpp processing code with
stubbed sensors; requires g++ on PATH. No hardware, radio, or flash access.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
INC=ROOT/'firmware/universal/include'
WX=ROOT/'firmware/universal/src/weather_manager.cpp'
MAIN=ROOT/'firmware/universal/src/main.cpp'
INI=ROOT/'firmware/universal/platformio.ini'
HEADER=INC/'v003_f11_macro_median.h'
F07=ROOT/'tests/test_f07_weather_acceptance.py'

# Import ONLY the F07 host stubs (they are proven doubles, not firmware inputs).
import importlib.util
spec=importlib.util.spec_from_file_location('f07_harness_for_f11',F07)
f07=importlib.util.module_from_spec(spec)
spec.loader.exec_module(f07)

CPP_BATCH=r'''
#include "v003_f11_macro_median.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace V003F11Macro;
int main(){
  static_assert(kReadings==5 && kReadSpacingMs==2000UL,"physical five median");
  float v[5]={1010.0f,1010.4f,1040.0f,1009.8f,1010.2f};
  assert(std::fabs(median5(v)-1010.2f)<0.001f);
  Batch b;
  assert(b.start(0xFFFFFF00U));
  assert(b.due(0xFFFFFF00U));
  for(int i=0;i<5;i++){
    uint32_t at=uint32_t(0xFFFFFF00ULL+uint64_t(i)*2000ULL);
    assert(b.due(at));
    Sample s{};s.temperatureC=30+i*0.1f;s.humidityPercent=70+i;
    s.stationPressureHpa=v[i];
    assert(b.push(at,true,s)==(i==4));
    if(i!=4)assert(!b.due(at));
  }
  assert(!b.active() && b.complete() && b.valid() && b.count()==5);
  Sample med=b.result();
  assert(std::fabs(med.temperatureC-30.2f)<0.001f);
  assert(std::fabs(med.humidityPercent-72.0f)<0.001f);
  assert(std::fabs(med.stationPressureHpa-1010.2f)<0.001f);
  assert(!b.due(100000U));
  assert(b.start(100000U));
  for(int i=0;i<5;i++){
    uint32_t at=100000U+i*2000U;
    Sample s{};s.temperatureC=30;s.humidityPercent=75;s.stationPressureHpa=1011;
    b.push(at,i!=2,s);
  }
  assert(b.complete() && !b.valid() && std::isnan(b.result().stationPressureHpa));
  assert(b.start(200000U));
  for(int i=0;i<5;i++){
    uint32_t at=200000U+i*(i==0?0:7000U); // delayed sample must invalidate entire batch
    Sample s{};s.temperatureC=30;s.humidityPercent=75;s.stationPressureHpa=1011;
    if(b.due(at))b.push(at,true,s);
  }
  assert(b.complete() && !b.valid());
  assert(validStep(600000U,0U) && !validStep(1200000U,0U));
  assert(validTendencySpan(6000000U) && !validTendencySpan(5400000U));
  assert(validStep(0x00080000U,0xFFFEDC80U)); // rollover arithmetic supported
  std::puts("PASS: five physical readings, 2s spacing, median/outlier rejection, incomplete-batch rejection, clock rollover");
}
'''

CPP_WEATHER=r'''
// Replace F07 millis stub in STUBS with a simulated monotonic clock.
void resetF11() {
  resetWeatherCalibration("F11 fixture reset");
  lastAcceptedStationPressure=NAN;
  pendingStationPressure=NAN;
  bmeConsecutiveFailures=dhtConsecutiveFailures=0;
  bmeAvailable=dhtAvailable=true;
  weatherInputsValid=false;
}
void macroAt(uint32_t when,float p,bool valid=true){
  fakeNow=when;
  processWeatherFromF11Median(valid,30.0f,75.0f,p);
}
int main(){
  resetF11();
  for(int i=0;i<=10;i++){
    macroAt(uint32_t(i)*600000U,1010.0f-float(i)*0.2f);
    assert(weatherInputsValid && historyCount==unsigned(i<10 ? i+1 : 10));
    if(i<10){
      assert(weatherState==WEATHER_CALIBRATING && zambrettiIndex==0);
      assert(std::isnan(pressureDelta));
    }
  }
  assert(weatherState==WEATHER_FRONT);
  assert(std::fabs(pressureDelta-(-2.0f))<0.01f);
  assert(weatherMeasurement.historyCount==10 && weatherMeasurement.inputsOK);
  // The result must be endpoint t100 - endpoint t0, not mean of prior ten.
  assert(pressureHistory[historyIndex] > 1009.7f && pressureHistory[historyIndex] < 1010.1f);
  std::puts("PASS: minute0/10/.../100 = first Zambretti forecast at 11th macro median; true endpoint trend");

  resetF11();
  for(int i=0;i<5;i++)macroAt(uint32_t(i)*600000U,1010.0f);
  assert(historyCount==5);
  macroAt(5U*600000U,NAN,false);
  assert(historyCount==0 && weatherState==WEATHER_CALIBRATING && !weatherInputsValid);
  macroAt(6U*600000U,1010.0f);
  assert(historyCount==1 && weatherState==WEATHER_CALIBRATING);
  std::puts("PASS: invalid five-reading batch resets forecast history; no stale median accepted");

  resetF11();
  macroAt(0U,1010.0f);
  macroAt(1200000U,1009.0f); // skipped ten-minute interval
  assert(historyCount==1 && weatherState==WEATHER_CALIBRATING);
  std::puts("PASS: missed 10-minute interval forces new calibration");

  resetF11();
  macroAt(0U,1010.0f);
  macroAt(600000U,1030.0f); // original 8 hPa rejection still applies to medians
  assert(historyCount==0 && !weatherInputsValid);
  macroAt(1200000U,1030.5f); // accept confirmed shift, fresh history
  assert(historyCount==1 && weatherInputsValid && weatherState==WEATHER_CALIBRATING);
  std::puts("PASS: original pressure spike rejection/confirmation preserved for median outputs");
}
'''

def run():
    cxx=shutil.which('g++') or os.environ.get('CXX')
    if not cxx: raise RuntimeError('g++ missing')
    flags=['-std=c++17','-Wall','-Wextra','-I',str(INC),
           '-DTHESIS_V003_F09_SHARED_SENSOR_BUS=1',
           '-DTHESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN=1',
           '-DTHESIS_V003_F10_MICRO_NOWCAST=1',
           '-DTHESIS_V003_F10_MICRO_NOWCAST_OPT_IN=1',
           '-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN=1',
           '-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN_OPT_IN=1',
           '-DWEATHER_FAST_TEST=0']
    with tempfile.TemporaryDirectory(prefix='thesis_f11_') as td:
        tmp=Path(td)
        for name,code in [('batch',CPP_BATCH),('weather',None)]:
            src=tmp/(name+'.cpp'); exe=tmp/(name+'.exe')
            if name=='weather':
                hdr=(INC/'weather_manager.h').read_text().replace('#pragma once','').replace('#include <Arduino.h>','')
                wx=re.sub(r'^#include.*$','',WX.read_text(),flags=re.MULTILINE)
                stub=f07.STUBS.replace('unsigned long millis() { static unsigned long t = 0; return ++t; }',
                    'uint32_t fakeNow=0; unsigned long millis() { return fakeNow; }')
                assert stub!=f07.STUBS
                code='\n'.join((stub, '#include \"v003_f11_macro_median.h\"',hdr,wx,CPP_WEATHER))
            src.write_text(code)
            res=subprocess.run([cxx,*flags,str(src),'-o',str(exe)],capture_output=True,text=True)
            if res.returncode: raise AssertionError(f'{name} compile failed:\n{res.stderr}')
            run=subprocess.run([str(exe)],capture_output=True,text=True)
            if run.returncode: raise AssertionError(f'{name} run failed: {run.stderr}\n{run.stdout}')
            print(run.stdout,end='')
        for bad in (["-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN=1"],
                    ["-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN=1","-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN_OPT_IN=1"],
                    ["-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN=2"]):
            src=tmp/'invalid.cpp'
            src.write_text('#include "v003_f11_macro_median.h"\nint main(){return 0;}\n')
            args=[cxx,'-std=c++17','-I',str(INC),'-DWEATHER_FAST_TEST=0',*bad,'-fsyntax-only',str(src)]
            res=subprocess.run(args,capture_output=True,text=True)
            assert res.returncode!=0, 'F11 build gate admitted invalid flags'
    main=MAIN.read_text(); ini=INI.read_text();wx=WX.read_text()
    assert 'f11InitialBatchPending' in main and 'f11MacroBatch.push(millis(), readOK, sample)' in main
    assert 'processWeatherFromF11Median(batchOK' in main
    assert 'f11MacroSampleForMicro = true' in main and '!f11MacroBatch.active()' in main
    assert 'PhysicalIndicators::update(millis(), weatherState, weatherInputsValid' in main
    assert '[env:tbeam-supreme-v003-f11-macro-five-median-v003-only]' in ini
    assert 'THESIS_V003_F11_MACRO_FIVE_MEDIAN_OPT_IN=1' in ini
    assert 'void processWeatherFromF11Median(' in wx and 'if (!f11UseMedian && dhtAvailable)' in wx and 'if (!f11UseMedian && bmeAvailable)' in wx
    print('PASS: firmware F11 env, Vessel-003 sensor identity, no double reads at shared boundaries, outputs disabled')
if __name__=='__main__':run()
