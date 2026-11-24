from pathlib import Path
import configparser
root=Path(__file__).resolve().parents[1]
read=lambda p:(root/p).read_text(encoding='utf-8')
h=read('firmware/shared/storage/dtn_f25b_session_lifecycle.h')
y=read('firmware/shared/storage/dtn_f24y_checked_vfs.h')
r=read('firmware/shared/storage/dtn_f25a_runtime_deny.h')
t=read('firmware/universal/src/dtn_f24y_target_backend.cpp')
i=read('firmware/universal/platformio.ini')
cfg=configparser.ConfigParser(interpolation=None);cfg.read_string(i)
p=cfg['env:tbeam-supreme-f25b-session-lifecycle-deny-build-only']
checks=[]
def check(name,ok):
 assert ok,name
 checks.append(name)
for label,ok in [
 ('inherits F25A',p['extends']=='env:tbeam-supreme-f25a-runtime-deny-build-only'),
 ('only new flag and link anchor',p['build_flags'].splitlines()[1:]==['${env:tbeam-supreme-f25a-runtime-deny-build-only.build_flags}','-D THESIS_DTN_F25B_BUILD_ONLY=1','-Wl,--undefined=thesis_f25b_link_anchor']),
 ('immutable deny', 'const DTNF25B::DenyWitness evidenceWitness_{};' in r),
 ('actual runtime uses strict service','DTNF25B::Service<Backend,DTNF24W::DenyWitness,DTNF25B::DenyWitness> service_' in r),
 ('no runtime start/discovery','service_.start(' not in r and 'service_.discover(' not in r),
 ('target strict anchor','&BService::start,&BService::discover,&BService::append<ZQueue>' in t),
 ('target create prohibited','bool createSession(uint32_t vessel,const char* current){\n#if defined(ARDUINO) || defined(ESP_PLATFORM)\n  (void)vessel;(void)current;return fail();' in y),
 ('exclusive mkdir','Sys::kind(path)!=FileKind::Missing || !Sys::createDirectory(path)' in y),
 ('no overwrite or truncate','O_TRUNC' not in y and 'remove(' not in h and 'rename(' not in h),
 ('one service','DTNF24Z::Service<CheckedBackend<Backend,Witness>,MountWitness,Witness> service_' in h),
 ('no alternate queue','PendingQueue' not in h),
 ('private backend','Policy<Witness>& p_;Backend backend_' in h),
 ('fresh reservation around mount','p_.fresh() && backend_.mount() && p_.fresh() && backend_.createSession' in h),
 ('closure count independently required','!w.closedCount(count) || count!=s.count' in h),
 ('exact per-path closure','!w.closed(s.paths[i],claims[i])' in h),
 ('prior epoch','c.closedEpoch<epoch' in h),
 ('media/vessel binding','c.vessel==vessel && c.media==media' in h),
 ('snapshot bounded','s.count>12' in h),
 ('stable claims rechecked','!same(c,claims[i])' in h),
 ('old absent not silently skipped','found==(n!=0)' in h),
 ('old size enforced','actual==n && live(true)' in h and 'n==bytes_[s]' in h),
 ('old read only','old && (m!=DTNF24W::Mode::Read || !n)' in h),
 ('current journal bounded','n>limit_[s]-current' in h),
 ('serialized service wrapper',h.count('Entry e(gate_);if(!e.ok)return Result::Deferred;')==7),
 ('no closure self-publication','w.closed(' in h and 'w.closedCount(' in h and 'markClosed' not in h),
 ('fault latches','if(r==Result::Fault)failed_=true' in h),
 ('new target no physical grant','struct DenyWitness : DTNF24O::DenyWitness' in h and 'bool closedCount(size_t&)const{return false;}' in h),
]:check(label,ok)
print(f'F25B lifecycle source contract PASS: {len(checks)} checks; scoped static evidence only')
