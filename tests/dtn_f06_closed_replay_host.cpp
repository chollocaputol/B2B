#include "../firmware/shared/storage/dtn_f06_closed_replay.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
using namespace DTNF06ClosedReplay;
namespace L=DTNAckLedger;

struct MockFile {
  std::string* bytes=nullptr;
  uint64_t failAt=UINT64_MAX;
  bool open=false;
  bool isOpen() const {return open && bytes!=nullptr;}
  uint64_t size() const {return bytes?static_cast<uint64_t>(bytes->size()):0U;}
  bool readAt(uint64_t at,uint8_t& output) const {
    if(!isOpen() || at==failAt || at>=size())return false;
    output=static_cast<uint8_t>((*bytes)[static_cast<size_t>(at)]);
    return true;
  }
  void close(){open=false;}
};
struct MockVolume {
  std::string bytes;
  std::string lastPath;
  unsigned readOpens=0U,writeOpens=0U;
  bool deny=false;
  bool openReadOnly(const char* path,MockFile& output){
    ++readOpens;lastPath=path;
    if(deny)return false;
    output.bytes=&bytes;output.open=true;return true;
  }
};
struct Gate {
  bool authorized=true,closed=true,identityMatches=true;
  bool allowClosedJournal(const L::Stream&,const char* path)const{
    return authorized && closed && identityMatches &&
       strncmp(path,"/THESIS/F06TEST/V002/",21U)==0;
  }
  bool allowClosedJournalBytes(const L::Stream& stream,const char* path,
                               uint64_t) const {
    return allowClosedJournal(stream,path); // F24B fixture has NO true seal
  }
};
struct MockDigest {
  bool valid=true;
  mutable unsigned calls=0U;
  // TEST DOUBLE only, deliberately NOT SHA256; production must use a real
  // complete-file cryptographic verifier before treating this as sealed.
  bool verifyCompleteSha256(const MockFile& f,uint64_t n,const uint8_t expected[32])const{
    ++calls;
    return valid && f.isOpen() && f.size()==n && expected[0]==0xA5U;
  }
};
struct SlotStore {
  L::Snapshot slots[2]{};
  bool present[2]={false,false};
  L::SlotRead read(unsigned n,L::Snapshot& out){
    if(!present[n])return L::SlotRead::Missing;
    out=slots[n];return L::SlotRead::Present;
  }
  bool write(unsigned n,const L::Snapshot& value){
    slots[n]=value;present[n]=true;return true;
  }
};
static L::Id128 id(uint8_t n){
  L::Id128 out{};out.bytes[0]=n;out.bytes[6]=0x40U;out.bytes[8]=0x80U;
  return out;
}
static L::Stream stream(L::Journal j=L::Journal::SosEvents){
  L::Stream s{};s.uploader=id(1U);s.vessel=2U;
  strcpy(s.session,"S58BAFE2");s.journal=j;s.sha256[0]=0xA5U;
  return s;
}
static std::string event(unsigned n,uint32_t vessel=2U){
  return "{\"kind\":\"sos_rx\",\"vessel_id\":"+std::to_string(vessel)+
      ",\"boot_token\":42,\"event_index\":"+std::to_string(n)+
      ",\"uptime_ms\":"+std::to_string(n*10U)+"}\n";
}
using Ledger=L::Ledger<SlotStore>;
using Replay=Reader<MockFile,MockVolume,Gate,MockDigest,Ledger>;
static void initial(Ledger& ledger,SlotStore& store,L::Stream s){
  assert(ledger.load(store,s)==L::Load::Fresh);
  assert(ledger.initializeFresh(store,s));
}
int main(){
  auto key=stream();
  SlotStore store;Ledger ledger;initial(ledger,store,key);
  MockVolume volume;volume.bytes=event(1)+event(2)+event(3)+event(4);
  Gate gate;MockDigest digest;Replay reader;
  assert(reader.begin(volume,key,gate,digest,ledger)==Open::Ready);
  assert(volume.lastPath=="/THESIS/F06TEST/V002/S58BAFE2/sos_events.jsonl");
  assert(volume.readOpens==1 && volume.writeOpens==0 && digest.calls==1);
  char json[DTNJsonlSnapshot::kMaxLineBytes+1U]{};
  Candidate got{};
  assert(reader.next(gate,ledger,json,sizeof(json),got)==Read::CandidateUnverified);
  assert(got.frame.ordinal==0U && got.fields.vesselId==2U && got.fields.eventIndex==1U);
  assert(reader.next(gate,ledger,json,sizeof(json),got)==Read::CandidateUnverified);
  assert(got.frame.ordinal==1U);
  assert(reader.next(gate,ledger,json,sizeof(json),got)==Read::CandidateUnverified);
  assert(got.frame.ordinal==2U);
  assert(reader.next(gate,ledger,json,sizeof(json),got)==Read::CandidateUnverified);
  assert(got.frame.ordinal==3U);
  assert(reader.next(gate,ledger,json,sizeof(json),got)==Read::End);
  reader.close();assert(!reader.opened());

  // Explicit per-record replies 0=accepted, 1=rejected, 2=duplicate.
  // Ordinal 2 is still durably acknowledged, even though 1 has a gap.
  L::RecordRef records[]={{0U,id(10U)},{1U,id(11U)},{2U,id(12U)}};
  L::Batch batch{id(30U),key.uploader,records,3U};
  L::ItemReply items[]={{id(10U),L::Reply::Accepted},
                        {id(11U),L::Reply::Rejected},
                        {id(12U),L::Reply::Duplicate}};
  L::Response response{1U,id(30U),key.uploader,items,3U};
  assert(ledger.apply(store,batch,response,true)==L::Commit::Applied);
  assert(ledger.firstPending()==1U && ledger.acknowledged(2U));
  Ledger reboot;assert(reboot.load(store,key)==L::Load::Loaded);
  assert(reader.begin(volume,key,gate,digest,reboot)==Open::Ready);
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::CandidateUnverified);
  assert(got.frame.ordinal==1U); // skipped 0 from durable prefix
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::CandidateUnverified);
  assert(got.frame.ordinal==3U); // skipped out-of-order accepted 2
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::End);

  // F24F: revocation between consecutive next() calls may not read SD at all.
  // A later accidental restoration of the gate must not revive that reader.
  assert(reader.begin(volume,key,gate,digest,reboot)==Open::Ready);
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::CandidateUnverified);
  const auto cursorBeforeRevocation=reader.cursor();
  gate.authorized=false;
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::NotAuthorized);
  assert(reader.cursor().ordinal==cursorBeforeRevocation.ordinal);
  assert(got.fields.eventIndex==0U); // stale candidate never reissued
  gate.authorized=true;
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::NotAuthorized);
  assert(reader.cursor().ordinal==cursorBeforeRevocation.ordinal);
  // Only the sole actual SD owner may perform close() in device firmware.
  reader.close();

  // A different digest, uploader, session, or journal cannot borrow this sidecar.
  auto wrong=key;wrong.sha256[1]=1U;
  assert(reader.begin(volume,wrong,gate,digest,reboot)==Open::LedgerMismatch);
  assert(!reader.opened());
  wrong=key;wrong.session[1]='A';
  assert(reader.begin(volume,wrong,gate,digest,reboot)==Open::LedgerMismatch);
  wrong=key;wrong.journal=L::Journal::Telemetry;
  assert(reader.begin(volume,wrong,gate,digest,reboot)==Open::LedgerMismatch);
  wrong=key;wrong.journal=L::Journal::Diagnostics;
  assert(reader.begin(volume,wrong,gate,digest,reboot)==Open::BadStream);
  wrong=key;wrong.vessel=10000U;
  assert(!pathFor(wrong,json,sizeof(json)));
  char tiny[8]{};
  assert(!pathFor(key,tiny,sizeof(tiny)));

  gate.closed=false;
  assert(reader.begin(volume,key,gate,digest,reboot)==Open::NotAuthorized);
  gate.closed=true;gate.identityMatches=false;
  assert(reader.begin(volume,key,gate,digest,reboot)==Open::NotAuthorized);
  gate.identityMatches=true;
  digest.valid=false;
  assert(reader.begin(volume,key,gate,digest,reboot)==Open::DigestMismatch);
  digest.valid=true;
  volume.deny=true;
  assert(reader.begin(volume,key,gate,digest,reboot)==Open::Missing);
  volume.deny=false;
  assert(reader.begin(volume,key,gate,digest,reboot)==Open::Ready);
  volume.bytes.push_back('X');
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::SourceChanged);
  reader.close();volume.bytes.pop_back();

  // Malformed record is NOT skipped, even if its ordinal was previously ACK'd.
  MockVolume damaged;damaged.bytes=event(1)+"{bad}\n"+event(3);
  assert(reader.begin(damaged,key,gate,digest,reboot)==Open::Ready);
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::Malformed);
  assert(reader.cursor().ordinal==1U); // refuses to cross damaged ordinal
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::Malformed);
  reader.close();
  MockVolume misVessel;misVessel.bytes=event(1)+event(2,3U);
  assert(reader.begin(misVessel,key,gate,digest,reboot)==Open::Ready);
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::WrongVessel);
  reader.close();

  MockVolume partial;partial.bytes=event(1)+event(2).substr(0,20);
  assert(reader.begin(partial,key,gate,digest,reboot)==Open::Ready);
  assert(reader.next(gate,reboot,json,sizeof(json),got)==Read::IncompleteTail);
  reader.close();

  MockVolume oversized;oversized.bytes=std::string(static_cast<size_t>(kMaxJournalBytes)+1U,'x');
  assert(reader.begin(oversized,key,gate,digest,reboot)==Open::Oversize);

  MockVolume window;for(unsigned i=1;i<=66U;++i)window.bytes+=event(i);
  SlotStore emptyStore;Ledger fresh;initial(fresh,emptyStore,key);
  assert(reader.begin(window,key,gate,digest,fresh)==Open::Ready);
  for(unsigned i=0;i<64U;++i){
    assert(reader.next(gate,fresh,json,sizeof(json),got)==Read::CandidateUnverified);
    assert(got.frame.ordinal==i);
  }
  assert(reader.next(gate,fresh,json,sizeof(json),got)==Read::WindowBlocked);
  assert(reader.cursor().ordinal==64U);
  reader.close();

  // Separate journal streams must use separate stores and byte sources.
  auto weather=stream(L::Journal::Telemetry);
  SlotStore weatherStore;Ledger weatherLedger;initial(weatherLedger,weatherStore,weather);
  MockVolume weatherVolume;weatherVolume.bytes=
      "{\"kind\":\"weather\",\"vessel_id\":2,\"boot_token\":43,\"event_index\":1,\"uptime_ms\":0}\n";
  assert(reader.begin(weatherVolume,weather,gate,digest,weatherLedger)==Open::Ready);
  assert(weatherVolume.lastPath=="/THESIS/F06TEST/V002/S58BAFE2/telemetry.jsonl");
  assert(reader.next(gate,weatherLedger,json,sizeof(json),got)==Read::CandidateUnverified);
  assert(got.frame.ordinal==0U);
  assert(weatherVolume.writeOpens==0U);
  std::puts("F24B closed F06 read-only replay / gap skip / fail-closed: PASS");
}
