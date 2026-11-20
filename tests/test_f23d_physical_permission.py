"""Actual firmware provider and adapter/final gate; mocks only."""
from pathlib import Path
import shutil,subprocess,tempfile
from f23d_physical_support import remove_physical
ROOT=Path(__file__).resolve().parents[1]
BASE="53815809722074215588cd5c89e988996c8ea2ad"
def command(args):
 p=subprocess.run(args,cwd=ROOT,capture_output=True,text=True)
 assert p.returncode==0,p.stdout+p.stderr
 return p.stdout
def extract(s,a,b):return s.split(a,1)[1].split(b,1)[0]
def run():
 cxx=shutil.which("g++");assert cxx
 for p in ("firmware/universal/src/main.cpp","firmware/universal/src/lora_manager.cpp","firmware/universal/src/weather_manager.cpp","firmware/universal/platformio.ini"):
  assert remove_physical((ROOT/p).read_text())==command(["git","show",BASE+":"+p]),p
 for name in ("f23d_bench_contract.h","f23d_readiness_contract.h","f23d_run_control.h","f23c_lab_contract.h","f23c_lab_transaction.h"):
  p="firmware/shared/relay/"+name
  assert (ROOT/p).read_text()==command(["git","show",BASE+":"+p]),p
 source=(ROOT/"firmware/universal/src/lora_manager.cpp").read_text()
 assert source.index("if(!f23dFinalRadioGate(data,length))")<source.index("return radio.transmit(")
 provider=(ROOT/"firmware/shared/relay/f23d_physical_permission.h").read_text()
 for token in ("Preferences","digitalRead","esp_efuse","Serial.","putUInt","SD."):assert token not in provider
 print("PASS: exact accepted source restored; offline contracts unchanged; no invented hardware grant")
 with tempfile.TemporaryDirectory(prefix="f23d_physical_") as td:
  t=Path(td);exe=t/"host.exe"
  def build(src):
   f=t/"probe.cpp";f.write_text(src)
   command([cxx,"-std=c++17","-DARDUINO=10819","-O2","-Wall","-Wextra","-Werror","-I",str(ROOT),str(f),"-o",str(exe)])
   print(command([str(exe)]).strip())
  build((ROOT/"tests/f23d_physical_permission_host.cpp").read_text())
  gate=extract(source,"// F23D final gate begin\n","// F23D final gate end")
  adapter=extract(source,"// F23D adapter body begin\n","// F23D adapter body end")
  stubs=r'''
#define THESIS_F23D_PHYSICAL_CANDIDATE 1
#define THESIS_F22_FLEET_OPERATIONAL 1
#define THESIS_F05_RECOVERY 1
#define THESIS_F06_SD_SANDBOX 1
#define THESIS_SD_LOGGER 1
#define THESIS_V003_F19_DUAL_DISPLAY 1
#define THESIS_V003_F15_SD_PILOT_ARMED 1
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "firmware/shared/relay/f23d_physical_permission.h"
#include "firmware/shared/relay/f23d_run_evidence.h"
#include "firmware/shared/relay/f23d_evidence_queue.h"
static uint32_t clockMs=0;static bool operatorPresent=true,delayAfterLog=false,loseAfterLog=false;static int scenario=0;static int calls=0,rx=0,restore=0,txrc=0;
static uint32_t millis(){return clockMs;}
static F23DPhysical::Evidence reader(uint32_t now,uint32_t run,uint32_t event){F23DPhysical::Evidence e;e.boardBound=true;e.operatorAuthorized=operatorPresent;e.timingApproved=true;e.run=run;e.event=event;e.sampledAt=now;e.callEntryBoundMs=10;e.radioToRxBoundMs=1000;return e;}
static F23DPhysical::Permission f23dPermission{reader};static F23DBench::RunControl f23dControl;
static const VesselPacket* f23dPhysicalLease=nullptr;static F23DReadiness::EvidenceQueue f23dEvidenceQueue;
static F23DReadiness::RunEvidence f23dRunEvidence;static int f23dLastRxRestoreRc=0;
static F23DBench::Context f23dContext(){F23DBench::Context c;c.identityReady=true;c.self=2;c.recoverySafe=true;c.physicalPermission=f23dPermission.evaluate(c,millis());return c;}
static std::vector<std::string> stages;
static void f23dQueueRunEvidence(const char* s,int,const VesselPacket*){stages.emplace_back(s);if(stages.back()=="RADIO_INVOKE"){if(delayAfterLog)clockMs=15000;if(loseAfterLog)operatorPresent=false;if(scenario==6)clockMs=13990;if(scenario==7)for(int i=0;i<13;++i)f23dEvidenceQueue.push("fill");}}
static constexpr int RADIOLIB_ERR_NONE=0;
static void startReceive(){++rx;f23dLastRxRestoreRc=restore;if(scenario==8)clockMs=15000;}
namespace VesselIdentity {static bool ready(){return true;}}
struct SerialStub {void println(const char*){}};static SerialStub Serial;
static void interruptMacCadForControlTx(){}
struct RadioMock {int transmit(uint8_t*,size_t){assert(f23dControl.attempts()>0);++calls;return txrc;}};static RadioMock radio;
'''
  wrapper_start=source.index("static int transmitWithIdentity(")
  wrapper=source[wrapper_start:source.index("\n}",wrapper_start)+3]
  body=r'''
int main(int argc,char** argv){
 scenario=argc>1?argv[1][0]-'0':0;
 auto c=f23dContext();assert(f23dControl.command("R23D SELECT 7 42",c,0,99)==F23DBench::Control::Selected);
 f23dPermission.selected(7,42,0);c=f23dContext();assert(f23dControl.command("R23D ARM 99",c,0,0)==F23DBench::Control::Armed);f23dPermission.armed(0);
 VesselPacket p{};p.magic=PACKET_MAGIC;p.version=PACKET_VERSION;p.packetType=PACKET_SOS;p.originNodeId=1;p.sequenceNumber=42;
 assert(f23dControl.observe(p,f23dContext(),true,0)==F23CLab::Admission::Accepted);
 clockMs=250;delayAfterLog=scenario==1;loseAfterLog=scenario==5;operatorPresent=scenario!=2;txrc=scenario==3?-7:0;restore=scenario==4?-8:0;
 F23DBenchRadio radio;int rc=0;bool dispatched=f23dControl.dispatch(f23dContext(),millis(),radio,rc);
 if(scenario==2){assert(!dispatched && calls==0);}else{
 assert(dispatched && f23dControl.attempts()==1);assert(rx==1);
 if(scenario==1 || scenario==5 || scenario==6 || scenario==7){assert(calls==0 && rc==-997);}else assert(calls==1);
 }
 if(scenario)assert(!f23dControl.armed());
 assert(f23dPhysicalLease==nullptr);
 std::puts("PASS: actual physical adapter and final gate scenario");
}
'''
  build(stubs+gate+wrapper+adapter+body)
  for scenario in range(1,9):print(command([str(exe),str(scenario)]).strip())
  required={"THESIS_F22_FLEET_OPERATIONAL":1,"THESIS_F05_RECOVERY":1,"THESIS_F06_SD_SANDBOX":1,"THESIS_SD_LOGGER":1,"THESIS_V003_F19_DUAL_DISPLAY":1,"THESIS_V003_F15_SD_PILOT_ARMED":1,"THESIS_F23D_PHYSICAL_CANDIDATE":1}
  cases=[({},True),(required,True),({"THESIS_F23D_PHYSICAL_CANDIDATE":2},False)]
  for key in required:
   if key!="THESIS_F23D_PHYSICAL_CANDIDATE":cases.append(({**required,key:0},False))
  for key in ("THESIS_F23D_CANDIDATE","THESIS_F23D_OFFLINE_REVIEW","THESIS_F23D_PHYSICAL_PERMISSION","THESIS_LAB_RELAY_TX","THESIS_F23C_BUILD_ONLY","THESIS_F23D_EVENT_QUERY","THESIS_F23D_RX_OBSERVER","THESIS_F23D_RELAY_EVIDENCE","THESIS_V003_F13_LIVE_OUTPUTS"):
   cases.append(({**required,key:1},False))
  src=t/"guard.cpp";src.write_text('#include "firmware/shared/relay/f23d_physical_contract.h"\nint main(){}\n')
  for flags,success in cases:
   p=subprocess.run([cxx,"-std=c++17","-fsyntax-only","-I",str(ROOT)]+["-D"+k+"="+str(v) for k,v in flags.items()]+[str(src)],capture_output=True,text=True)
   assert (p.returncode==0)==success,str(flags)+p.stderr
  print(f"PASS: {len(cases)} physical contract compile gates")
if __name__=="__main__":run()
