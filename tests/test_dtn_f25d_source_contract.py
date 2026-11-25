from pathlib import Path
import configparser
root=Path(__file__).resolve().parents[1]
read=lambda name:(root/name).read_text(encoding='utf-8')
w=read('firmware/shared/storage/dtn_f24w_mount_owner.h')
z=read('firmware/shared/storage/dtn_f24z_session_service.h')
b=read('firmware/shared/storage/dtn_f25b_session_lifecycle.h')
a=read('firmware/shared/storage/dtn_f25a_runtime_deny.h')
backend=read('firmware/universal/src/dtn_f24y_target_backend.cpp')
ini=configparser.ConfigParser(interpolation=None);ini.read_string((read('firmware/universal/platformio.ini')+'\n'+read('tests/fixtures/dtn/sf7_intermediate_profiles.ini')))
p=ini['env:tbeam-supreme-f25d-integrated-acceptance-deny-build-only']
mount=w[w.index(' bool mountImpl('):w.index(' bool same()const')]
start=z[z.index(' Result start('):z.index(' Result discover(')]
summary=z[z.index(' Result summary('):z.index(' Result shutdown(')]
checks=[
 ('legacy mount preserved','bool mount(){return mountImpl(false,0,0,0);}' in w),
 ('expected identity mandatory in composed start','owner_.mountExpected(media_,epoch_,serial_)' in start and 'owner_.mount()' not in start),
 ('single validated mount snapshot',mount.count('w_.view()')==1 and 'const Identity i=w_.view();' in mount),
 ('validity and expected identity before backend',mount.index('!valid(i)')<mount.index('i.media!=media || i.epoch!=epoch || i.owner!=owner')<mount.index('bound_=i')<mount.index('backend_.mount()')),
 ('post-mount identity retained','!backend_.mount() || !same()' in mount),
 ('identity values sourced from evidence','media_=e.mediaTag;epoch_=e.mountEpoch;serial_=e.ownerSerial' in start),
 ('summary admission before completed or ledger access',summary.index('result={};const auto admitted=admit(false);')<summary.index('if(!replay_.completed())')<summary.index('replay_.ledger().pending()')),
 ('summary nonblocking entry','Entry lock(gate_);if(!lock.acquired)return Result::Deferred;' in summary),
 ('identity and state required by admission','if(!started_||stopped_)return Result::Rejected;' in z and '!authority(e,vessel_)||!same(e)||!owner_.checkedIdentity(media_,epoch_,serial_)' in z),
 ('closure wrapper retained','if(stable()!=Result::Done)return Result::Fault;return observe(service_.summary(s));' in b),
 ('permanent witness retained','const DTNF24W::DenyWitness mountWitness_{};' in a and 'const DTNF25B::DenyWitness evidenceWitness_{};' in a and 'service_.start(' not in a),
 ('actual runtime unchanged','DTNF25A::Runtime<Target> f25aRuntime;' in backend),
 ('profile inherits F25C',p['extends']=='env:tbeam-supreme-f25c-control-journal-deny-build-only'),
 ('no new flag or upload configuration',p['build_flags'].strip()=='${env:tbeam-supreme-f25c-control-journal-deny-build-only.build_flags}' and set(p)=={'extends','build_flags'}),
]
for label,ok in checks:assert ok,label
print(f'F25D integrated source contract PASS: {len(checks)} checks; scoped static evidence only')
