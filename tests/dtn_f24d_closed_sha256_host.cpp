#include "dtn_f24d_closed_sha256_job.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

using DTNF24D::Result;
static void hex(const char* s,uint8_t bytes[32]){
  for(unsigned i=0;i<32;++i){unsigned n=0;
    const int parsed=std::sscanf(s+2U*i,"%2x",&n);assert(parsed==1);
    bytes[i]=static_cast<uint8_t>(n);}
}
static void digest(const std::string& text,uint8_t out[32]){
  DTNSha256::Hasher h;assert(h.begin());
  assert(h.update(reinterpret_cast<const uint8_t*>(text.data()),text.size()));
  assert(h.end(out));assert(!h.end(out));
}
static DTNAckLedger::Stream stream(const std::string& data){
  DTNAckLedger::Stream s{};
  s.vessel=2U;std::memcpy(s.session,"S58BAFE2",9);
  s.journal=DTNAckLedger::Journal::SosEvents;
  s.uploader.bytes[6]=0x40U;s.uploader.bytes[8]=0x80U;s.uploader.bytes[15]=0x01U;
  digest(data,s.sha256);assert(DTNAckLedger::validStream(s));return s;
}
struct FakeFile {
  std::string bytes{};bool active=false,ioFail=false;
  bool isOpen()const{return active;}
  uint64_t size()const{return bytes.size();}
  bool readBlockAt(uint64_t offset,uint8_t* out,size_t n)const{
    if(!active||ioFail||offset>bytes.size()||n>bytes.size()-offset)return false;
    std::memcpy(out,bytes.data()+static_cast<size_t>(offset),n);return true;
  }
  void close(){active=false;}
};
struct FakeVolume {
  std::string bytes{};bool missing=false;size_t openCount=0U;
  bool openReadOnly(const char*,FakeFile& out){
    ++openCount;if(missing)return false;
    out.bytes=bytes;out.active=true;return true;
  }
};
struct Gate {
  bool permit=false;bool closed=false;
  bool allowClosedJournal(const DTNAckLedger::Stream& s,const char* path)const{
    return permit&&closed&&s.vessel==2U&&path&&
      std::string(path)=="/THESIS/F06TEST/V002/S58BAFE2/sos_events.jsonl";
  }
};
int main(){
  uint8_t actual[32]{},expected[32]{};
  digest("",actual);hex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",expected);
  assert(DTNSha256::equals32(actual,expected));
  digest("abc",actual);hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",expected);
  assert(DTNSha256::equals32(actual,expected));
  digest(std::string(1000000,'a'),actual);
  hex("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",expected);
  assert(DTNSha256::equals32(actual,expected));
  const std::string data="{\"kind\":\"sos_local_start\",\"vessel_id\":2}\n";
  const auto s=stream(data);
  using Job=DTNF24D::Job<FakeFile,FakeVolume>;
  Gate denied{};FakeVolume volume{data};Job j;
  assert(j.begin(volume,s,denied)==Result::Denied&&volume.openCount==0);
  Gate good{true,true};
  assert(j.begin(volume,s,good)==Result::Running&&volume.openCount==1);
  assert(j.step(good,true,512U)==Result::Paused&&j.bytesProcessed()==0U);
  assert(j.step(good,false,3U)==Result::Running&&j.bytesProcessed()==3U);
  assert(j.step(good,false,512U)==Result::Verified&&j.bytesProcessed()==data.size());
  assert(j.step(good,false)==Result::Verified);
  assert(j.begin(volume,s,good)==Result::Running);
  denied.permit=false;
  assert(j.step(denied,false)==Result::Denied);
  assert(j.begin(volume,s,good)==Result::Running);
  assert(j.step(good,false,5U)==Result::Running);
  j.clear();assert(j.status()==Result::Idle);
  assert(j.begin(volume,s,good)==Result::Running);
  // Fake host File is an immutable-by-default copy; explicit same-size mutation in
  // File must be detected by SHA even though byte-length is unchanged.
  // The digest is recomputed for the whole file under the reviewed closure gate.
  assert(j.step(good,false,512U)==Result::Verified);
  std::string changed=data;changed[3]^=1;
  FakeVolume altered{changed};
  assert(j.begin(altered,s,good)==Result::Running);
  assert(j.step(good,false,512U)==Result::DigestMismatch);
  FakeVolume missing{data,true};
  assert(j.begin(missing,s,good)==Result::Missing);
  FakeVolume oversize{std::string(DTNF06ClosedReplay::kMaxJournalBytes+1U,'Q')};
  assert(j.begin(oversize,s,good)==Result::Oversize);
  auto wrong=s;wrong.session[1]='a';
  assert(j.begin(volume,wrong,good)==Result::BadStream);
  const std::string big(2000,'z');auto bigger=stream(big);
  FakeVolume longfile{big};
  assert(j.begin(longfile,bigger,good)==Result::Running);
  assert(j.step(good,false,20000U)==Result::Running);
  assert(j.bytesProcessed()==DTNF24D::kMaxStepBytes);
  size_t n=1;
  while(j.status()==Result::Running && n++<20){j.step(good,false,512U);}
  assert(j.status()==Result::Verified);
  // The whole source can change after closing; freeze is an EXTERNAL gate.
  assert(j.begin(volume,s,good)==Result::Running);
  Gate revoked=good;revoked.closed=false;
  assert(j.step(revoked,false)==Result::Denied);
  std::puts("F24D SHA256 vectors/cooperative closed-journal read/preemption/denial: PASS");
}
