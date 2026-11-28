#pragma once
// F24Z: one owner and existing SOS port/writer/replay. No new packet queue.
// Transaction-level nonblocking admission. Witness and external queue must be
// independently synchronized; no blocking I/O inside an interrupt lock.
// No physical grant, runtime task, or hardware scheduling proof is supplied.
#include <atomic>
#include "dtn_owner_sos_port.h"
#include "dtn_sos_restart_recovery.h"
#if THESIS_DTN_F26B_BUILD_ONLY == 1
#include "dtn_capacity_guard.h"
#endif
#if THESIS_DTN_F26C_BUILD_ONLY == 1 && THESIS_DTN_F26B_BUILD_ONLY != 1
#error F26C requires F26B
#endif
namespace DTNF24Z {
#if THESIS_DTN_F26C_BUILD_ONLY == 1 && !defined(ARDUINO) && !defined(ESP_PLATFORM)
struct ArchiveFrame {uint8_t bytes[DTNF24M::kFrameBytes]{};size_t length=0;};
#endif
enum class Result:uint8_t { Done, Deferred, Empty, Progress, Rejected, Fault
#if THESIS_DTN_F26B_BUILD_ONLY == 1
 , CapacityBlocked
#endif
 };
struct Summary { size_t pending=0,delivered=0,sessions=0; bool completed=false;
#if THESIS_DTN_F26B_BUILD_ONLY == 1
 DTNF26B::Status capacity{};
#endif
 };
template<class Backend,class MountWitness,class EvidenceWitness> class Service final {
 using Owner=DTNF24W::MountOwner<Backend,MountWitness>;
 const EvidenceWitness& witness_; Owner owner_; DTNF24X::Port<Owner,EvidenceWitness> port_;
 DTNF24M::SessionWriter writer_; DTNF24N::Replay<32,12> replay_; DTNF24N::ReceiptWriter receipts_;
 std::atomic_flag gate_=ATOMIC_FLAG_INIT;
 struct Entry {std::atomic_flag& flag;bool acquired;explicit Entry(std::atomic_flag& f):flag(f),acquired(!f.test_and_set(std::memory_order_acquire)){} ~Entry(){if(acquired)flag.clear(std::memory_order_release);} };
 uint32_t vessel_=0;char current_[64]{};uint64_t media_=0,epoch_=0,serial_=0;
 bool started_=false,discovered_=false,fault_=false,stopped_=false;
 Result fail(){fault_=true;owner_.quarantine();return Result::Fault;}
 bool same(const DTNF24O::Evidence& e)const{return e.mediaTag==media_&&e.mountEpoch==epoch_&&e.ownerSerial==serial_&&e.vessel==vessel_;}
 static bool busy(const DTNF24O::Evidence& e,bool old){return e.radioBusy||e.exportActive||e.historyActive||(old&&e.urgentPending);}
 static bool authority(DTNF24O::Evidence e,uint32_t v){e.radioBusy=false;e.exportActive=false;e.historyActive=false;return DTNF24O::validEvidence(e,v);}
 Result admit(bool old){
  if(fault_)return Result::Fault;
  if(!started_||stopped_)return Result::Rejected;
  const auto e=witness_.view();
  if(!authority(e,vessel_)||!same(e)||!owner_.checkedIdentity(media_,epoch_,serial_))return fail();
  return busy(e,old)?Result::Deferred:Result::Done;
 }
 bool liveOld()const{const auto e=witness_.view();return same(e)&&DTNF24O::validEvidence(e,vessel_)&&!e.urgentPending;}
#if THESIS_DTN_F26C_BUILD_ONLY == 1
 template<class Queue> Result bypassBlocked(Queue& q){
  if(q.faulted())return fail();
  // Only an already persisted duplicate or a missing counterpart may bypass.
  // Other pending records remain in their original order; no new event bypass.
  for(size_t i=1;i<q.pending();++i){
   const auto* p=q.at(i);if(!p)return fail();
   const auto d=DTNF26B::admit(replay_.ledger(),*p,writer_.written()/DTNF24M::kFrameBytes);
   if(d==DTNF26B::Admission::Conflict || d==DTNF26B::Admission::Invalid)return fail();
   if(d==DTNF26B::Admission::AlreadyPresent)return q.acknowledgeDurableAppendAt(i)?Result::Done:fail();
   if(d!=DTNF26B::Admission::New || !DTNF26B::partner(replay_.ledger(),*p))continue;
   if(!writer_.appendChecked(port_,current_,vessel_,*p,"a","r") ||
      replay_.ledger().accept(*p)!=DTNF24N::Add::Inserted || !q.acknowledgeDurableAppendAt(i))return fail();
   return Result::Done;
  }
  return Result::CapacityBlocked;
 }
#endif
 public:
 template<class... Args> Service(const MountWitness& m,const EvidenceWitness& w,Args&&... a):witness_(w),owner_(m,std::forward<Args>(a)...),port_(owner_,w){}
 Service(const Service&)=delete;Service& operator=(const Service&)=delete;
 Result start(uint32_t vessel,const char* current){
  Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  if(fault_)return Result::Fault;
  if(started_||stopped_)return Result::Rejected;
  char path[96]{};const auto e=witness_.view();
  if(!DTNF24M::sandboxPath(path,sizeof(path),current,vessel)||!authority(e,vessel))return Result::Rejected;
  if(busy(e,false))return Result::Deferred;
  vessel_=vessel;media_=e.mediaTag;epoch_=e.mountEpoch;serial_=e.ownerSerial;strncpy(current_,current,sizeof(current_)-1);
  if(!owner_.mountExpected(media_,epoch_,serial_)||!port_.bind(vessel_,current_))return fail();
  started_=true;return Result::Done;
 }
 Result discover(){
  Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  const auto admitted=admit(true);if(admitted!=Result::Done)return admitted;
  if(discovered_)return Result::Rejected;
  DTNF24W::SessionSnapshot snapshot{};
  if(!owner_.discover(vessel_,current_,snapshot,[&](){return liveOld();})||!liveOld()||!replay_.begin(vessel_,current_))return fail();
  for(size_t i=0;i<snapshot.count;++i)if(!replay_.addClosed(snapshot.paths[i]))return fail();
  if(!replay_.lock())return fail();
  discovered_=true;return Result::Done;
 }
 template<class Queue> Result append(Queue& queue){
  Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  const auto admitted=admit(false);if(admitted!=Result::Done)return admitted;
  if(!queue.front())return Result::Empty;
#if THESIS_DTN_F26B_BUILD_ONLY == 1
  // No new persistent packet before complete old-session recovery.
  if(!replay_.completed())return Result::Deferred;
  const auto decision=DTNF26B::admit(replay_.ledger(),*queue.front(),writer_.written()/DTNF24M::kFrameBytes);
#if THESIS_DTN_F26C_BUILD_ONLY == 1
  if(queue.faulted())return fail();
  if(decision==DTNF26B::Admission::Blocked)return bypassBlocked(queue);
#endif
  if(decision==DTNF26B::Admission::Blocked)return Result::CapacityBlocked;
  if(decision==DTNF26B::Admission::Conflict || decision==DTNF26B::Admission::Invalid)return fail();
  // Exact duplicate already checked by replay or a prior checked append.
  // Assumes the same exclusive immutable owner/closure witness contract.
  if(decision==DTNF26B::Admission::AlreadyPresent)return queue.acknowledgeDurableAppend()?Result::Done:fail();
  if(!writer_.appendChecked(port_,current_,vessel_,*queue.front(),"a","r"))return fail();
  if(replay_.ledger().accept(*queue.front())!=DTNF24N::Add::Inserted || !queue.acknowledgeDurableAppend())return fail();
#else
  if(!writer_.appendChecked(port_,current_,vessel_,*queue.front(),"a","r")||!queue.acknowledgeDurableAppend())return fail();
#endif
  return Result::Done;
 }
 template<class Verifier> Result poll(const Verifier& verifier){
  Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  const auto admitted=admit(true);if(admitted!=Result::Done)return admitted;
  if(!discovered_)return Result::Rejected;
  const auto result=replay_.poll(port_,"r",verifier,[&](){return liveOld();});
  if(result==DTNF24N::Poll::Failed)return fail();
  return result==DTNF24N::Poll::Completed?Result::Done:Result::Progress;
 }
 template<class Verifier> Result receipt(const DTNF24N::Receipt& receipt,const Verifier& verifier){
  Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  const auto admitted=admit(true);if(admitted!=Result::Done)return admitted;
  if(!replay_.completed())return Result::Rejected;
  if(receipts_.appendVerified(port_,current_,vessel_,replay_.ledger(),receipt,verifier,"a","r"))return Result::Done;
  return receipts_.faulted()||port_.faulted()?fail():Result::Rejected;
 }
 Result summary(Summary& result){
  Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  result={};const auto admitted=admit(false);if(admitted!=Result::Done)return admitted;
  if(!replay_.completed())return Result::Rejected;
  result={replay_.ledger().pending(),replay_.ledger().delivered(),replay_.sessions(),true};
#if THESIS_DTN_F26B_BUILD_ONLY == 1
  result.capacity=DTNF26B::inspect(replay_.ledger(),writer_.written()/DTNF24M::kFrameBytes,receipts_.written()/DTNF24N::kReceiptBytes,replay_.unverified());
#endif
  return Result::Done;
 }
#if THESIS_DTN_F26C_BUILD_ONLY == 1 && !defined(ARDUINO) && !defined(ESP_PLATFORM)
 // Called only by the strict maintenance wrapper, which binds the path and
 // exact frame offset to a separately witnessed closed session inventory.
 Result archiveAccess(){
  Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  const auto a=admit(true);if(a!=Result::Done)return a;
  return discovered_?Result::Done:Result::Rejected;
 }
 Result inspectClosedJournal(const char* dir,bool receipt,size_t expected){
  Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  const auto a=admit(true);if(a!=Result::Done)return a;
  if(!discovered_ || !dir || !strcmp(dir,current_))return Result::Rejected;
  char path[96]{};
  if(!(receipt?DTNF24N::receiptPath(path,sizeof(path),dir,vessel_):DTNF24M::sandboxPath(path,sizeof(path),dir,vessel_)))return Result::Rejected;
  const bool found=port_.exists(path);
  if(!port_.storageHealthy() || found!=(expected!=0))return fail();
  if(!found)return liveOld()?Result::Done:fail();
  auto f=port_.open(path,"r");if(!f)return fail();
  const bool ok=f.size()==expected;f.close();
  return ok && liveOld() && port_.storageHealthy()?Result::Done:fail();
 }
 Result readClosedFrame(const char* dir,bool receipt,size_t expected,size_t offset,ArchiveFrame& out){
  out={};Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  const auto a=admit(true);if(a!=Result::Done)return a;
  if(!discovered_ || !dir || !strcmp(dir,current_))return Result::Rejected;
  const size_t n=receipt?DTNF24N::kReceiptBytes:DTNF24M::kFrameBytes;
  if(!expected || expected%n || offset%n || offset>=expected)return Result::Rejected;
  char path[96]{};
  if(!(receipt?DTNF24N::receiptPath(path,sizeof(path),dir,vessel_):DTNF24M::sandboxPath(path,sizeof(path),dir,vessel_)))return Result::Rejected;
  if(!port_.exists(path) || !port_.storageHealthy())return fail();
  auto f=port_.open(path,"r");if(!f)return fail();
  ArchiveFrame copy{};const bool ok=f.size()==expected && f.seek(offset) && f.read(copy.bytes,n)==int(n);f.close();
  if(!ok || !liveOld() || !port_.storageHealthy())return fail();
  DTNF24M::PacketEntry p{};DTNF24N::Receipt rc{};
  if(!(receipt?DTNF24N::decodeReceipt(copy.bytes,n,rc):DTNF24M::decode(copy.bytes,n,p)))return fail();
  copy.length=n;out=copy;return Result::Done;
 }
#endif
 Result shutdown(){
  Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  const auto admitted=admit(false);if(admitted!=Result::Done)return admitted;
  if(!owner_.shutdown())return fail();
  stopped_=true;return Result::Done;
 }
};
} // namespace DTNF24Z
