"""F23D offline controller, actual adapter body, source isolation and compile gates."""
from pathlib import Path
import configparser
import os
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
BASE="0463ebc158938909c84d2991e6e3435dbb6dfbf6"
BEGIN="// F23D begin"
END="// F23D end"

def run_cmd(args):
    r=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,encoding="utf-8")
    assert r.returncode==0,r.stdout+r.stderr
    return r.stdout

def remove_readiness(text):
    result=[];inside=False
    for line in text.splitlines(keepends=True):
        if line.strip()=="// F23D readiness begin":
            assert not inside
            inside=True
        elif line.strip()=="// F23D readiness end":
            assert inside
            inside=False
        elif not inside: result.append(line)
    assert not inside
    return "".join(result)

def strip(text):
    out=[];inside=False
    for line in text.splitlines(keepends=True):
        if line.strip()==BEGIN:
            assert not inside
            inside=True
        elif line.strip()==END:
            assert inside
            inside=False
        elif not inside:out.append(line)
    assert not inside
    return "".join(out)

def run():
    cxx=shutil.which("g++");assert cxx
    for path in ("firmware/universal/src/main.cpp","firmware/universal/src/lora_manager.cpp",
                 "firmware/universal/src/weather_manager.cpp"):
        before=run_cmd(["git","show",BASE+":"+path])
        after=(ROOT/path).read_text(encoding="utf-8")
        accepted=run_cmd(["git","show","1fc20613ffb8457c2b1552fdcc62379c9176b638:"+path])
        assert remove_readiness(after)==accepted,path+": unguarded readiness edits"
        after=remove_readiness(after)
        assert strip(after)==before,path
    for folder in ("firmware/shared/communication","firmware/shared/storage","firmware/shared/hardware"):
        assert not run_cmd(["git","diff",BASE,"--",folder]).strip(),folder
    for path in ("f23c_lab_contract.h","f23c_lab_transaction.h","relay_policy.h",
                 "relay_queue.h","relay_preflight.h","relay_dryrun_scheduler.h",
                 "relay_tx_readiness.h","relay_authorization_contract.h","lab_relay_gate.h"):
        full="firmware/shared/relay/"+path
        assert (ROOT/full).read_text(encoding="utf-8")==run_cmd(["git","show",BASE+":"+full]),path
    ini=(ROOT/"firmware/universal/platformio.ini").read_text()
    assert ini.split("; F23D appended profile",1)[0]==run_cmd(["git","show",BASE+":firmware/universal/platformio.ini"])
    cfg=configparser.ConfigParser(interpolation=None);cfg.read_string(ini)
    env=cfg["env:tbeam-supreme-f23d-v002-bench-candidate-build-only"]
    assert env["extends"]=="env:tbeam-supreme-fleet-universal-operational-build-only"
    assert env["build_flags"].splitlines()==["",
        "${env:tbeam-supreme-fleet-universal-operational-build-only.build_flags}",
        "-D THESIS_F23D_CANDIDATE=1","-D THESIS_F23D_OFFLINE_REVIEW=1"]
    lora=(ROOT/"firmware/universal/src/lora_manager.cpp").read_text()
    adapter=lora.split("// F23D adapter body begin\n",1)[1].split("// F23D adapter body end",1)[0]
    assert "f23dControl.maySubmit(packet, context)" in adapter
    assert adapter.index("maySubmit")<adapter.index("transmitWithIdentity")
    print("PASS: F22/F23C source and safety contracts preserved; isolated guarded physical adapter body")

    with tempfile.TemporaryDirectory(prefix="thesis_f23d_") as td:
        temp=Path(td);exe=temp/("controller.exe" if os.name=="nt" else "controller")
        run_cmd([cxx,"-std=c++17","-O2","-Wall","-Wextra","-Werror","-I",str(ROOT),
                 str(ROOT/"tests/f23d_run_control_host.cpp"),"-o",str(exe)])
        print(run_cmd([str(exe)]).strip())
        stubs=r'''
#include <cassert>
#include <cstdio>
#include "firmware/shared/relay/f23d_run_control.h"
static F23DBench::RunControl f23dControl;
static F23DBench::Context current;
static F23DBench::Context f23dContext(){return current;}
static int calls=0,receives=0;
static VesselPacket submitted{};
static int transmitWithIdentity(const uint8_t* data,size_t length){
  assert(length==sizeof(VesselPacket));memcpy(&submitted,data,length);++calls;return -7;
}
static void startReceive(){++receives;}
static uint32_t millis(){return 250;}
struct SerialMock{template<class...Args>void printf(const char*,Args...){}} Serial;
'''
        test=r'''
int main(){
 current.identityReady=true;current.self=2;current.physicalPermission=true;current.recoverySafe=true;
 assert(f23dControl.command("R23D SELECT 1 42",current,0,99)==F23DBench::Control::Selected);
 assert(f23dControl.command("R23D ARM 99",current,0,0)==F23DBench::Control::Armed);
 VesselPacket p{};p.magic=PACKET_MAGIC;p.version=1;p.packetType=PACKET_SOS;p.originNodeId=1;p.sequenceNumber=42;
 assert(f23dControl.observe(p,current,true,0)==F23CLab::Admission::Accepted);
 F23DBenchRadio adapter;int rc=0;
 assert(f23dControl.dispatch(current,250,adapter,rc));
 assert(calls==1 && receives==1 && rc==-7 && submitted.originNodeId==1 && submitted.hopCount==1);
 assert(f23dControl.attempts()==1);
 assert(adapter.submit(submitted)==-998 && calls==1); // consumed single-use dispatch lease
 assert(!f23dControl.dispatch(current,500,adapter,rc) && calls==1);
 current.physicalPermission=false;
 assert(adapter.submit(submitted)==-998 && calls==1);
 current.physicalPermission=true;current.self=3;
 assert(adapter.submit(submitted)==-998 && calls==1);
 current.self=2;current.localControlBusy=true;
 assert(adapter.submit(submitted)==-998 && calls==1);
 std::puts("PASS: actual F23D radio adapter body compiled/executed with mocked radio only");
}
'''
        src=temp/"adapter.cpp";src.write_text(stubs+adapter+test)
        run_cmd([cxx,"-std=c++17","-Wall","-Wextra","-Werror","-I",str(ROOT),str(src),"-o",str(exe)])
        print(run_cmd([str(exe)]).strip())
        required={"THESIS_F23D_CANDIDATE":1,"THESIS_F23D_OFFLINE_REVIEW":1,
                  "THESIS_F22_FLEET_OPERATIONAL":1,"THESIS_F05_RECOVERY":1,
                  "THESIS_F06_SD_SANDBOX":1,"THESIS_SD_LOGGER":1,
                  "THESIS_V003_F19_DUAL_DISPLAY":1,"THESIS_V003_F15_SD_PILOT_ARMED":1}
        matrix=[({},True),(required,True)]
        matrix += [({**required,k:0},False) for k in required if k!="THESIS_F23D_CANDIDATE"]
        matrix += [({"THESIS_F23D_CANDIDATE":2},False),
                   ({"THESIS_F23D_PHYSICAL_PERMISSION":1},False),
                   ({**required,"THESIS_F23D_PHYSICAL_PERMISSION":1},False),
                   ({**required,"THESIS_F23C_BUILD_ONLY":1},False),
                   ({**required,"THESIS_LAB_RELAY_TX":1},False),
                   ({**required,"THESIS_V003_F13_LIVE_OUTPUTS":1},False)]
        src.write_text('#include "firmware/shared/relay/f23d_bench_contract.h"\nint main(){}\n')
        for defines,expected in matrix:
            args=[cxx,"-std=c++17","-fsyntax-only","-I",str(ROOT)]
            args += ["-D"+k+"="+str(v) for k,v in defines.items()]
            result=subprocess.run(args+[str(src)],capture_output=True,text=True)
            assert (result.returncode==0)==expected,str(defines)+result.stderr
        print(f"PASS: {len(matrix)} F23D positive/negative compile gates; physical permission cannot be installed")
if __name__=="__main__":run()
