#pragma once
// F24H: portable writer retirement protocol. NO Arduino, SD, Wi-Fi or RF.
// An injected Port must be the sole real main-loop SD owner; NONE EXISTS YET.
// This state machine supplies CANDIDATE close facts only. It CANNOT approve
// F24F replay, write a F24G seal, or prove FAT/media immutability/durability.
#include <stddef.h>
#include <stdint.h>
#include "dtn_sos_seal.h"

namespace DTNF24H {
using Stream = DTNAckLedger::Stream;
// Pre-close session identity EXCLUDES SHA256: the final digest does not exist
// until after all source writes have stopped and the whole file is hashed.
struct SessionKey {
  DTNAckLedger::Id128 uploader{};
  uint32_t vessel=0U;
  char session[9]{};
  DTNAckLedger::Journal journal=DTNAckLedger::Journal::SosEvents;
};
inline bool validKey(const SessionKey& k){
  if(!DTNAckLedger::uuidFormat(k.uploader) ||
     k.journal!=DTNAckLedger::Journal::SosEvents ||
     k.vessel<1U || k.vessel>9999U ||
     k.session[0]!='S' || k.session[8]!='\0')return false;
  for(unsigned i=1U;i<8U;++i){
    const char c=k.session[i];
    if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))return false;
  }
  return true;
}
inline bool sameKey(const SessionKey& a,const SessionKey& b){
  return DTNAckLedger::sameId(a.uploader,b.uploader) &&
    a.vessel==b.vessel &&
    memcmp(a.session,b.session,9U)==0 && a.journal==b.journal;
}
inline bool postHashMatches(const SessionKey& k,const Stream& stream){
  return validKey(k) && DTNAckLedger::validStream(stream) &&
    DTNAckLedger::sameId(k.uploader,stream.uploader) &&
    k.vessel==stream.vessel &&
    memcmp(k.session,stream.session,9U)==0 && k.journal==stream.journal;
}
enum class State : uint8_t { Untrusted, Draining, Closing, Checking, RetiredCandidate, Failed };
struct Snapshot {
  SessionKey key{};
  uint32_t vessel=0U;
  uint64_t mediaTag=0U, mountEpoch=0U, sourceEpoch=0U;
  uint64_t ownerLeaseSerial=0U, closeSerial=0U;
  uint32_t urgentQueued=0U, researchQueued=0U, writerHandles=0U;
  bool mounted=false, mediaVerified=false, soleMainLoopOwner=false;
  bool producerQuiesced=false, activeSOS=false, urgentOrRadio=false;
  bool exportActive=false, historyActive=false, writeInFlight=false;
  bool flushSucceeded=false, writeProtected=false;
};
struct RetiredFacts {
  SessionKey key{};
  uint64_t fileBytes=0U,mediaTag=0U,mountEpoch=0U;
  uint64_t sourceEpoch=0U,ownerLeaseSerial=0U,closeSerial=0U;
  bool retiredSession=false,appendQueueDrained=false,flushSucceeded=false;
  bool allWriterHandlesClosed=false,finalNewlineVerified=false;
  bool protectedAgainstWrites=false;
};
struct Candidate {
  RetiredFacts facts{};
  bool eligible=false; // candidate only; NEVER F24F permission
};
// Only after a separate complete F24D SHA scan can the verified-digest
// Stream be associated with these candidate facts. This checks association,
// NOT the authenticity of that hash, real media or hardware write protection.
inline bool bindPostHash(const Candidate& candidate,const Stream& hashed,
                         DTNF24G::CloseFacts& out){
  out=DTNF24G::CloseFacts{};
  if(!candidate.eligible || !postHashMatches(candidate.facts.key,hashed))return false;
  const RetiredFacts& f=candidate.facts;
  out.stream=hashed;out.fileBytes=f.fileBytes;out.mediaTag=f.mediaTag;
  out.mountEpoch=f.mountEpoch;out.sourceEpoch=f.sourceEpoch;
  out.ownerLeaseSerial=f.ownerLeaseSerial;out.closeSerial=f.closeSerial;
  out.retiredSession=f.retiredSession;
  out.appendQueueDrained=f.appendQueueDrained;out.flushSucceeded=f.flushSucceeded;
  out.allWriterHandlesClosed=f.allWriterHandlesClosed;
  out.finalNewlineVerified=f.finalNewlineVerified;
  out.protectedAgainstWrites=f.protectedAgainstWrites;
  return true;
}
// Port contract (not physically implemented):
// Snapshot snapshot() const; bool quiesceProducers();
// bool appendNextUrgent(); bool appendNextResearch();
// bool flushAndCloseAll(); bool inspectSOSLastByte(uint64_t&,bool&);
// bool protectClosedSource();
// Each append must durably finish that record before removing its queue entry;
// each possible write must advance sourceEpoch. A port cannot silently drop
// records on a failed write. None of these assumptions are proven on hardware.
class Controller {
 public:
  State state()const{return state_;}
  Candidate candidate()const{return candidate_;}
  void losePower(){*this=Controller{};} // reboot is NOT evidence of closure

  template<class Port> State request(Port& port,const SessionKey& key){
    // No restart after Failed/RetiredCandidate without a new Controller.
    if(state_!=State::Untrusted)return fail();
    if(!validKey(key))return fail();
    const Snapshot before=port.snapshot();
    if(!base(before,key) || before.producerQuiesced ||
       before.urgentQueued>kMaxQueue || before.researchQueued>kMaxQueue)return fail();
    if(!port.quiesceProducers())return fail();
    const Snapshot after=port.snapshot();
    if(!sameOwner(before,after) || !base(after,key) ||
       !after.producerQuiesced || after.urgentQueued!=before.urgentQueued ||
       after.researchQueued!=before.researchQueued ||
       after.sourceEpoch!=before.sourceEpoch)return fail();
    key_=key; initial_=after; lastEpoch_=after.sourceEpoch;
    state_=State::Draining;return state_;
  }

