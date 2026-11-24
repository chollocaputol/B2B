// F24R: test actual F23D File method call sites through F24Q lease adapter,
// plus export/history style retained File read methods with disposable RAM files.
// Synthetic witness only. Arduino File destructor safety is NOT validated.
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include "../firmware/shared/relay/f23d_sd_journal.h"
#include "../firmware/shared/storage/dtn_f24r_guarded_file_port.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"F24R HOST FAIL line %d: %s\n",__LINE__,#x);exit(12);}}while(0)
constexpr const char* FILE_APPEND="a";
constexpr const char* FILE_READ="r";
struct FakeVolume;
struct File {
 FakeVolume* volume=nullptr;bool append=false;size_t pos=0;
 File()=default;File(FakeVolume*,bool);
 File(const File&)=delete;File& operator=(const File&)=delete;
 File(File&& x)noexcept:volume(x.volume),append(x.append),pos(x.pos){x.volume=nullptr;}
 File& operator=(File&& x)noexcept{if(this!=&x){close();volume=x.volume;append=x.append;pos=x.pos;x.volume=nullptr;}return *this;}
 ~File(){close();}
 explicit operator bool()const{return volume!=nullptr;}
 size_t size()const;size_t write(const uint8_t*,size_t);size_t read(uint8_t*,size_t);
 int read();bool isDirectory()const{return false;}bool seek(size_t);void flush();int getWriteError()const{return 0;}void close();
};
struct FakeVolume {
 std::vector<uint8_t> data{};bool created=false;int opens=0,existsCalls=0,active=0,peak=0,reads=0,writes=0,closes=0;
 bool exists(const char* path){++existsCalls;return created && path && strstr(path,"f23d_multihop.jsonl");}
 File open(const char* path,const char* mode){
  ++opens;if(!path || !mode)return {};
  if(!strstr(path,"f23d_multihop.jsonl") && !strstr(path,"sos_events.jsonl"))return {};
  if(!strcmp(mode,"a")){created=true;return File(this,true);}
  if(!strcmp(mode,"r") && created)return File(this,false);
  return {};
 }
};
File::File(FakeVolume* v,bool a):volume(v),append(a),pos(a?v->data.size():0){
 if(v){++v->active;if(v->active>v->peak)v->peak=v->active;}
}
size_t File::size()const{return volume?volume->data.size():0;}
size_t File::write(const uint8_t* d,size_t n){
 if(!volume || !append || pos!=volume->data.size())return 0;
 volume->data.insert(volume->data.end(),d,d+n);pos+=n;volume->writes++;return n;
}
size_t File::read(uint8_t* d,size_t n){
 if(!volume || append || pos+n>volume->data.size())return 0;
 memcpy(d,volume->data.data()+pos,n);pos+=n;volume->reads++;return n;
}
int File::read(){uint8_t x=0;return read(&x,1)==1?int(x):-1;}
bool File::seek(size_t p){if(!volume || p>volume->data.size())return false;pos=p;return true;}
void File::flush(){}
void File::close(){if(volume){--volume->active;volume->closes++;volume=nullptr;}}
struct Witness {DTNF24Q::Facts facts{};DTNF24Q::Facts view()const{return facts;}};
static DTNF24Q::Facts allowed(){DTNF24Q::Facts f{};f.vessel=2;f.mediaTag=71;f.mountEpoch=72;f.ownerSerial=73;
 f.independentlyAuthorized=f.soleOwner=f.allPathsInterposed=true;
 f.legacyCoordinated=f.f23dCoordinated=f.otherHandlesClosed=true;return f;}
