from pathlib import Path
root=Path(__file__).resolve().parents[1]
read=lambda p:(root/p).read_text(encoding='utf-8')
w=read('firmware/shared/storage/dtn_f24w_mount_owner.h'); y=read('firmware/shared/storage/dtn_f24y_checked_vfs.h'); z=read('firmware/shared/storage/dtn_f24z_session_service.h'); t=read('firmware/universal/src/dtn_f24y_target_backend.cpp'); ini=read('firmware/universal/platformio.ini'); log=read('firmware/shared/storage/thesis_sd_logger.h'); h=read('tests/dtn_f24z_session_service_host.cpp')
checks=[]
def check(label,condition):
    assert condition,label
    checks.append(label)
for label,condition in [
 ('same private owner backend','backend_.discover(vessel,current,copy,allowed)' in w),
 ('identity guard','return same() && guard();' in w),
 ('no outstanding descriptor','!live() || outstanding())return false' in w),
 ('snapshot bounded','char paths[12][64]' in w),
 ('snapshot result after verified scan','!backend_.discover(vessel,current,copy,allowed) || !allowed())return fail();' in w),
 ('directory pointer private','DIR* directory_=nullptr' in y),
 ('no implicit backend destructor','~Backend' not in y),
 ('exact vessel root','/THESIS/F06TEST/V%03lu' in y),
 ('valid current grammar','DTNF24M::sandboxPath(journal,sizeof(journal),current,vessel)' in y),
 ('no raw directory escape','out=copy;return true;' in y),
 ('bound iteration','i<64' in y),
 ('errno differentiates EOF','errno=0;const auto* entry=Sys::dirNext(directory_);const int err=errno' in y),
 ('require actual EOF','exhausted=err==0' in y and '!exhausted' in y),
 ('ignore dot entries','!strcmp(name,".")||!strcmp(name,"..")' in y),
 ('directory grammar','kind!=FileKind::Directory||!session' in y),
 ('current excluded and required','foundCurrent=true;continue;' in y and '!foundCurrent' in y),
 ('capacity fail not truncate','copy.count>=12' in y),
 ('duplicates fail','!strcmp(copy.paths[j],candidate)' in y),
 ('directory close checked','Sys::dirClose(directory_)!=0' in y),
 ('only successful close releases pointer','if(!allowed()||Sys::dirClose(directory_)!=0)return fail();\n  directory_=nullptr;' in y),
 ('no unmount if directory uncertain','fd_[1]>=0||directory_)return false;if(!driver_.unmount()' in y),
 ('same existing SOS port','DTNF24X::Port<Owner,EvidenceWitness> port_' in z),
 ('existing writer','DTNF24M::SessionWriter writer_' in z),
 ('existing replay','DTNF24N::Replay<32,12> replay_' in z),
 ('existing receipt writer','DTNF24N::ReceiptWriter receipts_' in z),
 ('no new queue','PendingQueue' not in z),
 ('atomic nonblocking entry','test_and_set(std::memory_order_acquire)' in z and 'clear(std::memory_order_release)' in z),
 ('all public operations serialized',z.count('Entry lock(gate_);if(!lock.acquired)return Result::Deferred;')==10 and all(x in z for x in ('Result archiveAccess()', 'Result inspectClosedJournal(', 'Result readClosedFrame('))),
 ('authority separate from busy','return busy(e,old)?Result::Deferred:Result::Done;' in z),
 ('identity loss still faults','!authority(e,vessel_)||!same(e)||!owner_.checkedIdentity' in z),
 ('urgent blocks old-session operations','(old&&e.urgentPending)' in z),
 ('discovery feeds actual replay','replay_.addClosed(snapshot.paths[i])' in z and 'replay_.lock()' in z),
 ('append acknowledges only after checked append','!writer_.appendChecked(port_,current_,vessel_,*queue.front(),"a","r")||!queue.acknowledgeDurableAppend()' in z),
 ('receipt requires completed replay','if(!replay_.completed())return Result::Rejected;' in z),
 ('target deny witnesses','DTNF24Z::Service<Target,DTNF24W::DenyWitness,DTNF24O::DenyWitness>' in t),
 ('target directory anchor','&Target::discover<ZGuard>' in t),
 ('target service anchor','&ZService::append<ZQueue>' in t and '&ZService::poll<ZVerifier>' in t),
 ('target retained only','-Wl,--undefined=thesis_f24z_link_anchor' in ini),
 ('profile inherits Y','extends = env:tbeam-supreme-f24y-checked-vfs-deny-build-only' in ini),
 ('logger unmodified deny path','static DTNF24W::DenyMountFacade<File> interposedSD;' in log and 'DTNF24Z' not in log),
 ('directory read error test','errno=EIO;return nullptr;' in h),
 ('reentrant deferral tested','r.service.summary(s)==R::Deferred' in h and 'r.service.shutdown()==R::Deferred' in h),
 ('radio deferral tested','r.service.discover()==R::Deferred' in h and 'r.service.append(q)==R::Deferred' in h),
 ('mock receipt restart tested','FakeShoreVerifier{p}' in h and 'static void restart(' in h),
 ('loss prevents unmount','r.c.unmount==0' in h and 'Fault::Loss' in h),
]:check(label,condition)
print(f'F24Z owner discovery and serialized SOS service source contract PASS: {len(checks)} checks; no physical grant')
