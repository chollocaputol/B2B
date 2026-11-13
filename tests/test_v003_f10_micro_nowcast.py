"""F10 host-only nowcast policy and firmware isolation checks (no device access).

Run from repository root: python tests/test_v003_f10_micro_nowcast.py
Requires g++ on PATH; MSYS2 UCRT64 g++ works on Windows.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / "firmware/universal/include"
MAIN = ROOT / "firmware/universal/src/main.cpp"
WEATHER_H = INC / "weather_manager.h"
WEATHER_CPP = ROOT / "firmware/universal/src/weather_manager.cpp"
INI = ROOT / "firmware/universal/platformio.ini"

CODE = r'''
#include "v003_f10_micro_nowcast.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
using namespace V003F10Micro;

int main() {
  static_assert(kIntervalMs == 60000U && kWindowCount == 11, "10-minute / 1-minute contract");
  assert(std::isnan(dewPointDepression(30.0f, 0.0f)));
  assert(std::fabs(dewPointDepression(30.0f, 70.0f) - 6.0f) < 1.0f);

  // Micro + macro coincide at 10 min: use the fresh macro sample, not a
  // duplicate physical read. Reuse is bounded, including millis() rollover.
  assert(shouldReuseMacroSample(600500U, true, 600100U));
  assert(shouldReuseMacroSample(604999U, true, 600000U));
  assert(!shouldReuseMacroSample(605001U, true, 600000U));
  assert(!shouldReuseMacroSample(600500U, false, 600100U));
  assert(!shouldReuseMacroSample(660000U, true, 600000U));
  assert(shouldReuseMacroSample(0x000000F0U, true, 0xFFFFF100U));

  Forecaster sharedBoundary;
  for (int i = 1; i <= 9; ++i)
    assert(sharedBoundary.ingest(uint32_t(i)*60000U, 30.0f, 70.0f, 1010.0f));
  // At 600s the macro's valid measurement supplies the tenth minute point.
  assert(shouldReuseMacroSample(600250U, true, 600100U));
  assert(sharedBoundary.ingest(600250U, 30.0f, 70.0f, 1010.0f));
  assert(sharedBoundary.count() == 10 && sharedBoundary.state() == State::WARMUP);
  // No history reset: the next direct micro sample completes the full window.
  assert(sharedBoundary.ingest(660250U, 30.0f, 70.0f, 1010.0f));
  assert(sharedBoundary.count() == 11 && sharedBoundary.state() == State::NO_PATTERN);
  std::puts("PASS: fresh macro sample reuse avoids 10-minute double-read reset");

  Forecaster normal;
  for (int i = 0; i < 11; ++i) {
    assert(normal.ingest(uint32_t(i)*60000U, 30.0f, 70.0f, 1010.0f));
    assert(normal.count() == (i < 10 ? i+1 : 11));
    assert(normal.state() == (i < 10 ? State::WARMUP : State::NO_PATTERN));
  }
  assert(normal.pattern() == Pattern::NONE);

  Forecaster rainSignal;
  for (int i = 0; i < 12; ++i) {
    assert(rainSignal.ingest(uint32_t(i)*60000U,
             30.0f - i*0.15f, 65.0f + i*0.9f, 1010.0f + i*0.14f));
    if (i == 10) assert(rainSignal.state() == State::CANDIDATE);
    if (i == 11) assert(rainSignal.state() == State::RISK_WATCH);
  }
  assert(rainSignal.pattern() == Pattern::PRESSURE_RISE);
  assert(rainSignal.deltaTemperature() <= -1.0f);
  assert(rainSignal.deltaHumidity() >= 6.0f);
  assert(rainSignal.deltaPressure() >= 1.2f);
  assert(rainSignal.dewPointGap() > 0.0f);

  // A cooling/humidifying trend by itself is not enough for a watch.
  Forecaster noPressure;
  for (int i=0; i<12; ++i) {
    assert(noPressure.ingest(uint32_t(i)*60000U,
           30.0f-i*0.15f, 65.0f+i*0.9f, 1010.0f));
  }
  assert(noPressure.state() == State::NO_PATTERN);

  // Both pressure directions are explicit, without claiming an event diagnosis.
  Forecaster falling;
  for (int i=0; i<12; ++i) {
    assert(falling.ingest(uint32_t(i)*60000U,
           30.0f-i*0.15f, 65.0f+i*0.9f, 1010.0f-i*0.14f));
  }
  assert(falling.state() == State::RISK_WATCH);
  assert(falling.pattern() == Pattern::PRESSURE_FALL);

  // Faults reset history and clear every pending or active watch.
  assert(!rainSignal.ingest(12U*60000U, 29.0f, NAN, 1011.0f));
  assert(rainSignal.state() == State::INVALID && rainSignal.count() == 0);
  assert(rainSignal.ingest(13U*60000U, 28.0f, 80.0f, 1012.0f));
  assert(rainSignal.state() == State::WARMUP && rainSignal.count() == 1);
  assert(!rainSignal.ingest(14U*60000U, 28.0f, 80.0f, 1020.0f));
  assert(rainSignal.state() == State::INVALID && rainSignal.count() == 0);

  // Missed acquisition resets comparability; premature ticks are ignored.
  Forecaster gaps;
  assert(gaps.ingest(100000U, 29.0f, 70.0f, 1010.0f));
  assert(!gaps.ingest(120000U, 29.0f, 70.0f, 1010.0f));
  assert(gaps.count() == 1);
  assert(gaps.ingest(300000U, 29.0f, 70.0f, 1010.0f));
  assert(gaps.count() == 1 && gaps.state() == State::WARMUP);

  // Arithmetic remains valid across the 32-bit millis() rollover.
  Forecaster rollover;
  for (int i=0; i<12; ++i) {
    uint32_t now = uint32_t(0xFFF90000ULL + uint64_t(i)*60000ULL);
    assert(rollover.ingest(now, 30.0f-i*0.15f,
           65.0f+i*0.9f, 1010.0f+i*0.14f));
  }
  assert(rollover.state() == State::RISK_WATCH);
  std::puts("PASS: F10 1min/10min window, warmup, rise/fall signatures, persistence");
  std::puts("PASS: no single-variable watch, sensor fault, pressure spike, gap, rollover");
  return 0;
}
'''


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run():
    compiler = shutil.which("g++")
    require(compiler is not None, "g++ missing; add C:\\msys64\\ucrt64\\bin to PATH")
    flags = ["-std=c++17", "-Wall", "-Wextra", "-pedantic", "-I", str(INC)]
    with tempfile.TemporaryDirectory(prefix="thesis_f10_") as td:
        tmp = Path(td)
        program = tmp / "nowcast.cpp"
        exe = tmp / "nowcast.exe"
        program.write_text(CODE, encoding="utf-8")
        valid = subprocess.run([compiler, *flags,
            "-DTHESIS_V003_F09_SHARED_SENSOR_BUS=1",
            "-DTHESIS_V003_F10_MICRO_NOWCAST=1",
            "-DTHESIS_V003_F10_MICRO_NOWCAST_OPT_IN=1",
            str(program), "-o", str(exe)], capture_output=True, text=True)
        require(valid.returncode == 0, "F10 host compile failed: " + valid.stderr)
        execution = subprocess.run([str(exe)], capture_output=True, text=True)
        require(execution.returncode == 0, "F10 host assertions failed: " + execution.stderr)
        print(execution.stdout, end="")
        for bad in (
            ["-DTHESIS_V003_F10_MICRO_NOWCAST=1", "-DTHESIS_V003_F09_SHARED_SENSOR_BUS=1"],
            ["-DTHESIS_V003_F10_MICRO_NOWCAST=1", "-DTHESIS_V003_F10_MICRO_NOWCAST_OPT_IN=1"],
            ["-DTHESIS_V003_F10_MICRO_NOWCAST=2"],
        ):
            trial = subprocess.run([compiler, *flags, *bad, "-fsyntax-only", str(program)],
                                   capture_output=True, text=True)
            require(trial.returncode != 0, "Invalid F10 build gate accepted: " + " ".join(bad))
        print("PASS: compile-time F09 dependency, F10 opt-in and invalid flag gates")

    main = MAIN.read_text(encoding="utf-8")
    wx = WEATHER_CPP.read_text(encoding="utf-8")
    wh = WEATHER_H.read_text(encoding="utf-8")
    ini = INI.read_text(encoding="utf-8")
    require("if (f09SensorAuthorized) {\n      processWeather();" in main,
            "F09 identity-gated macro sampling modified")
    require("if (f09SensorAuthorized &&" in main and "readMicroWeatherSample(sample)" in main,
            "F10 acquisition must be identity-gated")
    require("[F10-MICRO]" in main and "f10Forecaster.ingest" in main,
            "F10 serial diagnostics missing")
    require("V003F10Micro::shouldReuseMacroSample(" in main and
            "weatherSampleAttempted, weatherMeasurement.uptimeMs" in main and
            "weatherMeasurement.inputsOK" in main and
            "weatherMeasurement.selectedTempC" in main and
            "weatherMeasurement.selectedRH" in main and
            "weatherMeasurement.stationPressureHpa" in main and
            "source=%s" in main and "MACRO_REUSED" in main,
            "F10 must reuse fresh macro sensor samples at colliding sample times")
    require("#if THESIS_V003_F10_MICRO_NOWCAST == 1" in wx and
            "switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);" in wx,
            "F10 should restore OLED bus after sensor reads")
    require("bool readMicroWeatherSample(MicroWeatherSample &sample)" in wh + wx,
            "Dedicated micro sensor read contract missing")
    require("[env:tbeam-supreme-v003-f10-micro-nowcast-v003-only]" in ini and
            "-D WEATHER_FAST_TEST=0" in ini.split(
              "[env:tbeam-supreme-v003-f10-micro-nowcast-v003-only]", 1)[1],
            "F10 must preserve genuine ten-minute macro sampling")
    require("f10Forecaster.state()" in main and
            "PhysicalIndicators::update(millis(), weatherState, weatherInputsValid" in main,
            "F10 must not drive physical indicators from an experimental nowcast")
    print("PASS: F09 macro unchanged; F10 sensors isolated; OLED restored; no actuator/radio ownership")

if __name__ == "__main__":
    run()
