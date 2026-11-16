"""F22R3B host-only C++ SD permissions and per-vessel path regression.
No microSD mount, filesystem writes, GPIO, NVS writes, firmware upload or radio.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT/'firmware/shared/storage/f22_fleet_storage_policy.h'
MAIN = ROOT/'firmware/universal/src/main.cpp'
INI = ROOT/'firmware/universal/platformio.ini'
LOGGER = ROOT/'firmware/shared/storage/thesis_sd_logger.h'
PACKET = ROOT/'firmware/shared/communication/packet_protocol.h'
CXX = r'''
#include <cassert>
#include <cstring>
#include <iostream>
#include "f22_fleet_storage_policy.h"
using namespace F22FleetStoragePolicy;
int main() {
  char root[64] = {};
  for (unsigned long id: {1UL,2UL,3UL,4UL,9999UL}) {
    assert(ownSandboxRoot(root,sizeof(root),id));
    char expected[64]={};
    snprintf(expected,sizeof(expected),"/THESIS/F06TEST/V%03lu",id);
    assert(strcmp(root,expected)==0);
    char session[64]={};
    assert(ownSessionPath(session,sizeof(session),id,"S0ABCDE1"));
    char expectedSession[80]={};
    snprintf(expectedSession,sizeof(expectedSession),"%s/S0ABCDE1",root);
    assert(strcmp(session,expectedSession)==0);
    Evidence e{};
    e.vesselId=id;
    e.provisionedIdentity=true;
    Decision d=evaluate(e);
    assert(d.identityAccepted && !d.mountEligible && !d.newSessionWriteEligible);
    assert(!d.urgentJournalEligible && !d.routineJournalEligible && !d.researchJournalEligible);
    assert(!d.formatOrEraseAllowed && !d.physicalOutputsEnabled && !d.autoEnroll);
    e.boardAndSdPinsReviewed=true;
    e.sdMediaPresent=true;
    e.sdMediaReviewed=true;
    e.historyProtectionApproved=true;
    e.sdExplicitOptIn=true;
    d=evaluate(e);
    assert(d.mountEligible && d.ownF06HistoryReadEligible);
    assert(!d.newSessionWriteEligible && !d.urgentJournalEligible);
    e.isolatedF06SessionConfirmed=true;
    d=evaluate(e);
    assert(d.urgentJournalEligible && !d.routineJournalEligible && !d.researchJournalEligible);
    e.sosOrTailActive=true;
    d=evaluate(e);
    assert(d.routineJournalEligible && !d.researchJournalEligible);
    e.sosOrTailActive=false;
    e.continuousResearchConsent=true;
    d=evaluate(e);
    assert(d.routineJournalEligible && d.researchJournalEligible);
    e.legacyV002ReadOnlyOptIn=true;
    d=evaluate(e);
    assert(d.checkedVesselId == id);
    assert(d.legacyV002HistoryReadEligible == (id==2));
    char legacy[64] = {};
    assert(legacyV002ReadPath(legacy,sizeof(legacy),d,id)==(id==2));
    if(id==2) {
      assert(strcmp(legacy,"/THESIS/VESSEL_002")==0);
      Decision mismatched = d; mismatched.checkedVesselId = 3U;
      assert(!legacyV002ReadPath(legacy,sizeof(legacy),mismatched,id));
    }
    e.provisionedIdentity=false;
    d=evaluate(e);
    assert(!d.identityAccepted && !d.mountEligible && !d.researchJournalEligible);
  }
  assert(!ownSandboxRoot(root,sizeof(root),0));
  assert(!ownSandboxRoot(root,sizeof(root),10000));
  assert(!ownSandboxRoot(root,7,1));
  assert(!ownSessionPath(root,sizeof(root),3,"../secret"));
  assert(!ownSessionPath(root,sizeof(root),3,"Sabc1234"));
  assert(!ownSessionPath(root,sizeof(root),3,"S01234567"));
  assert(!ownSessionPath(root,sizeof(root),3,"S0000000/x"));
  assert(!ownSessionPath(root,12,9999,"S00ABCDE"));
  Evidence wrong{}; wrong.vesselId=0; wrong.provisionedIdentity=true;
  wrong.boardAndSdPinsReviewed=wrong.sdMediaPresent=wrong.sdMediaReviewed=true;
  wrong.historyProtectionApproved=wrong.sdExplicitOptIn=wrong.isolatedF06SessionConfirmed=true;
  assert(!evaluate(wrong).mountEligible);
  wrong.vesselId=10000;
  assert(!evaluate(wrong).mountEligible);
  std::cout << "PASS: IDs 1/2/3/4/9999; fail-closed storage eligibility; V002 read-only history; isolated paths; no write/erase calls\n";
}
'''

def run():
    assert HEADER.is_file() and MAIN.is_file() and INI.is_file()
    compiler = os.environ.get('CXX') or shutil.which('g++')
    assert compiler, 'g++ not found (add MSYS2 compiler to PATH)'
    with tempfile.TemporaryDirectory(prefix='thesis_f22r3b_') as td:
        td=Path(td)
        cpp=td/'policy.cpp'; cpp.write_text(CXX,encoding='utf-8')
        exe=td/('policy.exe' if os.name=='nt' else 'policy')
        cmd=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(HEADER.parent),str(cpp),'-o',str(exe)]
        proc=subprocess.run(cmd,capture_output=True,text=True)
        assert proc.returncode==0,proc.stderr
        proc=subprocess.run([str(exe)],capture_output=True,text=True)
        assert proc.returncode==0,proc.stderr
        print(proc.stdout,end='')
    main=MAIN.read_text(encoding='utf-8')
    ini=INI.read_text(encoding='utf-8')
    logger=LOGGER.read_text(encoding='utf-8')
    packet=PACKET.read_text(encoding='utf-8')
    assert main.count('THESIS_F22R3B_STORAGE_POLICY_PREVIEW')>=3
    assert '#include "../../shared/storage/f22_fleet_storage_policy.h"' in main
    assert 'F22R3B-STORAGE-PREVIEW' in main
    assert 'THESIS_V003_F15_SD_PILOT_ARMED != 0' in main
    assert 'THESIS_F22R3A_STANDARD_FLEET_PREVIEW != 1' in main
    assert 'F22FleetStoragePolicy::Evidence f22r3bEvidence{};' in main
    assert 'f22r3bEvidence.provisionedIdentity = VesselIdentity::ready();' in main
    assert 'F22FleetStoragePolicy::evaluate(f22r3bEvidence)' in main
    assert main.count('#line 212') == 1 and main.count('#line 913') == 1
    assert 'tbeam-supreme-f22r3b-storage-policy-preview-build-only' in ini
    env=ini.split('[env:tbeam-supreme-f22r3b-storage-policy-preview-build-only]',1)[1].split('\n[env:',1)[0]
    assert 'extends = env:tbeam-supreme-f22r3a-standard-v003-multiid-build-only' in env
    assert 'THESIS_F22R3B_STORAGE_POLICY_PREVIEW=1' in env
    assert 'THESIS_V003_F15_SD_PILOT_ARMED=1' not in env
    # Legacy storage driver is intentionally UNCHANGED and still fails closed.
    assert 'return id >= 1 && id <= 3;' in logger
    assert 'if (!ready || vesselId != 3U)' in logger
    assert 'SD.begin(47, sdSPI, 1000000, "/sd", 5, false)' in logger
    assert 'PACKET_VERSION' in packet and 'struct' in packet
    header=HEADER.read_text(encoding='utf-8')
    for forbidden in ('SD.begin','SD.open','SD.mkdir','Preferences','pinMode(', 'digitalWrite(', 'Serial.', 'transmitSOS('):
        assert forbidden not in header,forbidden
    print('PASS: passive-only main/INI preview; untouched F15/F19 SD and packet contracts; output/relay OFF')

if __name__=='__main__':
    run()
