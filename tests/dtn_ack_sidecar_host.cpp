#include "dtn_ack_sidecar.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
namespace fs=std::filesystem;
using namespace DTNAckLedger;
using namespace DTNAckSidecar;

static Id128 id(uint8_t seed){
  Id128 x{};
  for(unsigned i=0;i<16U;++i)x.bytes[i]=static_cast<uint8_t>(seed+i*11U);
  x.bytes[6]=static_cast<uint8_t>((x.bytes[6]&15U)|0x40U);
  x.bytes[8]=static_cast<uint8_t>((x.bytes[8]&63U)|0x80U);
  return x;
}
static Stream stream(Journal journal=Journal::SosEvents){
  Stream s{};s.uploader=id(10U);s.vessel=2U;
  memcpy(s.session,"S58BAFE2",9U);s.journal=journal;
  for(unsigned i=0;i<32U;++i)s.sha256[i]=static_cast<uint8_t>(17U+i);
  return s;
}
struct Gate {
 bool allow=true;
 bool permit(const Stream&)const{return allow;}
};
struct MockPort {
 std::map<std::string,std::vector<uint8_t>> files;
 bool errorRead=false,failBeforeWrite=false,torn=false,damage=false,ackLost=false;
 bool readFile(const char* path,uint8_t* buffer,size_t capacity,size_t& n,bool& exists){
   n=0U;exists=false;
   if(errorRead)return false;
   const auto it=files.find(path);
   if(it==files.end())return true;
   exists=true;n=it->second.size();
   if(n>capacity)return true;
   if(n)memcpy(buffer,it->second.data(),n);
   return true;
 }
 bool writeFile(const char* path,const uint8_t* buffer,size_t length){
   if(failBeforeWrite)return false;
   size_t writeLength=torn?length/2U:length;
   files[path]=std::vector<uint8_t>(buffer,buffer+writeLength);
   if(damage)files[path][5]^=0xAAU;
   return !torn&&!ackLost;
 }
};
struct NativePort {
 fs::path base;
 explicit NativePort(fs::path d):base(std::move(d)){}
 fs::path local(const char* path)const{
   assert(path&&path[0]=='/'&&strstr(path,"..") == nullptr);
   const std::string relative(path+1U);
   assert(relative.rfind("THESIS/DTNSYNC/",0)==0);
   return base / fs::path(relative);
 }
 bool readFile(const char* path,uint8_t* buffer,size_t capacity,size_t& n,bool& exists){
   n=0U;exists=false;
   const fs::path p=local(path);
   std::error_code ec;
   exists=fs::exists(p,ec);
   if(ec)return false;
   if(!exists)return true;
   if(!fs::is_regular_file(p,ec)||ec)return false;
   const auto size=fs::file_size(p,ec);
   if(ec)return false;
   if(size>capacity){n=capacity+1U;return true;}
   FILE* f=std::fopen(p.string().c_str(),"rb");
   if(!f)return false;
   n=static_cast<size_t>(size);
   const size_t got=n?std::fread(buffer,1U,n,f):0U;
   const int closed=std::fclose(f);
   return got==n && closed==0;
 }
 bool writeFile(const char* path,const uint8_t* data,size_t length){
   const fs::path p=local(path);
   std::error_code ec;
   fs::create_directories(p.parent_path(),ec); // disposable HOST fixture only
   if(ec)return false;
   FILE* f=std::fopen(p.string().c_str(),"wb"); // inactive slot only
   if(!f)return false;
   bool ok=(std::fwrite(data,1U,length,f)==length);
   if(std::fflush(f)!=0)ok=false;
#ifdef _WIN32
   if(_commit(_fileno(f))!=0)ok=false;
#else
   if(::fsync(::fileno(f))!=0)ok=false;
#endif
   if(std::fclose(f)!=0)ok=false;
   return ok;
 }
};
static Batch makeBatch(const Stream& s,const RecordRef* refs,size_t count,Id128 key){
 Batch b{};b.batchId=key;b.uploader=s.uploader;b.records=refs;b.count=count;return b;
}
static Response makeResponse(const Batch& b,const ItemReply* items,size_t count){
 Response r{};r.schemaVersion=1U;r.batchId=b.batchId;
 r.uploader=b.uploader;r.items=items;r.count=count;return r;
}
int main(){
  static_assert(kWireBytes==94U,"stable 94-byte format");
  const Stream sos=stream(),tlm=stream(Journal::Telemetry);
  uint8_t raw[kWireBytes]{};
  const Snapshot genesis=make(sos,1U,0U,0U);
  assert(encode(genesis,raw));
  Snapshot decoded{};
  assert(decode(raw,kWireBytes,decoded)&&same(decoded,genesis));
  assert(!decode(raw,kWireBytes-1U,decoded));
  assert(!valid(decoded));
  raw[26]^=0xFFU;
  assert(!decode(raw,kWireBytes,decoded));
  assert(encode(genesis,raw));
  assert(raw[0]=='2'&&raw[1]=='L'&&raw[2]=='A'&&raw[3]=='D');
  char p0[kPathBytes]{},p1[kPathBytes]{},pt[kPathBytes]{};
  assert(sidecarPath(sos,0U,p0,sizeof(p0)));
  assert(sidecarPath(sos,1U,p1,sizeof(p1)));
  assert(sidecarPath(tlm,0U,pt,sizeof(pt)));
  assert(strcmp(p0,"/THESIS/DTNSYNC/V002/S58BAFE2/sos_0.ack")==0);
  assert(strcmp(p0,p1)!=0&&strcmp(p0,pt)!=0);
  assert(!sidecarPath(sos,2U,p0,sizeof(p0)));
  assert(!sidecarPath(sos,0U,p0,3U));
  assert(!sidecarPath(stream(Journal::Diagnostics),0U,p0,sizeof(p0)));

  // No bind without live permission; after permission revocation all I/O
  // must fail without silently initializing fresh or acknowledging a record.
  MockPort port;Gate gate{};
  Store<MockPort,Gate> store;
  gate.allow=false;
  assert(store.bind(port,gate,sos)==Bind::Denied);
  Snapshot tmp{};
  assert(store.read(0U,tmp)==SlotRead::Error);
  gate.allow=true;
  assert(store.bind(port,gate,sos)==Bind::Ready);
  Ledger<Store<MockPort,Gate>> ledger;
  assert(ledger.load(store,sos)==Load::Fresh);
  assert(ledger.initializeFresh(store,sos));
  assert(port.files.size()==1U&&ledger.firstPending()==0U);
  assert(!store.write(0U,make(sos,2U,1U,0U))); // never touch ACTIVE slot

  const RecordRef refs[]={{0U,id(40U)},{1U,id(41U)},{2U,id(42U)}};
  const Batch batch=makeBatch(sos,refs,3U,id(90U));
  const ItemReply replies[]={{id(40U),Reply::Accepted},
                              {id(41U),Reply::Rejected},
                              {id(42U),Reply::Duplicate}};
  assert(ledger.apply(store,batch,makeResponse(batch,replies,3U),true)==Commit::Applied);
  assert(ledger.firstPending()==1U&&ledger.acknowledged(2U));
  assert(port.files.size()==2U);
  Store<MockPort,Gate> reloadedStore;
  assert(reloadedStore.bind(port,gate,sos)==Bind::Ready);
  Ledger<Store<MockPort,Gate>> reboot;
  assert(reboot.load(reloadedStore,sos)==Load::Loaded);
  assert(reboot.firstPending()==1U&&reboot.acknowledged(2U));
  Store<MockPort,Gate> telemetryStore;
  assert(telemetryStore.bind(port,gate,tlm)==Bind::Ready);
  Ledger<Store<MockPort,Gate>> telemetry;
  assert(telemetry.load(telemetryStore,tlm)==Load::Fresh);
  assert(telemetry.initializeFresh(telemetryStore,tlm));
  assert(telemetry.firstPending()==0U&&reboot.firstPending()==1U);
  Store<MockPort,Gate> wrongStore;
  Stream other=sos;other.uploader=id(80U);
  assert(wrongStore.bind(port,gate,other)==Bind::Ready);
  Ledger<Store<MockPort,Gate>> wrong;
  assert(wrong.load(wrongStore,other)==Load::WrongStream);
  assert(!wrong.initializeFresh(wrongStore,other));
  gate.allow=false;
  assert(reloadedStore.read(0U,tmp)==SlotRead::Error);
  Ledger<Store<MockPort,Gate>> denied;
  assert(denied.load(reloadedStore,sos)==Load::IoError);
  gate.allow=true;

  // Use the previously accepted later ordinal 2 while accepting the gap.
  const RecordRef gap[]={{1U,id(41U)}};
  const Batch next=makeBatch(sos,gap,1U,id(91U));
  const ItemReply gapAck[]={{id(41U),Reply::Accepted}};
  MockPort torn=port;
  Store<MockPort,Gate> tornStore;
  assert(tornStore.bind(torn,gate,sos)==Bind::Ready);
  Ledger<Store<MockPort,Gate>> interrupted;
  assert(interrupted.load(tornStore,sos)==Load::Loaded);
  torn.torn=true;
  assert(interrupted.apply(tornStore,next,makeResponse(next,gapAck,1U),true)==Commit::StorageFailure);
  assert(!interrupted.ready());
  torn.torn=false;
  Store<MockPort,Gate> tornRecovery;
  assert(tornRecovery.bind(torn,gate,sos)==Bind::Ready);
  Ledger<Store<MockPort,Gate>> reattempt;
  assert(reattempt.load(tornRecovery,sos)==Load::Loaded);
  assert(reattempt.firstPending()==1U&&reattempt.acknowledged(2U));
  assert(reattempt.apply(tornRecovery,next,makeResponse(next,gapAck,1U),true)==Commit::Applied);
  assert(reattempt.firstPending()==3U);

  // Lost local write acknowledgment: durable bytes may already exist. Never
  // assume either result until reload; the old active slot is preserved.
  MockPort uncertain=port;uncertain.ackLost=true;
  Store<MockPort,Gate> uncertainStore;
  assert(uncertainStore.bind(uncertain,gate,sos)==Bind::Ready);
  Ledger<Store<MockPort,Gate>> u;
  assert(u.load(uncertainStore,sos)==Load::Loaded);
  assert(u.apply(uncertainStore,next,makeResponse(next,gapAck,1U),true)==Commit::StorageFailure);
  assert(!u.ready());
  uncertain.ackLost=false;
  Ledger<Store<MockPort,Gate>> rechecked;
  assert(rechecked.load(uncertainStore,sos)==Load::Loaded);
  assert(rechecked.firstPending()==3U);

  // Both slots malformed -> no fresh initialization, never erase unknown state.
  MockPort bothBad=port;
  for(auto& pair:bothBad.files)pair.second.assign(14U,0x55U);
  Store<MockPort,Gate> badStore;
  assert(badStore.bind(bothBad,gate,sos)==Bind::Ready);
  Ledger<Store<MockPort,Gate>> bad;
  assert(bad.load(badStore,sos)==Load::Corrupt);
  assert(!bad.initializeFresh(badStore,sos));

  // Persist to a disposable, real host directory; destroy and reopen the
  // Store objects to prove this is not merely an in-memory mock.
  const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path root=fs::temp_directory_path()/
      ("THESIS_F24C_DISPOSABLE_"+std::to_string(stamp));
  assert(!fs::exists(root));
  assert(fs::create_directory(root));
  {
    NativePort disk(root);Gate physicalGate{};
    Store<NativePort,Gate> diskStore;
    assert(diskStore.bind(disk,physicalGate,sos)==Bind::Ready);
    Ledger<Store<NativePort,Gate>> durable;
    assert(durable.load(diskStore,sos)==Load::Fresh);
    assert(durable.initializeFresh(diskStore,sos));
    assert(durable.apply(diskStore,batch,makeResponse(batch,replies,3U),true)==Commit::Applied);
  }
  {
    NativePort disk(root);Gate physicalGate{};
    Store<NativePort,Gate> diskStore;
    assert(diskStore.bind(disk,physicalGate,sos)==Bind::Ready);
    Ledger<Store<NativePort,Gate>> durable;
    assert(durable.load(diskStore,sos)==Load::Loaded);
    assert(durable.firstPending()==1U&&durable.acknowledged(2U));
    assert(durable.apply(diskStore,next,makeResponse(next,gapAck,1U),true)==Commit::Applied);
  }
  {
    NativePort disk(root);Gate physicalGate{};
    Store<NativePort,Gate> diskStore;
    assert(diskStore.bind(disk,physicalGate,sos)==Bind::Ready);
    Ledger<Store<NativePort,Gate>> finalBoot;
    assert(finalBoot.load(diskStore,sos)==Load::Loaded);
    assert(finalBoot.firstPending()==3U&&finalBoot.ackMask()==0U);
  }
  const auto filesRemoved=fs::remove_all(root);
  assert(filesRemoved>=4U && !fs::exists(root));
  std::puts("F24C two-slot sidecar wire/durable host files/torn-write/reboot: PASS");
}
