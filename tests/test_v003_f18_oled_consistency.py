#!/usr/bin/env python3
"""F18 OLED visual-consistency regression; host-only, no device or SD access."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
assert source.count('// F18 OLED CONSISTENCY:') == 2
assert source.count('u8g2.drawStr(0, 9, "MACRO WX 2/8 EMP");') == 1
assert source.count('u8g2.drawStr(0, 9, "MICRO WX 2/8 EXP");') == 1
assert 'EXPERIMENTAL 10-30m' in source
assert 'NO SAFETY ALL-CLEAR' in source
assert 'FORECAST: DISABLED' in source
assert 'MACRO_REUSED' in source
assert 'F18-MICRO-SD' in source
assert 'if (oledPage == 1 && duration >= BUTTON_LONG_PRESS_MS)' in source
assert 'static_assert(!V003F13Indicators::kPhysicalOutputsEnabled,' in source
assert 'THESIS_V003_F13_LIVE_OUTPUTS != 0' in source
pio = (ROOT / 'firmware/universal/platformio.ini').read_text(encoding='utf-8')
assert '-D THESIS_V003_F13_LIVE_OUTPUTS=0' in pio

# Build representative macro/micro renderers using the actual source snippets.
micro = source.split('// F18 OLED CONSISTENCY: shared six-row 6x10 layout with macro view.\n',1)[1].split('\n#endif\n\nvoid oledWeatherPage()',1)[0]
micro = 'void oledMicroWeatherPage() {' + micro.split('void oledMicroWeatherPage() {',1)[1]
macro = source.split('void oledWeatherPage() {\n#if THESIS_V003_F18_WEATHER_INTERFACE == 1\n',1)[1].split('\n  return;\n#endif\n  u8g2.clearBuffer();',1)[0] + '\n  return;\n}'
macro = 'void oledWeatherPage() {\n' + macro
for fn in (micro, macro):
    for y in (9,19,29,39,49,59):
        assert f'u8g2.drawStr(0, {y},' in fn, (y,fn[:300])
    assert 'u8g2.sendBuffer();' in fn

# Ensure the visible literal rows fit the 128px OLED with a 6px wide glyph.
for fn in (micro, macro):
    for label in re.findall(r'u8g2.drawStr\(0,\s*\d+,\s*"([^"]+)"\)',fn):
        assert len(label) <= 21, (label, len(label))

# Numeric format bounds: extreme F10 physical deltas must still fit 21 chars.
for d_t in (-125.0,0.0,125.0):
    for d_rh in (-100.0,0.0,100.0):
        assert len(f'dT:{d_t:+.1f}C dRH:{d_rh:+.0f}pp') <= 21

# Compile the production render functions with a fake OLED and public-state
# stubs. This catches C++ syntax errors without any PlatformIO or radio access.
mock = r"""
#include <cstdio>
#include <cstring>
#include <math.h>
#include <cstdint>
#include <string>
#include <vector>
#define THESIS_V003_F18_WEATHER_INTERFACE 1
static int u8g2_font_6x10_tf;
struct FakeOLED {
  std::vector<std::pair<int,std::string>> lines;
  void clearBuffer(){lines.clear();}
  void setFont(const int&){}
  void drawStr(int, int y, const char* s){lines.emplace_back(y,s);}
  void sendBuffer(){}
} u8g2;
namespace V003F10Micro {
  enum class State { WARMUP, NO_PATTERN, CANDIDATE, RISK_WATCH, INVALID };
  enum class Pattern { NONE, PRESSURE_RISE, PRESSURE_FALL };
  constexpr unsigned kWindowCount=11;
}
struct ForecastStub {
 V003F10Micro::State state_{V003F10Micro::State::WARMUP};
 V003F10Micro::Pattern pattern_{V003F10Micro::Pattern::NONE};
 unsigned count_{0};
 V003F10Micro::State state() const {return state_;}
 V003F10Micro::Pattern pattern() const {return pattern_;}
 unsigned count() const {return count_;}
 float deltaTemperature() const {return 0.1f;}
 float deltaHumidity() const {return 1.0f;}
 float deltaPressure() const {return 0.12f;}
} f10Forecaster;
bool f18WeatherMicroView = false;
bool weatherSampleAttempted=true, weatherInputsValid=true;
bool dhtReadingValid=true, bmePressureReadingValid=true;
bool tempHumidityReadingValid=true;
unsigned weatherState=0, zambrettiIndex=0;
constexpr unsigned WEATHER_CALIBRATING=0, WEATHER_HISTORY_SIZE=10;
struct { unsigned historyCount=2;} weatherMeasurement;
float temperatureC=31.0f, humidityPercent=75.5f, pressureStationHpa=1009.9f;
float pressureDelta=0.2f;
const char* shortWeatherName(unsigned){return "CALIBRATING";}
"""

with tempfile.TemporaryDirectory() as temp:
    file = Path(temp)/'oled_test.cpp'
    file.write_text(mock + '\n' + micro + '\n' + macro + r"""
int main(){
  oledWeatherPage();
  if(u8g2.lines.size()!=6 || u8g2.lines.at(0).second!="MACRO WX 2/8 EMP") return 1;
  for(auto &p:u8g2.lines) if(p.second.size()>21) return 2;
  f18WeatherMicroView=true;
  oledWeatherPage();
  if(u8g2.lines.size()!=6 || u8g2.lines.at(0).second!="MICRO WX 2/8 EXP") return 3;
  for(auto &p:u8g2.lines) if(p.second.size()>21) return 4;
  f10Forecaster.count_=11;
  f10Forecaster.state_=V003F10Micro::State::RISK_WATCH;
  f10Forecaster.pattern_=V003F10Micro::Pattern::PRESSURE_FALL;
  oledWeatherPage();
  if(u8g2.lines.at(2).second.find("FALL")==std::string::npos) return 5;
  for(auto &p:u8g2.lines) if(p.second.size()>21) return 6;
  return 0;
}
""",encoding='utf-8')
    exe=Path(temp)/'oled_test.exe'
    compile=subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(file),'-o',str(exe)],capture_output=True,text=True)
    assert compile.returncode==0,compile.stderr
    run=subprocess.run([str(exe)],capture_output=True,text=True)
    assert run.returncode==0,f'Mock OLED render failed: {run.returncode}: {run.stderr}'

print('PASS F18 OLED: shared six-line macro/micro layout, <=21 glyph columns')
print('PASS F18 OLED: compiled real render functions, valid warmup and risk states')
print('PASS F18 OLED: OLED-only render changes; run full F17/F18 regressions before flashing')
print('NO FLASH / NO COMMIT / NO PUSH / NO SD ACCESS')
