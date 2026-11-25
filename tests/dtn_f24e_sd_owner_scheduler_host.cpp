#include "dtn_f24e_sd_owner_scheduler.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace DTNF24E;
struct FakeVolume;
struct FakeFile {
  FakeVolume* volume=nullptr;
  bool opened=false;
  bool isOpen()const{return opened;}
  uint64_t size()const;
  bool readBlockAt(uint64_t offset,uint8_t* out,size_t length)const;
  void close();
};
struct FakeVolume {
  std::string data;
  unsigned opens=0U,reads=0U,closes=0U,metadata=0U;
  bool missing=false,readError=false;
  bool openReadOnly(const char* path,FakeFile& file){
    ++opens;
    if(missing||std::strcmp(path,"/THESIS/F06TEST/V002/S58BAFE2/sos_events.jsonl"))return false;
    file.volume=this;file.opened=true;return true;
  }
};
uint64_t FakeFile::size()const {
  if(volume)++volume->metadata;
  return volume?volume->data.size():0U;
}
bool FakeFile::readBlockAt(uint64_t offset,uint8_t* out,size_t length)const {
  if(!volume||!opened||volume->readError||offset>volume->data.size()||
     length>volume->data.size()-offset)return false;
  ++volume->reads;
  std::memcpy(out,volume->data.data()+offset,length);return true;
}
void FakeFile::close(){if(volume&&opened)++volume->closes;opened=false;volume=nullptr;}
struct Gate {
  bool closure=true,media=true,operatorGrant=true,owner=true;
  bool allowClosedJournal(const DTNAckLedger::Stream& s,const char* path)const{
    return closure&&media&&operatorGrant&&owner&&s.vessel==2U&&path&&
      std::strcmp(path,"/THESIS/F06TEST/V002/S58BAFE2/sos_events.jsonl")==0;
  }
};
static DTNAckLedger::Stream stream(const std::string& data){
  DTNAckLedger::Stream s{};
  s.uploader.bytes[6]=0x40U;s.uploader.bytes[8]=0x80U;s.uploader.bytes[15]=1U;
  s.vessel=2U;s.journal=DTNAckLedger::Journal::SosEvents;
  std::memcpy(s.session,"S58BAFE2",9U);
  DTNSha256::Hasher h;assert(h.begin());
  assert(h.update(reinterpret_cast<const uint8_t*>(data.data()),data.size()));
  assert(h.end(s.sha256));assert(DTNAckLedger::validStream(s));return s;
}
int main(){
  const std::string original(2049U,'X');
  const auto s=stream(original);
  using Schedule=Scheduler<FakeFile,FakeVolume>;
  FakeVolume vol{original};Schedule scheduler;
  Gate deny{};deny.operatorGrant=false;
  assert(scheduler.queue(vol,s,deny)==Phase::Denied);
  assert(vol.opens==0&&vol.reads==0);
  assert(scheduler.verifiedAtScan()==nullptr);
  assert(!scheduler.reset(false)&&scheduler.reset(true));
  // Invalid input never reaches the volume.
  auto wrong=s;wrong.session[1]='z';
  assert(scheduler.queue(vol,wrong,Gate{})==Phase::Failed);
  assert(scheduler.detail()==DTNF24D::Result::BadStream&&vol.opens==0);
  assert(scheduler.reset(true));
  Gate gate{};
  assert(scheduler.queue(vol,s,gate)==Phase::Queued);
  assert(vol.opens==0);
  assert(scheduler.poll(gate,false,false)==Phase::WaitingForOwner);
  assert(scheduler.poll(gate,true,true)==Phase::Preempted);
  assert(vol.opens==0&&vol.reads==0);
  assert(scheduler.poll(gate,true,false,9999U)==Phase::Hashing);
  assert(scheduler.hashedBytes()==DTNF24D::kMaxStepBytes);
  assert(vol.opens==1&&vol.reads==2);
  const unsigned reads=vol.reads,metadata=vol.metadata;
  assert(scheduler.poll(gate,false,false)==Phase::WaitingForOwner);
  assert(scheduler.poll(gate,true,true)==Phase::Preempted);
  assert(vol.reads==reads&&vol.metadata==metadata); // no SD I/O while deferred
  while(scheduler.phase()!=Phase::Verified){
    assert(scheduler.poll(gate,true,false,400U)==Phase::Hashing||
           scheduler.phase()==Phase::Verified);
  }
  assert(scheduler.verifiedAtScan()!=nullptr);
  assert(DTNAckLedger::sameStream(*scheduler.verifiedAtScan(),s));
  assert(scheduler.hashedBytes()==original.size());
  // A later revocation invalidates even a previously completed scan result.
  const unsigned readsAfterHash=vol.reads;
  Gate revokedAfter=gate;revokedAfter.media=false;
  assert(scheduler.poll(revokedAfter,false,false)==Phase::Denied);
  assert(scheduler.verifiedAtScan()==nullptr&&vol.reads==readsAfterHash);
  assert(scheduler.reset(true));
  // Denial while owner unavailable must NOT close SD via another task.
  assert(scheduler.queue(vol,s,gate)==Phase::Queued);
  assert(scheduler.poll(gate,true,false,80U)==Phase::Hashing);
  const unsigned closeBefore=vol.closes;
  gate.media=false;
  assert(scheduler.poll(gate,false,false)==Phase::Denied);
  assert(vol.closes==closeBefore&&scheduler.verifiedAtScan()==nullptr);
  assert(scheduler.poll(gate,true,false)==Phase::Denied);
  assert(vol.closes==closeBefore+1U);
  assert(!scheduler.reset(false)&&scheduler.reset(true));
  gate.media=true;
  assert(scheduler.queue(vol,s,gate)==Phase::Queued);
  assert(scheduler.poll(gate,true,false,512U)==Phase::Hashing);
  vol.data[100U]='Y'; // same length mutation during the scan
  while(scheduler.phase()==Phase::Hashing){scheduler.poll(gate,true,false,512U);}
  // Byte 100 was in the processed portion: mutated after it was hashed.
  // This is a genuine TOCTOU limitation; no false claim of detection.
  assert(scheduler.phase()==Phase::Verified);
  assert(scheduler.reset(true));
  // If mutation predates scan, complete SHA catches it.
  assert(scheduler.queue(vol,s,gate)==Phase::Queued);
  while(scheduler.phase()==Phase::Queued||scheduler.phase()==Phase::Hashing)
    scheduler.poll(gate,true,false,512U);
  assert(scheduler.phase()==Phase::Failed);
  assert(scheduler.detail()==DTNF24D::Result::DigestMismatch);
  assert(scheduler.reset(true));
  vol.data=original;
  assert(scheduler.queue(vol,s,gate)==Phase::Queued);
  assert(scheduler.poll(gate,true,false,64U)==Phase::Hashing);
  vol.data.push_back('Q');
  assert(scheduler.poll(gate,true,false,512U)==Phase::Failed);
  assert(scheduler.detail()==DTNF24D::Result::ChangedSource);
  assert(scheduler.reset(true));
  FakeVolume missing{original};missing.missing=true;
  assert(scheduler.queue(missing,s,gate)==Phase::Queued);
  assert(scheduler.poll(gate,true,false)==Phase::Failed);
  assert(scheduler.detail()==DTNF24D::Result::Missing);
  assert(scheduler.reset(true));
  FakeVolume failure{original};failure.readError=true;
  assert(scheduler.queue(failure,s,gate)==Phase::Queued);
  assert(scheduler.poll(gate,true,false)==Phase::Failed);
  assert(scheduler.detail()==DTNF24D::Result::ReadError);
  assert(scheduler.reset(true));
  std::puts("F24E SD-owner poll scheduling/priority/denial/size+digest integrity: PASS");
}