using Arb=DTNF24Q::Arbiter<Witness>;
using Port=DTNF24R::GuardedVolume<FakeVolume,Witness>;
static FakeVolume sd;
static Witness witness{allowed()};
static Arb arbiter(witness);
static DTNF24Q::Lease lease=arbiter.acquire(2,DTNF24Q::Lane::F23D);
static Port guarded(sd,arbiter,lease);
#define F23D_SD_ACCESS guarded
#define F23D_SD_FILE_TYPE Port::FileHandle
#include "../firmware/shared/storage/f23d_sd_file_writer.h"
#undef F23D_SD_ACCESS
#undef F23D_SD_FILE_TYPE
static void actualWriter(){
 CHECK(lease.serial==73);CHECK(arbiter.activeLane()==DTNF24Q::Lane::F23D);
 F23DSDFileWriter writer;
 const char* dir="/THESIS/F06TEST/V002/S0000001";
 CHECK(writer.begin(dir,2));CHECK(sd.opens==1);CHECK(sd.existsCalls==1);
 CHECK(sd.active==1);CHECK(sd.peak==1);
 const char row[]="{\"kind\":\"F23D\"}\n";
 CHECK(writer.appendChecked(row,sizeof(row)-1));CHECK(sd.peak==2);
 CHECK(sd.opens==2);CHECK(sd.active==1);
 CHECK(writer.finish()==F23DSD::Finish::Pending);
 CHECK(sd.active==1);CHECK(sd.opens==3);
 CHECK(writer.finish()==F23DSD::Finish::OK);
 CHECK(sd.active==0);CHECK(!memcmp(sd.data.data(),row,sizeof(row)-1));
 CHECK(guarded.release());CHECK(arbiter.activeLane()==DTNF24Q::Lane::None);
}
static void retainedReaders(){
 for(auto lane:{DTNF24Q::Lane::Export,DTNF24Q::Lane::History}){
  Witness w{allowed()};Arb a(w);auto l=a.acquire(2,lane);
  Port p(sd,a,l);const int before=sd.opens;
  auto f=p.open("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl",FILE_READ);
  CHECK(bool(f));CHECK(sd.opens==before+1);CHECK(f.size()==sd.data.size());
  CHECK(!f.isDirectory());CHECK(f.seek(0));CHECK(f.read()==int(sd.data[0]));
  uint8_t buf[3]{};CHECK(f.read(buf,3)==3);CHECK(!memcmp(buf,sd.data.data()+1,3));
  CHECK(f.write(buf,3)==0);CHECK(!p.release());
  f.close();CHECK(!bool(f));CHECK(p.release());CHECK(sd.active==0);
 }
}
static void denyBeforeBackend(){
 DTNF24Q::DenyWitness deny{};DTNF24Q::Arbiter<DTNF24Q::DenyWitness> a(deny);
 auto l=a.acquire(2,DTNF24Q::Lane::Export);
 DTNF24R::GuardedVolume<FakeVolume,DTNF24Q::DenyWitness> p(sd,a,l);
 const int before=sd.opens,beforeExists=sd.existsCalls;
 CHECK(!l.serial);CHECK(!p.exists("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl"));
 auto f=p.open("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl",FILE_READ);
 CHECK(!bool(f));CHECK(f.read()==-1);CHECK(f.size()==0);
 CHECK(sd.opens==before);CHECK(sd.existsCalls==beforeExists);
 CHECK(!p.release());
}
static void revocationStopsRawCalls(){
 Witness w{allowed()};Arb a(w);auto l=a.acquire(2,DTNF24Q::Lane::History);
 Port p(sd,a,l);auto file=p.open("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl",FILE_READ);
 CHECK(bool(file));const int reads=sd.reads,closes=sd.closes;
 w.facts.mountEpoch++;
 CHECK(file.read()==-1);CHECK(a.faulted());CHECK(a.quarantined());
 file.close();CHECK(sd.reads==reads);CHECK(sd.closes==closes);
 CHECK(!p.release());w.facts=allowed();CHECK(!p.release());
 // Deliberately do not claim physical close safety: FakeFile will automatically
 // close at scope exit, just as an Arduino File may. No actual SD permission.
}
static void contention(){
 Witness w{allowed()};Arb a(w);auto l=a.acquire(2,DTNF24Q::Lane::Export);
 Port p(sd,a,l);auto file=p.open("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl",FILE_READ);
 CHECK(bool(file));const int before=sd.opens;
 auto other=p.open("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl",FILE_READ);
 CHECK(!bool(other));CHECK(sd.opens==before);CHECK(!a.acquire(2,DTNF24Q::Lane::F23D).serial);
 file.close();CHECK(p.release());
}
int main(){actualWriter();retainedReaders();denyBeforeBackend();revocationStopsRawCalls();contention();
 printf("F24R guarded real F23D and retained export/history File methods PASS: %u host checks; synthetic owner only; physical destructor safety pending\n",checks);
 return 0;
}
