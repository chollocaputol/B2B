"""Scoped admission and non-destructive retirement preflight; no physical grant."""
from pathlib import Path
import configparser
root=Path(__file__).resolve().parents[1]
read=lambda p:(root/p).read_text(encoding='utf-8')
h=read('firmware/shared/storage/dtn_f26b_capacity_guard.h')
z=read('firmware/shared/storage/dtn_f24z_session_service.h')
b=read('firmware/shared/storage/dtn_f25b_session_lifecycle.h')
r=read('firmware/shared/storage/dtn_f25a_runtime_deny.h')
y=read('firmware/shared/storage/dtn_f24y_checked_vfs.h')
ini=configparser.ConfigParser(interpolation=None);ini.read_string(read('firmware/universal/platformio.ini'))
p=ini['env:tbeam-supreme-f26b-capacity-deny-build-only'];prior='env:tbeam-supreme-f26a-spi-separation-deny-build-only'
checks=[]
def check(name,condition):
 assert condition,name
 checks.append(name)
a=z[z.index(' template<class Queue> Result append'):z.index(' template<class Verifier> Result poll')]
check('only new macro',p['extends']==prior and p['build_flags'].splitlines()[1:]==['${'+prior+'.build_flags}','-D THESIS_DTN_F26B_BUILD_ONLY=1'])
check('typed and strict dependencies','#if THESIS_DTN_F25C_BUILD_ONLY != 1 || THESIS_DTN_F25B_BUILD_ONLY != 1' in h)
check('opt-in shared inclusion','#if THESIS_DTN_F26B_BUILD_ONLY == 1\n#include "dtn_f26b_capacity_guard.h"' in z)
check('single original ledger','DTNF24N::Replay<32,12> replay_' in z and 'kCapacity=32, kClosedLimit=12' in h)
check('no extra queue or ledger instance','PendingQueue' not in h+z+b and 'rows_[' not in h)
check('owner gate first',a.index('admit(false)')<a.index('if(!queue.front())')<a.index('if(!replay_.completed())'))
check('recovery before new append',a.index('if(!replay_.completed())return Result::Deferred;')<a.index('DTNF26B::admit(')<a.index('writer_.appendChecked('))
check('blocked no write',a.index('return Result::CapacityBlocked;')<a.index('writer_.appendChecked('))
check('conflict and invalid fail before write',a.index('decision==DTNF26B::Admission::Conflict || decision==DTNF26B::Admission::Invalid')<a.index('writer_.appendChecked('))
check('exact typed duplicate', 'DTNF24M::recordType(prior)==DTNF24M::recordType(p)' in h and 'memcmp(prior.bytes,p.bytes,DTNF24M::kWireBytes)' in h)
check('only verified prior duplicate ack','Admission::AlreadyPresent)return queue.acknowledgeDurableAppend()' in a)
check('active accepted after writer before ack',a.index('writer_.appendChecked(')<a.index('replay_.ledger().accept(*queue.front())')<a.index('|| !queue.acknowledgeDurableAppend()'))
check('old append retained','#else\n  if(!writer_.appendChecked(port_,current_,vessel_,*queue.front(),"a","r")||!queue.acknowledgeDurableAppend())return fail();' in a)
check('capacity and frame bound','ledger.count()>=C || frames>=C' in h)
check('counterpart before new event','if(partner(ledger,p))return Admission::New;' in h and 's.occupied+s.missingPartners+2<=C' in h)
check('overbudget reporting','s.occupied+s.missingPartners>C' in h)
check('unverified preflight','if(unverified)s.archive=ArchiveState::Unverified;' in h)
check('pending or missing partners block','ledger.pending() || s.missingPartners' in h)
check('archive required not permission','ArchiveState::NeedsClosedArchive' in h and 'NOT retirement eligibility' in h)
check('summary real ledger','result.capacity=DTNF26B::inspect(replay_.ledger(),writer_.written()/DTNF24M::kFrameBytes,receipts_.written()/DTNF24N::kReceiptBytes,replay_.unverified())' in z)
check('reserve inventory before mount','!w.closedCount(reservedClosedCount) || reservedClosedCount>=DTNF26B::kClosedLimit' in b and b.index('reservedClosedCount>=DTNF26B::kClosedLimit')<b.index('vessel=v;media=e.mediaTag;'))
check('stable count at freshness and lock','if(!reservedInventory())return false;' in b and 's.count!=reservedClosedCount' in b)
check('no filesystem retirement','remove(' not in h+z+b and 'rename(' not in h+z+b and 'unlink(' not in h+z+b)
check('target create still denied','bool createSession(uint32_t vessel,const char* current){\n#if defined(ARDUINO) || defined(ESP_PLATFORM)\n  (void)vessel;(void)current;return fail();' in y)
check('runtime immutable deny','const DTNF24W::DenyWitness mountWitness_{};' in r and 'const DTNF25B::DenyWitness evidenceWitness_{};' in r and 'service_.start(' not in r and 'service_.discover(' not in r)
print(f'F26B capacity source contract PASS: {len(checks)} checks; no retirement authorization')
