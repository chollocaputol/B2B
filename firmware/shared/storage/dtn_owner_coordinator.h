#pragma once
// F24J: portable SD-operation ownership coordinator; BUILD-ONLY / HOST ONLY.
// It interposes an F24I GuardedPort and checks witness/media/source continuity
// before AND after every operation. No real independent witness/SD port exists.
// It cannot prove a FAT fsync, card CID, write exclusion or an immutable file.
#include <stdint.h>
#include <limits.h>
#include "dtn_sd_owner_bridge.h"

namespace DTNF24J {
using DTNF24H::SessionKey;
using DTNF24H::Snapshot;
using DTNF24I::Witness;
enum class State : uint8_t { Unbound, Operational, Retiring, Retired, Revoked };
enum class Lane : uint8_t { Urgent, Research, Export, History, Hash, Seal };

// Port: the already-armed F24I GuardedPort (or another conforming injected
// interface); Observer: an independently implemented live witness provider.
// In the actual firmware preview both are perpetually DENIED. No SD calls.
template<class Port,class Observer> class Coordinator {
 public:
  Coordinator(Port& port,const Observer& observer):port_(port),observer_(observer){}
  Coordinator(const Coordinator&)=delete;
  Coordinator& operator=(const Coordinator&)=delete;
  State state()const{return state_;}
  bool revoked()const{return state_==State::Revoked;}
  bool bind(const SessionKey& key){
    if(state_!=State::Unbound)return false;
    const Snapshot s=port_.snapshot();
    const Witness w=observer_.view();
    if(!DTNF24I::matches(w,s,key) || !s.sourceEpoch || s.writeProtected ||
       s.producerQuiesced || s.flushSucceeded || s.writerHandles || s.writeInFlight){
      revoke();return false;
    }
    key_=key;media_=s.mediaTag;mount_=s.mountEpoch;
    owner_=s.ownerLeaseSerial;close_=s.closeSerial;epoch_=s.sourceEpoch;
    state_=State::Operational;return true;
  }
  // Refuse to expose a valid snapshot after revocation. This is the F24H
  // Port API's mandatory fail-closed behavior.
  Snapshot snapshot()const {Snapshot s{};return live(&s)?s:Snapshot{};}
  bool quiesceProducers(){
    if(state_!=State::Operational)return fail();
    Snapshot before{};
    if(!beginOperation(&before) || before.producerQuiesced ||
       before.writeProtected || before.flushSucceeded)return fail();
    const bool ok=port_.quiesceProducers();
    const Snapshot after=port_.snapshot();
    if(!ok || !complete(before,after,false) || !after.producerQuiesced ||
       after.urgentQueued!=before.urgentQueued ||
       after.researchQueued!=before.researchQueued)return fail();
    state_=State::Retiring;return true;
  }
  bool appendNextUrgent(){return append(true);}
  bool appendNextResearch(){return append(false);}
  bool flushAndCloseAll(){
    if(state_!=State::Retiring)return fail();
    Snapshot before{};
    if(!beginOperation(&before) || !before.producerQuiesced ||
       before.urgentQueued || before.researchQueued || before.flushSucceeded ||
       before.writeProtected)return fail();
    const bool ok=port_.flushAndCloseAll();
    const Snapshot after=port_.snapshot();
    if(!ok || !complete(before,after,false) || !after.flushSucceeded ||
       after.writerHandles || after.writeInFlight)return fail();
    return true;
  }
  bool inspectSOSLastByte(uint64_t& n,bool& newline){
    n=0;newline=false;
    if(state_!=State::Retiring)return fail();
    Snapshot before{};
    if(!beginOperation(&before) || !before.flushSucceeded ||
       before.writerHandles || before.urgentQueued ||
       before.researchQueued || before.writeProtected)return fail();
    uint64_t bytes=0;bool last=false;
    const bool ok=port_.inspectSOSLastByte(bytes,last);
    const Snapshot after=port_.snapshot();
    if(!ok || !complete(before,after,false) ||
       bytes>DTNF06ClosedReplay::kMaxJournalBytes || !last)return fail();
    n=bytes;newline=last;return true;
  }
  bool protectClosedSource(){
    if(state_!=State::Retiring)return fail();
    Snapshot before{};
    if(!beginOperation(&before) || !before.flushSucceeded ||
       before.writerHandles || before.urgentQueued ||
       before.researchQueued || before.writeProtected)return fail();
    const bool ok=port_.protectClosedSource();
    const Snapshot after=port_.snapshot();
    if(!ok || !complete(before,after,false) || !after.writeProtected ||
       !after.flushSucceeded || after.writerHandles || after.writeInFlight)return fail();
    state_=State::Retired;return true;
  }
  // Additional lanes are illustrative interposition points; the existing
  // logger export/history/hash/seal call sites are NOT yet routed through
  // these callbacks. Therefore all-writes-interposed is FALSE in firmware.
  // Action is a synthetic, caller-owned test action; not an SD access grant.
  template<class Action> bool run(Lane lane,Action action){
    const bool afterRetire=(lane==Lane::Hash || lane==Lane::Seal);
    if((afterRetire && state_!=State::Retired) ||
       (!afterRetire && state_!=State::Operational) ||
       (lane==Lane::Urgent || lane==Lane::Research))return fail();
    Snapshot before{};
    if(!beginOperation(&before))return fail();
    if(afterRetire){
      if(!before.writeProtected || !before.flushSucceeded ||
         before.writerHandles || before.urgentQueued || before.researchQueued)return fail();
    }else if(before.urgentQueued || before.researchQueued ||
             before.producerQuiesced || before.writeProtected)return fail();
    const bool ok=action();
    const Snapshot after=port_.snapshot();
    if(!ok || !complete(before,after,false))return fail();
    return true;
  }
 private:
  Port& port_;const Observer& observer_;
  mutable State state_=State::Unbound;
  mutable bool busy_=false;
  SessionKey key_{};
  uint64_t media_=0,mount_=0,owner_=0,close_=0,epoch_=0;
  void revoke()const{state_=State::Revoked;busy_=false;}
  bool fail()const{revoke();return false;}
  bool live(Snapshot* out=nullptr)const{
    if(state_!=State::Operational && state_!=State::Retiring &&
       state_!=State::Retired)return false;
    const Snapshot s=port_.snapshot();
    const Witness w=observer_.view();
    if(!DTNF24I::matches(w,s,key_) ||
       s.mediaTag!=media_ || s.mountEpoch!=mount_ ||
       s.ownerLeaseSerial!=owner_ || s.closeSerial!=close_ ||
       s.sourceEpoch!=epoch_){revoke();return false;}
    if(out)*out=s;
    return true;
  }
  bool beginOperation(Snapshot* before){
    if(busy_ || !live(before))return fail();
    if(before->writeInFlight || before->activeSOS || before->urgentOrRadio ||
       before->exportActive || before->historyActive)return fail();
    busy_=true;return true;
  }
  bool complete(const Snapshot& before,const Snapshot& after,bool appendMode){
    if(!busy_)return fail();
    busy_=false;
    if(appendMode){
      if(epoch_==UINT64_MAX || after.sourceEpoch!=epoch_+1U)return fail();
      epoch_=after.sourceEpoch;
    }else if(after.sourceEpoch!=epoch_)return fail();
    if(!live())return fail();
    if(after.urgentQueued>24U || after.researchQueued>24U ||
       after.writerHandles>before.writerHandles || after.writeInFlight ||
       after.activeSOS || after.urgentOrRadio ||
       after.exportActive || after.historyActive)return fail();
    return true;
  }
  bool append(bool urgent){
    if(state_!=State::Retiring)return fail();
    Snapshot before{};
    if(!beginOperation(&before) || !before.producerQuiesced ||
       before.flushSucceeded || before.writeProtected ||
       (urgent ? !before.urgentQueued : (!before.researchQueued || before.urgentQueued)))return fail();
    const bool ok=urgent?port_.appendNextUrgent():port_.appendNextResearch();
    const Snapshot after=port_.snapshot();
    if(!ok || !complete(before,after,true) ||
       after.urgentQueued!=before.urgentQueued-(urgent?1U:0U) ||
       after.researchQueued!=before.researchQueued-(urgent?0U:1U) ||
       after.writerHandles || after.flushSucceeded || after.writeProtected)return fail();
    return true;
  }
};
} // namespace DTNF24J
