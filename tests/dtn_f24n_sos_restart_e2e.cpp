// F24N — REAL host filesystem, independent processes, exact firmware 88-byte
// SOS frames, sealed-by-close read-only replay, mock shore only. NO physical SD.
#include "../firmware/shared/storage/dtn_f24n_sos_restart_recovery.h"
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
#define CHECK(c) do{++checks;if(!(c)){std::cerr<<"F24N CHECK FAIL "<<__FILE__<<":"<<__LINE__<<" "<<#c<<"\n";std::exit(51);}}while(0)
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
static void unit(){
 CHECK(sizeof(VesselPacket)==62);CHECK(DTNF24M::kFrameBytes==88);
 CHECK(DTNF24N::kReceiptBytes==40);
 const auto p=packet(2,70);DTNF24N::Ledger<3> l;
 CHECK(l.accept(p)==DTNF24N::Add::Inserted);
 CHECK(l.accept(p)==DTNF24N::Add::Duplicate);
 CHECK(l.count()==1);CHECK(l.pending()==1);CHECK(l.delivered()==0);
 auto changed=p;changed.bytes[25]^=1;
 CHECK(l.accept(changed)==DTNF24N::Add::Conflict);CHECK(l.faulted());
 CHECK(l.nextPending()==nullptr);
 DTNF24N::Receipt missing{};uint8_t raw[40]{};
 CHECK(!DTNF24N::encodeReceipt(missing,raw));
 missing.origin=2;missing.event=70;missing.packetCrc=DTNF24M::crc32(p.bytes,62);
 missing.serverId=5;missing.serverTag=6;
 CHECK(DTNF24N::encodeReceipt(missing,raw));
 DTNF24N::Receipt decoded{};CHECK(DTNF24N::decodeReceipt(raw,40,decoded));
 CHECK(decoded.serverId==5);CHECK(decoded.serverTag==6);
 CHECK(!DTNF24N::decodeReceipt(raw,39,decoded));
 raw[15]^=1;CHECK(!DTNF24N::decodeReceipt(raw,40,decoded));raw[15]^=1;
 CHECK(DTNF24N::decodeReceipt(raw,40,decoded));
 DTNF24N::Ledger<3> r;CHECK(r.accept(p)==DTNF24N::Add::Inserted);
 CHECK(r.apply(decoded,DTNF24N::DenyReceiptVerifier{})==DTNF24N::ReceiptResult::Unverified);
 CHECK(r.pending()==1);
 auto no=packet(3,80);CHECK(r.accept(no)==DTNF24N::Add::Inserted);
 auto no2=packet(1,81);CHECK(r.accept(no2)==DTNF24N::Add::Inserted);
 CHECK(r.accept(packet(1,82))==DTNF24N::Add::Full);CHECK(r.faulted());
}
static void init(const fs::path& root){
 CHECK(!fs::exists(root));CHECK(fs::create_directories(root));sessions(root);
 CHECK(serverRows(root).size()==0);
 HostSD sd(root);DTNF24M::SessionWriter writer;
 for(const auto& p:{packet(2,70),packet(1,71),packet(3,72)})
  CHECK(writer.appendChecked(sd,S1,2,p,"a","r"));
 CHECK(writer.written()==3*88);ensureNoOpen();
 auto source=bytes(packetFile(sd,S1));CHECK(source.size()==3*88);
 for(int i=0;i<3;++i){
  auto e=packet(i==0?2:(i==1?1:3),70+i);
  DTNF24M::PacketEntry seen{};
  CHECK(DTNF24M::decode(source.data()+i*88,88,seen));
  CHECK(memcmp(seen.bytes,e.bytes,62)==0);
 }
 put(root/"research_only.bin",{'R','E','S','E','A','R','C','H','_','N','O','_','D','T','N'});
 CHECK(fs::exists(root/"research_only.bin"));
}
static void offline(const fs::path& root){
 HostSD sd(root);const int before=appendOpens;
 auto r=recover(sd,S2,{S1},DTNF24N::DenyReceiptVerifier{});
 CHECK(r.ledger().count()==3);CHECK(r.ledger().pending()==3);
 CHECK(r.ledger().delivered()==0);CHECK(r.ledger().nextPending()!=nullptr);
 CHECK(appendOpens==before);CHECK(!fs::exists(receiptFile(sd,S2)));
 ensureNoOpen();
}
static void lost(const fs::path& root){
 HostSD sd(root);auto r=recover(sd,S2,{S1},FakeShoreVerifier{root});
 const auto* p=r.ledger().nextPending();CHECK(p!=nullptr);
 auto accepted=serverAccept(root,*p);CHECK(accepted.origin==2 && accepted.event==70);
 CHECK(serverRows(root).size()==1);
 CHECK(!fs::exists(receiptFile(sd,S2)));
 CHECK(r.ledger().pending()==3);ensureNoOpen();
}
static void online1(const fs::path& root){
 HostSD sd(root);auto r=recover(sd,S2,{S1},FakeShoreVerifier{root});
 CHECK(r.ledger().pending()==3);
 DTNF24N::ReceiptWriter writer;
 const auto* p0=r.ledger().nextPending();CHECK(p0 && p0->event==70);
 const auto first=serverAccept(root,*p0);CHECK(serverRows(root).size()==1);
 CHECK(writer.appendVerified(sd,S2,2,r.ledger(),first,FakeShoreVerifier{root},"a","r"));
 CHECK(r.ledger().pending()==2);CHECK(r.ledger().delivered()==1);
 const auto* p1=r.ledger().nextPending();CHECK(p1 && p1->event==71);
 auto second=serverAccept(root,*p1);
 CHECK(writer.appendVerified(sd,S2,2,r.ledger(),second,FakeShoreVerifier{root},"a","r"));
 CHECK(r.ledger().pending()==1);CHECK(r.ledger().delivered()==2);
 CHECK(writer.written()==2*40);CHECK(serverRows(root).size()==2);
 CHECK(bytes(receiptFile(sd,S2)).size()==80);
 CHECK(!writer.appendVerified(sd,S2,2,r.ledger(),first,FakeShoreVerifier{root},"a","r"));
 ensureNoOpen();
}
static void online2(const fs::path& root){
 HostSD sd(root);auto r=recover(sd,S3,{S1,S2},FakeShoreVerifier{root});
 CHECK(r.ledger().pending()==1);CHECK(r.ledger().delivered()==2);
 auto pending=r.ledger().nextPending();CHECK(pending && pending->event==72);
 const auto third=serverAccept(root,*pending);CHECK(serverRows(root).size()==3);
 DTNF24N::ReceiptWriter writer;
 CHECK(writer.appendVerified(sd,S3,2,r.ledger(),third,FakeShoreVerifier{root},"a","r"));
 CHECK(r.ledger().pending()==0);CHECK(r.ledger().delivered()==3);
 CHECK(writer.written()==40);ensureNoOpen();
}
static void audit(const fs::path& root){
 HostSD sd(root);const int oldAppend=appendOpens;
 auto r=recover(sd,S4,{S1,S2,S3},FakeShoreVerifier{root});
 CHECK(r.ledger().count()==3);CHECK(r.ledger().pending()==0);
 CHECK(r.ledger().delivered()==3);CHECK(r.unverified()==0);
 CHECK(r.ledger().nextPending()==nullptr);CHECK(serverRows(root).size()==3);
 CHECK(bytes(packetFile(sd,S1)).size()==3*88);
 const auto b=bytes(packetFile(sd,S1));
 for(size_t i=0;i<3;++i){DTNF24M::PacketEntry out{};
  CHECK(DTNF24M::decode(b.data()+i*88,88,out));
  auto ref=packet(i==0?2:(i==1?1:3),70+unsigned(i));
  CHECK(memcmp(ref.bytes,out.bytes,62)==0);
 }
 CHECK(bytes(root/"research_only.bin")==std::vector<uint8_t>({'R','E','S','E','A','R','C','H','_','N','O','_','D','T','N'}));
 CHECK(appendOpens==oldAppend);ensureNoOpen();
}
static void faults(const fs::path& root){
 HostSD sd(root);
 // Reboot with no receipt verifier: even correctly formatted persisted claims
 // are NOT proof of final delivery and all SOS packets remain pending.
 auto withheld=recover(sd,S4,{S1,S2,S3},DTNF24N::DenyReceiptVerifier{});
 CHECK(withheld.ledger().count()==3);CHECK(withheld.ledger().pending()==3);
 CHECK(withheld.ledger().delivered()==0);CHECK(withheld.unverified()==3);
 // A second session reuses origin/event with a changed weather byte: conflict.
 DTNF24M::SessionWriter conflictWriter;
 auto changed=packet(2,70);changed.bytes[DTNF24M::kWireBytes-8]^=2;
 CHECK(conflictWriter.appendChecked(sd,S4,2,changed,"a","r"));
 DTNF24N::Replay<16,12> conflict;CHECK(conflict.begin(2,S5));
 CHECK(conflict.addClosed(S1));CHECK(conflict.addClosed(S4));CHECK(conflict.lock());
 bool failed=false;
 for(int i=0;i<30;++i){auto result=conflict.poll(sd,"r",FakeShoreVerifier{root},HostReadAuthority{&sd});
  CHECK(handles==0);
  if(result==DTNF24N::Poll::Failed){failed=true;break;}
  CHECK(result==DTNF24N::Poll::Progress);
 }
 CHECK(failed);CHECK(conflict.failed());CHECK(conflict.ledger().nextPending()==nullptr);
 // Torn F24M frame must NEVER be skipped or repaired.
 const auto full=bytes(packetFile(sd,S1));
 put(packetFile(sd,S5),std::vector<uint8_t>(full.begin(),full.begin()+50));
 DTNF24N::Replay<16,12> torn;CHECK(torn.begin(2,S6));
 CHECK(torn.addClosed(S5));CHECK(torn.lock());
 CHECK(torn.poll(sd,"r",FakeShoreVerifier{root},HostReadAuthority{&sd})==DTNF24N::Poll::Failed);
 CHECK(torn.failed());CHECK(torn.ledger().nextPending()==nullptr);
 // Closed-journal growth between polls is also forbidden, even if new frames
 // have valid CRCs: no silent acceptance of a concurrent writer.
 put(packetFile(sd,S5),std::vector<uint8_t>(full.begin(),full.begin()+176));
 DTNF24N::Replay<16,12> growing;CHECK(growing.begin(2,S6));
 CHECK(growing.addClosed(S5));CHECK(growing.lock());
 CHECK(growing.poll(sd,"r",FakeShoreVerifier{root},HostReadAuthority{&sd})==DTNF24N::Poll::Progress);
 put(packetFile(sd,S5),full);
 CHECK(growing.poll(sd,"r",FakeShoreVerifier{root},HostReadAuthority{&sd})==DTNF24N::Poll::Failed);
 CHECK(growing.failed());CHECK(handles==0);
 // Torn ACK/receipt journal likewise fails closed (no ACK credit).
 put(receiptFile(sd,S6),std::vector<uint8_t>(19,0x55));
 DTNF24N::Replay<16,12> tornAck;CHECK(tornAck.begin(2,S7));
 CHECK(tornAck.addClosed(S1));CHECK(tornAck.addClosed(S6));CHECK(tornAck.lock());
 failed=false;
 for(int i=0;i<40;++i){auto result=tornAck.poll(sd,"r",FakeShoreVerifier{root},HostReadAuthority{&sd});
  CHECK(handles==0);
  if(result==DTNF24N::Poll::Failed){failed=true;break;}
  CHECK(result==DTNF24N::Poll::Progress);
 }
 CHECK(failed);CHECK(tornAck.failed());
 // A syntactically intact but forged receipt, absent from shore mock, cannot
 // confer delivery credit or trigger a packet deletion.
 DTNF24N::Receipt fake{};auto p=packet(2,70);
 fake.origin=2;fake.event=70;fake.packetCrc=DTNF24M::crc32(p.bytes,62);
 fake.serverId=0xF12345678ULL;fake.serverTag=4;
 uint8_t raw[DTNF24N::kReceiptBytes]{};CHECK(DTNF24N::encodeReceipt(fake,raw));
 put(receiptFile(sd,S7),std::vector<uint8_t>(raw,raw+sizeof(raw)));
 auto untrusted=recover(sd,S6,{S1,S7},FakeShoreVerifier{root});
 CHECK(untrusted.ledger().pending()==3);CHECK(untrusted.ledger().delivered()==0);
 CHECK(untrusted.unverified()==1);
 // Validated namespace: wrong vessel, traversal and current dir rejected.
 DTNF24N::Replay<4,2> invalid;CHECK(invalid.begin(2,S4));
 CHECK(!invalid.addClosed("/THESIS/F06TEST/V001/SABC1001"));
 CHECK(invalid.failed());
 DTNF24N::Replay<4,2> dup;CHECK(dup.begin(2,S4));CHECK(dup.addClosed(S1));
 CHECK(!dup.addClosed(S1));CHECK(dup.failed());
 DTNF24N::Replay<4,2> current;CHECK(current.begin(2,S4));CHECK(!current.addClosed(S4));
 DTNF24N::Replay<4,1> cap;CHECK(cap.begin(2,S4));CHECK(cap.addClosed(S1));
 CHECK(!cap.addClosed(S2));CHECK(cap.failed());
 // Denied disk access cannot replay an 'empty' source as successful.
 HostSD denied(root);denied.denied=true;
 DTNF24N::Replay<4,1> preview;CHECK(preview.begin(2,S4));
 CHECK(preview.addClosed(S1));CHECK(preview.lock());
 CHECK(preview.poll(denied,"r",FakeShoreVerifier{root},HostReadAuthority{&denied})==DTNF24N::Poll::Failed);
 CHECK(preview.ledger().count()==0);CHECK(preview.failed());
 // No physical authority is inferred from a simulated missing file.
 ensureNoOpen();
}
int main(int argc,char** argv){
 if(argc!=3){std::cerr<<"usage: f24n_test {unit|init|offline|lost|online1|online2|audit|faults} <root>\n";return 2;}
 const fs::path root(argv[2]);const std::string mode=argv[1];
 if(mode=="unit")unit();
 else if(mode=="init")init(root);
 else if(mode=="offline")offline(root);
 else if(mode=="lost")lost(root);
 else if(mode=="online1")online1(root);
 else if(mode=="online2")online2(root);
 else if(mode=="audit")audit(root);
 else if(mode=="faults")faults(root);
 else {std::cerr<<"invalid mode\n";return 2;}
 CHECK(handles==0);
 std::cout<<"F24N "<<mode<<" PASS "<<checks<<" checks; open_files="<<handles
          <<" append_opens="<<appendOpens<<" read_opens="<<readOpens<<"\n";
 return 0;
}