  template<class Port> State poll(Port& port){
    if(state_!=State::Draining && state_!=State::Closing &&
       state_!=State::Checking)return state_;
    const Snapshot before=port.snapshot();
    if(!validCurrent(before))return fail();
    if(state_==State::Draining){
      if(before.urgentQueued || before.researchQueued){
        const bool urgent=before.urgentQueued!=0U;
        if(!(urgent?port.appendNextUrgent():port.appendNextResearch()))return fail();
        const Snapshot after=port.snapshot();
        if(!validCurrentExceptEpoch(after) ||
           after.sourceEpoch<=before.sourceEpoch ||
           after.sourceEpoch==UINT64_MAX ||
           after.urgentQueued!=before.urgentQueued-(urgent?1U:0U) ||
           after.researchQueued!=before.researchQueued-(urgent?0U:1U) ||
           after.writerHandles!=0U || after.writeInFlight ||
           after.flushSucceeded || after.writeProtected)return fail();
        lastEpoch_=after.sourceEpoch;
        return state_;
      }
      state_=State::Closing;return state_;
    }
    if(state_==State::Closing){
      if(before.urgentQueued || before.researchQueued)return fail();
      if(!port.flushAndCloseAll())return fail();
      const Snapshot after=port.snapshot();
      if(!validCurrent(after) || after.writerHandles!=0U ||
         after.writeInFlight || !after.flushSucceeded)return fail();
      state_=State::Checking;return state_;
    }
    // Inspect the *SOS* journal under the same lease, then request producer
    // write protection. A separate actual port must prove write protection.
    if(!before.flushSucceeded || before.writerHandles ||
       before.urgentQueued || before.researchQueued)return fail();
    uint64_t fileBytes=0U;bool lastNewline=false;
    if(!port.inspectSOSLastByte(fileBytes,lastNewline) || !lastNewline ||
       fileBytes>DTNF06ClosedReplay::kMaxJournalBytes)return fail();
    if(!validCurrent(port.snapshot()))return fail();
    if(!port.protectClosedSource())return fail();
    const Snapshot after=port.snapshot();
    if(!validCurrent(after) || !after.writeProtected ||
       !after.flushSucceeded || after.writerHandles || after.writeInFlight ||
       after.urgentQueued || after.researchQueued)return fail();
    Candidate c{};
    c.facts.key=key_;c.facts.fileBytes=fileBytes;
    c.facts.mediaTag=after.mediaTag;c.facts.mountEpoch=after.mountEpoch;
    c.facts.sourceEpoch=after.sourceEpoch;c.facts.closeSerial=after.closeSerial;
    c.facts.ownerLeaseSerial=after.ownerLeaseSerial;
    c.facts.retiredSession=true;c.facts.appendQueueDrained=true;
    c.facts.flushSucceeded=true;c.facts.allWriterHandlesClosed=true;
    c.facts.finalNewlineVerified=true;c.facts.protectedAgainstWrites=true;
    c.eligible=true; candidate_=c;state_=State::RetiredCandidate;
    return state_;
  }

  // An owner must independently recheck these values before relying on the
  // candidate. Recheck cannot prove unobserved media changes or a durable lock.
  bool stillCandidate(const Snapshot& now)const{
    return state_==State::RetiredCandidate && candidate_.eligible &&
      validCurrent(now) && now.flushSucceeded && now.writeProtected &&
      now.writerHandles==0U && now.urgentQueued==0U &&
      now.researchQueued==0U && !now.writeInFlight;
  }
 private:
  static constexpr uint32_t kMaxQueue=24U; // current logger FIFO capacities
  State state_=State::Untrusted;
  SessionKey key_{};
  Snapshot initial_{};
  uint64_t lastEpoch_=0U;
  Candidate candidate_{};
  State fail(){state_=State::Failed;candidate_=Candidate{};return state_;}
  static bool base(const Snapshot& x,const SessionKey& key){
    return sameKey(x.key,key) &&
      x.vessel==key.vessel && x.mounted && x.mediaVerified &&
      x.soleMainLoopOwner && !x.activeSOS && !x.urgentOrRadio &&
      !x.exportActive && !x.historyActive && !x.writeInFlight &&
      x.mediaTag!=0U && x.mountEpoch!=0U && x.sourceEpoch!=0U &&
      x.ownerLeaseSerial!=0U && x.closeSerial!=0U;
  }
  static bool sameOwner(const Snapshot& a,const Snapshot& b){
    return sameKey(a.key,b.key) &&
      a.vessel==b.vessel && a.mediaTag==b.mediaTag &&
      a.mountEpoch==b.mountEpoch &&
      a.ownerLeaseSerial==b.ownerLeaseSerial &&
      a.closeSerial==b.closeSerial;
  }
  bool validCurrentExceptEpoch(const Snapshot& x)const {
    return base(x,key_) && x.producerQuiesced &&
      sameOwner(initial_,x) && x.sourceEpoch>=lastEpoch_ &&
      x.urgentQueued<=kMaxQueue && x.researchQueued<=kMaxQueue;
  }
  bool validCurrent(const Snapshot& x)const{
    return validCurrentExceptEpoch(x) && x.sourceEpoch==lastEpoch_;
  }
};
} // namespace DTNF24H
