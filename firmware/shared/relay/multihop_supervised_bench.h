#pragma once
// Supervised V002 physical bench permission; separate from F23D and fleet.
#include <stdint.h>
namespace MultihopBench {
class Session {
public:
 enum class State:uint8_t {Off,Prepared,Live};
 static constexpr uint32_t kSessionMs=600000UL;
 static constexpr uint16_t kMaxAttempts=128;
 static constexpr uint32_t kMaxRadioToRxMs=5000UL;
 bool press(uint32_t self,bool ready,bool emergency,uint32_t now) {
  if(self!=2U || !ready || emergency)return false;
  if(state_==State::Off){state_=State::Prepared;return true;}
  if(state_==State::Prepared){state_=State::Live;began_=now;attempts_=0;return true;}
  revoke();return true;
 }
 void revoke(){state_=State::Off;attempts_=0;}
 State state()const{return state_;}
 bool live(uint32_t now)const{return state_==State::Live &&
   uint32_t(now-began_)<kSessionMs && attempts_<kMaxAttempts;}
 bool preparing(uint32_t now)const{return state_==State::Prepared || live(now);}
 const char* label(uint32_t now)const{
  if(state_==State::Off)return "OFF";
  if(state_==State::Prepared)return "PREP";
  return live(now)?"LIVE":"EXPIRED";
 }
 bool reserve(uint32_t now,uint32_t self,bool ready,bool emergency,bool radioBusy){
  if(!live(now)||self!=2U||!ready||emergency||radioBusy)return false;
  ++attempts_;return true;
 }
 bool completed(uint32_t txToRxMs,bool txOk,bool rxOk){
  if(!txOk || !rxOk || txToRxMs>kMaxRadioToRxMs){revoke();return false;}
  return true;
 }
 uint16_t attempts()const{return attempts_;}
private:
 State state_=State::Off;uint32_t began_=0;uint16_t attempts_=0;
};
}
