#pragma once
// F24F HOST / BUILD-ONLY SOS replay permission contract.
// NO physical close attestor, CID reader, mount, SD I/O, network, credential
// provisioner, upload or sidecar writer is supplied by this header.
// Evidence MUST come from independently reviewed, authoritative SD-owner code;
// copying a previous hash or setting bools is NOT proof of immutability.
// Default state always DENIES. Rejected leases stay revoked forever.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dtn_ack_ledger.h"

namespace DTNF24F {
using Stream=DTNAckLedger::Stream;
// Exact F06 SOS journal only. No telemetry, diagnostics, wildcard directory,
// or arbitrary path can be borrowed by the operational SOS DTN pipeline.
inline bool sosPath(const Stream& stream,char* output,size_t capacity){
  if(!output || capacity==0U || !DTNAckLedger::validStream(stream) ||
     stream.journal!=DTNAckLedger::Journal::SosEvents)return false;
  const int n=snprintf(output,capacity,"/THESIS/F06TEST/V%03lu/%s/sos_events.jsonl",
                       static_cast<unsigned long>(stream.vessel),stream.session);
  return n>0 && static_cast<size_t>(n)<capacity;
}

// Treat F06 events as CANDIDATES, not authenticated emergency actions.
// 'sos_rx', ACKs, radio relays, and TX attempts are evidence, NOT a new SOS.
enum class SosRecordClass:uint8_t {
  ActivationCandidate, CancellationCandidate, RadioEvidenceOnly, Unsupported
};
inline SosRecordClass classify(const char* kind){
  if(!kind)return SosRecordClass::Unsupported;
  if(strcmp(kind,"sos_local_start")==0)return SosRecordClass::ActivationCandidate;
  if(strcmp(kind,"sos_local_cancel")==0)return SosRecordClass::CancellationCandidate;
  const char* const evidenceKinds[]={"sos_rx","sos_ack_tx","sos_ack_rx","sos_tx_attempt",
       "cancel_rx","cancel_ack_rx","cancel_ack_tx","cancel_tx_attempt","relay_tx"};
  for(const char* evidence: evidenceKinds){
    if(strcmp(kind,evidence)==0)return SosRecordClass::RadioEvidenceOnly;
  }
  return SosRecordClass::Unsupported;
}

// These are INPUTS ONLY. The firmware currently has no independently trusted
// way to set all proof fields, so no physical source is authorized by F24F.
struct ClosureProof {
  Stream stream{};
  uint64_t fileBytes=0U;
  uint64_t mediaTag=0U;     // verified owner-provided media binding, NOT raw CID
  uint64_t mountEpoch=0U;   // changes on EVERY mount/reinitialization
  uint64_t sourceEpoch=0U;  // changes on EVERY possible source mutation
  uint64_t closeSerial=0U;  // positive independently committed close generation
  bool independentlyApproved=false;
  bool loggerClosed=false;
  bool fullFileSealed=false;
  bool finalLineComplete=false;
  bool immutableSourceProtected=false;
};
struct OwnerNow {
  DTNAckLedger::Id128 uploader{};
  uint32_t vessel=0U;
  uint64_t mediaTag=0U;
  uint64_t mountEpoch=0U;
  uint64_t sourceEpoch=0U;
  uint64_t exclusiveLeaseSerial=0U;
  bool mounted=false;
  bool mediaIdentityVerified=false;
  bool soleMainLoopOwner=false;
  bool sourceReadProtected=false;
  bool writerActive=true;
  bool pendingAppends=true;
  bool currentSession=true;
  bool urgentOrRadio=true;
};

class SosClosedLease {
public:
  SosClosedLease()=default; // permanently default-denied without proof + owner
  SosClosedLease(const ClosureProof& proof,const OwnerNow& owner):
      proof_(proof),owner_(&owner),leaseSerial_(owner.exclusiveLeaseSerial){}
  SosClosedLease(const SosClosedLease&)=delete;
  SosClosedLease& operator=(const SosClosedLease&)=delete;
  bool revoked()const{return revoked_;}
  bool allowClosedJournalBytes(const Stream& stream,const char* path,
                               uint64_t observedBytes)const{
    if(!allowClosedJournal(stream,path))return false;
    if(observedBytes!=proof_.fileBytes){revoked_=true;return false;}
    return true;
  }
  // Pure RAM check; MUST NOT access SD, SPI, or LoRa interrupts here.
  bool allowClosedJournal(const Stream& candidate,const char* exactPath)const{
    if(revoked_)return false;
    char expected[96]{};
    const OwnerNow* const now=owner_;
    const bool accepted=now && sosPath(proof_.stream,expected,sizeof(expected)) &&
      exactPath && strcmp(exactPath,expected)==0 &&
      DTNAckLedger::sameStream(candidate,proof_.stream) &&
      proof_.independentlyApproved && proof_.loggerClosed &&
      proof_.fullFileSealed && proof_.finalLineComplete &&
      proof_.immutableSourceProtected && proof_.closeSerial!=0U &&
      proof_.mediaTag!=0U && proof_.mountEpoch!=0U &&
      proof_.sourceEpoch!=0U && leaseSerial_!=0U &&
      now->mounted && now->mediaIdentityVerified &&
      now->soleMainLoopOwner && now->sourceReadProtected &&
      !now->writerActive && !now->pendingAppends && !now->currentSession &&
      !now->urgentOrRadio &&
      now->vessel==candidate.vessel &&
      DTNAckLedger::sameId(now->uploader,candidate.uploader) &&
      now->mediaTag==proof_.mediaTag &&
      now->mountEpoch==proof_.mountEpoch &&
      now->sourceEpoch==proof_.sourceEpoch &&
      now->exclusiveLeaseSerial==leaseSerial_;
    if(!accepted)revoked_=true;
    return accepted;
  }
private:
  ClosureProof proof_{};
  const OwnerNow* owner_=nullptr; // supplied by the exclusive main-loop owner
  uint64_t leaseSerial_=0U;
  mutable bool revoked_=false;
};
} // namespace DTNF24F
