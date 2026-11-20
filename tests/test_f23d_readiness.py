"""Physical-readiness diagnostics and permission requirements; offline only."""
from pathlib import Path
import configparser
import json
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
BASE="1fc20613ffb8457c2b1552fdcc62379c9176b638"
BEGIN="// F23D readiness begin"
END="// F23D readiness end"
def run_cmd(args):
    r=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,encoding="utf-8")
    assert r.returncode==0,r.stdout+r.stderr
    return r.stdout
def strip(text):
    result=[];inside=False
    for line in text.splitlines(keepends=True):
        if line.strip()==BEGIN:
            assert not inside;inside=True
        elif line.strip()==END:
            assert inside;inside=False
        elif not inside:result.append(line)
    assert not inside
    return "".join(result)
def extract(text,start,end):
    assert text.count(start)==1 and text.count(end)==1
    return text.split(start,1)[1].split(end,1)[0]

def run():
    cxx=shutil.which("g++");assert cxx
    for name in ("main.cpp","lora_manager.cpp","weather_manager.cpp"):
        path="firmware/universal/src/"+name
        current=(ROOT/path).read_text(encoding="utf-8")
        assert strip(current)==run_cmd(["git","show",BASE+":"+path]),path
    for folder in ("firmware/shared/hardware","firmware/shared/storage","firmware/shared/communication"):
        assert not run_cmd(["git","diff",BASE,"--",folder]).strip(),folder
    for name in ("f23c_lab_contract.h","f23c_lab_transaction.h","f23d_bench_contract.h",
                 "f23d_run_control.h","relay_authorization_contract.h","relay_tx_readiness.h"):
        path="firmware/shared/relay/"+name
        assert (ROOT/path).read_text()==run_cmd(["git","show",BASE+":"+path]),path
    ini=(ROOT/"firmware/universal/platformio.ini").read_text()
    assert ini.split("; F23D readiness profiles",1)[0]==run_cmd(["git","show",BASE+":firmware/universal/platformio.ini"])
    cfg=configparser.ConfigParser(interpolation=None);cfg.read_string(ini)
    for role,flag,parent in (
        ("v001-event-query","THESIS_F23D_EVENT_QUERY","fleet-universal-operational"),
        ("v003-rx-observer","THESIS_F23D_RX_OBSERVER","fleet-universal-operational"),
        ("v002-relay-evidence","THESIS_F23D_RELAY_EVIDENCE","f23d-v002-bench-candidate")):
        env=cfg["env:tbeam-supreme-f23d-"+role+"-build-only"]
        assert env["extends"]=="env:tbeam-supreme-"+parent+"-build-only"
        assert env["build_flags"].splitlines()[-1]=="-D "+flag+"=1"
    source=(ROOT/"firmware/universal/src/lora_manager.cpp").read_text()
    collector=extract(source,"// F23D query collector begin\n","// F23D query collector end")
    command=extract(source,"// F23D query command begin\n","// F23D query command end")
    for forbidden in ("Preferences","getUInt","putUInt","putBytes","SD.","radio.","transmitSOS","nextEmergencyId++","++nextEmergencyId"):
        assert forbidden not in collector+command,forbidden
    assert "s.next=nextEmergencyId" in collector and "R23D NEXT" in command
    assert "f23dEvidenceQueue.drain" in source and "micros()-began" in source
    assert "f23dQueueRxEvidence(buffer,length,rssi,snr" in source
    print("PASS: three explicitly gated diagnostic roles; cached query has no NVS/SD/radio operations; baseline source preserved")

    with tempfile.TemporaryDirectory(prefix="f23d_readiness_") as td:
        temp=Path(td);exe=temp/"host.exe"
        def build(text):
            src=temp/"probe.cpp";src.write_text(text,encoding="utf-8")
            run_cmd([cxx,"-std=c++17","-O2","-Wall","-Wextra","-Werror","-I",str(ROOT),str(src),"-o",str(exe)])
            return run_cmd([str(exe)])
        print(build((ROOT/"tests/f23d_readiness_host.cpp").read_text()).strip())
        query_stubs=r'''
#define THESIS_F23D_EVENT_QUERY 1
#include <cassert>
#include <cstdio>
#include <string>
#include "firmware/shared/relay/f23d_lab_diagnostics.h"
#include "firmware/shared/relay/f23d_evidence_queue.h"
static uint32_t nextEmergencyId=42,f23dMaxWriteUs=0,f23dMaxEnqueueUs=0;
static bool nvsReady=true,activeEmergency=false,sosPending=false,cancelPending=false;
namespace VesselIdentity {static uint32_t id=1;static bool valid=true;uint32_t nodeId(){return id;}bool ready(){return valid;}}
namespace F05Recovery {static bool storageBlocked=false,recoveredCancelNeedsHold=false;}
static uint32_t millis(){return 100;}
static F23DReadiness::EvidenceQueue f23dEvidenceQueue;
struct Writer{std::string text;int availableForWrite(){return 1024;}size_t write(const uint8_t* p,size_t n){text.append((const char*)p,n);return n;}};
'''
        query_test=r'''
int main(){
 Writer w;assert(!f23dHandleDiagnosticCommand("TEST ALL"));assert(!f23dHandleDiagnosticCommand("R23D SELECT 1 42"));
 assert(f23dHandleDiagnosticCommand("R23D NEXT"));
 while(f23dEvidenceQueue.size())f23dEvidenceQueue.drain(w);
 assert(w.text.find("next=42")!=std::string::npos && w.text.find("assessment=READY")!=std::string::npos);
 assert(nextEmergencyId==42 && nvsReady && !activeEmergency && !sosPending && !cancelPending);
 w.text.clear();activeEmergency=true;
 assert(f23dHandleDiagnosticCommand("R23D NEXT"));
 while(f23dEvidenceQueue.size())f23dEvidenceQueue.drain(w);
 assert(w.text.find("assessment=BLOCKED")!=std::string::npos && nextEmergencyId==42);
 std::puts("PASS: actual cached collector and serial-query body execute without hardware/NVS APIs");
}
'''
        print(build(query_stubs+collector+command+query_test).strip())
        adapter=extract(source,"// F23D adapter body begin\n","// F23D adapter body end")
        adapter_stubs=r'''
#define THESIS_F23D_RELAY_EVIDENCE 1
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "firmware/shared/relay/f23d_run_evidence.h"
static F23DBench::RunControl f23dControl;
static F23DReadiness::RunEvidence f23dRunEvidence;
static F23DBench::Context ctx;
static F23DBench::Context f23dContext(){return ctx;}
static int calls=0,f23dLastRxRestoreRc=-999;
static constexpr int RADIOLIB_ERR_NONE=0;
static std::vector<std::string> stages;
static void f23dQueueRunEvidence(const char* stage,int,const VesselPacket*){stages.emplace_back(stage);}
static int transmitWithIdentity(const uint8_t*,size_t n){assert(n==62 && stages.back()=="RADIO_INVOKE" && f23dControl.attempts()==1);++calls;return 0;}
static void startReceive(){f23dLastRxRestoreRc=-9;}
'''
        adapter_test=r'''
int main(){
 ctx.identityReady=true;ctx.self=2;ctx.physicalPermission=true;ctx.recoverySafe=true;
 assert(f23dControl.command("R23D SELECT 1 42",ctx,0,99)==F23DBench::Control::Selected);
 assert(f23dControl.command("R23D ARM 99",ctx,0,0)==F23DBench::Control::Armed);
 VesselPacket p{};p.magic=PACKET_MAGIC;p.version=1;p.packetType=PACKET_SOS;p.originNodeId=1;p.sequenceNumber=42;
 assert(f23dControl.observe(p,ctx,true,0)==F23CLab::Admission::Accepted);
 F23DBenchRadio radio;int rc=0;assert(f23dControl.dispatch(ctx,250,radio,rc));
 assert(calls==1 && rc==0 && !f23dControl.armed() && f23dControl.attempts()==1);
 assert(stages.front()=="RESERVED" && stages.back()=="RADIO_RESULT");
 assert(!f23dControl.dispatch(ctx,1000,radio,rc));
 std::puts("PASS: actual evidence adapter reserves before invocation and revokes on RX restoration failure");
}
'''
        print(build(adapter_stubs+adapter+adapter_test).strip())
        required={"THESIS_F22_FLEET_OPERATIONAL":1,"THESIS_F05_RECOVERY":1,
            "THESIS_F06_SD_SANDBOX":1,"THESIS_SD_LOGGER":1,
            "THESIS_V003_F19_DUAL_DISPLAY":1,"THESIS_V003_F15_SD_PILOT_ARMED":1}
        cases=[({},True)]
        for flag in ("THESIS_F23D_EVENT_QUERY","THESIS_F23D_RX_OBSERVER","THESIS_F23D_RELAY_EVIDENCE"):
            flags={**required,flag:1}
            if flag=="THESIS_F23D_RELAY_EVIDENCE":
                flags.update(THESIS_F23D_CANDIDATE=1,THESIS_F23D_OFFLINE_REVIEW=1)
            cases.append((flags,True))
            cases.append(({flag:2},False))
            for key in required:cases.append(({**flags,key:0},False))
            cases.append(({**flags,"THESIS_LAB_RELAY_TX":1},False))
            cases.append(({**flags,"THESIS_F23D_PHYSICAL_PERMISSION":1},False))
            cases.append(({**flags,"THESIS_F23C_BUILD_ONLY":1},False))
        cases.append(({**required,"THESIS_F23D_EVENT_QUERY":1,"THESIS_F23D_RX_OBSERVER":1},False))
        src=temp/"guard.cpp";src.write_text('#include "firmware/shared/relay/f23d_readiness_contract.h"\nint main(){}\n')
        for flags,success in cases:
            args=[cxx,"-std=c++17","-fsyntax-only","-I",str(ROOT)]
            args+=["-D"+k+"="+str(v) for k,v in flags.items()]
            r=subprocess.run(args+[str(src)],capture_output=True,text=True)
            assert (r.returncode==0)==success,str(flags)+r.stderr
        src.write_text('#include "firmware/shared/relay/f23d_permission_qualification.h"\nint main(){}\n')
        r=subprocess.run([cxx,"-std=c++17","-DARDUINO=1","-fsyntax-only","-I",str(ROOT),str(src)],capture_output=True,text=True)
        assert r.returncode!=0 and "host-only" in r.stderr
        print(f"PASS: {len(cases)} readiness compile gates; host permission model rejected by Arduino builds")
    # Conservative illustrative timeline: one-shot success, no retry/collision.
    toa=0.484352;processing=0.100;reaction=0.400;debounce=0.045
    budgets=[]
    for event in range(1,6):
        sos_slot=(350+3*980+(event*13%5)*37)/1000
        cancel_slot=(300+3*980+(event*17%5)*37)/1000
        rx_b=1.5+debounce+sos_slot+toa
        tx_b=max(rx_b+0.250,rx_b+toa)+processing
        rx_c=tx_b+toa
        def finish(delay):
            cancel_rx_b=rx_c+delay+cancel_slot+toa
            cancel_tx_b=max(cancel_rx_b+0.250,cancel_rx_b+toa,tx_b+1.000)+processing
            return cancel_tx_b+toa
        budgets.append({"event_mod5":event%5,"serial_finish_s":round(finish(reaction),6),
                        "button_finish_s":round(finish(5.0+debounce),6)})
    assert max(x["serial_finish_s"] for x in budgets)<15
    assert min(x["button_finish_s"] for x in budgets)>15
    print("PASS: timing budget retains 15s limit; serial-assisted path fits assumed bounds, button-only path exceeds it")
    print(json.dumps(budgets,separators=(",",":")))
if __name__=="__main__":run()
