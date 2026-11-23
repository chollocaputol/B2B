// F24O owner-wrapped F24M/F24N host integration, real files, independent processes.
// F24N — REAL host filesystem, independent processes, exact firmware 88-byte
// SOS frames, sealed-by-close read-only replay, mock shore only. NO physical SD.
#include "../firmware/shared/storage/dtn_f24n_sos_restart_recovery.h"
#include "../firmware/shared/storage/dtn_f24o_owned_sos_port.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <cstdint>
#include <cstdlib>
namespace fs=std::filesystem;
static unsigned checks=0;
#define CHECK(c) do{++checks;if(!(c)){std::cerr<<"F24O CHECK FAIL "<<__FILE__<<":"<<__LINE__<<" "<<#c<<"\n";std::exit(51);}}while(0)
static int handles=0,appendOpens=0,readOpens=0;
class HostFile final{
 FILE* f_=nullptr;
 public:
 HostFile()=default;explicit HostFile(FILE* f):f_(f){if(f_)++handles;}
 HostFile(HostFile&& o)noexcept:f_(o.f_){o.f_=nullptr;}
 HostFile& operator=(HostFile&& o)noexcept{if(this!=&o){close();f_=o.f_;o.f_=nullptr;}return *this;}
 HostFile(const HostFile&)=delete;HostFile& operator=(const HostFile&)=delete;
 ~HostFile(){close();}
 explicit operator bool()const{return f_!=nullptr;}
 size_t size()const{
  if(!f_)return 0;
  auto old=std::ftell(f_);if(old<0 || std::fseek(f_,0,SEEK_END))return 0;
  auto end=std::ftell(f_);std::fseek(f_,old,SEEK_SET);
  return end<0?0:size_t(end);
 }
 bool seek(size_t at){return f_ && std::fseek(f_,long(at),SEEK_SET)==0;}
 int read(uint8_t* dst,size_t count){return f_?int(std::fread(dst,1,count,f_)):0;}
 size_t write(const uint8_t* src,size_t count){return f_?std::fwrite(src,1,count,f_):0;}
 void flush(){if(f_)std::fflush(f_);}
 int getWriteError()const{return f_?std::ferror(f_):1;}
 void close(){if(f_){std::fclose(f_);f_=nullptr;--handles;}}
};
class HostSD final{
 fs::path root_;
 public:
 bool denied=false;
 explicit HostSD(fs::path root):root_(std::move(root)){}
 fs::path host(const char* path)const{
  if(!path || path[0]!='/')return root_/"INVALID";
  return root_/std::string(path+1);
 }
 bool exists(const char* path)const{return !denied && fs::exists(host(path));}
 HostFile open(const char* path,const char* mode){
  if(denied || !mode || !path)return HostFile{};
  fs::path name=host(path);
  if(!fs::is_directory(name.parent_path()))return HostFile{};
  if(!strcmp(mode,"r")){++readOpens;return HostFile(std::fopen(name.string().c_str(),"rb"));}
  if(!strcmp(mode,"a")){++appendOpens;return HostFile(std::fopen(name.string().c_str(),"ab+"));}
  return HostFile{};
 }
};
static constexpr const char* S1="/THESIS/F06TEST/V002/SABC1001";
static constexpr const char* S2="/THESIS/F06TEST/V002/SABC1002";
static constexpr const char* S3="/THESIS/F06TEST/V002/SABC1003";
static constexpr const char* S4="/THESIS/F06TEST/V002/SABC1004";
static constexpr const char* S5="/THESIS/F06TEST/V002/SABC1005";
static constexpr const char* S6="/THESIS/F06TEST/V002/SABC1006";
static constexpr const char* S7="/THESIS/F06TEST/V002/SABC1007";
static VesselPacket sos(uint32_t origin,uint32_t event){
 VesselPacket p{};p.magic=PACKET_MAGIC;p.version=PACKET_VERSION;
 p.packetType=PACKET_SOS;p.originNodeId=origin;p.sequenceNumber=event;
 p.timestamp=123456+event;p.hopCount=origin==2?0:1;
 p.flags=FLAG_GPS_FIX|FLAG_TEMP_HUMIDITY_VALID|FLAG_PRESSURE_VALID;
 std::memcpy(p.nodeName,"VESSEL_002",10);
 p.latitude_e7=81111111+int(event);p.longitude_e7=1249999888-int(event);
 p.shoreDistance_m=700+event;p.batteryLevel=56;
 p.weather.temperature_x10=int16_t(289+event);
 p.weather.humidity_x10=674;p.weather.pressure_x10=10123;
 p.weather.pressureTrend_x10=-8;p.weather.zambrettiState=7;
 p.weather.sensorFusionState=2;p.weather.weatherState=3;
 std::memcpy(p.message,"SOS",3);return p;
}
static DTNF24M::PacketEntry packet(uint32_t origin,uint32_t event){
 const VesselPacket p=sos(origin,event);
 DTNF24M::PacketEntry e{};e.origin=origin;e.event=event;
 e.source=origin==2?DTNF24M::kLocal:DTNF24M::kReceived;
 std::memcpy(e.bytes,&p,62);return e;
}
static std::vector<uint8_t> bytes(const fs::path& file){
 std::ifstream f(file,std::ios::binary);
 return {std::istreambuf_iterator<char>(f),{}};
}
static void put(const fs::path& name,const std::vector<uint8_t>& data){
 std::ofstream f(name,std::ios::binary|std::ios::trunc);
 CHECK(bool(f));f.write((const char*)data.data(),std::streamsize(data.size()));
 CHECK(bool(f));
}
static void sessions(const fs::path& root){
 for(const char* s:{S1,S2,S3,S4,S5,S6,S7})fs::create_directories(root/std::string(s+1));
}
static fs::path packetFile(HostSD& sd,const char* dir){
 char path[96]{};CHECK(DTNF24M::sandboxPath(path,sizeof(path),dir,2));return sd.host(path);
}
static fs::path receiptFile(HostSD& sd,const char* dir){
 char path[96]{};CHECK(DTNF24N::receiptPath(path,sizeof(path),dir,2));return sd.host(path);
}
static fs::path serverFile(const fs::path& root){return root/"mock_shore.f24n";}
static std::vector<DTNF24N::Receipt> serverRows(const fs::path& root){
 std::vector<DTNF24N::Receipt> out;auto data=bytes(serverFile(root));
 CHECK(data.size()%DTNF24N::kReceiptBytes==0);
 for(size_t i=0;i<data.size();i+=DTNF24N::kReceiptBytes){
  DTNF24N::Receipt r{};CHECK(DTNF24N::decodeReceipt(data.data()+i,DTNF24N::kReceiptBytes,r));out.push_back(r);
 }return out;
}
static DTNF24N::Receipt serverAccept(const fs::path& root,const DTNF24M::PacketEntry& p){
 auto existing=serverRows(root);
 for(const auto& r:existing){
  if(r.origin==p.origin && r.event==p.event){
   CHECK(r.packetCrc==DTNF24M::crc32(p.bytes,62));return r;
  }
 }
 DTNF24N::Receipt r{};r.origin=p.origin;r.event=p.event;
 r.packetCrc=DTNF24M::crc32(p.bytes,62);
 r.serverId=0xA102000000000000ULL+uint64_t(existing.size()+1);
 r.serverTag=0x15AC2026;
 uint8_t frame[DTNF24N::kReceiptBytes]{};CHECK(DTNF24N::encodeReceipt(r,frame));
 std::ofstream out(serverFile(root),std::ios::binary|std::ios::app);
 CHECK(bool(out));out.write((char*)frame,sizeof(frame));out.flush();CHECK(bool(out));
 return r;
}
struct HostReadAuthority {
 const HostSD* sd=nullptr;
 bool operator()()const{return sd && !sd->denied;}
};
struct FakeShoreVerifier{
 fs::path root;
 bool operator()(const DTNF24N::Receipt& r,const DTNF24M::PacketEntry& p)const{
  if(r.origin!=p.origin || r.event!=p.event ||
     r.packetCrc!=DTNF24M::crc32(p.bytes,62))return false;
  const std::vector<uint8_t> data=bytes(root/"mock_shore.f24n");
  if(data.size()%DTNF24N::kReceiptBytes)return false;
  for(size_t i=0;i<data.size();i+=DTNF24N::kReceiptBytes){
   DTNF24N::Receipt accepted{};
   if(DTNF24N::decodeReceipt(data.data()+i,DTNF24N::kReceiptBytes,accepted) &&
      accepted.origin==r.origin && accepted.event==r.event &&
      accepted.packetCrc==r.packetCrc && accepted.serverId==r.serverId &&
      accepted.serverTag==r.serverTag)return true;
  }
  return false;
 }
};
template<class Verifier>
static DTNF24N::Replay<16,12> recover(HostSD& sd,const char* current,
 const std::vector<const char*>& olds,const Verifier& verifier){
 DTNF24N::Replay<16,12> re;
 CHECK(re.begin(2,current));
 for(const auto& old:olds)CHECK(re.addClosed(old));
 CHECK(re.lock());
 bool done=false;
 for(unsigned tick=0;tick<200;++tick){
  auto rc=re.poll(sd,"r",verifier,HostReadAuthority{&sd});
  CHECK(handles==0);
  if(rc==DTNF24N::Poll::Completed){done=true;break;}
  CHECK(rc==DTNF24N::Poll::Progress);
 }
 CHECK(done);CHECK(re.completed());CHECK(!re.failed());
 return re;
}
static void ensureNoOpen(){CHECK(handles==0);}
struct SimulatedWitness {
 DTNF24O::Evidence e{};
 DTNF24O::Evidence view() const { return e; }
};
static SimulatedWitness witness(){
 SimulatedWitness w{};auto& e=w.e;
 e.vessel=2;e.mediaTag=0x9a9a9a9aULL;e.mountEpoch=7;e.ownerSerial=99;
 e.mounted=true;e.independentOwner=true;e.exclusive=true;
 e.f23dClosed=true;e.otherWriterClosed=true;e.closedSessionsVerified=true;
 return w;
}
using Port=DTNF24O::Port<HostSD,SimulatedWitness>;
static void checkReplay(Port& port,HostSD& sd,const fs::path& root,const char* current,
  std::initializer_list<const char*> olds,size_t expectedPending,size_t expectedDelivered,
  bool serverTrusted){
 DTNF24N::Replay<16,12> replay;
 CHECK(replay.begin(2,current));
 for(const char* s:olds)CHECK(replay.addClosed(s));
 CHECK(replay.lock());
 bool completed=false;
 for(unsigned i=0;i<80;++i){
  auto result=serverTrusted?
    replay.poll(port,"r",FakeShoreVerifier{root},HostReadAuthority{&sd}):
    replay.poll(port,"r",DTNF24N::DenyReceiptVerifier{},HostReadAuthority{&sd});
  CHECK(!port.hasHandle());CHECK(handles==0);
  if(result==DTNF24N::Poll::Completed){completed=true;break;}
  CHECK(result==DTNF24N::Poll::Progress);
 }
 CHECK(completed);CHECK(replay.ledger().pending()==expectedPending);
 CHECK(replay.ledger().delivered()==expectedDelivered);
 CHECK(!port.faulted());
}
static void ownerUnit(){
 CHECK(sizeof(VesselPacket)==62);CHECK(DTNF24M::kFrameBytes==88);
 CHECK(DTNF24N::kReceiptBytes==40);
 auto e=witness().e;
 CHECK(DTNF24O::validEvidence(e,2));
 e.f23dClosed=false;CHECK(!DTNF24O::validEvidence(e,2));
 e=witness().e;e.otherWriterClosed=false;CHECK(!DTNF24O::validEvidence(e,2));
 e=witness().e;e.radioBusy=true;CHECK(!DTNF24O::validEvidence(e,2));
 e=witness().e;e.exportActive=true;CHECK(!DTNF24O::validEvidence(e,2));
 e=witness().e;e.historyActive=true;CHECK(!DTNF24O::validEvidence(e,2));
 e=witness().e;e.ownerSerial=0;CHECK(!DTNF24O::validEvidence(e,2));
 e=witness().e;e.mediaTag=0;CHECK(!DTNF24O::validEvidence(e,2));
 e=witness().e;e.mountEpoch=0;CHECK(!DTNF24O::validEvidence(e,2));
 bool current=false;
 CHECK(DTNF24O::supportedPath("/THESIS/F06TEST/V002/SABC1001/dtn_sos_packets.f24m",S1,2,current));CHECK(current);
 CHECK(DTNF24O::supportedPath("/THESIS/F06TEST/V002/SABC1002/dtn_shore_receipts.f24n",S1,2,current));CHECK(!current);
 CHECK(!DTNF24O::supportedPath("/THESIS/F06TEST/V001/SABC1001/dtn_sos_packets.f24m",S1,2,current));
 CHECK(!DTNF24O::supportedPath("/THESIS/F06TEST/V002/SABC1001/../../secrets",S1,2,current));
 CHECK(!DTNF24O::supportedPath("/THESIS/F06TEST/V002/SABC1001/telemetry.jsonl",S1,2,current));
 CHECK(!DTNF24O::supportedPath("/THESIS/F06TEST/V002/SABC1001/sos_events.jsonl",S1,2,current));
 CHECK(!DTNF24O::supportedPath("/THESIS/F06TEST/V002/SABC1001/diagnostics.jsonl",S1,2,current));
 CHECK(!DTNF24O::supportedPath("/THESIS/F06TEST/V002/SABC1001/f23d_multihop.jsonl",S1,2,current));
 CHECK(!DTNF24O::supportedPath("/THESIS/F06TEST/V002/SABC1001/dtn_shore_receipts.f24n.extra",S1,2,current));
}
static void initOwner(const fs::path& root){
 CHECK(!fs::exists(root));CHECK(fs::create_directories(root));sessions(root);
 HostSD sd(root);auto w=witness();Port port(sd,w);
 CHECK(port.bind(2,S1));CHECK(port.bound());CHECK(!port.hasHandle());
 CHECK(!port.exists("/THESIS/F06TEST/V002/SABC1001/dtn_sos_packets.f24m"));
 DTNF24M::SessionWriter writer;
 CHECK(writer.appendChecked(port,S1,2,packet(2,70),"a","r"));
 CHECK(writer.appendChecked(port,S1,2,packet(1,71),"a","r"));
 CHECK(writer.appendChecked(port,S1,2,packet(3,72),"a","r"));
 CHECK(!port.hasHandle());CHECK(handles==0);
 CHECK(writer.written()==3U*DTNF24M::kFrameBytes);
 CHECK(bytes(packetFile(sd,S1)).size()==264);
 const auto b=bytes(packetFile(sd,S1));
 for(size_t i=0;i<3;++i){
  DTNF24M::PacketEntry decoded{};CHECK(DTNF24M::decode(b.data()+i*88,88,decoded));
  const auto original=packet(i==0?2:(i==1?1:3),70+uint32_t(i));
  CHECK(memcmp(decoded.bytes,original.bytes,62)==0);
 }
 put(root/"research_only.bin",{'R','E','S','E','A','R','C','H'});
 CHECK(!port.faulted());ensureNoOpen();
}
static void offlineOwner(const fs::path& root){
 HostSD sd(root);auto w=witness();Port port(sd,w);CHECK(port.bind(2,S2));
 checkReplay(port,sd,root,S2,{S1},3,0,false);
 CHECK(!fs::exists(receiptFile(sd,S2)));
 CHECK(bytes(root/"research_only.bin")==std::vector<uint8_t>({'R','E','S','E','A','R','C','H'}));
 ensureNoOpen();
}
static void onlineOwner(const fs::path& root){
 HostSD sd(root);auto w=witness();Port port(sd,w);CHECK(port.bind(2,S2));
 DTNF24N::Replay<16,12> replay;CHECK(replay.begin(2,S2));CHECK(replay.addClosed(S1));CHECK(replay.lock());
 bool done=false;
 for(int i=0;i<40;++i){const auto out=replay.poll(port,"r",FakeShoreVerifier{root},HostReadAuthority{&sd});
  CHECK(handles==0);CHECK(!port.hasHandle());
  if(out==DTNF24N::Poll::Completed){done=true;break;}CHECK(out==DTNF24N::Poll::Progress);
 }
 CHECK(done);CHECK(replay.ledger().pending()==3);
 // A genuine-looking neighbor receipt without a shore verifier MUST NOT count.
 const DTNF24M::PacketEntry* first=replay.ledger().nextPending();CHECK(first!=nullptr);
 auto accepted=serverAccept(root,*first);
 CHECK(replay.ledger().apply(accepted,DTNF24N::DenyReceiptVerifier{})==DTNF24N::ReceiptResult::Unverified);
 CHECK(replay.ledger().pending()==3);
 DTNF24N::ReceiptWriter writer;
 CHECK(writer.appendVerified(port,S2,2,replay.ledger(),accepted,FakeShoreVerifier{root},"a","r"));
 CHECK(replay.ledger().pending()==2);
 auto second=replay.ledger().nextPending();CHECK(second!=nullptr);
 const auto r2=serverAccept(root,*second);
 CHECK(writer.appendVerified(port,S2,2,replay.ledger(),r2,FakeShoreVerifier{root},"a","r"));
 CHECK(replay.ledger().pending()==1);
 CHECK(bytes(receiptFile(sd,S2)).size()==80);
 CHECK(!port.hasHandle());CHECK(handles==0);CHECK(!port.faulted());
}
static void restartOwner(const fs::path& root){
 HostSD sd(root);auto w=witness();Port port(sd,w);CHECK(port.bind(2,S3));
 DTNF24N::Replay<16,12> replay;
 CHECK(replay.begin(2,S3));CHECK(replay.addClosed(S1));CHECK(replay.addClosed(S2));CHECK(replay.lock());
 bool complete=false;
 for(int i=0;i<50;++i){const auto out=replay.poll(port,"r",FakeShoreVerifier{root},HostReadAuthority{&sd});
  CHECK(handles==0);if(out==DTNF24N::Poll::Completed){complete=true;break;}CHECK(out==DTNF24N::Poll::Progress);
 }
 CHECK(complete);CHECK(replay.ledger().delivered()==2);CHECK(replay.ledger().pending()==1);
 const auto* third=replay.ledger().nextPending();CHECK(third!=nullptr);
 auto r3=serverAccept(root,*third);DTNF24N::ReceiptWriter writer;
 CHECK(writer.appendVerified(port,S3,2,replay.ledger(),r3,FakeShoreVerifier{root},"a","r"));
 CHECK(replay.ledger().delivered()==3);CHECK(replay.ledger().pending()==0);
 CHECK(!port.hasHandle());CHECK(!port.faulted());CHECK(handles==0);
}
static void auditOwner(const fs::path& root){
 HostSD sd(root);auto w=witness();Port port(sd,w);CHECK(port.bind(2,S4));
 checkReplay(port,sd,root,S4,{S1,S2,S3},0,3,true);
 CHECK(bytes(packetFile(sd,S1)).size()==264);
 CHECK(bytes(root/"research_only.bin")==std::vector<uint8_t>({'R','E','S','E','A','R','C','H'}));
 CHECK(serverRows(root).size()==3);CHECK(handles==0);
}
static void faultsOwner(const fs::path& root){
 HostSD sd(root);
 {auto w=witness();w.e.f23dClosed=false;Port p(sd,w);CHECK(!p.bind(2,S5));CHECK(p.faulted());}
 {auto w=witness();w.e.otherWriterClosed=false;Port p(sd,w);CHECK(!p.bind(2,S5));CHECK(p.faulted());}
 {auto w=witness();w.e.radioBusy=true;Port p(sd,w);CHECK(!p.bind(2,S5));CHECK(p.faulted());}
 {DTNF24O::DenyWitness w;DTNF24O::Port<HostSD,DTNF24O::DenyWitness> p(sd,w);
  CHECK(!p.bind(2,S5));CHECK(p.faulted());}
 {auto w=witness();Port p(sd,w);CHECK(p.bind(2,S5));
  CHECK(!p.open("/THESIS/F06TEST/V002/SABC1001/dtn_sos_packets.f24m","a"));
  CHECK(p.faulted());CHECK(handles==0);}
 {auto w=witness();Port p(sd,w);CHECK(p.bind(2,S5));
  CHECK(!p.open("/THESIS/F06TEST/V002/SABC1001/telemetry.jsonl","r"));
  CHECK(p.faulted());CHECK(handles==0);}
 {auto w=witness();Port p(sd,w);CHECK(p.bind(2,S5));
  char path[96]{};CHECK(DTNF24M::sandboxPath(path,sizeof(path),S1,2));
  w.e.urgentPending=true;CHECK(!p.open(path,"r"));CHECK(p.faulted());}
 {auto w=witness();Port p(sd,w);CHECK(p.bind(2,S5));
  char path[96]{};CHECK(DTNF24M::sandboxPath(path,sizeof(path),S1,2));
  auto f=p.open(path,"r");CHECK(bool(f));CHECK(p.hasHandle());
  CHECK(!p.open(path,"r"));CHECK(p.faulted());f.close();CHECK(!p.hasHandle());CHECK(handles==0);}
 {auto w=witness();Port p(sd,w);CHECK(p.bind(2,S5));
  char path[96]{};CHECK(DTNF24M::sandboxPath(path,sizeof(path),S1,2));
  auto f=p.open(path,"r");CHECK(bool(f));
  w.e.mediaTag++;
  uint8_t b[10]{};CHECK(f.read(b,sizeof(b))==0);CHECK(p.faulted());
  f.close();CHECK(!p.hasHandle());CHECK(handles==0);}
 {auto w=witness();Port p(sd,w);CHECK(p.bind(2,S5));
  w.e.exportActive=true;CHECK(!p.exists("/THESIS/F06TEST/V002/SABC1001/dtn_sos_packets.f24m"));CHECK(p.faulted());}
 {auto w=witness();Port p(sd,w);CHECK(p.bind(2,S5));
  w.e.f23dClosed=false;CHECK(!p.exists("/THESIS/F06TEST/V002/SABC1001/dtn_sos_packets.f24m"));CHECK(p.faulted());}
 {auto w=witness();Port p(sd,w);CHECK(p.bind(2,S5));
  w.e.ownerSerial++;CHECK(!p.exists("/THESIS/F06TEST/V002/SABC1001/dtn_sos_packets.f24m"));CHECK(p.faulted());}
 // Real filesystem is unaltered by failed ownership or invalid opens.
 CHECK(bytes(packetFile(sd,S1)).size()==264);
 CHECK(bytes(root/"research_only.bin")==std::vector<uint8_t>({'R','E','S','E','A','R','C','H'}));
 ensureNoOpen();
}
int main(int argc,char** argv){
 if(argc!=3){std::cerr<<"usage: f24o_test {unit|init|offline|online|restart|audit|faults} <root>\n";return 2;}
 const fs::path root(argv[2]);const std::string mode=argv[1];
 if(mode=="unit")ownerUnit();
 else if(mode=="init")initOwner(root);
 else if(mode=="offline")offlineOwner(root);
 else if(mode=="online")onlineOwner(root);
 else if(mode=="restart")restartOwner(root);
 else if(mode=="audit")auditOwner(root);
 else if(mode=="faults")faultsOwner(root);
 else {std::cerr<<"invalid mode\n";return 2;}
 CHECK(handles==0);
 std::cout<<"F24O "<<mode<<" PASS "<<checks<<" checks; open_files="<<handles
          <<" append_opens="<<appendOpens<<" read_opens="<<readOpens<<"\n";
 return 0;
}
