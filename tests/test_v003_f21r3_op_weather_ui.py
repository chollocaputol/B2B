#!/usr/bin/env python3
"""F21R3: all 25 combinations, wrap-safe freshness, real C++ icon renderers.
NO devices, radio, SOS, GPIO, SD, or flash are exercised.
"""
from pathlib import Path
import shutil, subprocess, tempfile
R=Path(__file__).resolve().parents[1]
m=(R/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
h=R/'firmware/universal/include/v003_f19_display_modes.h'
w=R/'firmware/universal/include/v003_f21r3_weather_display.h'
r4=R/'firmware/universal/include/v003_f21r4_ui_policy.h'
assert h.exists() and w.exists() and r4.exists()
assert 'U8G2_SH1106_128X64_NONAME_F_HW_I2C' in m
assert 'u8g2.setDisplayRotation(f21WantPortrait?U8G2_R1:U8G2_R0)' in m
assert 'const bool f21WantPortrait=(f19Display.operational() || f19SOS) && f19Display.portrait()' in m
assert 'if (f19SOS && f21r42SOSBrowse.forceSOS())' in m and m.index('if (f19SOS && f21r42SOSBrowse.forceSOS())') < m.index('if (f19Display.operational() || f19SOS) {',m.index('void updateOLED() {'))
assert 'V003F19Display::HomeChoice::Portrait' in m and 'V003F19Display::HomeChoice::Landscape' in m
assert 'ThesisSDLogger::hybridSetContinuous(sdHybridMenuCursor == 1)' in m
assert 'V003F21WeatherDisplay::select(x)' in m
assert 'x.microState=static_cast<uint8_t>(f10Forecaster.state())' in m
assert 'x.macroPressureFinite=isfinite(pressureDelta)' in m
assert 'gpsHasFix && peer.gpsValid' in m and 'gpsHasFix && chosen.positionValid' in m
for name in ('GPS','Weather','Nearby','SOS'):
 for orient in ('Portrait','Landscape'):
  assert 'oledF21'+orient+name+'Page();' in m,orient+name
p=m.split('// F21R3: OLED-only icon/phone-style OP presentation.',1)[1].split('#endif // F21R3 icon renderers',1)[0]
for unwanted in ('1/4','2/4','3/4','4/4','ZAMBRETTI','Z:%','12 HR','NEXT 12H','87%',
                 'RAIN','SUNNY','CLEAR SKY'):
 assert unwanted not in p,unwanted
for side_effect in ('ThesisSDLogger::','transmitSOS(', 'cancelActiveSOS(', 'digitalWrite(',
     'pinMode(', 'transmitPacket(', 'sendPacket(', 'GPSService::poll(',
     'processWeather(', 'readMicroWeatherSample(', 'preferences.put'):
 assert side_effect not in p,side_effect
for need in ('F21Icon::Boat','F21Icon::Battery','F21Icon::Pin','F21Icon::Clock','F21Icon::Home',
             'F21Icon::Peers','F21Icon::Cloud','F21Icon::Radio','F21Icon::Alert',
             '"10-30m EXP"'):
 # horizon is defined in policy header, not renderer fragment
 assert need in (p+w.read_text(encoding='utf-8')),need
assert 'oledF21PortraitWeatherPage' in p and 'oledF21LandscapeWeatherPage' in p
assert 'x.macroLastStartMs=lastWeatherTime' in p
assert 'x.microLastMs=lastF10MicroTime' in p
compiler=shutil.which('g++')
assert compiler, 'STOP: g++ required for real host C++ regression'

weather_matrix = r'''
#include <stdint.h>
#include <string.h>
#include "v003_f21r3_weather_display.h"
using namespace V003F21WeatherDisplay;
int main(){
 Input x{};x.nowMs=1000000;x.macroAttempted=true;x.macroInputsValid=true;
 x.macroLastStartMs=990000;x.macroHistoryCount=10;x.macroPressureFinite=true;
 x.microLastMs=999000;x.microHistoryCount=11;
 const Macro expectedMacro[5]={Macro::Steady,Macro::Front,Macro::Squall,Macro::Calibrating,Macro::Stale};
 const Outlook matrix[5][5]={
  {Outlook::Baseline,Outlook::NoPattern,Outlook::Developing,Outlook::RiskWatch,Outlook::Unavailable},
  {Outlook::FrontSignal,Outlook::FrontSignal,Outlook::Developing,Outlook::RiskWatch,Outlook::FrontSignal},
  {Outlook::SquallSignal,Outlook::SquallSignal,Outlook::Developing,Outlook::RiskWatch,Outlook::SquallSignal},
  {Outlook::Baseline,Outlook::NoPattern,Outlook::Developing,Outlook::RiskWatch,Outlook::Unavailable},
  {Outlook::Unavailable,Outlook::NoPattern,Outlook::Developing,Outlook::RiskWatch,Outlook::Unavailable}
 };
 for(unsigned macro=0;macro<5;++macro){
  x.macroState=(macro==0?1:macro==1?2:macro==2?3:0);
  x.macroLastStartMs=macro==4?100000:990000;
  for(unsigned micro=0;micro<5;++micro){
   x.microState=(uint8_t)micro;
   x.microHistoryCount=micro==0?4:11;
   View v=select(x);
   if(v.current!=expectedMacro[macro])return 10+macro;
   if(v.outlook!=matrix[macro][micro])return 20+macro*5+micro;
   if(v.macroDataGap!=(macro==4))return 45+macro;
   if(v.horizon==(Horizon::Observed100MinTrend) && macro!=1 && macro!=2)return 55;
   if(v.horizon==Horizon::Observed100MinTrend && micro>=2 && micro<=3)return 56;
  }
 }
 x.macroLastStartMs=990000;x.macroState=1;x.microState=1;x.microHistoryCount=11;
 x.macroInputsValid=false;
 if(select(x).current!=Macro::SensorError||!select(x).macroDataGap||
    select(x).outlook!=Outlook::NoPattern)return 60;
 x.macroInputsValid=true;x.macroAttempted=false;
 if(select(x).current!=Macro::Waiting)return 61;
 x.macroAttempted=true;x.macroState=3;x.microLastMs=100;
 if(select(x).outlook!=Outlook::SquallSignal)return 62;
 x.microLastMs=999000;x.microState=4;
 if(select(x).outlook!=Outlook::SquallSignal)return 63;
 // Validate uint32 wrap-around age calculation on time stamps.
 x.nowMs=4000;x.macroLastStartMs=0xFFFFFC00U;x.microLastMs=0xFFFFFC00U;
 x.macroState=1;x.microState=1;x.microHistoryCount=11;
 if(select(x).current!=Macro::Steady||select(x).micro!=Micro::NoPattern)return 64;
 if(strcmp(currentText(Macro::Front),"FRONT")||
    strcmp(outlookText(Outlook::RiskWatch),"RISK WATCH")||
    strcmp(horizonText(Horizon::Observed100MinTrend),"100m TREND"))return 65;
 return 0;
}
'''
policy = r'''
#include "v003_f19_display_modes.h"
using namespace V003F19Display;
int main(){
 State s;
 if(!s.operational()||s.portrait()||s.orientation()!=Orientation::Landscape)return 1;
 if(!s.openHomeMenu(950,false))return 2;
 if(!s.moveHomeCursor(100,false)||!s.moveHomeCursor(100,false))return 3;
 if(s.homeMenuCursor()!=2||s.confirmHomeMenu(950,false)!=HomeChoice::Portrait)return 4;
 if(!s.portrait())return 5;
 if(!s.openHomeMenu(950,false))return 6;
 for(int i=0;i<3;++i)if(!s.moveHomeCursor(100,false))return 7;
 if(s.confirmHomeMenu(950,false)!=HomeChoice::Landscape||s.portrait())return 8;
 if(!s.openHomeMenu(950,false))return 9;
 if(s.confirmHomeMenu(950,true)!=HomeChoice::None)return 10;
 for(int i=0;i<4;++i)if(!s.moveHomeCursor(100,false))return 11;
 if(s.confirmHomeMenu(950,false)!=HomeChoice::Back)return 12;
 if(!s.openHomeMenu(950,false))return 13;
 if(!s.moveHomeCursor(100,false))return 14;
 if(s.confirmHomeMenu(950,false)!=HomeChoice::Development||s.operational())return 15;
 if(!s.returnFromDevelopment(true,2000,false)||!s.operational())return 16;
 return 0;
}
'''

stubs=r'''
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "v003_f19_display_modes.h"
#include "v003_f21r3_weather_display.h"
#include "v003_f21r4_ui_policy.h"
const uint8_t u8g2_font_4x6_tf[]={0};
const uint8_t u8g2_font_5x8_tf[]={0};
const uint8_t u8g2_font_6x10_tf[]={0};
const uint8_t u8g2_font_7x14B_tf[]={0};
struct FakeOLED {
 bool portrait=false;int cw=5;int drawCount=0;
 void clearBuffer(){}void setFont(const uint8_t* f){
   cw=f==u8g2_font_7x14B_tf?7:f==u8g2_font_6x10_tf?6:f==u8g2_font_4x6_tf?4:5;}
 void setDrawColor(uint8_t){}void drawBox(int,int,int,int){}
 void drawFrame(int,int,int,int){}void drawDisc(int,int,int){}
 void drawCircle(int,int,int){}void drawHLine(int,int,int){}
 void drawLine(int,int,int,int){}void sendBuffer(){}
 void drawStr(int x,int y,const char* s){
   const int maxW=portrait?64:128,maxH=portrait?128:64;
   if(x<0||y<0||x+int(strlen(s))*cw>maxW||y>maxH){
     fprintf(stderr,"OLED overflow: '%s' x=%d y=%d cw=%d dims=%dx%d\n",
       s,x,y,cw,maxW,maxH);abort();
   }
   ++drawCount;
 }
}u8g2;
namespace VesselIdentity {bool ready(){return true;}uint32_t nodeId(){return 3;}}
struct TimeGPS {bool isValid()const{return true;}unsigned long age()const{return 0;}
 unsigned hour()const{return 6;}unsigned minute()const{return 42;}};
struct DateGPS {bool isValid()const{return true;}unsigned long age()const{return 0;}};
struct GPSFake {TimeGPS time;DateGPS date;DateGPS location;} gps;
struct TinyGPSPlus {static double distanceBetween(double,double,double,double){return 1000.;}
 static double courseTo(double,double,double,double){return 45.;}};
struct PeerDisplaySnapshot {bool active=false,gpsValid=false;uint32_t id=0;
 double latitude=0,longitude=0;unsigned ageSeconds=0;};
struct SOSRecordSnapshot {bool known=false,cancelled=false,positionValid=false;
 uint32_t originId=0,eventId=0,receivedAtMs=0;double latitude=0,longitude=0;};
bool fakePeer=false;bool fakeSOS=false;
bool getPeerSnapshotById(uint32_t,PeerDisplaySnapshot& v){if(!fakePeer)return false;v.active=true;v.gpsValid=true;v.id=2;v.ageSeconds=3;return true;}
bool nextActivePeerId(uint32_t prev,uint32_t& next){if(prev!=0||!fakePeer)return false;next=2;return true;}
bool getSOSRecordByIndex(size_t idx,SOSRecordSnapshot& v){if(!fakeSOS||idx)return false;v.known=true;v.positionValid=true;v.originId=2;v.eventId=12;return true;}
bool localSOS=false;bool hasActiveLocalSOS(){return localSOS;}
unsigned getActiveSOSCount(){return fakeSOS?1:0;}
unsigned long millis(){return 15000;}
void formatUptime(char* out,size_t count){snprintf(out,count,"0d 00:01:00");}
const unsigned WEATHER_HISTORY_SIZE=10;
const int WEATHER_CALIBRATING=0;
uint8_t weatherState=1;
struct WeatherMeasure {unsigned historyCount=10;} weatherMeasurement;
bool weatherSampleAttempted=true,weatherInputsValid=true,gpsHasFix=false;
unsigned long lastWeatherTime=10000;
double pressureDelta=-0.2;
uint32_t lastF10MicroTime=14000;
enum class FakeMicro:uint8_t {WARMUP=0,NO_PATTERN=1,CANDIDATE=2,RISK_WATCH=3,INVALID=4};
struct FakeForecaster{unsigned count()const{return 11;} FakeMicro state()const{return FakeMicro::NO_PATTERN;}}f10Forecaster;
double latitude=0,longitude=0;
V003F19Display::State f19Display;
bool oledPeerMenuOpen=false;
uint8_t oledMenuPeerCount=0,oledMenuCursor=0;
uint32_t oledMenuPeerIds[10]{};
uint32_t selectedOLEDPeerId=0;
const size_t kSOSMenuCapacity=10;
int f21r4BatteryPercent=100;
bool sosMenuOpen=false;
uint8_t sosMenuCount=2,sosMenuCursor=0;
uint32_t sosMenuOrigins[10]={1,2};uint32_t sosMenuEvents[10]={12,8};
uint32_t selectedSOSOrigin=0,selectedSOSEvent=0;
bool findSOSRecord(uint32_t origin,uint32_t event,SOSRecordSnapshot &v){
 if(!fakeSOS || origin!=2 || event!=12)return false;
 v.known=true;v.cancelled=false;v.positionValid=true;
 v.originId=2;v.eventId=12;v.receivedAtMs=1000;return true;
}
bool displaySOSRecord(SOSRecordSnapshot &v){return getSOSRecordByIndex(0,v);}
'''
with tempfile.TemporaryDirectory(prefix='thesis_f21r3_cpp_') as tmp:
 d=Path(tmp)
 for name,source in ((h.name,h.read_text()),(w.name,w.read_text()),(r4.name,r4.read_text())):
  (d/name).write_text(source,encoding='utf-8')
 def compile_run(code,name):
  p=d/(name+'.cpp');p.write_text(code,encoding='utf-8')
  exe=d/(name+'.exe')
  c=subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-Werror',
       '-I',str(d),str(p),'-o',str(exe)],capture_output=True,text=True,timeout=60)
  assert c.returncode==0,(name,c.stdout,c.stderr)
  r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
  assert r.returncode==0,(name,r.returncode,r.stdout,r.stderr)
 compile_run(policy,'orientation_policy')
 compile_run(weather_matrix,'weather_25_matrix')
 # The renderer is extracted FROM the actual patched source, not a mock template.
 code=stubs+'\n// F21R3: OLED-only icon/phone-style OP presentation.'+p+'''
int main(){
 u8g2.portrait=true;
 oledF21PortraitGPSPage();oledF21PortraitWeatherPage();
 oledF21PortraitNearbyPage();oledF21PortraitSOSPage();
 oledF21PortraitHomeMenu();oledF21PortraitPeerMenu();
 fakePeer=true;selectedOLEDPeerId=2;fakeSOS=true;
 oledF21PortraitNearbyPage();oledF21PortraitSOSPage();
 sosMenuOpen=true;oledF21PortraitSOSPage();sosMenuOpen=false;
 localSOS=true;oledF21PortraitSOSPage();localSOS=false;
 f21r4BatteryPercent=-1;oledF21PortraitGPSPage();
 u8g2.portrait=false;
 oledF21LandscapeGPSPage();oledF21LandscapeWeatherPage();
 oledF21LandscapeNearbyPage();oledF21LandscapeSOSPage();
 oledF21LandscapeHomeMenu();oledF21LandscapePeerMenu();
 sosMenuOpen=true;oledF21LandscapeSOSPage();sosMenuOpen=false;
 if(u8g2.drawCount<70)return 4;
 return 0;
}
'''
 compile_run(code,'real_icon_renderers')
print('PASS F21R3: 25/25 macro/micro combinations, stale/invalid guards, SOS and orientation policies')
print('PASS F21R3: actual icon renderers compile + execute with 64x128/128x64 width checks')
print('PASS F21R3: display-only boundaries, no invented battery or 12-hour forecast')
