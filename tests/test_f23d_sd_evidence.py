"""F23D actual SD state machine, File API, owner, callbacks and offline checker."""
from pathlib import Path
import copy,hashlib,importlib.util,json,shutil,subprocess,tempfile
from f23d_sd_support import remove_sd
ROOT=Path(__file__).resolve().parents[1]
BASE="a07fa009f9d584b6427ca6e5dffec851a09bca38"
def run_cmd(args):
 p=subprocess.run(args,cwd=ROOT,capture_output=True,text=True)
 assert p.returncode==0,p.stdout+p.stderr
 return p.stdout
def extract(s,a,b):return s.split(a,1)[1].split(b,1)[0]
def run():
 cxx=shutil.which("g++");assert cxx
 paths=["firmware/universal/src/"+x for x in ("main.cpp","lora_manager.cpp","weather_manager.cpp")]
 paths+=["firmware/shared/storage/thesis_sd_logger.h","firmware/universal/platformio.ini"]
 for p in paths:assert remove_sd((ROOT/p).read_text())==run_cmd(["git","show",BASE+":"+p]),p
 for p in ("firmware/shared/relay/f23d_bench_contract.h","firmware/shared/relay/f23d_physical_contract.h","firmware/shared/relay/f23d_physical_permission.h","firmware/shared/relay/f23d_run_control.h","firmware/shared/relay/f23c_lab_transaction.h"):
  assert (ROOT/p).read_text()==run_cmd(["git","show",BASE+":"+p]),p
 source=(ROOT/"firmware/universal/src/lora_manager.cpp").read_text()
 assert "SD.open" not in source and "SD.exists" not in source and "sdSPI." not in source
 capture=(ROOT/"firmware/shared/relay/f23d_sd_capture.h").read_text()
 for forbidden in ("SD.open","SD.exists","SPI.","Preferences","Serial."):assert forbidden not in capture
 print("PASS: exact accepted code restored; contracts unchanged; radio captures have no storage APIs")
 with tempfile.TemporaryDirectory(prefix="f23d_sd_") as td:
  t=Path(td);exe=t/"host.exe"
  def build(src):
   f=t/"host.cpp";f.write_text(src)
   run_cmd([cxx,"-std=c++17","-DARDUINO=10819","-O2","-Wall","-Wextra","-Werror","-I",str(ROOT),"-I",str(ROOT/"firmware/shared/storage"),str(f),"-o",str(exe)])
   return run_cmd([str(exe)])
  print(build((ROOT/"tests/f23d_sd_journal_host.cpp").read_text()).strip())
  synthetic=run_cmd([str(exe),"journals"])
  journals={}
  for local in (1,2,3):
   text=synthetic.split("JOURNAL_"+str(local)+"\n",1)[1].split("JOURNAL_",1)[0]
   journals[local]=t/("V00"+str(local)+".jsonl");journals[local].write_text(text)
  spec=importlib.util.spec_from_file_location("checker",ROOT/"scripts/fleet/check_f23d_sd_evidence.py")
  checker=importlib.util.module_from_spec(spec);spec.loader.exec_module(checker)
  report=t/"synthetic_measurement.txt";report.write_text("SYNTHETIC MOCK ONLY: no physical measurements or hardware claims.")
  manifest={"schema":1,"run":7,"event":42,"sessions":{str(i):"S%07X"%i for i in (1,2,3)},
   "sealed_verified":{str(i):True for i in (1,2,3)},
   "isolation":{"arrangement":"Synthetic offline mock topology","reviewed":True,
    "both_direct_directions_blocked":True,"adjacent_bidirectional_links_verified":True,
    "measurement_report":report.name,"measurement_sha256":hashlib.sha256(report.read_bytes()).hexdigest(),
    "negative_control":{"relay_disabled":True,"event":41,"journals":{str(i):journals[i].name for i in (1,2,3)}}}}
  assert checker.check(journals,manifest,t)["status"]=="PASS",checker.check(journals,manifest,t)
  checks=1
  original={i:journals[i].read_text() for i in journals}
  def restore():
   for i,p in journals.items():p.write_text(original[i])
  def edit(local,fn):
   rows=[json.loads(x) for x in original[local].splitlines()]
   rows=fn(rows)
   for i,r in enumerate(rows,1):r["i"]=i;r["written"]=i-1
   journals[local].write_text("".join(json.dumps(r)+"\n" for r in rows))
  cases=[
   (3,lambda rows:[r for r in rows if not (r["record"]=="RX" and r["type"]==2)]),
   (3,lambda rows:[r for r in rows if not (r["record"]=="RX" and r["type"]==4)]),
   (2,lambda rows:rows[:-1]),
   (1,lambda rows:[r for r in rows if not (r["record"]=="TX_RESULT" and r["event"]==42 and r["type"]==4)]),
   (2,lambda rows:rows[:-1]+[copy.deepcopy(next(r for r in rows if r["record"]=="RESERVED" and r["type"]==2))]+rows[-1:]),
  ]
  for local,fn in cases:
   restore();edit(local,fn);assert checker.check(journals,manifest,t)["status"]=="FAIL";checks+=1
  for field,value in (("lost",1),("health",2),("urgent_lost",1),("session","S0000009"),("run",7),("hop",0),("canonical",0),("state","DUPLICATE")):
   restore()
   def mutate(rows):
    row=next(r for r in rows if r["record"]=="RX" and r["type"]==2);row[field]=value;return rows
   edit(3,mutate);assert checker.check(journals,manifest,t)["status"]=="FAIL",field;checks+=1
  for mode in ("gap","truncated","reboot","unknown","bad_json"):
   restore()
   rows=[json.loads(x) for x in original[1].splitlines()]
   if mode=="gap":rows[3]["i"]+=1
   if mode=="reboot":rows[3]["session"]="S0000009"
   if mode=="unknown":rows[3]["extra"]="not allowed"
   text="".join(json.dumps(r)+"\n" for r in rows)
   if mode=="truncated":text=text[:-10]
   if mode=="bad_json":text="{bad}\n"
   journals[1].write_text(text);assert checker.check(journals,manifest,t)["status"]=="FAIL";checks+=1
  restore()
  for missing in ("isolation","sealed_verified"):
   m=copy.deepcopy(manifest);del m[missing]
   expected="INCONCLUSIVE" if missing=="isolation" else "FAIL"
   assert checker.check(journals,m,t)["status"]==expected;checks+=1
  m=copy.deepcopy(manifest);m["isolation"]["measurement_sha256"]="0"*64
  assert checker.check(journals,m,t)["status"]=="INCONCLUSIVE";checks+=1
  m=copy.deepcopy(manifest);m["isolation"]["negative_control"]["event"]=42
  assert checker.check(journals,m,t)["status"]=="INCONCLUSIVE";checks+=1
  manifest_path=t/"manifest.json";manifest_path.write_text(json.dumps(manifest))
  cli=run_cmd([shutil.which("python") or "python",str(ROOT/"scripts/fleet/check_f23d_sd_evidence.py"),
   "--v001",str(journals[1]),"--v002",str(journals[2]),"--v003",str(journals[3]),"--manifest",str(manifest_path)])
  assert json.loads(cli)["status"]=="PASS"
  print(f"PASS: {checks} checker success/failure/inconclusive cases; actual firmware-generated synthetic journals")

  owner=(ROOT/"firmware/shared/storage/thesis_sd_logger.h").read_text().rsplit("// F23D SD begin\n",1)[1].split("// F23D SD end",1)[0]
  pre=r'''
#define THESIS_F23D_SD_EVIDENCE 1
#define THESIS_F23D_SD_ROLE 1
#include <cassert>
#include <cstdio>
#include "tests/f23d_sd_mock_storage.h"
#include "firmware/shared/relay/f23d_sd_journal.h"
static bool ready=true;static uint32_t vesselId=1,lostUrgent=0;
static char DIRECTORY[64]="/THESIS/F06TEST/V001/S0000001";
static uint32_t millis(){return 100;}
static bool isAuthorizedVessel(uint32_t id){return id==1;}
static unsigned ordinary=0,urgent=0;
struct Q{unsigned count=1;};static Q urgentQueue;
static void pollEvents(){++urgent;--urgentQueue.count;}
static void poll(){++ordinary;}
static void pollHistory(){++ordinary;}
static void pollExport(){++ordinary;}
'''
  post=r'''
int main(){
 pollF23DEvidence(false);assert(urgent==1 && f23dJournal.written()==0 && ordinary==0);
 pollF23DEvidence(false);assert(f23dJournal.readyToStart());
 assert(f23dJournal.start(101));const unsigned before=storageOps;
 for(int i=0;i<5;++i)pollF23DEvidence(false);
 assert(storageOps==before && ordinary==0);
 assert(f23dJournal.requestSeal());pollF23DEvidence(true);assert(storageOps==before);
 for(int i=0;i<1500 && !f23dJournal.sealed();++i)pollF23DEvidence(false);
 assert(f23dJournal.sealed());std::puts("PASS: actual logger owner prioritizes urgent, freezes capture and verifies incremental drain");
}
'''
  print(build(pre+owner+post).strip())
  # Actual capture branches: no Serial or storage dependency in the executable.
  def branch(marker):
   part=source.split(marker,1)[1]
   return extract(part,"// F23D SD begin\n","// F23D SD end")
  run_branch=branch("static void f23dQueueRunEvidence(const char* stage,int code,const VesselPacket* packet) {\n")
  rx_branch=branch("                               float snr,const char* validation,const char* outcome) {\n")
  stubs=r'''
#define THESIS_F23D_SD_ROLE 2
#define THESIS_F23D_RELAY_EVIDENCE 1
#define THESIS_F23D_CANDIDATE 1
#define THESIS_F23D_OFFLINE_REVIEW 1
#define THESIS_F22_FLEET_OPERATIONAL 1
#define THESIS_SD_FLEET 1
#define THESIS_SD_LOGGER 1
#define THESIS_F06_SD_SANDBOX 1
#define THESIS_F05_RECOVERY 1
#define THESIS_V003_F19_DUAL_DISPLAY 1
#define THESIS_V003_F15_SD_PILOT_ARMED 1
#include <cassert>
#include <cstdio>
#include <vector>
#include "firmware/shared/relay/f23d_sd_capture.h"
#include "firmware/shared/relay/f23d_run_evidence.h"
#include "firmware/shared/relay/f23d_evidence_queue.h"
static F23DReadiness::EvidenceQueue f23dEvidenceQueue;
static std::vector<F23DSD::Record> captured;static unsigned loss=0;
bool thesisF23dSDCapture(const F23DSD::Record& r){captured.push_back(r);return true;}
void thesisF23dSDFormatLoss(){++loss;}
namespace VesselIdentity{static bool ready(){return true;}}
static constexpr uint32_t NODE_ID=2;
static uint32_t millis(){return 100;}
static F23DReadiness::RunEvidence f23dRunEvidence;static F23DBench::RunControl f23dControl;
static int f23dLastRxRestoreRc=0;static bool f23dSDRadioCalled=true;
static F23DBench::Context f23dContext(){F23DBench::Context c;c.identityReady=true;c.self=2;c.recoverySafe=true;return c;}
'''
  tail=r'''
int main(){
 VesselPacket p{};p.magic=PACKET_MAGIC;p.version=1;p.packetType=2;p.originNodeId=1;p.sequenceNumber=42;
 actualRun("RESERVED",1,&p);assert(captured.size()==1 && captured[0].event==42 && captured[0].hop==0);
 actualRX(reinterpret_cast<const uint8_t*>(&p),62,-91,2.5f,"LEGACY_UNAUTHENTICATED","NEW");
 assert(captured.size()==2 && captured[1].rssi==-91 && captured[1].snr_x10==25 && captured[1].canonical!=0);
 actualRun("SELECTED",0,nullptr);assert(captured.size()==3 && f23dEvidenceQueue.size()==1);
 actualRun("THIS_STAGE_IS_MUCH_TOO_LONG",0,&p);assert(loss==1 && captured.size()==3);
 std::puts("PASS: actual radio run/RX SD capture branches execute without filesystem/Serial APIs");
}
'''
  print(build(stubs+"static void actualRun(const char* stage,int code,const VesselPacket* packet){\n"+run_branch+"}\n"+
   "static void actualRX(const uint8_t* bytes,size_t length,int16_t rssi,float snr,const char* validation,const char* outcome){\n"+rx_branch+"}\n"+tail).strip())
  # Compose the actual physical provider/controller/adapter with the new cached SD veto.
  import ast
  tree=ast.parse((ROOT/"tests/test_f23d_physical_permission.py").read_text())
  physical_stubs=next(n.value.value for n in ast.walk(tree) if isinstance(n,ast.Assign) and
   any(isinstance(x,ast.Name) and x.id=="stubs" for x in n.targets) and isinstance(n.value,ast.Constant))
  gate=extract(source,"// F23D final gate begin\n","// F23D final gate end")
  adapter=extract(source,"// F23D adapter body begin\n","// F23D adapter body end")
  start=source.index("static int transmitWithIdentity(")
  wrapper=source[start:source.index("\n}",start)+3]
  sd_stubs='#define THESIS_F23D_SD_EVIDENCE 1\n'+physical_stubs+r'''
static bool f23dSDRadioCalled=false,sdHealthy=true;
bool thesisF23dSDHealthy(){return sdHealthy;}
'''
  body=r'''
int main(int argc,char**){
 sdHealthy=argc==1;auto c=f23dContext();
 assert(f23dControl.command("R23D SELECT 7 42",c,0,99)==F23DBench::Control::Selected);
 f23dPermission.selected(7,42,0);c=f23dContext();
 assert(f23dControl.command("R23D ARM 99",c,0,0)==F23DBench::Control::Armed);f23dPermission.armed(0);
 VesselPacket p{};p.magic=PACKET_MAGIC;p.version=1;p.packetType=2;p.originNodeId=1;p.sequenceNumber=42;
 assert(f23dControl.observe(p,f23dContext(),true,0)==F23CLab::Admission::Accepted);
 clockMs=250;F23DBenchRadio radio;int rc=0;
 assert(f23dControl.dispatch(f23dContext(),clockMs,radio,rc));assert(f23dControl.attempts()==1);
 if(sdHealthy){assert(calls==1 && rc==0 && f23dSDRadioCalled);}
 else{assert(calls==0 && rc==-997 && !f23dControl.armed() && !f23dSDRadioCalled);}
 std::puts("PASS: actual physical provider/controller/adapter respects cached SD health veto without storage calls");
}
'''
  print(build(sd_stubs+gate+wrapper+adapter+body).strip())
  print(run_cmd([str(exe),"unhealthy"]).strip())
  required={"THESIS_F22_FLEET_OPERATIONAL":1,"THESIS_SD_FLEET":1,"THESIS_SD_LOGGER":1,
   "THESIS_F06_SD_SANDBOX":1,"THESIS_F05_RECOVERY":1,"THESIS_V003_F19_DUAL_DISPLAY":1,"THESIS_V003_F15_SD_PILOT_ARMED":1}
  roles=[
   {**required,"THESIS_F23D_SD_ROLE":1,"THESIS_F23D_EVENT_QUERY":1},
   {**required,"THESIS_F23D_SD_ROLE":2,"THESIS_F23D_RELAY_EVIDENCE":1,"THESIS_F23D_CANDIDATE":1,"THESIS_F23D_OFFLINE_REVIEW":1},
   {**required,"THESIS_F23D_SD_ROLE":3,"THESIS_F23D_RX_OBSERVER":1},
   {**required,"THESIS_F23D_SD_ROLE":2,"THESIS_F23D_PHYSICAL_CANDIDATE":1}]
  cases=[({},True)]
  for role in roles:
   cases.append((role,True))
   for key in required:cases.append(({**role,key:0},False))
   for key in ("THESIS_LAB_RELAY_TX","THESIS_F23D_PHYSICAL_PERMISSION","THESIS_V003_F13_LIVE_OUTPUTS"):
    cases.append(({**role,key:1},False))
  cases += [({"THESIS_F23D_SD_ROLE":4},False),({"THESIS_F23D_SD_ROLE":-1},False),({**required,"THESIS_F23D_SD_ROLE":2},False)]
  f=t/"guard.cpp";f.write_text('#include "firmware/shared/relay/f23d_sd_contract.h"\nint main(){}\n')
  for flags,ok in cases:
   p=subprocess.run([cxx,"-std=c++17","-fsyntax-only","-I",str(ROOT)]+["-D"+k+"="+str(v) for k,v in flags.items()]+[str(f)],capture_output=True,text=True)
   assert (p.returncode==0)==ok,str(flags)+p.stderr
  print(f"PASS: {len(cases)} SD role/safety compile gates")
if __name__=="__main__":run()
