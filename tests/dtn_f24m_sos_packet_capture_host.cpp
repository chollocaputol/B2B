// F24M actual 62-byte SOS capture/journal host regression; NO SD, RF or cloud.
// Uses OS TEMP sandbox and a mock Arduino SD/File facade backed by stdio.
#include "../firmware/shared/storage/dtn_f24m_sos_packet_store.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace fs=std::filesystem;
static unsigned checks=0;
#define CHECK(x) do {++checks; if(!(x)){std::fprintf(stderr,"CHECK failed %s:%d %s\n",__FILE__,__LINE__,#x);return 2;}}while(0)

class MockFile final {
 public:
  MockFile()=default;
  explicit MockFile(FILE* f):f_(f){if(f_)++openHandles;}
  MockFile(const MockFile&)=delete;
  MockFile& operator=(const MockFile&)=delete;
  MockFile(MockFile&& other)noexcept:f_(other.f_){other.f_=nullptr;}
  MockFile& operator=(MockFile&& other)noexcept{if(this!=&other){close();f_=other.f_;other.f_=nullptr;}return *this;}
  ~MockFile(){close();}
  explicit operator bool()const{return f_!=nullptr;}
  size_t size()const{
   if(!f_)return 0;
   const long old=std::ftell(f_);if(old<0 || std::fseek(f_,0,SEEK_END))return 0;
   const long n=std::ftell(f_);std::fseek(f_,old,SEEK_SET);
   return n<0 ? 0:size_t(n);
  }
  bool seek(size_t at){return f_ && std::fseek(f_,long(at),SEEK_SET)==0;}
  size_t write(const uint8_t* p,size_t n){return f_?std::fwrite(p,1,n,f_):0;}
  void flush(){if(f_)std::fflush(f_);}
  int getWriteError()const{return f_?std::ferror(f_):1;}
  int read(uint8_t* p,size_t n){return f_?int(std::fread(p,1,n,f_)):0;}
  void close(){if(f_){std::fclose(f_);f_=nullptr;--openHandles;}}
  static int openHandles;
 private:FILE* f_=nullptr;
};
int MockFile::openHandles=0;

class MockSD final {
 public:
  explicit MockSD(fs::path root):root_(std::move(root)){}
  bool allow=true;
  bool exists(const char* path)const{return fs::exists(translate(path));}
  MockFile open(const char* path,const char* mode){
   if(!allow || !path || !mode)return MockFile{};
   const fs::path file=translate(path);
   if(!fs::exists(file.parent_path()))return MockFile{};
   if(std::strcmp(mode,"a")==0)return MockFile(std::fopen(file.string().c_str(),"ab+"));
   if(std::strcmp(mode,"r")==0)return MockFile(std::fopen(file.string().c_str(),"rb"));
   return MockFile{};
  }
  fs::path translate(const char* path)const{
   if(!path || path[0]!='/')return root_/"INVALID";
   return root_/std::string(path+1);
  }
 private:fs::path root_;
};

