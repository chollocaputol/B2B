#!/usr/bin/env python3
"""F21R4.2 host-only emergency OLED UX test; never touches hardware."""
from pathlib import Path
import shutil, subprocess, tempfile

R=Path(__file__).resolve().parents[1]
main=(R/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
header=R/'firmware/universal/include/v003_f19_display_modes.h'
policy=header.read_text(encoding='utf-8')
button=main.split('void handleOLEDButton() {',1)[1].split('// OLED PAGES',1)[0]
render=main.split('// F21R3: OLED-only icon/phone-style OP presentation.',1)[1].split('#endif // F21R3 icon renderers',1)[0]
dispatch=main.split('void updateOLED() {',1)[1].split('void oledGPSPage()',1)[0]

# No GPIO0 path may send/cancel real SOS. GPIO2 start/cancel remains unchanged.
assert 'const auto action = SOSControls::pollAction(hasActiveLocalSOS());' in main
assert 'transmitSOS();' in main and 'cancelActiveSOS();' in main
assert button.index('if (f19ActiveSOS)')<button.index('f19Display.wake()')
for forbidden in ('transmitSOS(', 'cancelActiveSOS(', 'ThesisSDLogger::hybridSetContinuous(',
                  'pinMode(', 'digitalWrite(', 'transmitPacket('):
    assert forbidden not in button.split('if (f19ActiveSOS) {',1)[1].split('lastOLEDUpdate=0;return;',1)[0], forbidden
assert 'f21r42SOSBrowse.observe(V003F19Display::emergencyActive(' in button
assert 'if(!f21r42SOSBrowse.emergency() || f21r42SOSBrowse.forceSOS()) sosMenuOpen=false;' in button
assert 'if(sosMenuOpen)' in button
assert 'f21r42SOSBrowse.tap(millis())' in button
assert 'f21r42SOSBrowse.returnToSOS(millis())' in button
assert 'if(f21r42SOSBrowse.forceSOS() || f21r42SOSBrowse.page()==3U)' in button
assert 'if(duration>=BUTTON_LONG_PRESS_MS) confirmSOSMenu();' in button
assert 'else advanceSOSMenu();' in button
assert 'openSOSMenu();' in button
assert 'f21r42SOSBrowse.preempt();' in main
assert 'f19Display.dismissHomeMenuForSOS();' in button
assert 'if(sosMenuOpen && f21r42SOSBrowse.idleForReturn(millis()))' in button
assert 'f21r42SOSBrowse.noteInput(millis());' in button
assert 'if (f19SOS && f21r42SOSBrowse.forceSOS())' in dispatch
assert 'if (f19Display.operational() || f19SOS)' in dispatch
assert 'const uint8_t opPage=f19SOS ? f21r42SOSBrowse.page() : f19Display.operationalPage();' in dispatch
assert dispatch.index('if (f19SOS && f21r42SOSBrowse.forceSOS())') < dispatch.index('switch(opPage)')
assert 'case 3: oledF21PortraitSOSPage();' in dispatch
assert 'case 3: oledF21LandscapeSOSPage();' in dispatch

# Genuine local SOS and received distress remain selectable; never assert delivery
# unless backed by real network acknowledgment (none wired into display).
assert 'if(sosMenuOpen){oledF21PortraitSOSMenu();return;}' in render
assert 'if(sosMenuOpen){oledF21LandscapeSOSMenu();return;}' in render
assert 'hasActiveLocalSOS()?"MY SOS":"AUTO"' in render
assert 'hasActiveLocalSOS()?"MY SOS (AUTO)":"AUTO LATEST"' in render
assert 'const bool showLocal=local && selectedSOSOrigin==0U;' in render
assert 'if(local && selectedSOSOrigin==0U)' in render
assert 'f21r42ValidateSOSSelection();' in render
assert 'chosen.known && !chosen.cancelled' in render
assert 'BOAT REPLY: UNKNOWN' in render and 'BOAT REPLY' in render
assert 'TX NOT CONFIRMED' not in render and 'TX UNKNOWN' not in render
assert 'if(page!=3U && V003F19Display::emergencyActive(' in render
assert 'u8g2.drawStr(0,126,"SOS")' in render
assert 'u8g2.drawStr(3,62,"SOS ACTIVE")' in render
assert 'if(!V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount()))' in render
for forbidden in ('transmitSOS(', 'cancelActiveSOS(', 'ThesisSDLogger::',
                  'digitalWrite(', 'pinMode(', 'processWeather(', 'readMicroWeatherSample('):
    assert forbidden not in render, forbidden
assert 'SOSBrowse' in policy

# Exercise actual C++ transition logic, including wraparound millis and timeout.
cxx=r'''
#include <stdint.h>
#include "v003_f19_display_modes.h"
using namespace V003F19Display;
int main() {
 SOSBrowse v;
 if(v.tap(0) || v.forceSOS() || v.browsing()) return 1;
 if(!v.observe(true,100) || !v.forceSOS()) return 2;
 if(!v.tap(500) || !v.browsing() || v.page()!=0U) return 3;
 for(unsigned i=1;i<4;i++) {
  if(!v.tap(500+i) || v.page()!=i) return 4;
 }
 if(!v.tap(504) || v.page()!=0) return 5;
 if(v.observe(true,20503)) return 6; // still before 20s boundary
 if(!v.observe(true,20504) || !v.forceSOS() || v.browsing()) return 7;
 v.tap(21000);
 v.noteInput(21001);
 if(v.observe(true,41000)) return 8;
 if(!v.observe(true,41001) || !v.forceSOS()) return 9;
 v.tap(41100);
 if(v.idleForReturn(41101)) return 15;
 v.preempt();
 if(!v.forceSOS() || v.page()!=3) return 10;
 if(!v.observe(false,43000) || v.emergency() || v.forceSOS()) return 11;
 if(v.tap(43001)) return 12;
 v.observe(true,0xfffffff0U);
 v.tap(0xfffffff1U);
 if(v.observe(true,0x10U)) return 13; // rollover must not spuriously timeout
 if(!v.observe(true,0x4e11U) || !v.forceSOS()) return 14;
 return 0;
}
'''
compiler=shutil.which('g++')
assert compiler, 'g++ required for SOS state machine test'
with tempfile.TemporaryDirectory(prefix='thesis_f21r42_sos_') as td:
    d=Path(td); (d/'test.cpp').write_text(cxx,encoding='utf-8')
    exe=d/('sos.exe' if __import__('os').name=='nt' else 'sos')
    cmd=[compiler,'-std=c++11','-Wall','-Wextra','-Werror','-I',str(header.parent),str(d/'test.cpp'),'-o',str(exe)]
    result=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
    assert result.returncode==0, result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
    assert result.returncode==0,(result.returncode,result.stderr,result.stdout)
# Compile and exercise the existing unmodified multi-SOS record selector from main.cpp.
menu_source=main.split('// PHASE 3F-2 SOS MENU:',1)[1].split('// -------------------- GPS',1)[0]
menu_stub=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
const uint8_t u8g2_font_6x10_tf[]={0};
#define OLED_PAGE_TOTAL "8"
struct SOSRecordSnapshot {bool known=false,cancelled=false,positionValid=false;
 uint32_t originId=0,eventId=0,receivedAtMs=0;double latitude=0,longitude=0;};
SOSRecordSnapshot items[10]{};
bool getSOSRecordByIndex(size_t n, SOSRecordSnapshot& out) {
 if(n>=10 || !items[n].known) return false;
 out=items[n]; return true;
}
struct FakeSerial {void printf(const char*,...){} void println(const char*){}} Serial;
struct FakeOLED {void clearBuffer(){} void setFont(const uint8_t*){}
 void drawStr(int,int,const char*){} void sendBuffer(){}} u8g2;
'''
menu_main=r'''
int main(){
 items[0].known=true;items[0].originId=1;items[0].eventId=10;items[0].receivedAtMs=100;
 items[1].known=true;items[1].originId=2;items[1].eventId=20;items[1].receivedAtMs=200;
 items[2].known=true;items[2].originId=3;items[2].eventId=30;items[2].cancelled=true;
 openSOSMenu();
 if(!sosMenuOpen || sosMenuCount!=2U || sosMenuCursor!=0) return 1;
 advanceSOSMenu();advanceSOSMenu();
 if(sosMenuCursor!=2U) return 2;
 confirmSOSMenu();
 if(sosMenuOpen || selectedSOSOrigin!=2U || selectedSOSEvent!=20U) return 3;
 SOSRecordSnapshot chosen{};
 if(!displaySOSRecord(chosen) || chosen.originId!=2U) return 4;
 openSOSMenu();advanceSOSMenu();
 items[0].cancelled=true;
 confirmSOSMenu();
 if(!sosMenuOpen || selectedSOSOrigin!=2U) return 5; // cancelled -> reopen
 items[1].cancelled=true;
 selectedSOSOrigin=0;selectedSOSEvent=0;
 if(displaySOSRecord(chosen)) return 6; // none active
 openSOSMenu();
 if(sosMenuCount!=0 || !sosMenuOpen) return 7;
 drawSOSMenu();
 advanceSOSMenu(); // BACK
 confirmSOSMenu();
 if(sosMenuOpen) return 8;
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='thesis_f21r42_sos_events_') as td:
    d=Path(td); (d/'events.cpp').write_text(menu_stub+'\n// PHASE 3F-2 SOS MENU:'+menu_source+'\n'+menu_main,encoding='utf-8')
    exe=d/('events.exe' if __import__('os').name=='nt' else 'events')
    cmd=[compiler,'-std=c++11','-Wall','-Wextra','-Werror',str(d/'events.cpp'),'-o',str(exe)]
    result=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
    assert result.returncode==0,result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
    assert result.returncode==0,(result.returncode,result.stderr,result.stdout)
print('PASS F21R4.2: real C++ emergency browse transitions, timer, wrap, preemption')
print('PASS F21R4.2: compiled real 10-slot SOS selector, cancelled-event handling')
print('PASS F21R4.2: local/remote SOS selector, persistent badge, no hardware action')
