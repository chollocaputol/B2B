"""F12 Vessel-003 virtual weather/nowcast/SOS indicator routing regression.

Host-only: compiles the firmware's actual policy and F12 header with g++.
Does not contact serial ports, radio, storage or physical GPIOs.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / 'firmware/universal/include'
MAIN = ROOT / 'firmware/universal/src/main.cpp'
INI = ROOT / 'firmware/universal/platformio.ini'
HEADER = INC / 'v003_f12_indicator_preview.h'

STUB = '#pragma once\n#include <stdint.h>\n'
CPP = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include "v003_f12_indicator_preview.h"
using namespace V003F12Preview;
using WeatherIndicators::Mode;
using V003F10Micro::State;
void expect(const Result& r, Source cause, Mode mode,
            bool yellow, bool red, bool buzzer) {
    assert(r.authorized);
    assert(r.source == cause);
    assert(r.mode == mode);
    assert(r.virtualOutputs.yellow == yellow);
    assert(r.virtualOutputs.red == red);
    assert(r.virtualOutputs.buzzer == buzzer);
}
int main() {
    Controller c;
    c.begin(false);
    auto x = c.preview(1, WEATHER_SQUALL, true, State::RISK_WATCH, true, true);
    assert(!x.authorized && x.source == Source::UNAUTHORIZED);
    assert(!x.virtualOutputs.yellow && !x.virtualOutputs.red && !x.virtualOutputs.buzzer);
    std::cout << "PASS: unapproved vessel cannot preview a live weather/SOS output\n";

    c.begin(true);
    x = c.preview(2, WEATHER_CALIBRATING, true, State::WARMUP, false, false);
    expect(x, Source::OFF, Mode::Off, false, false, false);
    assert(x.changed);
    x = c.preview(3, WEATHER_CALIBRATING, true, State::CANDIDATE, false, false);
    expect(x, Source::OFF, Mode::Off, false, false, false);
    assert(!x.changed);
    x = c.preview(4, WEATHER_NORMAL, true, State::NO_PATTERN, false, false);
    expect(x, Source::OFF, Mode::Off, false, false, false);

    x = c.preview(1000, WEATHER_NORMAL, true, State::RISK_WATCH, false, false);
    expect(x, Source::MICRO_EXPERIMENTAL_WATCH, Mode::Front, true, false, false);
    assert(x.changed);
    x = c.preview(1150, WEATHER_NORMAL, true, State::RISK_WATCH, false, false);
    expect(x, Source::MICRO_EXPERIMENTAL_WATCH, Mode::Front, false, false, false);
    x = c.preview(1300, WEATHER_CALIBRATING, false, State::RISK_WATCH, false, false);
    expect(x, Source::MICRO_EXPERIMENTAL_WATCH, Mode::Front, true, false, false);
    x = c.preview(1450, WEATHER_NORMAL, true, State::INVALID, false, false);
    expect(x, Source::OFF, Mode::Off, false, false, false);
    std::cout << "PASS: only experimental RISK_WATCH maps to a virtual yellow-only watch\n";

    x = c.preview(2000, WEATHER_FRONT, true, State::RISK_WATCH, false, false);
    expect(x, Source::MACRO_FRONT, Mode::Front, true, false, false);
    x = c.preview(2050, WEATHER_FRONT, false, State::INVALID, false, false);
    expect(x, Source::OFF, Mode::Off, false, false, false);
    x = c.preview(3000, WEATHER_SQUALL, true, State::RISK_WATCH, false, false);
    expect(x, Source::MACRO_SQUALL, Mode::Squall, false, true, true);
    x = c.preview(3500, WEATHER_SQUALL, true, State::RISK_WATCH, false, false);
    expect(x, Source::MACRO_SQUALL, Mode::Squall, false, false, false);
    std::cout << "PASS: macro validity gate, FRONT/SQUALL precedence, distinct cadences\n";

    x = c.preview(4000, WEATHER_SQUALL, true, State::RISK_WATCH, true, false);
    expect(x, Source::SOS, Mode::SOS, false, true, true);
    x = c.preview(4100, WEATHER_SQUALL, true, State::RISK_WATCH, false, true);
    expect(x, Source::SOS, Mode::SOS, false, false, false);
    x = c.preview(4200, WEATHER_SQUALL, true, State::RISK_WATCH, false, false);
    expect(x, Source::MACRO_SQUALL, Mode::Squall, false, true, true);
    x = c.preview(4300, WEATHER_CALIBRATING, false, State::INVALID, false, true);
    expect(x, Source::SOS, Mode::SOS, false, true, true);
    std::cout << "PASS: authoritative local/received SOS preempts all weather and invalid sensors\n";

    // Use unsigned timing through wraparound. Source changes must reset cadence.
    const uint32_t start = UINT32_MAX - 10U;
    c.begin(true);
    x = c.preview(start, WEATHER_FRONT, true, State::WARMUP, false, false);
    expect(x, Source::MACRO_FRONT, Mode::Front, true, false, false);
    x = c.preview(start + 310U, WEATHER_FRONT, true, State::WARMUP, false, false);
    expect(x, Source::MACRO_FRONT, Mode::Front, true, false, false);
    c.begin(false);
    x = c.preview(start + 311U, WEATHER_FRONT, true, State::WARMUP, true, false);
    assert(!x.authorized && x.source == Source::UNAUTHORIZED);
    std::cout << "PASS: millis rollover and identity revocation\n";
}
'''

def build_and_run(compiler, tmp, flags=(), should_work=True, clue=None):
    source = tmp / 'f12.cpp'
    source.write_text(CPP)
    cmd = [compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
           '-I', str(tmp), '-I', str(INC), str(source), '-o', str(tmp / 'f12')]
    cmd.extend(flags)
    res = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if should_work:
        assert res.returncode == 0, res.stderr
        subprocess.run([str(tmp / 'f12')], check=True)
    else:
        assert res.returncode != 0 and (clue is None or clue in res.stderr), res.stderr

def run():
    main = MAIN.read_text()
    ini = INI.read_text()
    hdr = HEADER.read_text()
    assert 'static_assert(!PhysicalIndicators::kOutputsEnabled' in main
    assert 'PhysicalIndicators::kBuzzerDriverPin == -1' in main
    assert 'f12IndicatorPreview.begin(f09SensorAuthorized &&' in main
    assert 'VesselIdentity::ready() && VesselIdentity::nodeId() == 3U' in main
    assert 'hasActiveLocalSOS(), getActiveSOSCount() > 0' in main
    assert 'f10Forecaster.state()' in main
    assert 'GPIO_WRITES=0' in main
    assert 'alertDemoMode' not in main[main.index('const auto f12Result'):main.index('if (f12Result.authorized')]
    assert 'tbeam-supreme-v003-f12-indicator-preview-v003-only' in ini
    assert 'extends = env:tbeam-supreme-v003-f11-macro-five-median-v003-only' in ini
    assert 'THESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN=1' in ini
    for token in ('pinMode(', 'digitalWrite(', 'analogWrite(', 'ledcWrite(',
                  'transmitPacket(', 'transmitSOS(', 'processWeather(', 'readMicroWeatherSample('):
        assert token not in hdr, token
    print('PASS: F12 profile ownership: no hardware, LoRa, SD, history or OLED demo side effects')
    compiler = os.environ.get('CXX') or ('C:/msys64/ucrt64/bin/g++.exe' if os.name == 'nt' else 'g++')
    with tempfile.TemporaryDirectory(prefix='thesis_f12_preview_') as d:
        tmp = Path(d)
        (tmp / 'Arduino.h').write_text(STUB)
        good = ['-DTHESIS_V003_F09_SHARED_SENSOR_BUS=1',
                '-DTHESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN=1',
                '-DTHESIS_V003_F10_MICRO_NOWCAST=1', '-DTHESIS_V003_F10_MICRO_NOWCAST_OPT_IN=1',
                '-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN=1', '-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN_OPT_IN=1',
                '-DTHESIS_V003_F12_INDICATOR_PREVIEW=1',
                '-DTHESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN=1', '-DWEATHER_FAST_TEST=0']
        build_and_run(compiler, tmp, good)
        build_and_run(compiler, tmp,
                      [v for v in good if v != '-DTHESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN=1'],
                      should_work=False, clue='F12 preview requires')
        build_and_run(compiler, tmp,
                      [v for v in good if v != '-DTHESIS_V003_F11_MACRO_FIVE_MEDIAN=1'],
                      should_work=False, clue='F12 preview requires')
        build_and_run(compiler, tmp,
                      [v for v in good if v != '-DWEATHER_FAST_TEST=0']+['-DWEATHER_FAST_TEST=1'],
                      should_work=False, clue='F12 preview requires')
        build_and_run(compiler, tmp, good+['-DTHESIS_V003_F12_INDICATOR_PREVIEW=2'],
                      should_work=False, clue='Invalid F12 indicator preview flags')
        print('PASS: explicit F12 build gates and dependency gates rejected invalid profiles')

if __name__ == '__main__':
    run()
