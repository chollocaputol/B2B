#!/usr/bin/env python3
"""F21R4.3: Home/peer/SOS hold controls during local or received distress.

Host-only; tests exact source routing plus actual compiled F19 display State.
"""
from pathlib import Path
import shutil, subprocess, tempfile, os
ROOT=Path(__file__).resolve().parents[1]
src=(ROOT/'firmware/universal/src/main.cpp').read_text(encoding='utf8')
policy=ROOT/'firmware/universal/include/v003_f19_display_modes.h'
button=src.split('void handleOLEDButton() {',1)[1].split('// OLED PAGES',1)[0]
active=button.split('if (f19ActiveSOS) {',1)[1].split('lastOLEDUpdate=0;return;',1)[0]
assert active.index('if(sosMenuOpen)')<active.index('else if(f19Display.homeMenuOpen())')<active.index('else if(oledPeerMenuOpen)')<active.index('else if(duration>=BUTTON_LONG_PRESS_MS)')
assert active.index('else if(f21r42SOSBrowse.page()==0U)')<active.index('else if(f21r42SOSBrowse.page()==2U)')
assert 'f19Display.openHomeMenuForSOS(duration)' in active
assert 'f19Display.moveHomeCursorForSOS(duration)' in active
assert 'f19Display.confirmHomeMenuForSOS(duration)' in active
assert 'openOLEDPeerMenu();' in active
assert 'advanceOLEDPeerMenu();' in active
assert 'confirmOLEDPeerMenu();' in active
assert 'confirmSOSMenu();' in active and 'advanceSOSMenu();' in active
assert 'f21r42SOSBrowse.tap(millis())' in active
assert 'f21r42SOSBrowse.noteInput(millis())' in active
assert 'if(sosMenuOpen && f21r42SOSBrowse.idleForReturn(millis()))' in button
assert 'f19Display.dismissHomeMenuForSOS();' in src
sos_hook=src.split('if (consumeNewReceivedSOS()) {',1)[1].split('processSOSRetries();',1)[0]
for token in ('f21r42SOSBrowse.preempt();','f19Display.dismissHomeMenuForSOS();','oledPeerMenuOpen=false;','sosMenuOpen = false;'):
    assert token in sos_hook,token
for forbidden in ('transmitSOS(', 'cancelActiveSOS(', 'transmitPacket(', 'pinMode(', 'digitalWrite(', 'ThesisSDLogger::'):
    assert forbidden not in active, forbidden
render=src.split('// F21R3: OLED-only icon/phone-style OP presentation.',1)[1].split('#endif // F21R3 icon renderers',1)[0]
assert 'distress?"SOS MENU":"DISPLAY"' in render
assert 'distress?"SOS HOME MENU":"DISPLAY MENU"' in render
assert 'locked?\'X\':active?\'*\':\' \'' in render
assert '"SOS PEERS":"PEER MENU"' in render
assert '"SOS SELECT PEER":"SELECT PEER"' in render
assert '(f19Display.operational() || f19SOS) && f19Display.portrait()' in src
assert 'if (f19SOS && f21r42SOSBrowse.forceSOS())' in src
assert 'if(sosMenuOpen && f21r42SOSBrowse.idleForReturn(millis()))' in src
assert 'SOSControls::pollAction(hasActiveLocalSOS())' in src

cxx = r'''
#include <stdint.h>
#include "v003_f19_display_modes.h"
using namespace V003F19Display;
int main() {
 State s;
 SOSBrowse b;
 // The old non-emergency state contract continues to reject SOS methods.
 if(s.openHomeMenu(901,true)) return 1;
 if(!b.observe(true,100) || !b.forceSOS()) return 2;
 if(!b.tap(200) || b.page()!=0U) return 3;
 // SOS Home menu can open even when the original OP page is not HOME.
 s.tapOperational(30,false);
 if(!s.openHomeMenuForSOS(900) || !s.homeMenuOpen()) return 4;
 if(s.confirmHomeMenuForSOS(900)!=HomeChoice::None || !s.homeMenuOpen()) return 5;
 if(!s.moveHomeCursorForSOS(30) || s.homeMenuCursor()!=1U) return 6;
 if(s.confirmHomeMenuForSOS(900)!=HomeChoice::None || !s.homeMenuOpen()) return 7;
 if(!s.moveHomeCursorForSOS(30) || s.homeMenuCursor()!=2U) return 8;
 if(s.confirmHomeMenuForSOS(899)!=HomeChoice::None || !s.homeMenuOpen()) return 9;
 if(s.confirmHomeMenuForSOS(901)!=HomeChoice::Portrait || !s.portrait() || s.homeMenuOpen()) return 10;
 if(!s.openHomeMenuForSOS(900)) return 11;
 for(unsigned i=0;i<3;i++)if(!s.moveHomeCursorForSOS(20))return 12;
 if(s.confirmHomeMenuForSOS(901)!=HomeChoice::Landscape || s.portrait())return 13;
 if(!s.openHomeMenuForSOS(900))return 14;
 for(unsigned i=0;i<4;i++)if(!s.moveHomeCursorForSOS(20))return 15;
 if(s.confirmHomeMenuForSOS(901)!=HomeChoice::Back || s.homeMenuOpen())return 16;
 // Safe emergency override after inactivity; view state never cancels SOS.
 b.noteInput(200);
 if(b.observe(true,20199)) return 17;
 if(!b.observe(true,20200) || !b.forceSOS()) return 18;
 if(!b.tap(20500) || b.page()!=0U) return 19;
 s.openHomeMenuForSOS(910);
 s.dismissHomeMenuForSOS();
 if(s.homeMenuOpen())return 20;
 if(!b.observe(false,22000) || b.emergency())return 21;
 // The SOS-specific menu is usable while background UI is in DEV/sleep.
 State dev;
 dev.openHomeMenu(950,false); dev.moveHomeCursor(10,false);
 if(dev.confirmHomeMenu(950,false)!=HomeChoice::Development || dev.operational())return 22;
 if(!dev.openHomeMenuForSOS(900) || !dev.moveHomeCursorForSOS(10))return 23;
 dev.dismissHomeMenuForSOS();
 State asleep;
 asleep.openHomeMenu(950,false);
 if(asleep.confirmHomeMenu(950,false)!=HomeChoice::ScreenOff || !asleep.screenAsleep())return 24;
 if(!asleep.openHomeMenuForSOS(900))return 25;
 asleep.dismissHomeMenuForSOS();
 return 0;
}
'''
compiler=shutil.which('g++')
assert compiler,'g++ unavailable'
with tempfile.TemporaryDirectory(prefix='thesis_f21r43_sos_menu_') as temp:
    d=Path(temp)
    (d/'test.cpp').write_text(cxx,encoding='utf8')
    exe=d/('check.exe' if os.name=='nt' else 'check')
    p=subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-Werror','-I',str(policy.parent),str(d/'test.cpp'),'-o',str(exe)],capture_output=True,text=True,timeout=90)
    assert p.returncode==0,p.stderr
    p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
    assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
print('PASS F21R4.3: real C++ menu policy, locked unsafe choices and orientation')
print('PASS F21R4.3: Home/Peers/SOS hold dispatch, preemption, idle return')
print('PASS F21R4.3: SOS action and storage isolated from GPIO0 browsing')
