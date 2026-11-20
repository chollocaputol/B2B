"""F22 functional USB enrollment: source safety checks and portable host parser tests.

This test performs no serial access, NVS writes, SD I/O, flash or Git changes.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil
import importlib.util

ROOT=Path(__file__).resolve().parents[1]
SCRIPT=ROOT/'scripts/fleet/onboard.py'
HEADER=ROOT/'firmware/shared/hardware/fleet_usb_enrollment.h'
MAIN=ROOT/'firmware/universal/src/main.cpp'
INI=ROOT/'firmware/universal/platformio.ini'


def compile_native_enrollment_header():
    compiler=shutil.which('g++')
    if compiler is None:
        raise AssertionError('g++ is required for the USB enrollment C++ host test')
    with tempfile.TemporaryDirectory(prefix='fleet_usb_cpp_') as td:
        temp=Path(td)
        (temp/'fleet_usb_enrollment.h').write_bytes(HEADER.read_bytes())
        (temp/'Arduino.h').write_text(r"""
#pragma once
#include <deque>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>
class MockSerial {
public:
  std::deque<int> input;
  std::string output;
  int available(){return int(input.size());}
  int peek(){return input.empty()?-1:input.front();}
  int read(){if(input.empty())return -1;int x=input.front();input.pop_front();return x;}
  void println(const char* t){output+=t;output+='\n';}
  void printf(const char* format,...){
    char text[1024];va_list a;va_start(a,format);vsnprintf(text,sizeof(text),format,a);va_end(a);output+=text;
  }
};
extern MockSerial Serial;
""")
        (temp/'Preferences.h').write_text(r"""
#pragma once
#include <cstdint>
#include <string>
#include <cstring>
namespace Model {inline bool hasId=false,hasLabel=false; inline uint32_t id=0; inline std::string label;}
class Preferences{
public:
 bool begin(const char*,bool){return true;}
 bool isKey(const char* key){return !strcmp(key,"nodeId")?Model::hasId:Model::hasLabel;}
 size_t putUInt(const char*,uint32_t v){Model::hasId=true;Model::id=v;return 4;}
 size_t putString(const char*,const char* v){Model::hasLabel=true;Model::label=v;return strlen(v);}
 void end(){}
};
""")
        (temp/'esp_mac.h').write_text(r"""
#pragma once
#include <cstdint>
static constexpr int ESP_OK=0;
inline int esp_efuse_mac_get_default(uint8_t* mac){
  const uint8_t source[]={0,0x11,0x22,0x33,0x44,0x55};
  for(int i=0;i<6;i++){mac[i]=source[i];}return ESP_OK;
}
""")
        (temp/'vessel_identity.h').write_text(r"""
#pragma once
#include "Preferences.h"
#include <string>
#include <cstdint>
#include <cstdio>
namespace VesselIdentity{
 inline bool valid=false;
 inline uint32_t id=0;
 inline std::string vesselName="UNPROVISION";
 inline bool ready(){return valid;}
 inline uint32_t nodeId(){return valid?id:0;}
 inline const char* label(){return valid?vesselName.c_str():"UNPROVISION";}
 inline bool load(){
   valid=Model::hasId && Model::hasLabel;
   if(valid){char name[20];snprintf(name,sizeof(name),"Vessel-%03lu",(unsigned long)Model::id);
      valid=Model::id>=1 && Model::id<=9999 && Model::label==name;}
   if(valid){id=Model::id;vesselName=Model::label;}return valid;
 }
}
""")
        (temp/'test.cpp').write_text(r"""
