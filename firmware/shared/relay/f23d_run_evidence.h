#pragma once
#include "f23d_run_control.h"
namespace F23DReadiness {
class RunEvidence {
public:
  void command(const char* line,F23DBench::Control result,uint32_t now) {
    if(result==F23DBench::Control::Selected) {
      run_=0;const char* p=line+12;
      while(*p>='0' && *p<='9'){run_=run_*10U+uint32_t(*p-'0');++p;}
      selectedAt_=now;selected_=true;
    }
    if(result==F23DBench::Control::Armed){armAt_=now;wasArmed_=true;}
  }
  void reserved(uint8_t type) {
    if(type==PACKET_SOS)sosUsed_=true;
    if(type==PACKET_SOS_CANCEL)cancelUsed_=true;
  }
  void cancellationAccepted(){cancelSeen_=true;}
  unsigned sosRemaining() const{return !sosUsed_ && !cancelSeen_?1U:0U;}
  unsigned cancelRemaining() const{return cancelUsed_?0U:1U;}
  uint32_t run() const{return run_;}
  uint32_t deadline() const{return wasArmed_?armAt_+F23CLab::kRunLifetimeMs:0;}
  uint32_t selectDeadline() const{return selected_?selectedAt_+F23CLab::kRunLifetimeMs:0;}
  uint32_t remaining(uint32_t now) const {
    const uint32_t age=now-armAt_;
    return wasArmed_ && age<F23CLab::kRunLifetimeMs?F23CLab::kRunLifetimeMs-age:0;
  }
  bool deferralDue(uint32_t now) {
    if(deferred_ && uint32_t(now-lastDeferral_)<1000)return false;
    deferred_=true;lastDeferral_=now;return true;
  }
  static const char* controlName(F23DBench::Control result) {
    static const char* names[]={"NOT_COMMAND","INVALID","SELECTED","ARMED","DENIED","LOCKED","STATUS","DISARMED"};
    const unsigned i=unsigned(result);return i<8?names[i]:"UNKNOWN";
  }
  static const char* admissionName(F23CLab::Admission result) {
    static const char* names[]={"DISARMED","IDENTITY_DENIED","EXPIRED","INVALID","WRONG_ROUTE","WRONG_EVENT","STATE_REJECTED","UNKNOWN_CANCEL","DUPLICATE","ACCEPTED"};
    const unsigned i=unsigned(result);return i<10?names[i]:"UNKNOWN";
  }
private:
  uint32_t run_=0,selectedAt_=0,armAt_=0,lastDeferral_=0;
  bool selected_=false,wasArmed_=false,deferred_=false,sosUsed_=false,cancelUsed_=false,cancelSeen_=false;
};
}
