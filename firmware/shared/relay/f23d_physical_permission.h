#pragma once
#include "f23d_physical_contract.h"
#include "f23d_run_control.h"
namespace F23DPhysical {
// Implementations must independently check physical board binding, deliberate
// local operator control and run/event scope. Numeric fleet IDs are not proof.
// Bounds require reviewed worst-case measurements INCLUDING driver timeouts,
// preemption/call-entry latency, RF operation and RX restoration. Zero denies.
struct Evidence {
  bool boardBound=false,operatorAuthorized=false,timingApproved=false;
  uint32_t run=0,event=0,sampledAt=0;
  uint32_t callEntryBoundMs=0,radioToRxBoundMs=0;
};
using Reader=Evidence (*)(uint32_t now,uint32_t run,uint32_t event);
inline Evidence unavailableEvidence(uint32_t,uint32_t,uint32_t){return Evidence{};}
enum class Reason : uint8_t {
  Ready,NoSelection,Terminal,Identity,Recovery,Binding,Operator,Scope,Timing,
  Expired,InsufficientTime,NotArmed,LocalBusy,EvidenceLost
};
class Permission {
public:
  explicit Permission(Reader reader=unavailableEvidence):reader_(reader?reader:unavailableEvidence){}
  void selected(uint32_t run,uint32_t event,uint32_t now) {
    if(selected_ || terminal_ || !run || !event){revoke(Reason::Terminal);return;}
    selected_=true;run_=run;event_=event;selectedAt_=now;
  }
  void armed(uint32_t now){if(!terminal_ && selected_){armed_=true;armedAt_=now;}}
  bool evaluate(const F23DBench::Context& base,uint32_t now) {
    if(terminal_)return false;
    if(!selected_)return deny(Reason::NoSelection,false);
    if(!base.identityReady || base.self!=2)return deny(Reason::Identity);
    if(!base.recoverySafe)return deny(Reason::Recovery);
    const uint32_t age=now-(armed_?armedAt_:selectedAt_);
    if(age>=F23CLab::kRunLifetimeMs)return deny(Reason::Expired);
    const Evidence e=reader_(now,run_,event_);
    if(!e.boardBound)return deny(Reason::Binding);
    if(!e.operatorAuthorized)return deny(Reason::Operator);
    if(e.run!=run_ || e.event!=event_ || e.sampledAt!=now)return deny(Reason::Scope);
    if(!e.timingApproved || !e.callEntryBoundMs || !e.radioToRxBoundMs ||
       e.callEntryBoundMs>=F23CLab::kRunLifetimeMs ||
       e.radioToRxBoundMs>=F23CLab::kRunLifetimeMs)return deny(Reason::Timing);
    const uint32_t required=e.callEntryBoundMs+e.radioToRxBoundMs;
    if(required>=F23CLab::kRunLifetimeMs-age)return deny(Reason::InsufficientTime);
    reason_=Reason::Ready;return true;
  }
  bool beforeRadio(const F23DBench::Context& base,uint32_t now,uint32_t lost) {
    if(terminal_)return false;
    if(lost)return deny(Reason::EvidenceLost);
    if(!armed_)return deny(Reason::NotArmed);
    if(base.localControlBusy)return deny(Reason::LocalBusy);
    return evaluate(base,now);
  }
  void revoke(Reason reason=Reason::Terminal){
    terminal_=true;armed_=false;
    if(reason!=Reason::Terminal || reason_==Reason::Ready || reason_==Reason::NoSelection)reason_=reason;
  }
  Reason reason()const{return reason_;}
  bool terminal()const{return terminal_;}
  uint32_t run()const{return run_;}
  uint32_t event()const{return event_;}
private:
  bool deny(Reason reason,bool latch=true){reason_=reason;if(latch){terminal_=true;armed_=false;}return false;}
  Reader reader_;uint32_t run_=0,event_=0,selectedAt_=0,armedAt_=0;
  bool selected_=false,armed_=false,terminal_=false;
  Reason reason_=Reason::NoSelection;
};
}
