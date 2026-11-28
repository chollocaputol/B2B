#pragma once
// F24I: HOST / BUILD-ONLY independent-witness bridge for F24H retirement.
// Does not supply physical SD authority, CID/media truth, exclusivity,
// flush durability, write lock, journal replay, HTTP or any upload permission.
// The witness and underlying Port are injected. Positive host tests use fakes.
#include <stdint.h>
#include <limits.h>
#include "dtn_writer_retirement.h"

namespace DTNF24I {
using DTNF24H::SessionKey;
using DTNF24H::Snapshot;

// Independent owner implementation must provide these CURRENT facts.
// Merely setting flags is NOT proof. All fields default to DENIED.
struct Witness {
  SessionKey key{};
  uint32_t vessel=0U;
  uint64_t mediaTag=0U,mountEpoch=0U,sourceEpoch=0U;
  uint64_t ownerLeaseSerial=0U,closeSerial=0U;
  bool explicitPhysicalApproval=false;
  bool mediaIdentityIndependentlyVerified=false;
  bool soleMainLoopSDOwner=false;
  bool allWritesInterposed=false;
  bool sourceWriteExclusionEnforceable=false;
  bool registeredRetirementPort=false;
  bool activeSOS=false,urgentOrRadio=false;
  bool concurrentExport=false,concurrentHistory=false;
};

enum class Status : uint8_t { Unbound, Armed, Revoked };
inline bool matches(const Witness& w,const Snapshot& s,const SessionKey& key){
  return DTNF24H::validKey(key) && DTNF24H::sameKey(key,s.key) &&
    DTNF24H::sameKey(key,w.key) &&
    w.vessel==key.vessel && s.vessel==key.vessel &&
    w.explicitPhysicalApproval && w.mediaIdentityIndependentlyVerified &&
    w.soleMainLoopSDOwner && w.allWritesInterposed &&
    w.sourceWriteExclusionEnforceable && w.registeredRetirementPort &&
    w.mediaTag!=0U && w.mountEpoch!=0U && w.sourceEpoch!=0U &&
    w.ownerLeaseSerial!=0U && w.closeSerial!=0U &&
    w.mediaTag==s.mediaTag && w.mountEpoch==s.mountEpoch &&
    w.sourceEpoch==s.sourceEpoch &&
    w.ownerLeaseSerial==s.ownerLeaseSerial && w.closeSerial==s.closeSerial &&
    s.mounted && s.mediaVerified && s.soleMainLoopOwner &&
    !s.activeSOS && !s.urgentOrRadio && !s.exportActive &&
    !s.historyActive && !s.writeInFlight &&
    !w.activeSOS && !w.urgentOrRadio &&
    !w.concurrentExport && !w.concurrentHistory &&
    s.urgentQueued<=24U && s.researchQueued<=24U;
}

// All Port operations must be exclusively mediated by this adapter.
// Observer::view() must query independent, up-to-date *RAM* owner evidence;
// it MUST NOT perform SD I/O or depend on the Port snapshot for provenance.
// Port implements the exact F24H retirement Port API.
// This is a software interface, not a hardware proof or permission grant.
template<class Port,class Observer> class GuardedPort {
public:
  GuardedPort(Port& p,const Observer& observer):port_(p),observer_(observer){}
  GuardedPort(const GuardedPort&)=delete;
  GuardedPort& operator=(const GuardedPort&)=delete;
  Status status()const{return status_;}
  bool revoked()const{return status_==Status::Revoked;}
  bool arm(const SessionKey& key){
    if(status_!=Status::Unbound)return false; // no reset/retry
    const Snapshot current=port_.snapshot();
    if(!matches(observer_.view(),current,key)){
      revoke();return false;
    }
    key_=key;sourceEpoch_=current.sourceEpoch;
    mediaTag_=current.mediaTag;mountEpoch_=current.mountEpoch;
    lease_=current.ownerLeaseSerial;closeSerial_=current.closeSerial;
    status_=Status::Armed;return true;
  }
  // A refused snapshot deliberately cannot pass F24H::Controller's base gate.
  Snapshot snapshot()const {
    Snapshot s{};
    if(live(&s))return s;
    return Snapshot{};
  }
  bool quiesceProducers(){
    return act([this]{return port_.quiesceProducers();},false,Kind::Quiesce);
  }
  bool appendNextUrgent(){
    return act([this]{return port_.appendNextUrgent();},true,Kind::Urgent);
  }
  bool appendNextResearch(){
    return act([this]{return port_.appendNextResearch();},true,Kind::Research);
  }
  bool flushAndCloseAll(){
    return act([this]{return port_.flushAndCloseAll();},false,Kind::Flush);
  }
  bool inspectSOSLastByte(uint64_t& bytes,bool& finalNewline){
    bytes=0U;finalNewline=false;
    Snapshot s{};
    if(!live(&s) || !s.producerQuiesced || !s.flushSucceeded ||
       s.writerHandles || s.urgentQueued || s.researchQueued || s.writeProtected)
      return refuse();
    uint64_t size=0U;bool newline=false;
    if(!port_.inspectSOSLastByte(size,newline))return refuse();
    if(!live() || size>DTNF06ClosedReplay::kMaxJournalBytes)return refuse();
    bytes=size;finalNewline=newline;return true;
  }
  bool protectClosedSource(){
    return act([this]{return port_.protectClosedSource();},false,Kind::Protect);
  }
private:
  enum class Kind : uint8_t { Quiesce, Urgent, Research, Flush, Protect };
  Port& port_;const Observer& observer_;
  SessionKey key_{};
  mutable Status status_=Status::Unbound;
  mutable uint64_t sourceEpoch_=0U;
  uint64_t mediaTag_=0U,mountEpoch_=0U,lease_=0U,closeSerial_=0U;
  void revoke()const{status_=Status::Revoked;}
  bool refuse()const{revoke();return false;}
  bool live(Snapshot* out=nullptr)const {
    if(status_!=Status::Armed)return false;
    const Snapshot s=port_.snapshot();
    const Witness w=observer_.view();
    if(!matches(w,s,key_) || s.sourceEpoch!=sourceEpoch_ ||
       s.mediaTag!=mediaTag_ || s.mountEpoch!=mountEpoch_ ||
       s.ownerLeaseSerial!=lease_ || s.closeSerial!=closeSerial_){
      revoke();return false;
    }
    if(out)*out=s;
    return true;
  }
  template<class F> bool act(F f,bool append,Kind kind){
    Snapshot before{};
    if(!live(&before))return refuse();
    if(before.writeProtected || before.writeInFlight)return refuse();
    if(kind==Kind::Quiesce){
      if(before.producerQuiesced)return refuse();
    }else if(!before.producerQuiesced)return refuse();
    if(kind==Kind::Urgent && !before.urgentQueued)return refuse();
    if(kind==Kind::Research && (!before.researchQueued || before.urgentQueued))
      return refuse();
    if(kind==Kind::Flush && (before.urgentQueued || before.researchQueued))
      return refuse();
    if(kind==Kind::Protect &&
       (!before.flushSucceeded || before.writerHandles ||
        before.urgentQueued || before.researchQueued))return refuse();
    if(!f())return refuse();
    // Only a successful append may advance sourceEpoch, exactly by one.
    const Snapshot after=port_.snapshot();
    if(append){
      if(sourceEpoch_==UINT64_MAX || after.sourceEpoch!=sourceEpoch_+1U)
        return refuse();
      sourceEpoch_=after.sourceEpoch;
    }else if(after.sourceEpoch!=sourceEpoch_)return refuse();
    if(!live())return refuse();
    if(kind==Kind::Quiesce && (!after.producerQuiesced ||
       after.urgentQueued!=before.urgentQueued ||
       after.researchQueued!=before.researchQueued))return refuse();
    if(kind==Kind::Urgent && (after.urgentQueued!=before.urgentQueued-1U ||
       after.researchQueued!=before.researchQueued))return refuse();
    if(kind==Kind::Research && (after.researchQueued!=before.researchQueued-1U ||
       after.urgentQueued!=before.urgentQueued))return refuse();
    if(kind==Kind::Flush && (!after.flushSucceeded || after.writerHandles))
      return refuse();
    if(kind==Kind::Protect && !after.writeProtected)return refuse();
    return true;
  }
};

// The ONLY concrete runtime preview is the following RAM-only DENIED port.
// It never reads/mounts/writes SD, closes handles or creates proof.
struct NoPhysicalObserver { Witness view()const{return Witness{};} };
struct NoPhysicalPort {
  uint32_t observedUrgent=0U,observedResearch=0U;
  Snapshot snapshot()const{
    Snapshot s{};s.urgentQueued=observedUrgent;
    s.researchQueued=observedResearch;return s;
  }
  bool quiesceProducers(){return false;}
  bool appendNextUrgent(){return false;}
  bool appendNextResearch(){return false;}
  bool flushAndCloseAll(){return false;}
  bool inspectSOSLastByte(uint64_t&,bool&){return false;}
  bool protectClosedSource(){return false;}
};
} // namespace DTNF24I
