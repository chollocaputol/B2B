#pragma once
// F24U DTN-only two-phase FILE LIFETIME retirement, synthetic witness only.
// No physical SD owner, no Arduino File destructor guarantee, no unsafe
// post-revocation close, mount/unmount or media access. Firmware uses DENY.
// If the owner/media disappears unexpectedly, the only admissible outcome
// is UNSAFE_QUARANTINED; a reboot/manual recovery still needs approval.
#include <stdint.h>

namespace DTNF24U {
enum class Status : uint8_t {
 NotStarted, DrainRequired, ReadyToRelease, Released,
 Denied, UnsafeQuarantined
};
template<class GuardedVolume> class Teardown final {
 public:
 explicit Teardown(GuardedVolume& volume):volume_(volume){}
 Teardown(const Teardown&)=delete;
 Teardown& operator=(const Teardown&)=delete;
 Status begin(){
  if(state_==Status::UnsafeQuarantined || state_==Status::Released)return state_;
  if(state_==Status::DrainRequired || state_==Status::ReadyToRelease)return observe();
  if(!volume_.beginRetirement()){
   state_=volume_.faulted()?Status::UnsafeQuarantined:Status::Denied;
   return state_;
  }
  started_=true;
  return observe();
 }
 Status observe(){
  if(state_==Status::Released || state_==Status::UnsafeQuarantined)return state_;
  if(!started_)return state_;
  // A fresh witness check is mandatory, including when there are zero
  // handles. This prevents an owner epoch switch from looking drained.
  if(!volume_.verifyRetirementOwner() || volume_.faulted()){
   state_=Status::UnsafeQuarantined;return state_;
  }
  state_=volume_.hasOpenHandle()?Status::DrainRequired:Status::ReadyToRelease;
  return state_;
 }
 Status finish(){
  if(observe()!=Status::ReadyToRelease)return state_;
  if(!volume_.finishRetirement()){
   state_=volume_.faulted()?Status::UnsafeQuarantined:Status::Denied;
   return state_;
  }
  state_=Status::Released;return state_;
 }
 Status status()const{return state_;}
 private:
 GuardedVolume& volume_;
 bool started_=false;
 Status state_=Status::NotStarted;
};
} // namespace DTNF24U
