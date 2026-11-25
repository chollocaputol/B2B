#include "../firmware/shared/storage/dtn_f24f_sos_closed_lease.h"
#include "../firmware/shared/storage/dtn_f06_closed_replay.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

namespace F=DTNF24F;
namespace A=DTNAckLedger;
namespace R=DTNF06ClosedReplay;
static A::Id128 uuid(uint8_t value){
  A::Id128 v{};v.bytes[0]=value;v.bytes[6]=0x40U;v.bytes[8]=0x80U;return v;
}
static A::Stream stream(){
  A::Stream s{};s.uploader=uuid(2U);s.vessel=2U;
  strcpy(s.session,"S58BAFE2");s.journal=A::Journal::SosEvents;
  s.sha256[0]=0xA5U;return s;
}
static F::ClosureProof proof(const A::Stream& s){
  F::ClosureProof p{};p.stream=s;p.fileBytes=156U;p.mediaTag=887U;
  p.mountEpoch=33U;p.sourceEpoch=17U;p.closeSerial=5U;
  p.independentlyApproved=true;p.loggerClosed=true;
  p.fullFileSealed=true;p.finalLineComplete=true;
  p.immutableSourceProtected=true;return p;
}
static F::OwnerNow owner(const A::Stream& s){
  F::OwnerNow o{};o.uploader=s.uploader;o.vessel=s.vessel;
  o.mediaTag=887U;o.mountEpoch=33U;o.sourceEpoch=17U;
  o.exclusiveLeaseSerial=223U;
  o.mounted=true;o.mediaIdentityVerified=true;
  o.soleMainLoopOwner=true;o.sourceReadProtected=true;
  o.writerActive=false;o.pendingAppends=false;o.currentSession=false;
  o.urgentOrRadio=false;return o;
}
static const char* const path="/THESIS/F06TEST/V002/S58BAFE2/sos_events.jsonl";
static bool allowed(const F::ClosureProof& p,F::OwnerNow& o,const A::Stream& s){
  F::SosClosedLease lease(p,o);return lease.allowClosedJournal(s,path);
}
struct File{
  std::string* bytes=nullptr;
  bool isopen=false;
  unsigned* reads=nullptr;
  F::OwnerNow* revokeOwner=nullptr;
  unsigned* byteReads=nullptr;
  unsigned revokeAfter=0;
  bool isOpen()const{if(reads)++*reads;return isopen;}
  uint64_t size()const{if(reads)++*reads;return bytes?bytes->size():0U;}
  bool readAt(uint64_t off,uint8_t& out)const{
    if(reads)++*reads;
    if(!bytes || off>=bytes->size())return false;
    out=static_cast<uint8_t>((*bytes)[static_cast<size_t>(off)]);
    if(byteReads && ++(*byteReads)==revokeAfter && revokeOwner)
      ++revokeOwner->sourceEpoch; // synthetic mutation *during* a JSONL read
    return true;
  }
  void close(){isopen=false;}
};
struct Volume{
  std::string bytes;
  unsigned opens=0,reads=0,byteReads=0,revokeAfter=0;
  F::OwnerNow* revokeOwner=nullptr;
  bool openReadOnly(const char*,File& f){
    ++opens;f.bytes=&bytes;f.isopen=true;f.reads=&reads;
    f.revokeOwner=revokeOwner;f.byteReads=&byteReads;f.revokeAfter=revokeAfter;
    return true;
  }
};
struct Digest{bool verifyCompleteSha256(const File& f,uint64_t n,const uint8_t expected[32])const{
  return f.isopen&&f.bytes&&f.bytes->size()==n&&expected[0]==0xA5U;
}}; // TEST DOUBLE only; not actual SHA-256
struct Slots{
  A::Snapshot s[2]{};bool present[2]={false,false};
  A::SlotRead read(unsigned slot,A::Snapshot& out){
    if(!present[slot])return A::SlotRead::Missing;
    out=s[slot];return A::SlotRead::Present;
  }
  bool write(unsigned slot,const A::Snapshot& snap){
    s[slot]=snap;present[slot]=true;return true;
  }
};
using Ledger=A::Ledger<Slots>;
using Reader=R::Reader<File,Volume,F::SosClosedLease,Digest,Ledger>;
static std::string line(unsigned i){
  return "{\"kind\":\"sos_rx\",\"vessel_id\":2,\"boot_token\":42,\"event_index\":"+
      std::to_string(i)+",\"uptime_ms\":1}\n";
}
int main(){
  const A::Stream s=stream();
  F::ClosureProof p=proof(s);
  F::OwnerNow o=owner(s);
  F::SosClosedLease empty;
  assert(!empty.allowClosedJournal(s,path));
  assert(allowed(p,o,s)); // fixture-supplied proof ONLY, not hardware proof
  {
    F::SosClosedLease sizeLease(p,o);
    assert(!sizeLease.allowClosedJournalBytes(s,path,155U));
    assert(sizeLease.revoked());
    assert(!sizeLease.allowClosedJournalBytes(s,path,156U));
  }
  F::SosClosedLease lock(p,o);
  assert(lock.allowClosedJournal(s,path));
  assert(!lock.revoked());
  assert(F::classify("sos_local_start")==F::SosRecordClass::ActivationCandidate);
  assert(F::classify("sos_local_cancel")==F::SosRecordClass::CancellationCandidate);
  assert(F::classify("sos_rx")==F::SosRecordClass::RadioEvidenceOnly);
  assert(F::classify("sos_ack_rx")==F::SosRecordClass::RadioEvidenceOnly);
  assert(F::classify("relay_tx")==F::SosRecordClass::RadioEvidenceOnly);
  assert(F::classify("weather")==F::SosRecordClass::Unsupported);
  assert(F::classify(nullptr)==F::SosRecordClass::Unsupported);
  char pth[96]{};assert(F::sosPath(s,pth,sizeof(pth)) && strcmp(pth,path)==0);
  assert(!lock.allowClosedJournal(s,"/THESIS/F06TEST/V002/S58BAFE2/telemetry.jsonl"));
  assert(lock.revoked());
  assert(!lock.allowClosedJournal(s,path)); // permanently fail closed
  auto other=s;other.journal=A::Journal::Telemetry;
  assert(!F::sosPath(other,pth,sizeof(pth)));
  assert(!allowed(p,o,other));
  other=s;other.vessel=3U;assert(!allowed(p,o,other));
  other=s;other.uploader=uuid(3U);assert(!allowed(p,o,other));
  other=s;other.session[1]='A';assert(!allowed(p,o,other));
  other=s;other.sha256[2]=7U;assert(!allowed(p,o,other));

  // All closure and source-owner requirements are independently necessary.
  p=proof(s);p.independentlyApproved=false;assert(!allowed(p,o,s));
  p=proof(s);p.loggerClosed=false;assert(!allowed(p,o,s));
  p=proof(s);p.fullFileSealed=false;assert(!allowed(p,o,s));
  p=proof(s);p.finalLineComplete=false;assert(!allowed(p,o,s));
  p=proof(s);p.immutableSourceProtected=false;assert(!allowed(p,o,s));
  p=proof(s);p.closeSerial=0;assert(!allowed(p,o,s));
  p=proof(s);p.mediaTag=0;assert(!allowed(p,o,s));
  p=proof(s);p.mountEpoch=0;assert(!allowed(p,o,s));
  p=proof(s);p.sourceEpoch=0;assert(!allowed(p,o,s));
  o=owner(s);o.mounted=false;assert(!allowed(proof(s),o,s));
  o=owner(s);o.mediaIdentityVerified=false;assert(!allowed(proof(s),o,s));
  o=owner(s);o.soleMainLoopOwner=false;assert(!allowed(proof(s),o,s));
  o=owner(s);o.sourceReadProtected=false;assert(!allowed(proof(s),o,s));
  o=owner(s);o.writerActive=true;assert(!allowed(proof(s),o,s));
  o=owner(s);o.pendingAppends=true;assert(!allowed(proof(s),o,s));
  o=owner(s);o.currentSession=true;assert(!allowed(proof(s),o,s));
  o=owner(s);o.urgentOrRadio=true;assert(!allowed(proof(s),o,s));
  o=owner(s);o.mediaTag=999;assert(!allowed(proof(s),o,s));
  o=owner(s);o.mountEpoch=34;assert(!allowed(proof(s),o,s));
  o=owner(s);o.sourceEpoch=18;assert(!allowed(proof(s),o,s));
  o=owner(s);o.exclusiveLeaseSerial=0;assert(!allowed(proof(s),o,s));
  o=owner(s);o.vessel=3;assert(!allowed(proof(s),o,s));
  o=owner(s);o.uploader=uuid(9);assert(!allowed(proof(s),o,s));

  // Only a simulated immutable closure approval can open a host journal.
  p=proof(s);o=owner(s);F::SosClosedLease gate(p,o);
  Slots slots;Ledger ledger;
  assert(ledger.load(slots,s)==A::Load::Fresh);
  assert(ledger.initializeFresh(slots,s));
  Volume volume;volume.bytes=line(1)+line(2);
  Reader replay;Digest digest;
  assert(replay.begin(volume,s,gate,digest,ledger)==R::Open::Ready);
  assert(volume.opens==1U);
  char json[DTNJsonlSnapshot::kMaxLineBytes+1U]{};
  R::Candidate candidate{};
  assert(replay.next(gate,ledger,json,sizeof(json),candidate)==R::Read::CandidateUnverified);
  assert(candidate.fields.eventIndex==1U);
  const uint64_t cursor=replay.cursor().offset;
  const unsigned readsBefore=volume.reads;
  // Sudden card exchange / unmount or source-mutation epoch change: no SD calls.
  o.mountEpoch++;
  assert(replay.next(gate,ledger,json,sizeof(json),candidate)==R::Read::NotAuthorized);
  assert(volume.reads==readsBefore);
  assert(replay.cursor().offset==cursor);
  assert(candidate.fields.eventIndex==0U);
  o.mountEpoch--; // restoring a value does NOT revive a revoked lease
  assert(replay.next(gate,ledger,json,sizeof(json),candidate)==R::Read::NotAuthorized);
  assert(volume.reads==readsBefore);
  replay.close(); // simulated sole owner has now reacquired SD for safe handle close
  assert(!replay.opened());

  // Revocation DURING a record read: sourceEpoch changes as the fourth byte
  // is read. The next byte must not be accessed and no cursor may advance.
  p=proof(s);o=owner(s);F::SosClosedLease duringReadGate(p,o);
  Volume changed;changed.bytes=line(1)+line(2);
  changed.revokeOwner=&o;changed.revokeAfter=4;
  Reader interrupted;
  assert(interrupted.begin(changed,s,duringReadGate,digest,ledger)==R::Open::Ready);
  assert(interrupted.next(duringReadGate,ledger,json,sizeof(json),candidate)==R::Read::NotAuthorized);
  assert(interrupted.cursor().ordinal==0U);
  assert(changed.byteReads==4U);
  const unsigned ioAtRevoke=changed.reads;
  assert(interrupted.next(duringReadGate,ledger,json,sizeof(json),candidate)==R::Read::NotAuthorized);
  assert(changed.reads==ioAtRevoke);
  interrupted.close(); // only after host fixture restores its simulated owner
  o=owner(s);

  // A denied begin() must not open the volume.
  F::SosClosedLease denied;
  assert(replay.begin(volume,s,denied,digest,ledger)==R::Open::NotAuthorized);
  assert(volume.opens==1U);

  // Simulated same-size source mutation must change authoritative sourceEpoch.
  p=proof(s);o=owner(s);F::SosClosedLease g2(p,o);
  assert(g2.allowClosedJournal(s,path));
  o.sourceEpoch++;
  assert(!g2.allowClosedJournal(s,path) && g2.revoked());
  o=owner(s);F::SosClosedLease g3(p,o);
  assert(g3.allowClosedJournal(s,path));
  o.urgentOrRadio=true;
  assert(!g3.allowClosedJournal(s,path));
  o.urgentOrRadio=false;
  assert(!g3.allowClosedJournal(s,path));

  std::puts("F24F SOS-only closed lease/replay-time revocation: PASS");
}
