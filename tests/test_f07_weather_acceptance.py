"""Run production weather-cycle and OLED code with host-only sensor/display doubles.
Requires Windows MSYS2 UCRT64 g++ (or CXX); never opens hardware or serial ports.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WX = ROOT / "firmware/universal/src/weather_manager.cpp"
MAIN = ROOT / "firmware/universal/src/main.cpp"
HEADER = ROOT / "firmware/universal/include/weather_manager.h"
STUBS = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using std::isfinite;
unsigned long millis() { static unsigned long t = 0; return ++t; }
struct SerialDouble {
  template<class... A> void printf(const char*, A...) {}
  template<class T> void print(T) {}
  template<class T> void println(T) {}
  int available() { return 0; }
  int read() { return -1; }
} Serial;
struct WireDouble {
  void beginTransmission(int) {}
  int endTransmission() { return 0; }
  void setClock(int) {}
} Wire;
void switchI2C(uint8_t, uint8_t) {}
void cancelActiveSOS() {}
#define BME680_OS_8X 8
#define BME680_OS_2X 2
#define BME680_OS_4X 4
#define BME680_FILTER_SIZE_3 3
#define DHT20_OK 0
struct Adafruit_BME680 {
  bool ok = true;
  float temperature = 30, humidity = 70, pressure = 101000;
  bool begin(int) { return true; }
  bool performReading() { return ok; }
  void setTemperatureOversampling(int) {}
  void setHumidityOversampling(int) {}
  void setPressureOversampling(int) {}
  void setIIRFilterSize(int) {}
};
struct DHT20 {
  bool connected = true;
  int status = 0;
  void begin() {}
  bool isConnected() { return connected; }
  int read() { return status; }
  float getTemperature() { return 30; }
  float getHumidity() { return 70; }
};
#define OLED_PAGE_TOTAL "8"
const int u8g2_font_6x10_tf = 0;
struct DisplayDouble {
  std::vector<std::string> lines;
  void clearBuffer() { lines.clear(); }
  void setFont(int) {}
  void drawStr(int, int, const char* s) { lines.emplace_back(s); }
  void sendBuffer() {}
} u8g2;
'''
CASES = r'''
void resetFixture() {
  resetWeatherCalibration("test");
  lastAcceptedStationPressure = NAN;
  pendingStationPressure = NAN;
  bmeConsecutiveFailures = dhtConsecutiveFailures = 0;
  bmeAvailable = dhtAvailable = true;
  bme.ok = dht.connected = true;
  dht.status = DHT20_OK;
}
void sample(float pressure) {
  bme.pressure = pressure * 100.0f;
  processWeather();
  oledWeatherPage();
}
void calibrated() {
  resetFixture();
  for (int i = 0; i < 11; ++i) sample(1010);
  assert(weatherState == WEATHER_NORMAL);
}
void missingPressure() {
  assert(!bmePressureReadingValid && !weatherInputsValid);
  assert(std::isnan(pressureStationHpa) && std::isnan(pressureMSL));
  assert(std::isnan(weatherMeasurement.stationPressureHpa));
  assert(historyCount == 0 && weatherState == WEATHER_CALIBRATING);
  assert(u8g2.lines[0].find("SENSOR FAIL") != std::string::npos);
  assert(u8g2.lines[2] == "P:-- hPa (BME FAIL)");
  assert(u8g2.lines[5] == "FORECAST: DISABLED");
}
int main() {
  resetFixture();
  for (int i = 1; i <= 10; ++i) {
    sample(1010);
    assert(weatherState == WEATHER_CALIBRATING);
    assert(historyCount == i);
    assert(u8g2.lines[5] == "CALIBRATING " + std::to_string(i) + "/10");
  }
  assert(std::isnan(weatherMeasurement.pressureDeltaHpa));
  sample(1010);
  assert(weatherState == WEATHER_NORMAL);
  assert(u8g2.lines[5].find("Z:") == 0);
  std::cout << "PASS: samples 1-10 calibrating; sample 11 classified; OLED agrees\n";

  calibrated();
  sample(1030); missingPressure();
  assert(lastAcceptedStationPressure == 1010);
  sample(1010);
  assert(bmePressureReadingValid && pressureStationHpa == 1010);
  assert(historyCount == 1 && weatherState == WEATHER_CALIBRATING);
  assert(std::isnan(pendingStationPressure));
  std::cout << "PASS: isolated spike rejected; baseline returns with fresh history\n";

  for (float shifted : {1030.0f, 990.0f}) {
    calibrated();
    sample(shifted); missingPressure();
    sample(shifted + 1);
    assert(bmePressureReadingValid && pressureStationHpa == shifted + 1);
    assert(historyCount == 1 && pressureHistory[0] == shifted + 1);
    assert(weatherState == WEATHER_CALIBRATING);
    for (int i = 2; i <= 10; ++i) {
      sample(shifted + 1);
      assert(historyCount == i && weatherState == WEATHER_CALIBRATING);
    }
    sample(shifted + 1);
    assert(weatherState == WEATHER_NORMAL);
  }
  std::cout << "PASS: sustained shifts recover both directions; rejected values excluded; full recalibration\n";

  calibrated(); sample(1030); missingPressure();
  bme.ok = false; sample(1030); missingPressure();
  assert(std::isnan(pendingStationPressure));
  bme.ok = true; sample(1030); missingPressure();
  sample(1031); assert(bmePressureReadingValid && historyCount == 1);
  std::cout << "PASS: failed read breaks consecutive recovery confirmation\n";

  for (float invalid : {869.0f, 1086.0f, NAN, INFINITY}) {
    calibrated(); sample(1030); missingPressure();
    sample(invalid); missingPressure();
    assert(std::isnan(pendingStationPressure));
    sample(1030); missingPressure();
    sample(1031); assert(bmePressureReadingValid && historyCount == 1);
  }
  calibrated(); sample(1030); sample(990); missingPressure();
  sample(1010); assert(bmePressureReadingValid && historyCount == 1);
  std::cout << "PASS: implausible/nonfinite and inconsistent spikes rejected\n";

  calibrated(); sample(1018); assert(bmePressureReadingValid);
  sample(1026.1f); missingPressure();
  std::cout << "PASS: existing 8 hPa boundary preserved\n";

  calibrated(); dht.connected = false; sample(1010);
  assert(!tempHumidityReadingValid && !weatherInputsValid);
  assert(std::isnan(temperatureC) && std::isnan(humidityPercent));
  assert(std::isnan(weatherMeasurement.selectedTempC));
  assert(weatherMeasurement.bmeTHOK && weatherMeasurement.tempSource == 0);
  assert(u8g2.lines[1] == "T:--.-C RH:--.-%");
  assert(u8g2.lines[4] == "DHT20: READ FAIL");
  assert(u8g2.lines[5] == "FORECAST: DISABLED");
  bme.ok = false; sample(1010); missingPressure();
  assert(u8g2.lines[4] == "DHT+BME: READ FAIL");
  dht.connected = true; bme.ok = true; sample(1010);
  assert(weatherInputsValid && historyCount == 1);
  calibrated(); dht.status = -1; sample(1010);
  assert(!tempHumidityReadingValid && std::isnan(temperatureC));
  assert(u8g2.lines[1] == "T:--.-C RH:--.-%");
  std::cout << "PASS: DHT failure, BME failure, both failed, recovery; no T/RH fallback\n";
}
'''

def run():
    header = HEADER.read_text().replace("#pragma once", "").replace("#include <Arduino.h>", "")
    weather = re.sub(r"^#include.*$", "", WX.read_text(), flags=re.MULTILINE)
    main = MAIN.read_text()
    start = main.index("void oledWeatherPage() {")
    end = main.index("\nvoid oledLinkPage()", start)
    oled = main[start:end]
    compiler = os.environ.get("CXX", r"C:\msys64\ucrt64\bin\g++.exe")
    with tempfile.TemporaryDirectory(prefix="thesis_f07_") as temp:
        cpp = Path(temp) / "acceptance.cpp"
        exe = Path(temp) / "acceptance.exe"
        cpp.write_text("\n".join((STUBS, header, weather, oled, CASES)))
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(exe)], check=True, env=env)
        subprocess.run([str(exe)], check=True, env=env)

if __name__ == "__main__":
    run()
