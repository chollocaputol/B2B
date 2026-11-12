#!/usr/bin/env python3
"""Host-only test for Vessel-003 F08 dry-run commands; NEVER drives GPIO."""
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "firmware/universal/include/v003_f08_dryrun.h"
SOURCE = ROOT / "firmware/universal/src/weather_manager.cpp"
MAIN = ROOT / "firmware/universal/src/main.cpp"
PINS = ROOT / "firmware/universal/include/physical_indicators.h"
INI = ROOT / "firmware/universal/platformio.ini"


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    for path in (HEADER, SOURCE, MAIN, PINS, INI):
        require(path.is_file(), f"missing: {path}")
    header = HEADER.read_text(encoding="utf-8")
    manager = SOURCE.read_text(encoding="utf-8")
    main_cpp = MAIN.read_text(encoding="utf-8")
    pins = PINS.read_text(encoding="utf-8")
    ini = INI.read_text(encoding="utf-8")

    # The dry-run decoder cannot be used as an alternate physical output driver.
    for forbidden in ("pinMode(", "digitalWrite(", "analogWrite(", "ledcWrite(", "tone("):
        require(forbidden not in header, f"unexpected hardware API in dry-run header: {forbidden}")
    require('#if THESIS_V003_F08_INDICATOR_BENCH == 1' in manager,
            "dry-run serial handler must be compile-time limited to bench build")
    require('V003F08DryRun::parse(testLine)' in manager,
            "dry-run serial handler not connected")
    require('!PhysicalIndicators::kOutputsEnabled' in main_cpp,
            "bench build output lock missing")
    require('static constexpr bool kOutputsEnabled = false;' in pins,
            "physical outputs are unexpectedly enabled")
    require('static constexpr int kBuzzerDriverPin = -1;' in pins,
            "hardware outputs are unexpectedly assigned")
    require('THESIS_V003_F08_INDICATOR_BENCH=1' in ini,
            "bench build profile is not defined")

    gpp = shutil.which("g++") or shutil.which("clang++")
    require(gpp, "host C++ compiler not found (g++/clang++)")
    snippet = r"""
#include <cassert>
#include <cstring>
#include "v003_f08_dryrun.h"
using namespace V003F08DryRun;
void expect(const char *command, Request expected, bool y, bool r, bool b) {
  const auto result = parse(command);
  assert(result == expected);
  const auto c = requestedChannels(result);
  assert(c.yellow == y && c.red == r && c.buzzer == b);
}
int main() {
  expect("F08 TEST OFF", Request::Off, false, false, false);
  expect("F08 TEST YELLOW", Request::Yellow, true, false, false);
  expect("F08 TEST RED", Request::Red, false, true, false);
  expect("F08 TEST BUZZER", Request::Buzzer, false, false, true);
  expect("F08 TEST ALL", Request::All, true, true, true);
  expect("F08 TEST HELP", Request::Help, false, false, false);
  expect("F08 TEST UNKNOWN", Request::Invalid, false, false, false);
  expect("TEST ALL", Request::NotF08, false, false, false);
  expect("CANCEL SOS", Request::NotF08, false, false, false);
  expect(nullptr, Request::NotF08, false, false, false);
  assert(std::strcmp(requestName(Request::Yellow), "YELLOW") == 0);
}
"""
    with tempfile.TemporaryDirectory(prefix="thesis_f08_dryrun_") as tmp:
        src = Path(tmp) / "dryrun.cpp"
        exe = Path(tmp) / "dryrun.exe"
        src.write_text(snippet, encoding="utf-8")
        subprocess.run([gpp, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-I", str(HEADER.parent), str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    print("PASS: F08 dry-run requests OFF/YELLOW/RED/BUZZER/ALL/HELP/INVALID")
    print("PASS: independent channel selection; SOS/CANCEL command ownership unchanged")
    print("PASS: no GPIO API in decoder; firmware bench output lock and profile present")


if __name__ == "__main__":
    main()
