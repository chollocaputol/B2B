#pragma once
#ifdef ARDUINO
#error "F23D permission qualification is host-only and cannot grant firmware RF access"
#endif
#include "f23d_run_control.h"
namespace F23DQualification {
struct LocalEvidence {
  bool provisioned=false,boardMatches=false,operatorPresent=false;
  bool physicalContractReviewed=false,recoverySafe=false,localBusy=false;
  uint32_t self=0;
};
// Requirements model only. Delegates all parsing, expiry, attempt accounting,
// single-use adapter leases and restart behavior to the actual RunControl.
// It is excluded from Arduino builds; no hardware grant or persistence exists.
class PermissionModel {
public:
  static constexpr bool kGrantsFirmwareRadio=false;
  F23DBench::Control command(const char* line,const LocalEvidence& e,uint32_t now,uint32_t nonce) {
    return run_.command(line,context(e),now,nonce);
  }
  F23CLab::Admission observe(const VesselPacket& p,const LocalEvidence& e,bool accepted,uint32_t now) {
    return run_.observe(p,context(e),accepted,now);
  }
  template<class Mock> bool dispatch(const LocalEvidence& e,uint32_t now,Mock& mock,int& rc) {
    return run_.dispatch(context(e),now,mock,rc);
  }
  bool armed() const{return run_.armed();}
  uint8_t attempts() const{return run_.attempts();}
private:
  static F23DBench::Context context(const LocalEvidence& e) {
    F23DBench::Context c;c.identityReady=e.provisioned;c.self=e.self;
    c.physicalPermission=e.boardMatches && e.operatorPresent && e.physicalContractReviewed;
    c.recoverySafe=e.recoverySafe;c.localControlBusy=e.localBusy;return c;
  }
  F23DBench::RunControl run_;
};
}
