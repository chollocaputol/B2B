#include "../firmware/shared/storage/dtn_ack_ledger.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace DTNAckLedger;

static Id128 id(uint8_t seed){
  Id128 x{};
  for(unsigned i=0;i<16U;++i)x.bytes[i]=static_cast<uint8_t>(seed+i*7U);
  x.bytes[6]=static_cast<uint8_t>((x.bytes[6]&0x0FU)|0x40U);
  x.bytes[8]=static_cast<uint8_t>((x.bytes[8]&0x3FU)|0x80U);
  return x;
}
static Stream stream(Journal j=Journal::SosEvents){
  Stream s{};s.uploader=id(99);s.vessel=2U;
  memcpy(s.session,"S58BAFE2",9U);s.journal=j;
  for(unsigned i=0;i<32U;++i)s.sha256[i]=static_cast<uint8_t>(i+1U);
  return s;
}
struct Store {
  bool present[2]={false,false};
  Snapshot slots[2]{};
  bool failWrite=false,corruptWrite=false,io=false;
  SlotRead read(unsigned n,Snapshot& s){
    if(io)return SlotRead::Error;
    if(!present[n])return SlotRead::Missing;
    s=slots[n];return SlotRead::Present;
  }
  bool write(unsigned n,const Snapshot& s){
    if(failWrite)return false;
    slots[n]=s;present[n]=true;
    if(corruptWrite)slots[n].check^=5U;
    return true;
  }
};
static Batch batch(const Id128& batchId,const Stream& s,
                   const RecordRef* records,size_t n){
  Batch b{};b.batchId=batchId;b.uploader=s.uploader;b.records=records;b.count=n;
  return b;
}
static Response response(const Batch& b,const ItemReply* replies,size_t n){
  Response r{};r.schemaVersion=1U;r.batchId=b.batchId;
  r.uploader=b.uploader;r.items=replies;r.count=n;return r;
}
int main(){
  const Stream sos=stream(),tele=stream(Journal::Telemetry);
  assert(validStream(sos)&&validStream(tele)&&!sameStream(sos,tele));
  Stream diagnostics=stream(Journal::Diagnostics);
  assert(!validStream(diagnostics)); // no diagnostics-as-telemetry
  Stream badDigest=sos;memset(badDigest.sha256,0,sizeof(badDigest.sha256));
  assert(!validStream(badDigest));
  Stream invalidName=sos;memcpy(invalidName.session,"../BAD00",9);
  assert(!validStream(invalidName));

  Store storeS,storeT;
  Ledger<Store> a,t;
  assert(a.load(storeS,sos)==Load::Fresh);
  assert(t.load(storeT,tele)==Load::Fresh);
  assert(a.initializeFresh(storeS,sos)&&t.initializeFresh(storeT,tele));
  assert(a.ready()&&a.firstPending()==0U&&t.firstPending()==0U);

  // Out-of-order response items must match all exact durable record IDs.
  const RecordRef refs[]={{0U,id(1)},{1U,id(2)},{2U,id(3)},{3U,id(4)}};
  const Batch b=batch(id(70),sos,refs,4U);
  const ItemReply partial[]={{id(4),Reply::Accepted},{id(2),Reply::Duplicate},
                             {id(3),Reply::Rejected},{id(1),Reply::Retry}};
  Response r=response(b,partial,4U);
  assert(a.apply(storeS,b,r,false)==Commit::Invalid);
  assert(!a.acknowledged(1U));
  Response wrongBatch=r;wrongBatch.batchId=id(71);
  assert(a.apply(storeS,b,wrongBatch,true)==Commit::Invalid);
  Response wrongUploader=r;wrongUploader.uploader=id(72);
  assert(a.apply(storeS,b,wrongUploader,true)==Commit::Invalid);
  Response wrongSchema=r;wrongSchema.schemaVersion=2;
  assert(a.apply(storeS,b,wrongSchema,true)==Commit::Invalid);
  ItemReply duplicateIds[]={{id(1),Reply::Accepted},{id(2),Reply::Accepted},
                            {id(2),Reply::Accepted},{id(4),Reply::Accepted}};
  Response duplicateR=response(b,duplicateIds,4U);
  assert(a.apply(storeS,b,duplicateR,true)==Commit::Invalid);
  ItemReply unknownIds[]={{id(1),Reply::Accepted},{id(2),Reply::Accepted},
                          {id(3),Reply::Accepted},{id(17),Reply::Accepted}};
  assert(a.apply(storeS,b,response(b,unknownIds,4U),true)==Commit::Invalid);
  assert(a.apply(storeS,b,r,true)==Commit::Applied);
  assert(a.firstPending()==0U&&a.ackMask()==0x0AU); // ordinals 1,3 held
  assert(a.acknowledged(1U)&&a.acknowledged(3U));
  assert(!a.acknowledged(0U)&&a.eligible(0U)&&!a.eligible(1U));
  assert(t.firstPending()==0U&&t.ackMask()==0U); // independent journal

  // Reboot retains accepted later records even behind a rejected gap.
  Ledger<Store> reboot;
  assert(reboot.load(storeS,sos)==Load::Loaded);
  assert(reboot.firstPending()==0U&&reboot.acknowledged(3U));
  assert(reboot.load(storeS,tele)==Load::WrongStream);
  Stream changed=sos;changed.sha256[0]^=0xCCU;
  assert(reboot.load(storeS,changed)==Load::WrongStream); // source changed
  assert(reboot.load(storeS,sos)==Load::Loaded);
  const RecordRef close[]={{0U,id(1)},{2U,id(3)}};
  const Batch b2=batch(id(73),sos,close,2U);
  const ItemReply closeAck[]={{id(3),Reply::Accepted},{id(1),Reply::Duplicate}};
  assert(reboot.apply(storeS,b2,response(b2,closeAck,2U),true)==Commit::Applied);
  assert(reboot.firstPending()==4U&&reboot.ackMask()==0U);
  assert(reboot.acknowledged(0U)&&reboot.acknowledged(3U));
  assert(!reboot.eligible(0U));
  assert(t.firstPending()==0U);

  // Per-journal stream does not accept a wrong batch origin or stale ordinal.
  assert(reboot.apply(storeS,b,r,true)==Commit::Invalid);
  const RecordRef trefs[]={{0U,id(20)}};
  const Batch tb=batch(id(81),tele,trefs,1U);
  const ItemReply tack[]={{id(20),Reply::Accepted}};
  assert(t.apply(storeT,tb,response(tb,tack,1U),true)==Commit::Applied);
  assert(t.firstPending()==1U&&reboot.firstPending()==4U);

  // Range guards and exact-ID checks reject unsorted and duplicated input.
  RecordRef unsorted[]={{5U,id(5)},{4U,id(6)}};
  Batch ub=batch(id(82),sos,unsorted,2U);
  ItemReply ur[]={{id(5),Reply::Accepted},{id(6),Reply::Accepted}};
  assert(reboot.apply(storeS,ub,response(ub,ur,2U),true)==Commit::Invalid);
  RecordRef far[]={{68U,id(6)}};
  Batch fb=batch(id(83),sos,far,1U);
  ItemReply fr[]={{id(6),Reply::Accepted}};
  assert(reboot.apply(storeS,fb,response(fb,fr,1U),true)==Commit::Invalid);

  // A rejected or retry-only server response must not write a checkpoint.
  RecordRef r5[]={{4U,id(7)},{5U,id(8)}};
  Batch b5=batch(id(84),sos,r5,2U);
  ItemReply no[]={{id(7),Reply::Rejected},{id(8),Reply::Retry}};
  const uint32_t generation=reboot.state().generation;
  assert(reboot.apply(storeS,b5,response(b5,no,2U),true)==Commit::NoNewAck);
  assert(reboot.state().generation==generation);

  // Power loss/torn alternate sidecar: fail-stop until reload, no lost source.
  Store fail=storeS;Ledger<Store> interrupted;
  assert(interrupted.load(fail,sos)==Load::Loaded);
  ItemReply yes[]={{id(7),Reply::Accepted},{id(8),Reply::Accepted}};
  fail.failWrite=true;
  assert(interrupted.apply(fail,b5,response(b5,yes,2U),true)==Commit::StorageFailure);
  assert(!interrupted.ready());
  fail.failWrite=false;
  Ledger<Store> restored;
  assert(restored.load(fail,sos)==Load::Loaded&&restored.firstPending()==4U);
  assert(restored.apply(fail,b5,response(b5,yes,2U),true)==Commit::Applied);
  assert(restored.firstPending()==6U);
  Store torn=storeS;Ledger<Store> t0;
  assert(t0.load(torn,sos)==Load::Loaded);
  torn.corruptWrite=true;
  assert(t0.apply(torn,b5,response(b5,yes,2U),true)==Commit::StorageFailure);
  torn.corruptWrite=false;
  Ledger<Store> t1;
  assert(t1.load(torn,sos)==Load::Loaded&&t1.firstPending()==4U);

  // Two individually valid slots with a rollback are not accepted.
  Store rollback;
  rollback.present[0]=rollback.present[1]=true;
  rollback.slots[0]=make(sos,1U,10U,0U);
  rollback.slots[1]=make(sos,2U,9U,0U);
  assert(t1.load(rollback,sos)==Load::Corrupt);
  Store crc=storeS;
  crc.slots[0].check^=1U;crc.slots[1].check^=7U;
  assert(t1.load(crc,sos)==Load::Corrupt);
  Store io;io.io=true;
  assert(t1.load(io,sos)==Load::IoError);

  // 64-window bit63 accepted survives a reboot; filling earlier gap compacts.
  Store longStore;Ledger<Store> longLedger;
  assert(longLedger.load(longStore,sos)==Load::Fresh);
  assert(longLedger.initializeFresh(longStore,sos));
  RecordRef last[]={{63U,id(9)}};
  Batch late=batch(id(85),sos,last,1U);
  ItemReply lateR[]={{id(9),Reply::Accepted}};
  assert(longLedger.apply(longStore,late,response(late,lateR,1U),true)==Commit::Applied);
  Ledger<Store> secondBoot;
  assert(secondBoot.load(longStore,sos)==Load::Loaded);
  assert(secondBoot.acknowledged(63U)&&!secondBoot.eligible(64U));
  for(uint64_t ordinal=0;ordinal<63U;++ordinal){
    RecordRef one[]={{ordinal,id(static_cast<uint8_t>(100U+ordinal))}};
    Batch single=batch(id(static_cast<uint8_t>(20U+ordinal)),sos,one,1U);
    ItemReply result[]={{one[0].recordId,Reply::Duplicate}};
    assert(secondBoot.apply(longStore,single,response(single,result,1U),true)==Commit::Applied);
  }
  assert(secondBoot.firstPending()==64U&&secondBoot.ackMask()==0U);
  Ledger<Store> finalBoot;
  assert(finalBoot.load(longStore,sos)==Load::Loaded&&finalBoot.firstPending()==64U);

  static_assert(kMaxBatch==32U && kAckWindow==64U,"bounded ledger");
  std::puts("F24A DTN per-journal ACK persistence / replay / corruption: PASS");
}
