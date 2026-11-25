"""Scoped source constraints; no retirement or physical permission."""
from pathlib import Path
import configparser
root=Path(__file__).resolve().parents[1]
read=lambda f:(root/f).read_text(encoding='utf-8')
base='firmware/shared/storage/'
m=read(base+'dtn_f24m_sos_packet_store.h'); z=read(base+'dtn_f24z_session_service.h'); b=read(base+'dtn_f25b_session_lifecycle.h'); y=read(base+'dtn_f24y_checked_vfs.h'); r=read(base+'dtn_f25a_runtime_deny.h')
ini=configparser.ConfigParser(interpolation=None);ini.read_string((read('firmware/universal/platformio.ini')+'\n'+read('tests/fixtures/dtn/sf7_intermediate_profiles.ini')))
p=ini['env:tbeam-supreme-f26c-maintenance-deny-build-only']; prior='env:tbeam-supreme-f26b-capacity-deny-build-only';checks=[]
def check(n,ok):
 assert ok,n
 checks.append(n)
check('only new flag',set(p)=={'extends','build_flags'} and p['extends']==prior and p['build_flags'].splitlines()[1:]==['${'+prior+'.build_flags}','-D THESIS_DTN_F26C_BUILD_ONLY=1'])
check('requires F26B','#if THESIS_DTN_F26C_BUILD_ONLY == 1 && THESIS_DTN_F26B_BUILD_ONLY != 1' in z)
check('same queue array','PacketEntry entries_[Capacity]{};' in m and m.count('PacketEntry entries_')==1 and 'PendingQueue<8>' in r)
check('index bounds','index<size_?' in m and 'if(fault_ || index>=size_)return false;' in m)
check('selected ack preserves order','entries_[(read_+i)%Capacity]=entries_[(read_+i+1)%Capacity]' in m)
a=z[z.index('template<class Queue> Result bypassBlocked'):z.index(' public:\n template<class... Args>')]
check('bounded search','for(size_t i=1;i<q.pending();++i)' in a)
check('conflict invalid fail', 'd==DTNF26B::Admission::Conflict || d==DTNF26B::Admission::Invalid' in a)
check('duplicate may coalesce','Admission::AlreadyPresent)return q.acknowledgeDurableAppendAt(i)' in a)
check('only partner new write','d!=DTNF26B::Admission::New || !DTNF26B::partner(replay_.ledger(),*p)' in a)
check('write then ledger then selected ack',a.index('writer_.appendChecked')<a.index('replay_.ledger().accept(*p)')<a.rindex('q.acknowledgeDurableAppendAt(i)'))
check('only after blocked head','if(decision==DTNF26B::Admission::Blocked)return bypassBlocked(queue);' in z)
check('no new ledger',z.count('DTNF24N::Replay<32,12> replay_')==1)
flag='#if THESIS_DTN_F26C_BUILD_ONLY == 1 && !defined(ARDUINO) && !defined(ESP_PLATFORM)'
check('host maintenance APIs guarded',flag+'\n // Positive host-only read-only maintenance.' in b and flag+'\n // Called only by the strict maintenance wrapper' in z and flag+'\n template<class Guard> bool discoverMaintenance' in y)
check('distinct grant defaults deny','decltype(bool(w.maintenance(v,d)))' in b and 'maintenanceGrant(const W&,uint32_t,const char*,long){return false;}' in b)
check('maintenance capacity <=12','n>DTNF26B::kClosedLimit' in b and 'n<=DTNF26B::kClosedLimit' in b)
check('no mkdir in maintenance mount','if(p_.maintenanceMode)return (!fault_ && p_.fresh() && backend_.mount() && p_.fresh()) || fail();' in b)
check('reserved absent current must not exist','maintenance?foundCurrent:!foundCurrent' in y)
check('no current paths in maintenance','if(maintenanceMode && isCurrent)return false;' in b)
check('old read-only enforced','old && (m!=DTNF24W::Mode::Read || !n)' in b and 'if(old_[s] ||' in b)
check('all maintenance write and replay entry points deny',b.count('if(policy_.maintenanceMode)return Result::Rejected;')==3)
check('stable maintenance grant','if(!maintenanceLive())return false;' in b and 'if(!p_.maintenanceLive())return false;' in b)
check('exact claimed paths and frame bounds','frame>=bytes/n' in b and 'service_.readClosedFrame(policy_.snapshot.paths[i],receipt,bytes,frame*n,out)' in b)
check('presence and size inspected','found!=(expected!=0)' in z and 'f.size()==expected' in z)
check('read-only exact copy','auto f=port_.open(path,"r")' in z and 'f.seek(offset) && f.read(copy.bytes,n)==int(n)' in z)
check('decode before output','DTNF24N::decodeReceipt(copy.bytes,n,rc):DTNF24M::decode(copy.bytes,n,p)' in z and 'copy.length=n;out=copy' in z)
check('no retirement mutation',not any(x in m+z+b+y for x in ('unlink(', 'remove(', 'rename(', 'O_TRUNC')))
check('runtime deny immutable','const DTNF24W::DenyWitness mountWitness_{};' in r and 'const DTNF25B::DenyWitness evidenceWitness_{};' in r and 'service_.startMaintenance(' not in r and 'service_.archive' not in r)
print(f'F26C maintenance source contract PASS: {len(checks)} checks; host-only archive extraction, no retirement authority')