static VesselPacket sos(uint32_t origin,uint32_t seq){
 VesselPacket p{};p.magic=PACKET_MAGIC;p.version=PACKET_VERSION;
 p.packetType=PACKET_SOS;p.originNodeId=origin;p.sequenceNumber=seq;
 p.timestamp=123456;p.hopCount=0;p.flags=FLAG_GPS_FIX|FLAG_TEMP_HUMIDITY_VALID|FLAG_PRESSURE_VALID;
 std::memcpy(p.nodeName,"VESSEL_002",10);
 p.latitude_e7=81234567;p.longitude_e7=1249876543;p.batteryLevel=75;
 p.shoreDistance_m=1200;p.weather.temperature_x10=295;p.weather.humidity_x10=651;
 p.weather.pressure_x10=10132;p.weather.pressureTrend_x10=-12;
 p.weather.zambrettiState=8;p.weather.sensorFusionState=2;p.weather.weatherState=3;
 std::memcpy(p.message,"SOS",3);return p;
}
static std::vector<uint8_t> bytes(const fs::path& p){
 std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};
}
int main(){
 CHECK(sizeof(VesselPacket)==62);
 VesselPacket local=sos(2,71),received=sos(1,70);
 received.hopCount=1;
 DTNF24M::PendingQueue<2> pending;
 CHECK(pending.capture(local,DTNF24M::kLocal)==DTNF24M::CaptureResult::Queued);
 CHECK(pending.pending()==1);
 CHECK(pending.capture(local,DTNF24M::kLocal)==DTNF24M::CaptureResult::Duplicate);
 CHECK(pending.pending()==1);
 VesselPacket conflict=local;conflict.weather.temperature_x10=299;
 CHECK(pending.capture(conflict,DTNF24M::kLocal)==DTNF24M::CaptureResult::Conflicting);
 CHECK(pending.pending()==1);
 VesselPacket normal=local;normal.packetType=PACKET_TELEMETRY;
 CHECK(pending.capture(normal,DTNF24M::kLocal)==DTNF24M::CaptureResult::Invalid);
 VesselPacket ack=local;ack.packetType=PACKET_SOS_ACK;
 CHECK(pending.capture(ack,DTNF24M::kLocal)==DTNF24M::CaptureResult::Invalid);
 VesselPacket cancel=local;cancel.packetType=PACKET_SOS_CANCEL;
 CHECK(pending.capture(cancel,DTNF24M::kLocal)==DTNF24M::CaptureResult::Invalid);
 VesselPacket cancelAck=local;cancelAck.packetType=PACKET_SOS_CANCEL_ACK;
 CHECK(pending.capture(cancelAck,DTNF24M::kLocal)==DTNF24M::CaptureResult::Invalid);
 VesselPacket noOrigin=local;noOrigin.originNodeId=0;
 CHECK(pending.capture(noOrigin,DTNF24M::kLocal)==DTNF24M::CaptureResult::Invalid);
 VesselPacket noEvent=local;noEvent.sequenceNumber=0;
 CHECK(pending.capture(noEvent,DTNF24M::kLocal)==DTNF24M::CaptureResult::Invalid);
 CHECK(pending.capture(local,99)==DTNF24M::CaptureResult::Invalid);
 CHECK(pending.capture(received,DTNF24M::kReceived)==DTNF24M::CaptureResult::Queued);
 CHECK(pending.pending()==2);
 VesselPacket full=sos(3,72);
 CHECK(pending.capture(full,DTNF24M::kLocal)==DTNF24M::CaptureResult::Full);
 CHECK(pending.lost()==1);
 CHECK(pending.pending()==2);
 CHECK(pending.front()->origin==2);
 CHECK(pending.front()->event==71);
 uint8_t first[DTNF24M::kFrameBytes]{};
 CHECK(DTNF24M::encode(*pending.front(),first));
 CHECK(DTNF24M::rd32(first+8)==2);
 CHECK(DTNF24M::rd32(first+12)==71);
 CHECK(DTNF24M::rd16(first+16)==62);
 CHECK(std::memcmp(first+22,&local,62)==0);
 DTNF24M::PacketEntry decoded{};
 CHECK(DTNF24M::decode(first,sizeof(first),decoded));
 CHECK(decoded.source==DTNF24M::kLocal);
 CHECK(std::memcmp(decoded.bytes,&local,62)==0);
 CHECK(!DTNF24M::decode(first,87,decoded));
 uint8_t broken[DTNF24M::kFrameBytes];std::memcpy(broken,first,sizeof(first));
 broken[35]^=1;
 CHECK(!DTNF24M::decode(broken,sizeof(broken),decoded));
 std::memcpy(broken,first,sizeof(first));broken[84]^=1;
 CHECK(!DTNF24M::decode(broken,sizeof(broken),decoded));
 std::memcpy(broken,first,sizeof(first));broken[4]=2;
 CHECK(!DTNF24M::decode(broken,sizeof(broken),decoded));
 std::memcpy(broken,first,sizeof(first));broken[6]=1;
 CHECK(!DTNF24M::decode(broken,sizeof(broken),decoded));
 CHECK(!DTNF24M::decode(nullptr,sizeof(first),decoded));
 CHECK(DTNF24M::kFrameBytes==88);

 const fs::path root=fs::temp_directory_path()/(
   "THESIS_F24M_SYNTHETIC_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 CHECK(fs::create_directory(root));
 const fs::path session=root/"THESIS"/"F06TEST"/"V002"/"SABC1234";
 fs::create_directories(session);
 CHECK(fs::is_directory(session));
 const char* dir="/THESIS/F06TEST/V002/SABC1234";
 char path[96]{};
 CHECK(DTNF24M::sandboxPath(path,sizeof(path),dir,2));
 CHECK(std::string(path)==std::string(dir)+"/dtn_sos_packets.f24m");
 CHECK(!DTNF24M::sandboxPath(path,sizeof(path),"/THESIS/VESSEL_002",2));
 CHECK(!DTNF24M::sandboxPath(path,sizeof(path),"/THESIS/F06TEST/V001/SABC1234",2));
 CHECK(!DTNF24M::sandboxPath(path,sizeof(path),"/THESIS/F06TEST/V002/SDDDXXXX",2));
 CHECK(!DTNF24M::sandboxPath(path,sizeof(path),dir,0));
 CHECK(!DTNF24M::sandboxPath(path,sizeof(path),dir,4));
 char tiny[4]{};
 CHECK(!DTNF24M::sandboxPath(tiny,sizeof(tiny),dir,2));
 MockSD sd(root);
 DTNF24M::SessionWriter writer;
 CHECK(writer.written()==0);
 CHECK(!writer.faulted());
 CHECK(writer.appendChecked(sd,dir,2,*pending.front(),"a","r"));
 CHECK(writer.written()==88);
 CHECK(MockFile::openHandles==0);
 CHECK(pending.acknowledgeDurableAppend());
 CHECK(pending.pending()==1);
 CHECK(pending.front()->origin==1);
 CHECK(writer.appendChecked(sd,dir,2,*pending.front(),"a","r"));
 CHECK(writer.written()==176);
 CHECK(MockFile::openHandles==0);
 CHECK(pending.acknowledgeDurableAppend());
 CHECK(pending.pending()==0);
 CHECK(!pending.acknowledgeDurableAppend());
 const fs::path saved=sd.translate(writer.path());
 const auto persistent=bytes(saved);
 CHECK(persistent.size()==176);
 CHECK(std::memcmp(persistent.data()+22,&local,62)==0);
 CHECK(std::memcmp(persistent.data()+88+22,&received,62)==0);
 DTNF24M::PacketEntry afterRestart1{},afterRestart2{};
 CHECK(DTNF24M::decode(persistent.data(),88,afterRestart1));
 CHECK(DTNF24M::decode(persistent.data()+88,88,afterRestart2));
 CHECK(afterRestart1.origin==2 && afterRestart1.event==71);
 CHECK(afterRestart2.origin==1 && afterRestart2.event==70);
 CHECK(afterRestart2.source==DTNF24M::kReceived);
 CHECK(std::memcmp(afterRestart2.bytes,&received,62)==0);
 DTNF24M::SessionWriter restart;
 CHECK(!restart.appendChecked(sd,dir,2,afterRestart1,"a","r"));
 CHECK(restart.faulted());
 CHECK(bytes(saved)==persistent);
 CHECK(MockFile::openHandles==0);
 // A different freshly allocated boot session cannot modify the old source.
 const fs::path newer=root/"THESIS"/"F06TEST"/"V002"/"SABC1235";
 fs::create_directories(newer);
 DTNF24M::SessionWriter nextSession;
 CHECK(nextSession.appendChecked(sd,"/THESIS/F06TEST/V002/SABC1235",2,afterRestart1,"a","r"));
 CHECK(nextSession.written()==88);
 CHECK(bytes(saved)==persistent);
 CHECK(MockFile::openHandles==0);
 // Denied owner must not create a file, and must latch unconfirmed result.
 const fs::path denied=root/"THESIS"/"F06TEST"/"V002"/"SABC1236";
 fs::create_directories(denied);
 sd.allow=false;
 DTNF24M::SessionWriter deny;
 CHECK(!deny.appendChecked(sd,"/THESIS/F06TEST/V002/SABC1236",2,afterRestart1,"a","r"));
 CHECK(deny.faulted());
 CHECK(!fs::exists(denied/"dtn_sos_packets.f24m"));
 sd.allow=true;
 CHECK(!deny.appendChecked(sd,"/THESIS/F06TEST/V002/SABC1236",2,afterRestart1,"a","r"));
 CHECK(!fs::exists(denied/"dtn_sos_packets.f24m"));
 // Torn append must fail strict whole-frame parser; never repair/truncate.
 const fs::path torn=root/"torn.bin";
 {std::ofstream f(torn,std::ios::binary);f.write(reinterpret_cast<const char*>(first),50);}
 const auto partial=bytes(torn);
 CHECK(partial.size()==50);
 CHECK(!DTNF24M::decode(partial.data(),partial.size(),decoded));
 CHECK(bytes(saved)==persistent);
 // A byte-for-byte conflict with identical SOS ID must be quarantined in RAM.
 DTNF24M::PendingQueue<2> bad;
 CHECK(bad.capture(local,DTNF24M::kLocal)==DTNF24M::CaptureResult::Queued);
 CHECK(bad.capture(conflict,DTNF24M::kLocal)==DTNF24M::CaptureResult::Conflicting);
 bad.latchStorageFault();
 CHECK(bad.faulted());
 CHECK(bad.pending()==1);
 CHECK(bad.capture(full,DTNF24M::kLocal)==DTNF24M::CaptureResult::Faulted);
 CHECK(!bad.acknowledgeDurableAppend());
 CHECK(MockFile::openHandles==0);
 fs::remove_all(root);
 CHECK(!fs::exists(root));
 std::printf("=== F24M FULL 62-BYTE SOS HOST PASS: %u CHECKS; 2 VERIFIED PACKETS; SOS ONLY; NO PHYSICAL SD ===\n",checks);
 return 0;
}
