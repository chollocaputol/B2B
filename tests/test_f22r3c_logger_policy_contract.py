"""F22R3C source-contract and pure host C++ tests; never mounts SD or probes hardware."""
from pathlib import Path
import os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
LOGGER=ROOT/'firmware/shared/storage/thesis_sd_logger.h'
INI=ROOT/'firmware/universal/platformio.ini'
POLICY=ROOT/'firmware/shared/storage/f22_fleet_storage_policy.h'
MAIN=ROOT/'firmware/universal/src/main.cpp'
HEADER_FLAG='THESIS_F22R3C_FAILCLOSED_LOGGER_CONTRACT'
CPP=r'''
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include "firmware/shared/storage/f22_fleet_storage_policy.h"
using namespace F22FleetStoragePolicy;
int main() {
 for (uint32_t id: {1U,2U,3U,4U,9999U}) {
   Evidence e{}; e.vesselId=id; e.provisionedIdentity=true;
   auto d=evaluate(e);
   assert(d.identityAccepted && d.checkedVesselId==id);
   assert(!d.mountEligible && !d.newSessionWriteEligible);
   assert(!d.urgentJournalEligible && !d.routineJournalEligible && !d.researchJournalEligible);
   assert(!d.ownF06HistoryReadEligible && !d.legacyV002HistoryReadEligible);
   assert(!d.formatOrEraseAllowed && !d.physicalOutputsEnabled && !d.autoEnroll);
   char p[80]{};
   assert(ownSandboxRoot(p,sizeof(p),id));
   assert(std::strstr(p,"/THESIS/F06TEST/V")==p);
 }
 for (uint32_t id: {0U,10000U,65535U}) {
   Evidence e{}; e.vesselId=id; e.provisionedIdentity=true;
   assert(!evaluate(e).identityAccepted && !evaluate(e).mountEligible);
 }
 Evidence e{};e.vesselId=3;e.provisionedIdentity=false;
 assert(!evaluate(e).identityAccepted);
 std::cout<<"PASS: ID1..9999 identity accepted, SD mount denied for all without media review\n";
}
'''
def run():
    logger=LOGGER.read_text(encoding='utf-8')
    ini=INI.read_text(encoding='utf-8')
    main=MAIN.read_text(encoding='utf-8')
    policy=POLICY.read_text(encoding='utf-8')
    assert HEADER_FLAG in logger and HEADER_FLAG+'=1' in ini
    assert '#include "f22_fleet_storage_policy.h"' in logger
    assert '#include "../hardware/vessel_identity.h"' in logger
    new=logger.split('inline bool isAuthorizedVessel(uint32_t id) {',1)[1].split('\n}',1)[0]
    assert new.count('F22FleetStoragePolicy::evaluate(evidence)')==1
    assert 'evidence.provisionedIdentity = VesselIdentity::ready() && VesselIdentity::nodeId() == id;' in new
    assert 'return decision.mountEligible;' in new
    assert '#elif THESIS_SD_FLEET == 1\n    return id >= 1 && id <= 3;' in new
    assert 'return id == 2;' in new
    assert '#error "F22R3C logger contract requires multi-ID build-only, SD pilot OFF, GPIO/relay OFF"' in logger
    assert 'THESIS_V003_F15_SD_PILOT_ARMED != 0' in logger
    assert 'THESIS_LAB_RELAY_TX != 0' in logger
    assert 'THESIS_V003_F13_LIVE_OUTPUTS != 0' in logger
    begin=logger.index('inline bool begin(XPowersAXP2101& pmu, uint32_t id)')
    auth=logger.index('if (!isAuthorizedVessel(vesselId))',begin)
    assert begin < auth < logger.index('pmu.setBLDO1Voltage(3300)',begin) < logger.index('SD.begin(',begin)
    assert main.count('VesselIdentity::load();') >= 1
    assert 'formatOrEraseAllowed = false' in policy
    assert 'legacyV002HistoryReadEligible = d.mountEligible &&' in policy
    profile='[env:tbeam-supreme-f22r3c-failclosed-logger-contract-build-only]'
    assert ini.count(profile)==1
    block=ini.split(profile,1)[1].split('\n[env:',1)[0]
    assert 'extends = env:tbeam-supreme-f22r3b-storage-policy-preview-build-only' in block
    assert '-D THESIS_F22R3C_FAILCLOSED_LOGGER_CONTRACT=1' in block
    assert 'THESIS_V003_F15_SD_PILOT_ARMED=1' not in block
    print('PASS: logger isAuthorizedVessel wired to F22 policy with absent media approval')
    print('PASS: old V002/V003 paths preserved; SD mount authorization precedes PMU and SD.begin')
    compiler=shutil.which('g++')
    assert compiler is not None,'g++ missing from PATH'
    with tempfile.TemporaryDirectory(prefix='thesis_f22r3c_') as d:
        t=Path(d);src=t/'contract.cpp';src.write_text(CPP,encoding='utf-8')
        exe=t/('contract.exe' if os.name=='nt' else 'contract')
        c=subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(ROOT),str(src),'-o',str(exe)],capture_output=True,text=True)
        assert c.returncode==0,c.stderr
        r=subprocess.run([str(exe)],capture_output=True,text=True)
        assert r.returncode==0,r.stderr
        print(r.stdout.strip())
    print('PASS: F22R3C fail-closed host policy contract; no SD hardware operations')
if __name__=='__main__':run()
