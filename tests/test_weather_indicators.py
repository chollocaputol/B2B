"""Host tests: production policy/disabled adapter/OLED demo and live-state call.
Never opens serial ports or hardware. Requires CXX or Windows MSYS2 UCRT64 g++.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ARDUINO = r"""
#pragma once
#include <cstdint>
#define LOW 0
#define HIGH 1
#define OUTPUT 1
extern int gpioWrites, gpioModes;
inline void digitalWrite(int, int) { ++gpioWrites; }
inline void pinMode(int, int) { ++gpioModes; }
"""
PREFIX = r"""
#include <cassert>
#include <cstdio>
#include <iostream>
#include "physical_indicators.h"
#include "communication/multi_peer_sos.h"
int gpioWrites = 0, gpioModes = 0;
uint8_t weatherState = WEATHER_NORMAL;
bool weatherInputsValid = true;
bool localSOS = false;
MultiPeerSOS::Table received;
uint32_t nowMs = 0;
uint32_t millis() { return nowMs; }
bool hasActiveLocalSOS() { return localSOS; }
size_t getActiveSOSCount() { return received.activeCount(); }
uint8_t alertDemoMode = 0;
const char* shortWeatherName(uint8_t) { return "test"; }
#define OLED_PAGE_TOTAL "8"
const int u8g2_font_6x10_tf = 0;
struct Display {
  void clearBuffer() {}
  void setFont(int) {}
  void drawStr(int, int, const char*) {}
  void sendBuffer() {}
} u8g2;
"""
CASES = r"""
using WeatherIndicators::Mode;
using WeatherIndicators::Policy;
void expect(WeatherIndicators::Outputs o, bool y, bool r, bool b) {
  assert(o.yellow == y && o.red == r && o.buzzer == b);
}
int main() {
  const uint8_t states[] = {WEATHER_CALIBRATING, WEATHER_NORMAL,
                            WEATHER_FRONT, WEATHER_SQUALL, 99};
  for (auto state : states) for (bool valid : {false, true}) {
    Policy p;
    for (uint32_t t = 0; t < 4000; ++t) {
      const auto o = p.update(t, state, valid, false, false);
      if (valid && state == WEATHER_FRONT) {
        const auto phase = t % 2000;
        expect(o, phase < 150 || (phase >= 300 && phase < 450), false, false);
      } else if (valid && state == WEATHER_SQUALL) {
        expect(o, false, t % 1000 < 500, t % 1000 < 500);
      } else expect(o, false, false, false);
    }
    for (bool local : {false, true}) for (bool remote : {false, true}) {
      if (!local && !remote) continue;
      Policy sos;
      for (uint32_t t = 0; t < 4000; ++t) {
        const auto o = sos.update(t, state, valid, local, remote);
        const auto phase = t % 2000;
        expect(o, false, t % 200 < 100, phase < 600 && phase % 200 < 100);
        assert(sos.mode() == Mode::SOS);
      }
    }
  }
  std::cout << "PASS: every weather/validity state; front double blink; squall warning; local/received/both SOS priority and distinct cadence\n";

  for (auto state : states) for (bool valid : {false, true}) {
    Policy p;
    p.update(1, WEATHER_FRONT, true, true, true);
    p.update(2, state, valid, false, true);
    assert(p.mode() == Mode::SOS);
    const auto o = p.update(3, state, valid, false, false);
    const Mode expected = !valid ? Mode::Off : state == WEATHER_FRONT ? Mode::Front :
        state == WEATHER_SQUALL ? Mode::Squall : Mode::Off;
    assert(p.mode() == expected);
    expect(o, expected == Mode::Front, expected == Mode::Squall, expected == Mode::Squall);
  }
  std::cout << "PASS: clearing SOS returns to every current weather/validity state\n";

  for (int pin : {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                  17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 45, 46, 47, 48, 49})
    assert(!PhysicalIndicators::unreservedPin(pin));
  PhysicalIndicators::begin();
  weatherState = WEATHER_FRONT;
  tick(); assert(PhysicalIndicators::policy.mode() == Mode::Front);
  localSOS = true; tick(); assert(PhysicalIndicators::policy.mode() == Mode::SOS);
  received.receiveSOS(1, 10, nowMs);
  localSOS = false; tick(); assert(PhysicalIndicators::policy.mode() == Mode::SOS);
  nowMs = 60001; tick(); assert(PhysicalIndicators::policy.mode() == Mode::SOS);
  received.receiveSOS(2, 20, nowMs);
  received.cancel(1, 9, nowMs); tick(); assert(PhysicalIndicators::policy.mode() == Mode::SOS);
  received.cancel(1, 10, nowMs); tick(); assert(PhysicalIndicators::policy.mode() == Mode::SOS);
  weatherState = WEATHER_SQUALL;
  received.cancel(2, 20, nowMs); tick(); assert(PhysicalIndicators::policy.mode() == Mode::Squall);
  received.receiveSOS(2, 20, nowMs); tick(); // cancelled duplicate stays cancelled
  assert(PhysicalIndicators::policy.mode() == Mode::Squall);
  std::cout << "PASS: production live-state call; received persistence, mismatched/multiple cancellations and cancelled duplicate\n";

  for (auto state : states) for (bool valid : {false, true}) {
    weatherState = state; weatherInputsValid = valid;
    for (alertDemoMode = 0; alertDemoMode <= 3; ++alertDemoMode) {
      tick();
      const auto before = PhysicalIndicators::policy.mode();
      oledAlertPage(); // actual production display function
      assert(weatherState == state && weatherInputsValid == valid);
      tick(); assert(PhysicalIndicators::policy.mode() == before);
      assert(gpioWrites == 0 && gpioModes == 0);
    }
  }
  localSOS = true;
  for (nowMs = 0; nowMs < 10000; ++nowMs) tick();
  assert(gpioWrites == 0 && gpioModes == 0);
  static_assert(!PhysicalIndicators::kOutputsEnabled);
  static_assert(!PhysicalIndicators::kWiringAndDriverReviewed);
  static_assert(PhysicalIndicators::kYellowPin == -1 && PhysicalIndicators::kRedPin == -1 &&
                PhysicalIndicators::kBuzzerDriverPin == -1);
  std::cout << "PASS: actual OLED demos isolated; disabled adapter performs zero pinMode/digitalWrite calls\n";

  Policy rollover;
  const uint32_t start = UINT32_MAX - 50;
  expect(rollover.update(start, WEATHER_FRONT, true, false, false), true, false, false);
  expect(rollover.update(start + 150U, WEATHER_FRONT, true, false, false), false, false, false);
  expect(rollover.update(start + 300U, WEATHER_FRONT, true, false, false), true, false, false);
  expect(rollover.update(start + 450U, WEATHER_FRONT, true, false, false), false, false, false);
  expect(rollover.update(start + 451U, WEATHER_FRONT, true, true, false), false, true, true);
  expect(rollover.update(start + 551U, WEATHER_FRONT, true, true, false), false, false, false);
  std::cout << "PASS: millis rollover and immediate SOS preemption\n";
}
"""

def run():
    main = (ROOT / "firmware/universal/src/main.cpp").read_text()
    call = re.search(r"  PhysicalIndicators::update\(.*?;", main, re.S)
    assert call and call.group().count("alertDemoMode") == 0
    assert "hasActiveLocalSOS(), getActiveSOSCount() > 0" in call.group()
    assert "SOSControls::updateIndicators();" not in main
    start = main.index("void oledAlertPage() {")
    end = main.index("\n}", start) + 2
    source = PREFIX + "\nvoid tick() {\n" + call.group() + "\n}\n" + main[start:end] + CASES
    compiler = os.environ.get("CXX", r"C:\msys64\ucrt64\bin\g++.exe")
    env = dict(os.environ)
    env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="thesis_indicators_") as tmp:
        temp = Path(tmp)
        (temp / "Arduino.h").write_text(ARDUINO)
        cpp, exe = temp / "indicators.cpp", temp / "indicators.exe"
        cpp.write_text(source)
        command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(temp),
                   "-I", str(ROOT / "firmware/universal/include"),
                   "-I", str(ROOT / "firmware/shared"), str(cpp), "-o", str(exe)]
        subprocess.run(command, env=env, check=True)
        subprocess.run([str(exe)], env=env, check=True)
        rejected = subprocess.run(command + ["-DSOS_LED_GPIO=1"], env=env, capture_output=True, text=True)
        assert rejected.returncode != 0 and "Leave legacy SOS output pins disabled" in rejected.stderr
        print("PASS: legacy output ownership conflict rejected at compile time")

if __name__ == "__main__":
    run()
