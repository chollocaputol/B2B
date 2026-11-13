"""F09 opt-in Vessel-003 shared BME680+DHT20 I2C bus guards.
Runs without PlatformIO or hardware; uses the local MSYS2 g++ on Windows.
"""
from pathlib import Path
import os, subprocess, tempfile, shutil

ROOT=Path(__file__).resolve().parents[1]
INC=ROOT/'firmware/universal/include'
MAIN=(ROOT/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
WEATHER=(ROOT/'firmware/universal/src/weather_manager.cpp').read_text(encoding='utf-8')
INI=(ROOT/'firmware/universal/platformio.ini').read_text(encoding='utf-8')


def require(ok,msg):
    if not ok: raise AssertionError(msg)


def compile_profile(compiler, tmp, defs, valid):
    core=[compiler,'-std=c++17','-fsyntax-only','-I',str(tmp),'-I',str(INC)]
    code=r'''#include "weather_manager.h"
static_assert(BME_SDA_PIN == 21 && BME_SCL_PIN == 48, "BME pins changed");
#if THESIS_V003_F09_SHARED_SENSOR_BUS == 1
static_assert(DHT_SDA_PIN == BME_SDA_PIN && DHT_SCL_PIN == BME_SCL_PIN,
              "F09 must share the weather bus");
#else
static_assert(DHT_SDA_PIN == 38 && DHT_SCL_PIN == 39,
              "Default and F08 profiles must preserve historical pins");
#endif
static_assert(V003F09SharedSensorBus::kDhtAddress == 0x38, "DHT20 address");
static_assert(V003F09SharedSensorBus::kBmeAddress == 0x77, "V003 BME680 address");
static_assert(V003F09SharedSensorBus::authorized(true,3U), "ID3 gate");
static_assert(!V003F09SharedSensorBus::authorized(true,1U), "ID1 blocked");
static_assert(!V003F09SharedSensorBus::authorized(true,2U), "ID2 blocked");
static_assert(!V003F09SharedSensorBus::authorized(false,3U), "invalid identity blocked");
int main(){return 0;}
'''
    path=tmp/'guard.cpp'; path.write_text(code)
    cmd=core+['-D'+x for x in defs]+[str(path)]
    run=subprocess.run(cmd,capture_output=True,text=True)
    if valid: require(run.returncode==0,'Compiler rejected expected valid profile: '+run.stderr)
    else: require(run.returncode!=0,'Compiler accepted invalid profile: '+','.join(defs))


def main():
    compiler=shutil.which('g++')
    require(compiler is not None,'g++ not found (add C:\\msys64\\ucrt64\\bin to PATH)')
    with tempfile.TemporaryDirectory(prefix='thesis_f09_') as td:
        tmp=Path(td)
        (tmp/'Arduino.h').write_text('#pragma once\n#include <stdint.h>\n')
        compile_profile(compiler,tmp,[],True)
        compile_profile(compiler,tmp,['THESIS_V003_F09_SHARED_SENSOR_BUS=1',
                                      'THESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN=1'],True)
        compile_profile(compiler,tmp,['THESIS_V003_F09_SHARED_SENSOR_BUS=1'],False)
        compile_profile(compiler,tmp,['THESIS_V003_F09_SHARED_SENSOR_BUS=2',
                                      'THESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN=1'],False)
        compile_profile(compiler,tmp,['THESIS_V003_F09_SHARED_SENSOR_BUS=1',
                                      'THESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN=2'],False)
    print('PASS: default 38/39 preserved; opt-in F09 DHT20/BME680 21/48; invalid gates rejected')

    require('if (activeWireSDA == sda && activeWireSCL == scl)' in MAIN,
            'I2C same-pair cache missing')
    require('switchI2C(BME_SDA_PIN, BME_SCL_PIN);' in WEATHER and
            'switchI2C(DHT_SDA_PIN, DHT_SCL_PIN);' in WEATHER,
            'Both sensors must use the caller-owned Wire bus')
    require('dht.begin();' in WEATHER and 'bme.performReading()' in WEATHER and
            'dht.read()' in WEATHER,'Real sensor reads changed')
    print('PASS: BME/DHT real reads and single Wire cache retained; no I2C mux hardware added')

    require(MAIN.index('VesselIdentity::load();\n  f09SensorAuthorized') <
            MAIN.index('if (f09SensorAuthorized) {\n    setupSensors();'),
            'Identity must be checked before sensor initialization')
    require('if (f09SensorAuthorized) {\n      processWeather();' in MAIN,
            'Wrong identity may still poll weather pins')
    require('!PhysicalIndicators::kOutputsEnabled' in MAIN and
            'THESIS_V003_F08_COMBINED_PROFILE != 0' in MAIN and
            'THESIS_LAB_RELAY_TX != 0' in MAIN and
            'THESIS_SD_LOGGER != 0' in MAIN,
            'F09 must be isolated from physical outputs, SD, relay, F08 GPIO drivers')
    require('F09-SAMPLE' in MAIN and 'F09-I2C' in MAIN,
            'Expected startup and sample evidence missing')
    print('PASS: early Vessel-003 identity gate; wrong-ID sensor operations blocked; outputs OFF')

    name='[env:tbeam-supreme-v003-f09-shared-i2c-sensors-v003-only]'
    require(name in INI,'Missing F09 build environment')
    env=INI.split(name,1)[1].split('\n[env:',1)[0]
    require('extends = env:tbeam-supreme' in env and
            'THESIS_V003_F09_SHARED_SENSOR_BUS=1' in env and
            'THESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN=1' in env and
            'WEATHER_FAST_TEST=1' in env and
            'THESIS_V003_F08_INDICATOR_BENCH=1' not in env,
            'F09 build must be separate from F08 weather-disabled profiles')
    print('PASS: dedicated 5s development profile, unchanged baseline and F08 environment flags')

if __name__=='__main__': main()
