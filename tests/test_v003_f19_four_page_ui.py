#!/usr/bin/env python3
"""F19R1: exact 4-page OP render boundaries, unknown/freshness semantics."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
src=(ROOT/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
render=src.split('// 128x64 landscape, six rows of 6x10 font.',1)[1].split('\n#endif // F19 renderer',1)[0]
assert 'WEATHER OP 2/4' in render and 'PEERS:%u OP 3/4' in render
assert 'SOS STATUS OP 4/4' in render and 'HOME MENU OP 1/4' in render
assert 'TIME: --:-- NO SYNC' in render and 'gps.date.age() <= 10000UL' in render
assert 'BAT:-- OP1/4' in render and 'H DST: -- (NOT SET)' in render
assert 'CURRENT: DATA VALID' in render and 'OUTLOOK: CALIBRATING' in render
assert 'Z:-- HIST:%u/%u' in render and 'WEATHER GUIDANCE ONLY' in render
assert 'MICRO: WATCH EXP' not in render and 'T:%.1fC RH:' not in render
assert 'SELECT PEER OP 3/4' in render and 'confirmOLEDPeerMenu()' in src
assert 'TinyGPSPlus::courseTo' in render and 'V003F19Display::cardinal8' in render
assert 'gpsHasFix && peer.gpsValid' in render
assert 'gpsHasFix && chosen.positionValid' in render
assert 'DIST: UNKNOWN' in render and 'DIR: UNKNOWN' in render
assert 'GPIO2: SOS CONTROL' in render
# Only fixed literal draw strings need <=21 chars at font width 6 (128px OLED).
for literal in re.findall(r'u8g2\.drawStr\(0,\s*\d+,\s*"([^"]+)"\)',render):
 assert len(literal)<=21,(len(literal),literal)
# Only enum-driven Home menu may select screen-off and Dev mode.
button=src.split('void handleOLEDButton() {',1)[1].split('\n// ============================================================\n// OLED PAGES',1)[0]
assert 'V003F19Display::HomeChoice::ScreenOff' in button
assert 'V003F19Display::HomeChoice::Development' in button
assert 'f19Display.returnFromDevelopment' in button
assert 'f19Display.openHomeMenu' in button
assert 'openOLEDPeerMenu()' in button
assert 'Serial.printf("[F19-UI] OP_PAGE=%u/4' in button
# Compile the ACTUAL extracted F19R1 renderer functions using host C++ stubs.
import tempfile,subprocess,shutil
compiler=shutil.which('g++')
assert compiler,'g++ unavailable for renderer test'
stubs=r"""
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "v003_f19_display_modes.h"
const uint8_t u8g2_font_6x10_tf[]={0};
struct FakeOLED {void clearBuffer(){} void setFont(const uint8_t*){}
 void drawStr(int,int,const char*){} void sendBuffer(){} void setPowerSave(int){} } u8g2;
namespace VesselIdentity {bool ready(){return true;}uint32_t nodeId(){return 3;}}
struct TimeGPS {bool isValid()const{return true;}unsigned long age()const{return 0;}
 unsigned hour()const{return 6;}unsigned minute()const{return 42;}};
struct DateGPS {bool isValid()const{return true;}unsigned long age()const{return 0;}};
struct GPSFake {TimeGPS time;DateGPS date;} gps;
struct TinyGPSPlus {static double distanceBetween(double,double,double,double){return 1000.;}
 static double courseTo(double,double,double,double){return 45.;}};
struct PeerDisplaySnapshot {bool active=false,gpsValid=false;uint32_t id=0;
 double latitude=0,longitude=0;unsigned ageSeconds=0;};
struct SOSRecordSnapshot {bool known=false,cancelled=false,positionValid=false;
 uint32_t originId=0,eventId=0,receivedAtMs=0;double latitude=0,longitude=0;};
bool getPeerSnapshotById(uint32_t,PeerDisplaySnapshot&){return false;}
bool nextActivePeerId(uint32_t,uint32_t&){return false;}
bool getSOSRecordByIndex(size_t,SOSRecordSnapshot&){return false;}
bool hasActiveLocalSOS(){return false;}
unsigned getActiveSOSCount(){return 0;}
unsigned long millis(){return 0;}
void formatUptime(char* out,size_t count){snprintf(out,count,"0d 00:01:00");}
const char* shortWeatherName(int){return "CALIBRATING";}
const unsigned WEATHER_HISTORY_SIZE=10;
const int WEATHER_CALIBRATING=0;
int weatherState=0;
struct WeatherMeasure {unsigned historyCount=1;} weatherMeasurement;
bool weatherSampleAttempted=false,weatherInputsValid=false,gpsHasFix=false;
unsigned long lastWeatherTime=0;
unsigned zambrettiIndex=0;
double latitude=0,longitude=0;
V003F19Display::State f19Display;
bool oledPeerMenuOpen=false;
uint8_t oledMenuPeerCount=0,oledMenuCursor=0;
uint32_t oledMenuPeerIds[10]{};
uint32_t selectedOLEDPeerId=0;
const size_t kSOSMenuCapacity=10;
"""
with tempfile.TemporaryDirectory(prefix='thesis_f19r1_render_') as td:
 d=Path(td)
 (d/'v003_f19_display_modes.h').write_bytes(
     (ROOT/'firmware/universal/include/v003_f19_display_modes.h').read_bytes())
 body=stubs+'\n// 128x64 landscape, six rows of 6x10 font.'+render+'\nint main(){oledF19GPSPage();oledF19WeatherPage();oledF19NearbyPage();oledF19SOSPage();return 0;}\n'
 (d/'test.cpp').write_text(body,encoding='utf-8')
 exe=d/'run.exe'
 p=subprocess.run([compiler,'-std=c++17','-DTHESIS_V003_F19_DUAL_DISPLAY=1',
     '-Wall','-Wextra','-Werror',str(d/'test.cpp'),'-o',str(exe)],
     capture_output=True,text=True)
 assert p.returncode==0,p.stderr
 p=subprocess.run([str(exe)],capture_output=True,text=True)
 assert p.returncode==0,(p.returncode,p.stderr)
print('PASS F19R1: real four-page C++ renderer functions compile and execute in stub harness')
print('PASS F19R1: four-page OP renderer/menu contracts, safe unknown/stale labels')
print('PASS F19R1: no raw sensor values in OP weather; macro outlook separately qualified')
