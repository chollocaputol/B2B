#pragma once
// PHASE 5.1.9A: RECEIVE-ONLY, VOLATILE SHADOW STATE.
// Not a durable ledger. Not production SOS, ACK, or relay permission.
// Without independently authorized provisioning, every v3 frame is rejected.
#include "v3_verification_boundary.h"
#include <stdint.h>
#include <stddef.h>
namespace Phase519 {
static constexpr size_t kMaxOrigins = 10;
enum class Outcome : uint8_t {
  RejectedCrypto, RejectedState, RejectedCapacity,
  ShadowAlert, ShadowDetail, ShadowCancel
};
struct Entry {
  bool used=false;
  uint32_t origin=0, epoch=0, event=0;
  bool active=false, details=false;
};
struct Ledger {
  Entry entries[kMaxOrigins]{};
  // This ledger is volatile and never autorizes operational SOS handling.
};
struct Result {
  Outcome outcome=Outcome::RejectedCrypto;
  Phase517::Verdict signature=Phase517::Verdict::Unsupported;
};
inline int comparePair(uint32_t ae,uint32_t ai,uint32_t be,uint32_t bi) {
  if (ae<be) return -1;
  if (ae>be) return 1;
  return ai<bi?-1:(ai>bi?1:0);
}
inline Result inspect(const uint8_t* wire,size_t n, const Phase517::Callbacks& cb,Ledger& shadow) {
  Phase517::Identity id;
  const auto verdict=Phase517::verify(wire,n,cb,id);
  // Phase517's name is VerifiedHostOnly to avoid granting RF permissions.
  if(verdict!=Phase517::Verdict::VerifiedHostOnly)return {Outcome::RejectedCrypto,verdict};
  size_t found=kMaxOrigins, empty=kMaxOrigins;
  for(size_t i=0;i<kMaxOrigins;i++){
    if(shadow.entries[i].used && shadow.entries[i].origin==id.origin){found=i;break;}
    if(!shadow.entries[i].used && empty==kMaxOrigins)empty=i;
  }
  if (id.kind==Phase517::kAlert) {
    if(found!=kMaxOrigins && comparePair(id.epoch,id.event,shadow.entries[found].epoch,shadow.entries[found].event)<=0)
      return {Outcome::RejectedState,verdict};
    if(found==kMaxOrigins){if(empty==kMaxOrigins)return {Outcome::RejectedCapacity,verdict}; found=empty;}
    shadow.entries[found]=Entry{true,id.origin,id.epoch,id.event,true,false};
    return {Outcome::ShadowAlert,verdict};
  }
  if(found==kMaxOrigins)return {Outcome::RejectedState,verdict};
  Entry& e=shadow.entries[found];
  if(comparePair(id.epoch,id.event,e.epoch,e.event)!=0 || !e.active)
    return {Outcome::RejectedState,verdict};
  if(id.kind==Phase517::kDetail){
    if(e.details)return {Outcome::RejectedState,verdict};
    e.details=true;
    return {Outcome::ShadowDetail,verdict};
  }
  if(id.kind==Phase517::kCancel){
    e.active=false;
    return {Outcome::ShadowCancel,verdict};
  }
  return {Outcome::RejectedState,verdict};
}
inline constexpr bool mayMutateOperationalSos(Outcome){return false;}
inline constexpr bool maySendAck(Outcome){return false;}
inline constexpr bool mayRelay(Outcome){return false;}
inline constexpr bool providesDestinationReceipt(Outcome){return false;}
}
static_assert(!Phase519::mayMutateOperationalSos(Phase519::Outcome::ShadowAlert),"shadow is not SOS authorization");
static_assert(!Phase519::maySendAck(Phase519::Outcome::ShadowCancel),"no ACK");
static_assert(!Phase519::mayRelay(Phase519::Outcome::ShadowAlert),"no relay");
