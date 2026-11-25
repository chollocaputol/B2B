#!/usr/bin/env python3
"""F24T static safety evidence, not proof of a physical SD owner."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=root/'firmware/shared/storage'
log=(base/'thesis_sd_logger.h').read_text()
q=(base/'dtn_f24q_cross_writer_arbiter.h').read_text()
r=(base/'dtn_f24r_guarded_file_port.h').read_text()
t=(base/'dtn_f24t_history_directory.h').read_text()
ini=((root/'firmware/universal/platformio.ini').read_text()+'\n'+(root/'tests/fixtures/dtn/sf7_intermediate_profiles.ini').read_text())
checks=[
 ('default off','#define THESIS_DTN_F24T_BUILD_ONLY 0' in log),
 ('F24S required','THESIS_DTN_F24S_BUILD_ONLY != 1' in log),
 ('F24R required','THESIS_DTN_F24R_BUILD_ONLY != 1' in log),
 ('F24Q required','THESIS_DTN_F24Q_BUILD_ONLY != 1' in log),
 ('F24K deny required','THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1' in log),
 ('F06 sandbox required','THESIS_F06_SD_SANDBOX != 1' in log),
 ('F23D disabled','THESIS_F23D_SD_EVIDENCE != 0' in log),
 ('Fleet disabled','THESIS_F22_FLEET_OPERATIONAL != 0' in log),
 ('Relay disabled','THESIS_LAB_RELAY_TX != 0' in log),
 ('GPIO disabled','THESIS_V003_F13_LIVE_OUTPUTS != 0' in log),
 ('F24T named adapter','f24tHistoryDirectory(f24rLegacyFiles)' in log),
 ('Actual logger invokes bounded scanner','f24tHistoryDirectory.scan(root,vesselId' in log),
 ('Previous raw directory preserved in else','File directory = interposedSD.open(root, FILE_READ);' in log),
 ('Previous raw child preserved in else','while (File entry = directory.openNextFile())' in log),
 ('F24Q F23D two-handle preserved','if(slot==1U && (active_!=Lane::F23D || !sawAppend_))return false;' in q),
 ('F24Q history child explicit','bool openHistoryChild(const Lease& l)' in q),
 ('F24Q generic F23D slot preserved','if(slot==1U && (active_!=Lane::F23D || !sawAppend_))return false;' in q),
 ('F24Q parent must remain until child closes','(active_==Lane::F23D || active_==Lane::History) && slot==0U && handles_[1]' in q),
 ('F24Q scoped slot query','bool hasHandle(uint8_t slot)const' in q),
 ('F24R guards openNextFile','!allow(Action::Enumerate)' in r),
 ('F24R guards name()','const char* name(){return allow(Action::Size)?raw_.name():nullptr;}' in r),
 ('F24R child opening guarded','arbiter_.openHistoryChild(lease_)' in r),
 ('F24R slot-specific validity','arbiter_.hasHandle(slot)' in r),
 ('F24T uses 64-entry scan bound','kMaxVisited=64' in t),
 ('F24T uses 48-output cap','kMaxShown=48' in t),
 ('F24T denies bad paths','validRoot(root,vessel)' in t),
 ('F24T rejects foreign child','strncmp(raw,root,rn)!=0' in t),
 ('F24T complete only on exhausted','if(!result.faulted)result.complete=true;' in t),
 ('F24T marks fault incomplete','if(result.faulted)result.complete=false;' in t),
 ('F24T no physical SD touch','#include <SD.h>' not in t),
 ('F24T no radio activation','RadioLib' not in t),
 ('F24T no real lease grants','DenyWitness' in log and 'f24rNoLease' in log),
 ('F24T inherits F24S profile','extends = env:tbeam-supreme-f24s-temporary-file-deny-build-only' in ini),
 ('F24T build flag on only','-D THESIS_DTN_F24T_BUILD_ONLY=1' in ini),
 ('F24T documents unresolved destructor','Real Arduino File teardown after revocation is NOT approved' in t),
]
for name,passed in checks:
 if not passed:raise AssertionError('F24T SOURCE CONTRACT FAILED: '+name)
print(f'F24T source-scoped deny-only invariants PASS: {len(checks)} checks; source directory guarded; hardware teardown unproven')
