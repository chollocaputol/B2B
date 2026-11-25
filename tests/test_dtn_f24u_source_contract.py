#!/usr/bin/env python3
"""F24U static source contracts only: never assert physical SD teardown is safe."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=root/'firmware/shared/storage'
q=(base/'dtn_f24q_cross_writer_arbiter.h').read_text()
r=(base/'dtn_f24r_guarded_file_port.h').read_text()
u=(base/'dtn_f24u_teardown_coordinator.h').read_text()
log=(base/'thesis_sd_logger.h').read_text()
ini=((root/'firmware/universal/platformio.ini').read_text()+'\n'+(root/'tests/fixtures/dtn/sf7_intermediate_profiles.ini').read_text())
checks=[
 ('F24U default disabled','#define THESIS_DTN_F24U_BUILD_ONLY 0' in log),
 ('F24U enabled only 0 or 1','THESIS_DTN_F24U_BUILD_ONLY != 0 && THESIS_DTN_F24U_BUILD_ONLY != 1' in log),
 ('F24T required','THESIS_DTN_F24T_BUILD_ONLY != 1' in log),
 ('F24S required','THESIS_DTN_F24S_BUILD_ONLY != 1' in log),
 ('F24R required','THESIS_DTN_F24R_BUILD_ONLY != 1' in log),
 ('F24Q required','THESIS_DTN_F24Q_BUILD_ONLY != 1' in log),
 ('F24K deny required','THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1' in log),
 ('F06 sandbox required','THESIS_F06_SD_SANDBOX != 1' in log),
 ('F23D physical disabled','THESIS_F23D_SD_EVIDENCE != 0' in log),
 ('Fleet disabled','THESIS_F22_FLEET_OPERATIONAL != 0' in log),
 ('Legacy relay disabled','THESIS_LAB_RELAY_TX != 0' in log),
 ('GPIO disabled','THESIS_V003_F13_LIVE_OUTPUTS != 0' in log),
 ('New coordinator include','#include "dtn_f24u_teardown_coordinator.h"' in log),
 ('Named DenyWitness','f24rNoPhysicalWitness' in log and 'DTNF24Q::DenyWitness' in log),
 ('Zero lease','f24rNoLease{}' in log),
 ('Build-only real logger instance','f24uNoGrantTeardown(f24rLegacyFiles)' in log),
 ('Build-only poll seam','f24uNoGrantTeardown.begin()' in log),
 ('F24U extends F24T','extends = env:tbeam-supreme-f24t-history-iterator-deny-build-only' in ini),
 ('F24U only explicit flag','-D THESIS_DTN_F24U_BUILD_ONLY=1' in ini),
 ('F24U no upload','upload_protocol' not in ini.split('[env:tbeam-supreme-f24u-teardown-deny-build-only]')[-1]),
 ('Arbiter retirement method','bool beginRetirement(const Lease& l)' in q),
 ('Arbiter finish method','bool finishRetirement(const Lease& l)' in q),
 ('Arbiter retirement observable','bool retiring()const' in q),
 ('Retirement prohibits new generic files','if(retiring_ || !check(l) || slot>=2U || handles_[slot])return false;' in q),
 ('Retirement prohibits new history child','if(retiring_ || !check(l) || active_!=Lane::History' in q),
 ('Retirement stops content actions','if(retiring_ && action!=Action::Flush)return false;' in q),
 ('Flush pre-owner recheck','if(!check(l) || slot>=2U || !handles_[slot])return false;' in q),
 ('Retirement requires drained slots','if(!retiring_ || !check(l) || hasOpenHandle())return false;' in q),
 ('Release refuses open slots','if(!check(l) || handles_[0] || handles_[1])return false;' in q),
 ('Release clears retirement','retiring_=false;vessel_=0;' in q),
 ('Original F23D slot contract','if(slot==1U && (active_!=Lane::F23D || !sawAppend_))return false;' in q),
 ('Original history child path','bool openHistoryChild(const Lease& l)' in q),
 ('Parent close still blocked with child','(active_==Lane::F23D || active_==Lane::History) && slot==0U && handles_[1]' in q),
 ('F24R exists disabled when retiring','arbiter_.retiring() || !admitted()' in r),
 ('F24R open disabled when retiring','arbiter_.retiring() || !admitted()' in r),
 ('F24R keeps explicit raw close','if(port_->closeSlot(slot_)){raw_.close();port_=nullptr;}' in r),
 ('F24R has no forced revoke close','raw_.close();port_=nullptr;' in r and 'Revoked: leave tracked handle quarantined' in r),
 ('F24R owner preflight','bool verifyRetirementOwner(){return arbiter_.retiring() && arbiter_.probe(lease_);}' in r),
 ('F24R close-owner drain','bool beginRetirement(){return arbiter_.beginRetirement(lease_);}' in r),
 ('F24R release-owner drain','bool finishRetirement(){return arbiter_.finishRetirement(lease_);}' in r),
 ('No unguarded physical SD in U header','#include <SD.h>' not in u and 'SD.open' not in u),
 ('No radio or cloud in U header','RadioLib' not in u and 'Supabase' not in u),
 ('U tracks unsafe quarantine','UnsafeQuarantined' in u),
 ('U does not claim safe hard removal','no unsafe' in u and 'post-revocation' in u),
 ('U does not auto-close files','close()' not in u and 'flush()' not in u),
 ('U checks owner after drain','volume_.verifyRetirementOwner()' in u),
 ('U checks outstanding handles','volume_.hasOpenHandle()' in u),
 ('U release on all drained','volume_.finishRetirement()' in u),
 ('U cannot release before start','if(!started_)return state_;' in u),
]
for label,ok in checks:
 if not ok:raise AssertionError('F24U SOURCE CONTRACT FAILED: '+label)
print(f'F24U DTN-only deny-build source invariants PASS: {len(checks)} checks; no physical media teardown guarantee')
