// F24V actual F24R guard + F24Q arbiter: backend close BEFORE slot release.
// Host synthetic witness only, with explicit negative tests for sudden loss.
#define THESIS_DTN_F24V_BUILD_ONLY 1
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <cstring>
#include <type_traits>
#include "../firmware/shared/storage/dtn_f24r_guarded_file_port.h"
#include "../firmware/shared/storage/dtn_f24v_core_close_policy.h"
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"F24V close gate FAIL %d: %s\n",__LINE__,#x);std::exit(31);}}while(0)
using namespace DTNF24Q;
struct Witness {Facts facts{};Facts view()const{return facts;}};
static Facts valid(){Facts x{};x.vessel=2;x.mediaTag=2001;x.mountEpoch=3001;x.ownerSerial=4001;
 x.independentlyAuthorized=x.soleOwner=x.allPathsInterposed=true;
 x.legacyCoordinated=x.f23dCoordinated=x.otherHandlesClosed=true;return x;}
struct Volume;
struct Raw {
 Volume* v=nullptr;
 Raw()=default;explicit Raw(Volume* p);
 Raw(const Raw&)=delete;Raw& operator=(const Raw&)=delete;
 Raw(Raw&& x)noexcept:v(x.v){x.v=nullptr;}
 Raw& operator=(Raw&& x)noexcept;
 ~Raw(){close();}
 explicit operator bool()const{return v!=nullptr;}
 size_t size()const{return 4;}
 void close();
};
struct Volume {
 Witness* witness=nullptr;unsigned backendOpens=0,backendCloses=0,postLossCloses=0;
 bool flipDuringClose=false,closeFailed=false;
 bool exists(const char*)const{return true;}
 Raw open(const char*,const char*){++backendOpens;return Raw(this);}
};
Raw::Raw(Volume* p):v(p){}
Raw& Raw::operator=(Raw&& x)noexcept{if(this!=&x){close();v=x.v;x.v=nullptr;}return *this;}
void Raw::close(){if(v){++v->backendCloses;
 if(v->flipDuringClose)v->witness->facts.mediaTag=999;
 if(!suitable(v->witness->facts,2) || v->witness->facts.mediaTag!=2001 || v->witness->facts.mountEpoch!=3001)++v->postLossCloses;
 v=nullptr;}}
using Arb=Arbiter<Witness>;
using Port=DTNF24R::GuardedVolume<Volume,Witness>;
static_assert(!std::is_copy_constructible<Port::FileHandle>::value,"guard must prohibit alias by value");
static_assert(!DTNF24V::kPhysicalOwnerGranted,"hardware permission must be absent");
static_assert(!DTNF24V::kRawFileDestructionCertified,"raw destructor still unsafe");
static void normal(){
 Witness w{valid()};Volume d{};d.witness=&w;Arb arb(w);auto lease=arb.acquire(2,Lane::History);
 C(lease.serial==4001);Port p(d,arb,lease);auto h=p.open("/sessions","r");
 C(bool(h));C(arb.hasOpenHandle());C(d.backendOpens==1);C(d.backendCloses==0);
 C(arb.permitClose(lease,0));C(arb.hasHandle(0));C(!p.faulted());
 h.close();C(d.backendCloses==1);C(!arb.hasHandle(0));C(!p.faulted());C(!bool(h));
 C(p.release());C(arb.activeLane()==Lane::None);C(d.postLossCloses==0);
}
static void duringClose(){
 Witness w{valid()};Volume d{};d.witness=&w;d.flipDuringClose=true;
 Arb arb(w);auto lease=arb.acquire(2,Lane::History);Port p(d,arb,lease);
 auto h=p.open("/sessions","r");C(bool(h));C(arb.hasHandle(0));C(d.backendCloses==0);
 h.close();C(d.backendCloses==1);C(d.postLossCloses==1);
 C(p.faulted());C(p.quarantined());C(p.hasOpenHandle());
 C(!p.release());C(!p.finishRetirement());C(!p.beginRetirement());
 // backend already closed: no second raw call even if guard cannot finalize.
 h.close();C(d.backendCloses==1);C(arb.hasHandle(0));
}
static void unexpectedLossDestructor(){
 Witness w{valid()};Volume d{};d.witness=&w;
 Arb arb(w);auto lease=arb.acquire(2,Lane::History);Port p(d,arb,lease);
 {auto h=p.open("/sessions","r");C(bool(h));C(d.backendCloses==0);
  w.facts.mountEpoch=2;C(!bool(h));C(p.faulted());C(p.quarantined());
  // h cannot legitimately close but raw's destructor still invokes close.
 }
 C(d.backendCloses==1);C(d.postLossCloses==1);
 C(arb.hasOpenHandle());C(!p.release());C(p.faulted());
}
static void cleanRetirement(){
 Witness w{valid()};Volume d{};d.witness=&w;Arb arb(w);
 auto lease=arb.acquire(2,Lane::Sos);Port p(d,arb,lease);
 auto h=p.open("/sos","a");C(bool(h));C(d.backendCloses==0);
 C(p.beginRetirement());C(p.retiring());C(!p.finishRetirement());
 C(!bool(p.open("/sos2","a")));C(d.backendOpens==1);
 C(arb.permitClose(lease,0));h.close();C(d.backendCloses==1);
 C(!arb.hasOpenHandle());C(p.finishRetirement());C(arb.activeLane()==Lane::None);
 C(d.postLossCloses==0);
}
int main(){
 normal();duringClose();unexpectedLossDestructor();cleanRetirement();
 std::printf("F24V guard backend-close-before-slot-release PASS: %u checks; unexpected loss destructor UNSAFE\n",checks);
 return 0;
}
