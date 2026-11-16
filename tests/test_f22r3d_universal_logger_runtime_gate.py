"""F22R3D dry-run SD logger: pure authorization and real-source guard checks."""
from pathlib import Path
import os,subprocess,tempfile,shutil
ROOT=Path(__file__).resolve().parents[1]
LOGGER=ROOT/'firmware/shared/storage/thesis_sd_logger.h'
MAIN=ROOT/'firmware/universal/src/main.cpp'
INI=ROOT/'firmware/universal/platformio.ini'
HEADER=ROOT/'firmware/shared/storage/f22_fleet_storage_runtime_gate.h'
CPP=r'''
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include "firmware/shared/storage/f22_fleet_storage_runtime_gate.h"
using namespace F22FleetStorageRuntimeGate;
int main(){
  for(uint32_t id: {1U,2U,3U,4U,9999U}) {
    auto deny=unreviewed(id,true);
    assert(deny.identityAccepted && bound(deny,id));
    assert(!mountAllowed(deny,id) && !urgentAllowed(deny,id));
    assert(!routineAllowed(deny,id) && !researchAllowed(deny,id,true));
    assert(!ownHistoryAllowed(deny,id) && !legacyV002Allowed(deny,id));
    assert(!deny.formatOrEraseAllowed && !deny.physicalOutputsEnabled && !deny.autoEnroll);
    auto invalid=unreviewed(id,false);
    assert(!mountAllowed(invalid,id));
    F22FleetStoragePolicy::Evidence reviewed{};
    reviewed.vesselId=id; reviewed.provisionedIdentity=true;
    reviewed.boardAndSdPinsReviewed=true; reviewed.sdMediaPresent=true;
    reviewed.sdMediaReviewed=true; reviewed.historyProtectionApproved=true;
    reviewed.sdExplicitOptIn=true; reviewed.isolatedF06SessionConfirmed=true;
    reviewed.continuousResearchConsent=true;
    auto d=F22FleetStoragePolicy::evaluate(reviewed);
    assert(mountAllowed(d,id) && urgentAllowed(d,id));
    assert(routineAllowed(d,id) && researchAllowed(d,id,true));
    assert(!researchAllowed(d,id,false));
    assert(ownHistoryAllowed(d,id) && !legacyV002Allowed(d,id));
    assert(!mountAllowed(d,(id==9999U)?1U:id+1U));
    assert(!ownHistoryAllowed(d,(id==9999U)?1U:id+1U));
    assert(!d.formatOrEraseAllowed);
    reviewed.sdMediaReviewed=false;
    assert(!mountAllowed(F22FleetStoragePolicy::evaluate(reviewed),id));
    reviewed.sdMediaReviewed=true;reviewed.isolatedF06SessionConfirmed=false;
    d=F22FleetStoragePolicy::evaluate(reviewed);
    assert(mountAllowed(d,id) && !urgentAllowed(d,id));
    assert(!researchAllowed(d,id,true));
    char root[48]={};assert(F22FleetStoragePolicy::ownSandboxRoot(root,sizeof(root),id));
    char session[80]={}; assert(F22FleetStoragePolicy::ownSessionPath(session,sizeof(session),id,"S12ABC34"));
    assert(std::strstr(session,root)==session);
    assert(!F22FleetStoragePolicy::ownSessionPath(session,sizeof(session),id,"../../V002"));
  }
  for(uint32_t id: {0U,10000U,65535U}) assert(!mountAllowed(unreviewed(id,true),id));
  F22FleetStoragePolicy::Evidence e{};
  e.vesselId=2U;e.provisionedIdentity=true;e.boardAndSdPinsReviewed=true;
  e.sdMediaPresent=true;e.sdMediaReviewed=true;e.historyProtectionApproved=true;
  e.sdExplicitOptIn=true;e.legacyV002ReadOnlyOptIn=true;
  auto d=F22FleetStoragePolicy::evaluate(e);
  assert(legacyV002Allowed(d,2U));assert(!legacyV002Allowed(d,3U));
  char oldpath[64];assert(F22FleetStoragePolicy::legacyV002ReadPath(oldpath,sizeof(oldpath),d,2U));
  assert(std::strcmp(oldpath,"/THESIS/VESSEL_002")==0);
  assert(!F22FleetStoragePolicy::legacyV002ReadPath(oldpath,sizeof(oldpath),d,3U));
  std::cout<<"PASS: ID1/2/3/4/9999 dry-run denied, hypothetical reviews bound to own ID, V002 legacy isolated, no format\n";
}
'''
def run():
    logger=LOGGER.read_text(encoding='utf-8');ini=INI.read_text(encoding='utf-8')
    main=MAIN.read_text(encoding='utf-8');header=HEADER.read_text(encoding='utf-8')
    flag='THESIS_F22R3D_FLEET_SD_DRYRUN'
    assert flag in logger and ('-D '+flag+'=1') in ini
    assert '#include "f22_fleet_storage_runtime_gate.h"' in logger
    block=logger.split('inline bool isAuthorizedVessel(uint32_t id) {',1)[1].split('\n}',1)[0]
    assert block.index('#if '+flag+' == 1') < block.index('#elif THESIS_F22R3C_FAILCLOSED_LOGGER_CONTRACT == 1')
    assert 'return F22FleetStorageRuntimeGate::mountAllowed(d,id);' in block
    assert 'return id >= 1 && id <= 3;' in block and 'return id == 2;' in block
    assert logger.count('F22FleetStorageRuntimeGate::researchAllowed(')==2
    assert 'F22FleetStorageRuntimeGate::routineAllowed(' in logger
    assert 'F22FleetStorageRuntimeGate::ownHistoryAllowed(' in logger
    assert 'F22FleetStorageRuntimeGate::legacyV002Allowed(' in logger
    assert '#error "F22R3D dry-run needs F22R3C failclosed, F19 build-only, SD/relay/GPIO OFF"' in logger
    begin=logger.index('inline bool begin(XPowersAXP2101& pmu, uint32_t id)')
    assert begin < logger.index('if (!isAuthorizedVessel(vesselId))',begin) < logger.index('pmu.setBLDO1Voltage(3300)',begin) < logger.index('SD.begin(',begin)
    assert 'SD.begin(47, sdSPI, 1000000, "/sd", 5, false)' in logger
    assert '/THESIS/F06TEST' in logger and 'vesselId = id' in logger
    assert 'VesselIdentity::load();' in main
    assert 'never reads NVS, SD, SPI' in header and 'unreviewed(' in header
    profile='[env:tbeam-supreme-f22r3d-fleet-sd-dryrun-build-only]'
    assert ini.count(profile)==1
    env=ini.split(profile,1)[1].split('\n[env:',1)[0]
    assert 'extends = env:tbeam-supreme-f22r3c-failclosed-logger-contract-build-only' in env
    assert '-D '+flag+'=1' in env and 'SD_PILOT_ARMED=1' not in env
    compiler=shutil.which('g++');assert compiler,'g++ not available'
    with tempfile.TemporaryDirectory(prefix='thesis_f22r3d_') as td:
        t=Path(td);src=t/'policy.cpp';src.write_text(CPP,encoding='utf-8')
        exe=t/('f22r3d_policy.exe' if os.name=='nt' else 'f22r3d_policy')
        c=subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(ROOT),str(src),'-o',str(exe)],capture_output=True,text=True)
        assert c.returncode==0,c.stderr
        r=subprocess.run([str(exe)],capture_output=True,text=True)
        assert r.returncode==0,r.stderr
        print(r.stdout.strip())
    print('PASS: real logger failclosed before PMU/SD.begin; no legacy F21/F22 profile modification')
    print('PASS: research/history/SD permission guards on new opt-in dry-run profile')
if __name__=='__main__':run()
