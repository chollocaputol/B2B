#pragma once
// THESIS F24Q: portable, FAIL-CLOSED cross-writer / long-lived handle contract.
// A model for future global SD arbitration, NOT a physical SD owner implementation.
// The F24Q firmware uses DenyWitness only. Never turn synthetic host facts into
// device permission. No SD, SPI, Arduino, radio, Wi-Fi or credentials here.
#include <stdint.h>

namespace DTNF24Q {
enum class Lane : uint8_t {None, Sos, LegacyAppend, Export, History, Audit, F23D};
enum class Mode : uint8_t {Read, Append};
enum class Action : uint8_t {Read, Write, Seek, Flush, Size, Enumerate};
struct Facts {
 uint32_t vessel=0;
 uint64_t mediaTag=0,mountEpoch=0,ownerSerial=0;
 bool independentlyAuthorized=false,soleOwner=false,allPathsInterposed=false;
 bool legacyCoordinated=false,f23dCoordinated=false,otherHandlesClosed=false;
 bool radioBusy=false,urgentPending=false;
};
struct DenyWitness { Facts view()const{return {};} };
inline bool suitable(const Facts& f,uint32_t vessel){
 return vessel>=1 && vessel<=3 && f.vessel==vessel &&
        f.mediaTag!=0 && f.mountEpoch!=0 && f.ownerSerial!=0 &&
        f.independentlyAuthorized && f.soleOwner && f.allPathsInterposed &&
        f.legacyCoordinated && f.f23dCoordinated && f.otherHandlesClosed &&
        !f.radioBusy && !f.urgentPending;
}
struct Lease { uint64_t serial=0; uint32_t generation=0; Lane lane=Lane::None; };

template<class Witness> class Arbiter final {
 public:
 explicit Arbiter(const Witness& witness):witness_(witness){}
 Arbiter(const Arbiter&)=delete; Arbiter& operator=(const Arbiter&)=delete;
 // Only an injected independently validated witness can create a lease.
 // Contention is a normal refusal (not a grant and not a revocation).
 Lease acquire(uint32_t vessel,Lane lane){
  if(fault_ || lane==Lane::None || active_!=Lane::None)return {};
  const Facts f=witness_.view();
  if(!suitable(f,vessel))return {};
  if(generation_==UINT32_MAX){fault_=true;return {};}
  vessel_=vessel;media_=f.mediaTag;mount_=f.mountEpoch;owner_=f.ownerSerial;
  ++generation_;active_=lane;sawAppend_=false;retiring_=false;return {owner_,generation_,lane};
 }
 bool open(const Lease& l,uint8_t slot,Mode mode){
  if(retiring_ || !check(l) || slot>=2U || handles_[slot])return false;
  // F24T deliberately preserves the original generic open contract.
  // History child handles must use the dedicated openHistoryChild operation.
  if(slot==1U && (active_!=Lane::F23D || !sawAppend_))return false;
  if(mode==Mode::Append && !writer(active_))return false;
  if(mode==Mode::Append && slot!=0U)return false;
  if(mode==Mode::Read && active_==Lane::LegacyAppend)return false;
  if(active_==Lane::F23D && slot==0U && (mode!=Mode::Append || sawAppend_))return false;
  // F23D keeps append handle slot 0 while it opens verification slot 1.
  handles_[slot]=true;read_[slot]=(mode==Mode::Read);
  if(active_==Lane::F23D && slot==0U)sawAppend_=true;
  return true;
 }
 // F24T: ONLY directory iteration can reserve history slot 1. The
 // generic open(l,1,Read) contract remains rejected for History lanes.
 bool openHistoryChild(const Lease& l){
  if(retiring_ || !check(l) || active_!=Lane::History || !handles_[0] || !read_[0] ||
     handles_[1])return false;
  handles_[1]=true;read_[1]=true;return true;
 }
 bool step(const Lease& l,uint8_t slot,Action action){
  if(!check(l) || slot>=2U || !handles_[slot])return false;
  // During orderly retirement no new content I/O is admitted. A writer may
  // flush its already-written bytes while the SAME verified owner is present;
  // raw close must follow while the lease is still valid.
  if(retiring_ && action!=Action::Flush)return false;
  switch(action){
   case Action::Read: case Action::Seek: case Action::Enumerate:
    return read_[slot] && (action!=Action::Enumerate || active_==Lane::History || active_==Lane::Sos);
   case Action::Write: case Action::Flush:
    return !read_[slot] && writer(active_);
   case Action::Size:return true;
  }
  return false;
 }
 // F24V: pre-close authorization does not release a lease slot.
 // The caller must run the raw backend close while the owner is still valid,
 // then call close() for a second witness check and slot release. This narrows
 // false-closed accounting but does NOT make Arduino File::close() infallible
 // or prevent a media/witness switch during the backend operation.
 bool permitClose(const Lease& l,uint8_t slot){
  if(!check(l) || slot>=2U || !handles_[slot])return false;
  if((active_==Lane::F23D || active_==Lane::History) && slot==0U && handles_[1])return false;
  return true;
 }
 // Closing a File after ownership or media revocation is ALSO SD I/O.
 // Never close by force: leave a quarantined outstanding handle until a
 // separately approved owner/teardown plan. No regrant in this instance.
 bool close(const Lease& l,uint8_t slot){
  if(!check(l) || slot>=2U || !handles_[slot])return false;
  if((active_==Lane::F23D || active_==Lane::History) && slot==0U && handles_[1])return false;
  handles_[slot]=false;read_[slot]=false;return true;
 }
 // F24U: begin a monotonic quiesce *before* revoking owner/media access.
 // Prevent all new opens and content actions, but allow ordered flush/close
 // while a fresh witness still verifies the exact media and owner epoch.
 // This is only a software lease model; no physical File teardown authority.
 bool beginRetirement(const Lease& l){
  if(!check(l))return false;
  retiring_=true;return true;
 }
 bool finishRetirement(const Lease& l){
  if(!retiring_ || !check(l) || hasOpenHandle())return false;
  return release(l);
 }
 bool retiring()const{return retiring_;}
 bool release(const Lease& l){
  if(!check(l) || handles_[0] || handles_[1])return false;
  active_=Lane::None;sawAppend_=false;retiring_=false;vessel_=0;media_=mount_=owner_=0;return true;
 }
 // F24R preflight: recheck the independently sampled witness BEFORE
 // volume metadata/exists operations which do not yet have a File slot.
 // Any changed owner/media/lease latches the same failure as File methods.
 bool probe(const Lease& l){return check(l);}
 bool hasHandle(uint8_t slot)const{return slot<2U && handles_[slot];}
 bool hasOpenHandle()const{return handles_[0] || handles_[1];}
 bool quarantined()const{return fault_ && hasOpenHandle();}
 bool faulted()const{return fault_;}
 Lane activeLane()const{return active_;}
 void revoke(){fault_=true;}
 private:
 static bool writer(Lane l){return l==Lane::Sos || l==Lane::LegacyAppend || l==Lane::F23D;}
 bool check(const Lease& l){
  if(fault_ || active_==Lane::None)return false;
  if(!l.serial || l.serial!=owner_ || l.generation!=generation_ || l.lane!=active_){
    fault_=true;return false;
  }
  const Facts f=witness_.view();
  if(!suitable(f,vessel_) || f.mediaTag!=media_ || f.mountEpoch!=mount_ || f.ownerSerial!=owner_){
    fault_=true;return false;
  }
  return true;
 }
 const Witness& witness_;
 uint32_t vessel_=0,generation_=0;
 uint64_t media_=0,mount_=0,owner_=0;
 Lane active_=Lane::None;
 bool handles_[2]{},read_[2]{},fault_=false,sawAppend_=false,retiring_=false;
};
} // namespace DTNF24Q