#include <cassert>
#include <string>
#include "fleet_usb_enrollment.h"
MockSerial Serial;
static void send(const std::string& command){
 for(unsigned char x:command) Serial.input.push_back(x);
 for(int n=0;n<200 && !Serial.input.empty();++n) FleetUSBEnrollment::poll();
}
int main(){
 assert(!FleetUSBEnrollment::poll());
 send("TEST HELP\n");
 assert(Serial.peek()=='T');
 Serial.input.clear();
 send("FLEET STATUS\n");
 assert(Serial.output.find("STATE=BLANK ID=0")!=std::string::npos);
 send("FLEET ENROLL 4 001122334455\n");
 assert(Model::hasId && Model::hasLabel && Model::id==4 && Model::label=="Vessel-004");
 assert(Serial.output.find("ENROLLED MAC=001122334455 ID=4")!=std::string::npos);
 send("FLEET ENROLL 5 001122334455\n");
 assert(Model::id==4 && Serial.output.find("ERROR ALREADY_PROVISIONED")!=std::string::npos);
 Model::hasId=false; Model::hasLabel=false; VesselIdentity::valid=false;
 send("FLEET ENROLL 7 001122334456\n");
 assert(!Model::hasId && !Model::hasLabel);
 Model::hasId=true;Model::id=9000;
 send("FLEET ENROLL 7 001122334455\n");
 assert(Model::id==9000 && !Model::hasLabel);
 assert(Serial.output.find("ERROR EXISTING_KEYS_REFUSE_OVERWRITE")!=std::string::npos);
 return 0;
}
""")
        exe=temp/'fleet_test'
        build=subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(temp),str(temp/'test.cpp'),'-o',str(exe)],capture_output=True,text=True)
        assert build.returncode==0,build.stderr
        execute=subprocess.run([str(exe)],capture_output=True,text=True)
        assert execute.returncode==0,execute.stderr
        print('PASS: native C++ USB parser, new ID write, reread, existing-ID lock, MAC mismatch')

def run():
    for p in (SCRIPT,HEADER,MAIN,INI):
        assert p.is_file(),f'missing {p}'
    r=subprocess.run([sys.executable,'-B',str(SCRIPT),'--self-test'],capture_output=True,text=True)
    assert r.returncode==0,r.stderr
    print(r.stdout.strip())
    h=HEADER.read_text(encoding='utf-8')
    m=MAIN.read_text(encoding='utf-8')
    ini=INI.read_text(encoding='utf-8')
    assert h.count('p.putUInt("nodeId"')==1
    assert h.count('p.putString("label"')==1
    assert 'p.clear(' not in h and 'p.remove(' not in h and 'SD.' not in h
    assert 'VesselIdentity::ready()' in h and 'p.isKey("nodeId") || p.isKey("label")' in h
    assert 'esp_efuse_mac_get_default' in h and 'strcmp(actualMac,suppliedMac)!=0' in h
    assert 'VesselIdentity::load()' in h and 'REBOOT_REQUIRED=1' in h
    assert "Serial.peek()!='F'" in h and 'FLEET STATUS' in h and 'FLEET ENROLL' in h
    assert 'THESIS_FLEET_USB_ENROLLMENT' in m and 'FleetUSBEnrollment::poll()' in m
    assert 'if (!sdCommandInProgress) processSerialTestCommands();' in m
    env='[env:tbeam-supreme-fleet-usb-enrollment-build-only]'
    assert env in ini
    section=ini.split(env,1)[1].split('\n[env:',1)[0]
    assert 'THESIS_FLEET_USB_ENROLLMENT=1' in section
    assert 'tbeam-supreme-f22r3a-standard-v003-multiid-build-only' in section
    assert 'THESIS_V003_F15_SD_PILOT_ARMED=1' not in section
    print('PASS: one-time NVS keys only, MAC binding, older serial commands preserved, SD-off enrollment build')
    spec=importlib.util.spec_from_file_location('fleet_onboard',SCRIPT)
    mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    class FakeSerial:
        def __init__(self):self.sent=[];self.replies=[]
        def reset_input_buffer(self):pass
        def write(self,data):
            self.sent.append(data)
            if data==b'FLEET STATUS\n':
                self.replies.append(b'[FLEET] STATUS MAC=ABCDEF123456 STATE=BLANK ID=0 LABEL=UNPROVISION\n')
        def flush(self):pass
        def readline(self):return self.replies.pop(0) if self.replies else b''
    fake=FakeSerial()
    observed=mod.get_status(fake,timeout=0.2)
    assert observed['state']=='BLANK' and observed['mac']=='ABCDEF123456'
    assert fake.sent==[b'FLEET STATUS\n']
    print('PASS: mocked status USB handshake; preview sends no enrollment command')
    compile_native_enrollment_header()

if __name__=='__main__':run()
