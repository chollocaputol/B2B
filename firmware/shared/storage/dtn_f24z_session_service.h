#pragma once
// F24Z: one owner and existing SOS port/writer/replay. No new packet queue.
// Transaction-level nonblocking admission. Witness and external queue must be
// independently synchronized; no blocking I/O inside an interrupt lock.
// No physical grant, runtime task, or hardware scheduling proof is supplied.
#include <atomic>
#include "dtn_f24x_owner_sos_port.h"
#include "dtn_f24n_sos_restart_recovery.h"
namespace DTNF24Z {
enum class Result:uint8_t { Done, Deferred, Empty, Progress, Rejected, Fault };
struct Summary { size_t pending=0,delivered=0,sessions=0; bool completed=false; };
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
  if(!writer_.appendChecked(port_,current_,vessel_,*queue.front(),"a","r")||!queue.acknowledgeDurableAppend())return fail();
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
  result={replay_.ledger().pending(),replay_.ledger().delivered(),replay_.sessions(),true};return Result::Done;
 }
 Result shutdown(){
  Entry lock(gate_);if(!lock.acquired)return Result::Deferred;
  const auto admitted=admit(false);if(admitted!=Result::Done)return admitted;
  if(!owner_.shutdown())return fail();
  stopped_=true;return Result::Done;
 }
};
} // namespace DTNF24Z
