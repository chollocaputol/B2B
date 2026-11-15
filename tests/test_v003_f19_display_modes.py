#!/usr/bin/env python3
"""F19R1: host-only operational UI/menu, navigation, and F19 build flag gates."""
from pathlib import Path
import shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
src=(ROOT/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
ini=(ROOT/'firmware/universal/platformio.ini').read_text(encoding='utf-8')
hdr=ROOT/'firmware/universal/include/v003_f19_display_modes.h'
assert 'THESIS_V003_F19_DUAL_DISPLAY=1' in ini
assert 'THESIS_V003_F19_EXPLICIT_OPT_IN=1' in ini
assert 'THESIS_V003_F15_SD_PILOT_ARMED=1' in ini
assert 'THESIS_V003_F13_LIVE_OUTPUTS != 0' in src
assert 'THESIS_LAB_RELAY_TX != 0' in src
assert 'WEATHER_FAST_TEST != 0' in src
assert '[F19-UI] MODE=OPERATIONAL OP_PAGES=4' in src
cxx=shutil.which('g++')
assert cxx,'g++ unavailable'
source=r"""
#include "v003_f19_display_modes.h"
#include <string.h>
using namespace V003F19Display;
int main() {
 State s;
 if(!s.operational() || s.operationalPage()!=0 || s.screenAsleep())return 1;
 if(!s.openHomeMenu(950,false) || !s.homeMenuOpen())return 2;
 if(s.tapOperational(10,false))return 3;
 if(!s.moveHomeCursor(10,false) || s.homeMenuCursor()!=1)return 4;
 if(s.confirmHomeMenu(950,false)!=HomeChoice::Development || s.operational())return 5;
 if(s.returnFromDevelopment(true,1999,false))return 6;
 if(s.returnFromDevelopment(true,2500,true))return 7;
 if(!s.returnFromDevelopment(true,2000,false) || !s.operational())return 8;
 if(!s.tapOperational(10,false) || s.operationalPage()!=1)return 9;
 if(!s.tapOperational(10,false) || s.operationalPage()!=2)return 10;
 if(!s.tapOperational(10,false) || s.operationalPage()!=3)return 11;
 if(!s.tapOperational(10,false) || s.operationalPage()!=0)return 12;
 if(s.tapOperational(900,false) || s.tapOperational(10,true))return 13;
 if(!s.openHomeMenu(900,false))return 14;
 if(s.confirmHomeMenu(900,false)!=HomeChoice::ScreenOff || !s.screenAsleep())return 15;
 if(s.tapOperational(10,false) || s.openHomeMenu(900,false))return 16;
 if(!s.wake() || s.screenAsleep() || s.wake())return 17;
 if(!emergencyActive(true,0) || !emergencyActive(false,1) || emergencyActive(false,0))return 18;
 if(strcmp(cardinal8(0),"N") || strcmp(cardinal8(45),"NE") ||
    strcmp(cardinal8(90),"E") || strcmp(cardinal8(225),"SW") ||
    strcmp(cardinal8(359.9),"N") || strcmp(cardinal8(-1),"--"))return 19;
 return 0;
}
"""
with tempfile.TemporaryDirectory(prefix='thesis_f19r1_policy_') as td:
 d=Path(td);(d/'test.cpp').write_text(source)
 exe=d/'policy.exe'
 c=subprocess.run([cxx,'-std=c++11','-Wall','-Wextra','-Werror',
   '-I',str(hdr.parent),str(d/'test.cpp'),'-o',str(exe)],capture_output=True,text=True)
 assert c.returncode==0,c.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True)
 assert r.returncode==0,(r.returncode,r.stderr)
# Keep original F19 negative compile-gate coverage: refuse unsafe combinations.
start=src.index('// F19: new Vessel-003 presentation only;')
stop=src.index('// Separate F15 hardware opt-in:',start)
guard=src[start:stop]
required={
 'THESIS_V003_F19_DUAL_DISPLAY':1,'THESIS_V003_F19_EXPLICIT_OPT_IN':1,
 'THESIS_V003_F18_WEATHER_INTERFACE':1,'THESIS_V003_F18_EXPLICIT_OPT_IN':1,
 'THESIS_V003_F17_INTEGRATED_PREFLIGHT':1,
 'THESIS_V003_F15_MACRO_SD_SANDBOX':1,
 'THESIS_V003_F16_SD_OLED_HYBRID':1,'THESIS_F06_SD_HYBRID':1,
 'THESIS_SD_OLED_UI':1,'THESIS_V003_F13_LIVE_OUTPUTS':0,
 'THESIS_LAB_RELAY_TX':0,'THESIS_V003_F14_INDICATOR_PROBE':0,
 'WEATHER_FAST_TEST':0,
}
def compile_guard(flags):
 r=subprocess.run([cxx,'-std=c++11','-x','c++','-fsyntax-only','-']+
    ['-D'+k+'='+str(v) for k,v in flags.items()],input=guard,
    capture_output=True,text=True)
 return r.returncode==0,r.stderr
ok,err=compile_guard(required);assert ok,err
for flag in required:
 bad=dict(required);bad[flag]=1-bad[flag]
 ok,_=compile_guard(bad)
 if flag!='THESIS_V003_F19_DUAL_DISPLAY':
  assert not ok,'unsafe compile guard accepted '+flag
for mode, opt in ((0,0),(0,1)):
 off=dict(required);off['THESIS_V003_F19_DUAL_DISPLAY']=mode
 off['THESIS_V003_F19_EXPLICIT_OPT_IN']=opt
 ok,err=compile_guard(off);assert ok,err
print('PASS F19R1: four pages, Home menu/sleep/wake, DEV return, true cardinal direction')
print('PASS F19R1: positive/negative actual C++ safety gate compilation')
