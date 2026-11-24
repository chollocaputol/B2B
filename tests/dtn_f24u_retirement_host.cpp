// F24U: synthetic owner-verified quiesce and File handle retirement only.
// The test intentionally demonstrates UNSAFE implicit destruction after revoke.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include "../firmware/shared/storage/dtn_f24r_guarded_file_port.h"
#include "../firmware/shared/storage/dtn_f24u_teardown_coordinator.h"
static unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){std::fprintf(stderr,"F24U FAIL line %d: %s\n",__LINE__,#v);std::exit(23);}}while(0)
struct Witness { DTNF24Q::Facts facts{};DTNF24Q::Facts view()const{return facts;} };
static DTNF24Q::Facts authorized(){DTNF24Q::Facts f{};
 f.vessel=2;f.mediaTag=501;f.mountEpoch=502;f.ownerSerial=503;
 f.independentlyAuthorized=f.soleOwner=f.allPathsInterposed=true;
 f.legacyCoordinated=f.f23dCoordinated=f.otherHandlesClosed=true;
 return f;
}
struct Volume;
struct File {
 Volume* volume=nullptr; bool writer=false,parent=false;unsigned next=0;
 File()=default;File(Volume*,bool,bool);
 File(const File&)=delete;File& operator=(const File&)=delete;
 File(File&& x)noexcept:volume(x.volume),writer(x.writer),parent(x.parent),next(x.next){x.volume=nullptr;}
 File& operator=(File&& x)noexcept {if(this!=&x){close();volume=x.volume;writer=x.writer;parent=x.parent;next=x.next;x.volume=nullptr;}return *this;}
 ~File(){close();}
 explicit operator bool()const{return volume!=nullptr;}
 size_t size()const{return 12;}bool isDirectory()const{return parent;}
 const char* name()const{return parent?"/THESIS/F06TEST/V002":"S0000001";}
 bool seek(size_t){return bool(volume);}int read(){return volume?7:-1;}
 int available(){return volume?1:0;}size_t read(uint8_t*,size_t n){return volume?n:0;}
 size_t write(const uint8_t*,size_t n){return volume&&writer?n:0;}
 size_t print(const char* s){return volume&&writer&&s?std::strlen(s):0;}
 size_t print(char){return volume&&writer?1:0;}
 void flush();int getWriteError()const{return 0;}
 void close();File openNextFile();
};
struct Volume {
 Witness* witness=nullptr; unsigned opens=0,children=0,existsCalls=0,active=0,peak=0,closes=0;
 unsigned closesAfterEpochLoss=0,flushes=0,reads=0,writes=0;
 bool existsFile(const char*){++existsCalls;return true;}
 bool exists(const char* s){return existsFile(s);}
 File open(const char* path,const char* mode){
  ++opens;
  if(!path || !mode)return {};
  if(!std::strcmp(mode,"a"))return File(this,true,false);
  if(!std::strcmp(mode,"r"))return File(this,false,std::strstr(path,"V002") && !std::strstr(path,"jsonl"));
  return {};
 }
};
File::File(Volume* v,bool w,bool p):volume(v),writer(w),parent(p){
 if(volume){++volume->active;if(volume->active>volume->peak)volume->peak=volume->active;}
}
void File::close(){if(volume){++volume->closes;--volume->active;
 if(volume->witness && (volume->witness->facts.mediaTag!=501 || volume->witness->facts.mountEpoch!=502 || volume->witness->facts.ownerSerial!=503)) ++volume->closesAfterEpochLoss;
 volume=nullptr;}}
void File::flush(){if(volume)++volume->flushes;}
File File::openNextFile(){if(!volume || !parent || next++>0)return {};
 ++volume->children;return File(volume,false,false);}
using Arb=DTNF24Q::Arbiter<Witness>;
using Port=DTNF24R::GuardedVolume<Volume,Witness>;
using Coordinator=DTNF24U::Teardown<Port>;
using Status=DTNF24U::Status;
static void noAuthority(){
 DTNF24Q::DenyWitness deny{};
 DTNF24Q::Arbiter<DTNF24Q::DenyWitness> arb(deny);
 DTNF24Q::Lease zero=arb.acquire(2,DTNF24Q::Lane::Sos);
 CHECK(zero.serial==0);CHECK(!arb.retiring());
 Volume sd{};DTNF24R::GuardedVolume<Volume,DTNF24Q::DenyWitness> p(sd,arb,zero);
 DTNF24U::Teardown<decltype(p)> t(p);
 CHECK(t.status()==Status::NotStarted);CHECK(t.finish()==Status::NotStarted);
 CHECK(t.begin()==Status::Denied);CHECK(t.status()==Status::Denied);
 CHECK(!p.hasOpenHandle());CHECK(!p.retiring());CHECK(!p.verifyRetirementOwner());
 CHECK(!p.open("/fake","a"));CHECK(sd.opens==0);CHECK(!p.exists("/fake"));CHECK(sd.existsCalls==0);
 CHECK(!p.finishRetirement());CHECK(!arb.faulted());CHECK(!arb.quarantined());
}
static void appendDrain(){
 Witness w{authorized()};Volume sd{&w};Arb arb(w);
 const auto l=arb.acquire(2,DTNF24Q::Lane::LegacyAppend);CHECK(l.serial==503);
 Port p(sd,arb,l);Coordinator t(p);
 auto f=p.open("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl","a");
 CHECK(bool(f));CHECK(sd.active==1);CHECK(sd.peak==1);
 CHECK(f.print("A")==1);CHECK(t.finish()==Status::NotStarted);
 CHECK(t.begin()==Status::DrainRequired);CHECK(t.status()==Status::DrainRequired);
 CHECK(p.retiring());CHECK(arb.retiring());CHECK(p.verifyRetirementOwner());
 CHECK(t.begin()==Status::DrainRequired);CHECK(!p.open("/new","a"));
 CHECK(!p.exists("/new"));CHECK(sd.opens==1);CHECK(sd.existsCalls==0);
 CHECK(f.print("B")==0);CHECK(f.write((const uint8_t*)"x",1)==0);
 CHECK(f.size()==0);CHECK(f.read()==-1);CHECK(f.available()==0);
 CHECK(!f.seek(0));CHECK(!p.release());CHECK(t.finish()==Status::DrainRequired);
 f.flush();CHECK(sd.flushes==1);
 f.close();CHECK(!bool(f));CHECK(sd.closes==1);CHECK(sd.active==0);
 CHECK(t.observe()==Status::ReadyToRelease);CHECK(t.finish()==Status::Released);
 CHECK(t.finish()==Status::Released);CHECK(t.begin()==Status::Released);
 CHECK(arb.activeLane()==DTNF24Q::Lane::None);CHECK(!arb.retiring());
 CHECK(!arb.quarantined());CHECK(sd.closesAfterEpochLoss==0);
 CHECK(!p.open("/new","a"));CHECK(sd.opens==1);
}
static void f23dTwoSlotDrain(){
 Witness w{authorized()};Volume sd{&w};Arb arb(w);
 auto l=arb.acquire(2,DTNF24Q::Lane::F23D);CHECK(bool(l.serial));
 Port p(sd,arb,l);Coordinator t(p);
 auto append=p.open("/f23d_multihop.jsonl","a");CHECK(bool(append));
 auto verify=p.open("/f23d_multihop.jsonl","r");CHECK(bool(verify));
 CHECK(sd.active==2);CHECK(sd.peak==2);
 CHECK(t.begin()==Status::DrainRequired);CHECK(!p.open("/another","r"));CHECK(sd.opens==2);
 append.close();CHECK(bool(append));CHECK(sd.closes==0);
 verify.close();CHECK(!bool(verify));CHECK(sd.closes==1);
 CHECK(t.finish()==Status::DrainRequired);
 append.flush();CHECK(sd.flushes==1);
 append.close();CHECK(!bool(append));CHECK(sd.closes==2);
 CHECK(t.finish()==Status::Released);CHECK(sd.active==0);
}
static void historyChildDrain(){
 Witness w{authorized()};Volume sd{&w};Arb arb(w);
 auto l=arb.acquire(2,DTNF24Q::Lane::History);CHECK(bool(l.serial));
 Port p(sd,arb,l);Coordinator t(p);
 auto dir=p.open("/THESIS/F06TEST/V002","r");CHECK(bool(dir));CHECK(dir.isDirectory());
 auto child=dir.openNextFile();CHECK(bool(child));CHECK(sd.children==1);
 CHECK(t.begin()==Status::DrainRequired);CHECK(!dir.openNextFile());CHECK(sd.children==1);
 dir.close();CHECK(bool(dir));CHECK(sd.closes==0);CHECK(t.finish()==Status::DrainRequired);
 child.close();CHECK(!bool(child));dir.close();CHECK(!bool(dir));
 CHECK(sd.closes==2);CHECK(t.finish()==Status::Released);CHECK(sd.closesAfterEpochLoss==0);
}
static void stolenMediaWhileOpen(){
 Witness w{authorized()};Volume sd{&w};Arb arb(w);
 auto l=arb.acquire(2,DTNF24Q::Lane::History);Port p(sd,arb,l);Coordinator t(p);
 {
  auto parent=p.open("/THESIS/F06TEST/V002","r");
  auto child=parent.openNextFile();CHECK(bool(parent));CHECK(bool(child));CHECK(sd.active==2);
  CHECK(t.begin()==Status::DrainRequired);
  w.facts.mountEpoch++;
  CHECK(t.observe()==Status::UnsafeQuarantined);CHECK(arb.faulted());
  CHECK(arb.quarantined());CHECK(!p.verifyRetirementOwner());
  CHECK(!p.open("/untrusted","r"));CHECK(sd.opens==1);
  child.close();parent.close(); // F24R rejects both raw closes after revocation
  CHECK(sd.closes==0);
  CHECK(t.finish()==Status::UnsafeQuarantined);CHECK(!p.release());
  CHECK(sd.active==2);
 }
 // The synthetic RawFile destructor still invokes unsafe close; this is the
 // deliberately UNRESOLVED physical gate, not evidence of safe teardown.
 CHECK(sd.active==0);CHECK(sd.closesAfterEpochLoss==2);
 CHECK(arb.quarantined());CHECK(t.status()==Status::UnsafeQuarantined);
 CHECK(!arb.acquire(2,DTNF24Q::Lane::Sos).serial);
}
static void lossAfterDrainBeforeRelease(){
 Witness w{authorized()};Volume sd{&w};Arb arb(w);
 auto l=arb.acquire(2,DTNF24Q::Lane::Sos);Port p(sd,arb,l);Coordinator t(p);
 CHECK(t.begin()==Status::ReadyToRelease);CHECK(sd.opens==0);CHECK(!p.hasOpenHandle());
 w.facts.ownerSerial++;
 CHECK(t.finish()==Status::UnsafeQuarantined);CHECK(arb.faulted());
 CHECK(!arb.quarantined());CHECK(arb.activeLane()==DTNF24Q::Lane::Sos);
 CHECK(!p.finishRetirement());CHECK(!p.release());
}
static void badOwnerOrPolicy(){
 Witness w{authorized()};Volume sd{&w};Arb arb(w);
 auto l=arb.acquire(2,DTNF24Q::Lane::Sos);CHECK(l.serial==503);
 Port p(sd,arb,l);Coordinator t(p);
 w.facts.urgentPending=true;
 CHECK(t.begin()==Status::UnsafeQuarantined);CHECK(arb.faulted());
 CHECK(!p.retiring());CHECK(!p.verifyRetirementOwner());CHECK(!p.release());
 Witness w2{authorized()};Arb a2(w2);
 auto l2=a2.acquire(2,DTNF24Q::Lane::F23D);CHECK(bool(l2.serial));
 DTNF24Q::Lease wrong=l2;wrong.generation++;
 CHECK(!a2.beginRetirement(wrong));CHECK(a2.faulted());
 CHECK(!a2.release(l2));
}
int main(){
 noAuthority();appendDrain();f23dTwoSlotDrain();historyChildDrain();
 stolenMediaWhileOpen();lossAfterDrainBeforeRelease();badOwnerOrPolicy();
 std::printf("F24U owner-verified pre-revocation teardown PASS: %u host checks; unexpected revocation remains UNSAFE for physical File destruction\n",checks);
 return 0;
}
