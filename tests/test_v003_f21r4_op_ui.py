#!/usr/bin/env python3
"""F21R4 host-only UI/policy regression. No device or SOS transmissions."""
from pathlib import Path
import shutil,subprocess,tempfile
R=Path(__file__).resolve().parents[1]
m=(R/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
p=R/'firmware/universal/include/v003_f21r4_ui_policy.h'
assert p.exists()
render=m.split('// F21R3: OLED-only icon/phone-style OP presentation.',1)[1].split('#endif // F21R3 icon renderers',1)[0]
button=m.split('void handleOLEDButton() {',1)[1].split('// OLED PAGES',1)[0]
dispatch=m.split('void updateOLED() {',1)[1].split('void oledGPSPage()',1)[0]
assert 'V003F21R4UI::uptimeText(millis(),b,sizeof(b))' in render
assert 'f21r4PmuReady' in m and 'pmu.isBatteryConnect()' in m and 'pmu.getBatteryPercent()' in m
assert 'f21r4PollBattery()' in m and 'f21r4BatteryPercent' in m
assert 'f21r4HomeLabel' not in render
assert 'oledF21HomeLabel(b,sizeof(b))' in render
assert 'V003F21R4UI::kHomeLatitude' in render and 'V003F21R4UI::kHomeLongitude' in render
assert 'gps.location.age()' in render
# F21R4.1 portrait HOME: remove boat glyph, enlarge battery to 6x10,
# preserve 2px battery-label gap, and leave other pictogram headers intact.
assert 'if(strcmp(title,"V003")==0)oledF21P(3,10,title);' in render
assert 'else{oledF21Icon(oledF21HeaderIcon(title),2,1);oledF21P(13,10,title);}' in render
assert 'portrait?u8g2_font_6x10_tf:u8g2_font_5x8_tf' in render
assert 'oledF21Icon(F21Icon::Battery,26,2)' in render
assert 'oledF21BatteryLabel(38,11,true)' in render
assert 'oledF21BatteryLabel(47,9,true)' not in render
assert 3 + 4*5 <= 26 and 26 + 10 + 2 == 38 and 38 + 4*6 <= 64
assert 'oledF21BatteryLabel(104,10,false)' in render
assert 'oledF21P(3,67,gpsHasFix?' in render
assert 'oledF21P(3,82,b)' in render
assert 'oledF21P(3,97,b)' in render
assert 'oledF21P(3,112,b)' in render
assert 'oledF21P(5,65,"NO PEERS")' in render
assert 'snprintf(b,sizeof(b),"AGE %lus",(unsigned long)peer.ageSeconds)' in render
assert 'oledF21P(5,111,b)' in render
for forbidden in ('"HOLD MENU");oledF21Dots(true,0)', '"GPIO2 SOS"', '"GAP/GUIDE"', '"GUIDE ONLY"'):
 assert forbidden not in render,forbidden
assert 'u8g2.drawStr(3,62,"HOLD MENU")' in render
assert 'f21r4ChooseSOS(chosen)' in render
assert 'f21r42ValidateSOSSelection();' in render
assert 'selectedSOSOrigin=0;selectedSOSEvent=0' in render
assert 'BOAT REPLY: UNKNOWN' in render and 'BOAT REPLY' in render
assert 'TX NOT CONFIRMED' not in render and 'TX UNKNOWN' not in render
assert 'void oledF21PortraitSOSMenu()' in render
assert 'void oledF21LandscapeSOSMenu()' in render
assert 'if(sosMenuOpen){oledF21PortraitSOSMenu();return;}' in render
assert 'if(sosMenuOpen){oledF21LandscapeSOSMenu();return;}' in render
assert 'const bool f19ActiveSOS=f21r42SOSBrowse.emergency();' in button
assert 'if(duration>=BUTTON_LONG_PRESS_MS) confirmSOSMenu();' in button
assert 'else advanceSOSMenu();' in button and 'openSOSMenu();' in button
assert button.index('if (f19ActiveSOS)')<button.index('if (f19Display.wake())')
assert dispatch.index('if (f19SOS && f21r42SOSBrowse.forceSOS())')<dispatch.index('if (f19Display.operational() || f19SOS) {')
assert 'const auto action = SOSControls::pollAction(hasActiveLocalSOS());' in m
assert 'transmitSOS();' in m and 'cancelActiveSOS();' in m
for forbidden in ('transmitSOS(', 'cancelActiveSOS(', 'ThesisSDLogger::', 'digitalWrite(', 'pinMode(', 'processWeather(', 'readMicroWeatherSample('):
 assert forbidden not in render,forbidden
compiler=shutil.which('g++')
assert compiler,'g++ not found'
code=r'''
#include <string.h>
#include <math.h>
#include "v003_f21r4_ui_policy.h"
int main() {
 char b[64]={};
 V003F21R4UI::uptimeText(0,b,sizeof b);
 if(strcmp(b,"UP 00:00:00"))return 1;
 V003F21R4UI::uptimeText(3599999U,b,sizeof b);
 if(strcmp(b,"UP 00:59:59"))return 2;
 V003F21R4UI::uptimeText(3600000U,b,sizeof b);
 if(strcmp(b,"UP 01:00:00"))return 3;
 V003F21R4UI::uptimeText(360000000U,b,sizeof b);
 if(strcmp(b,"UP 100h+"))return 4;
 V003F21R4UI::shortUptime(1800000U,b,sizeof b);
 if(strcmp(b,"UP 30m"))return 5;
 V003F21R4UI::shortUptime(7200000U,b,sizeof b);
 if(strcmp(b,"UP 2h"))return 6;
 V003F21R4UI::batteryText(-1,b,sizeof b);
 if(strcmp(b,"--%"))return 7;
 V003F21R4UI::batteryText(0,b,sizeof b);
 if(strcmp(b,"0%"))return 8;
 V003F21R4UI::batteryText(100,b,sizeof b);
 if(strcmp(b,"100%"))return 9;
 V003F21R4UI::batteryText(101,b,sizeof b);
 if(strcmp(b,"--%"))return 10;
 V003F21R4UI::distanceText(NAN,b,sizeof b);
 if(strcmp(b,"HOME --"))return 11;
 V003F21R4UI::distanceText(0.05,b,sizeof b);
 if(strcmp(b,"HOME 50m"))return 12;
 V003F21R4UI::distanceText(2.2,b,sizeof b);
 if(strcmp(b,"HOME 2.2km"))return 13;
 V003F21R4UI::distanceText(1000.,b,sizeof b);
 if(strcmp(b,"HOME 999+km"))return 14;
 if(!V003F21R4UI::freshPosition(true,true,10000,8.275013,124.262401))return 15;
 if(V003F21R4UI::freshPosition(true,true,10001,8.275013,124.262401))return 16;
 if(V003F21R4UI::freshPosition(false,true,0,8.275013,124.262401))return 17;
 if(V003F21R4UI::freshPosition(true,true,0,NAN,124.262401))return 18;
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='thesis_f21r4_pure_') as tmp:
 d=Path(tmp);(d/p.name).write_bytes(p.read_bytes());(d/'test.cpp').write_text(code,encoding='utf-8')
 exe=d/'run'
 c=subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-Werror','-I',str(d),str(d/'test.cpp'),'-o',str(exe)],capture_output=True,text=True,timeout=90)
 assert c.returncode==0,c.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
 assert r.returncode==0,(r.returncode,r.stderr)
print('PASS F21R4: source-only SOS priority/menu + formatting policy, 18 C++ checks')
