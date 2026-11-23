// F24J pure C++ TESTS: fake cards only. NO SD / Arduino / RF / network.
#include <cstdio>
#include <cstring>
#include "dtn_f24j_owner_coordinator.h"
using namespace DTNF24J;
static unsigned checks=0U;
#define CHECK(expr) do{if(!(expr)){std::fprintf(stderr,"F24J FAIL line %d: %s\n",__LINE__,#expr);return 1;}++checks;}while(false)
static SessionKey key(){
  SessionKey k{};k.uploader.bytes[0]=8;k.uploader.bytes[6]=0x40;
  k.uploader.bytes[8]=0x80;k.vessel=2;
  std::memcpy(k.session,"S58BAFE2",9);return k;
}
struct FakePort {
  Snapshot s{};
  bool allow=true,brokenAppend=false,brokenQueue=false,brokenFlush=false;
  bool brokenInspect=false,brokenProtect=false,extraEpoch=false,swapAfter=false;
  unsigned quiesce=0,urgent=0,research=0,flush=0,inspect=0,protect=0;
  uint64_t bytes=34;
  Snapshot snapshot()const{return s;}
  bool quiesceProducers(){++quiesce;if(!allow)return false;
    s.producerQuiesced=true;if(extraEpoch)++s.sourceEpoch;return true;}
  bool append(bool u){if(!allow || brokenAppend)return false;
    if(u){++urgent;if(!brokenQueue)--s.urgentQueued;}
    else{++research;if(!brokenQueue)--s.researchQueued;}
    if(!extraEpoch)++s.sourceEpoch;
    else s.sourceEpoch+=2;
    if(swapAfter)++s.mountEpoch;
    return true;}
  bool appendNextUrgent(){return append(true);}
  bool appendNextResearch(){return append(false);}
  bool flushAndCloseAll(){++flush;if(!allow || brokenFlush)return false;
    s.flushSucceeded=true;s.writerHandles=0;
    if(extraEpoch)++s.sourceEpoch;
    return true;}
  bool inspectSOSLastByte(uint64_t& n,bool& nl){++inspect;
    if(!allow || brokenInspect)return false;
    n=bytes;nl=true;if(swapAfter)++s.mediaTag;return true;}
  bool protectClosedSource(){++protect;
    if(!allow || brokenProtect)return false;
    s.writeProtected=true;if(extraEpoch)++s.sourceEpoch;return true;}
};
struct FakeObserver {
  const FakePort* p=nullptr;
  Witness w{};
  bool revoked=false,stale=false;
  Witness view()const{
    Witness copy=w;
    if(p && !stale)copy.sourceEpoch=p->s.sourceEpoch;
    if(revoked)copy.explicitPhysicalApproval=false;
    return copy;
  }
};
static FakePort healthy(){
  FakePort p{};p.s.key=key();p.s.vessel=2;p.s.mediaTag=111;
  p.s.mountEpoch=22;p.s.sourceEpoch=31;
  p.s.ownerLeaseSerial=41;p.s.closeSerial=51;
  p.s.mounted=true;p.s.mediaVerified=true;p.s.soleMainLoopOwner=true;
  return p;
}
static FakeObserver observer(const FakePort& p){
  FakeObserver o{};o.p=&p;o.w.key=p.s.key;
  o.w.vessel=p.s.vessel;o.w.mediaTag=p.s.mediaTag;
  o.w.mountEpoch=p.s.mountEpoch;o.w.sourceEpoch=p.s.sourceEpoch;
  o.w.ownerLeaseSerial=p.s.ownerLeaseSerial;o.w.closeSerial=p.s.closeSerial;
  o.w.explicitPhysicalApproval=true;
  o.w.mediaIdentityIndependentlyVerified=true;
  o.w.soleMainLoopSDOwner=true;
  o.w.allWritesInterposed=true;
  o.w.sourceWriteExclusionEnforceable=true;
  o.w.registeredRetirementPort=true;
  return o;
}
static bool arm(FakePort& p,FakeObserver& o,
  DTNF24I::GuardedPort<FakePort,FakeObserver>& bridge,
  Coordinator<DTNF24I::GuardedPort<FakePort,FakeObserver>,FakeObserver>& a){
  (void)o;return bridge.arm(p.s.key) && a.bind(p.s.key);
}
int main(){
  {DTNF24I::NoPhysicalPort p{};DTNF24I::NoPhysicalObserver o{};
   DTNF24I::GuardedPort<DTNF24I::NoPhysicalPort,DTNF24I::NoPhysicalObserver> b(p,o);
   Coordinator<decltype(b),DTNF24I::NoPhysicalObserver> a(b,o);
   CHECK(a.state()==State::Unbound);CHECK(!b.arm(key()));
   CHECK(!a.bind(key()));CHECK(a.revoked());CHECK(!a.bind(key()));
   CHECK(!a.snapshot().mounted);CHECK(!a.quiesceProducers());
   CHECK(!a.appendNextUrgent());CHECK(!a.appendNextResearch());
   CHECK(!a.flushAndCloseAll());uint64_t n=90;bool nl=true;
   CHECK(!a.inspectSOSLastByte(n,nl));CHECK(n==0 && !nl);
   CHECK(!a.protectClosedSource());}
  // Successful fully synthetic F24I -> F24J -> F24H integration.
  {FakePort p=healthy();FakeObserver o=observer(p);
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));CHECK(a.state()==State::Operational);
   CHECK(a.snapshot().sourceEpoch==31);
   unsigned reads=0;
   CHECK(a.run(Lane::History,[&]{++reads;return true;}));
   CHECK(a.run(Lane::Export,[&]{++reads;return true;}));
   CHECK(reads==2);
   p.s.urgentQueued=2;p.s.researchQueued=3;
   DTNF24H::Controller controller{};
   CHECK(controller.request(a,p.s.key)==DTNF24H::State::Draining);
   CHECK(a.state()==State::Retiring);
   CHECK(!a.run(Lane::Hash,[&]{++reads;return true;}));
   CHECK(a.revoked());
   CHECK(reads==2);
  }
  {FakePort p=healthy();FakeObserver o=observer(p);
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));
   p.s.urgentQueued=2;p.s.researchQueued=3;
   DTNF24H::Controller c{};
   CHECK(c.request(a,p.s.key)==DTNF24H::State::Draining);
   for(unsigned i=0;i<20 && c.state()!=DTNF24H::State::RetiredCandidate &&
       c.state()!=DTNF24H::State::Failed;++i)c.poll(a);
   CHECK(c.state()==DTNF24H::State::RetiredCandidate);
   CHECK(a.state()==State::Retired);
   CHECK(c.stillCandidate(a.snapshot()));
   CHECK(c.candidate().eligible);
   CHECK(p.quiesce==1);CHECK(p.urgent==2);CHECK(p.research==3);
   CHECK(p.flush==1);CHECK(p.inspect==1);CHECK(p.protect==1);
   CHECK(p.s.sourceEpoch==36);CHECK(p.s.writeProtected);
   unsigned hashes=0,seals=0;
   CHECK(a.run(Lane::Hash,[&]{++hashes;return true;}));
   CHECK(a.run(Lane::Seal,[&]{++seals;return true;}));
   CHECK(hashes==1 && seals==1);
   CHECK(a.snapshot().mounted);CHECK(!a.bind(key()));
   CHECK(a.state()==State::Retired);
   CHECK(!a.appendNextUrgent());CHECK(a.revoked());}
  // A reader cannot ignore a queued SOS or a radio preemption.
  for(unsigned mode=0;mode<5;++mode){
   FakePort p=healthy();FakeObserver o=observer(p);
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));
   if(mode==0)p.s.urgentQueued=1;
   if(mode==1)p.s.researchQueued=1;
   if(mode==2)p.s.activeSOS=true;
   if(mode==3)p.s.urgentOrRadio=true;
   if(mode==4)p.s.historyActive=true;
   unsigned actions=0;
   CHECK(!a.run(Lane::Export,[&]{++actions;return true;}));
   CHECK(actions==0);CHECK(a.revoked());}
  // Bind rejects fabricated, incomplete or changing witness/snapshot data.
  for(unsigned mode=0;mode<14;++mode){
   FakePort p=healthy();FakeObserver o=observer(p);
   switch(mode){
    case 0:o.w.explicitPhysicalApproval=false;break;
    case 1:o.w.mediaIdentityIndependentlyVerified=false;break;
    case 2:o.w.allWritesInterposed=false;break;
    case 3:o.w.soleMainLoopSDOwner=false;break;
    case 4:o.w.sourceWriteExclusionEnforceable=false;break;
    case 5:o.w.registeredRetirementPort=false;break;
    case 6:o.w.mediaTag++;break;
    case 7:o.w.mountEpoch++;break;
    case 8:o.w.closeSerial++;break;
    case 9:o.w.ownerLeaseSerial++;break;
    case 10:p.s.mounted=false;break;
    case 11:p.s.writeProtected=true;break;
    case 12:p.s.producerQuiesced=true;break;
    case 13:p.s.flushSucceeded=true;break;
   }
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   const bool bridgeArmed=b.arm(key());
   CHECK(!a.bind(key()));CHECK(a.revoked());
   CHECK(!a.snapshot().mounted);
   if(!bridgeArmed)CHECK(b.revoked());
  }
  // Same-size mutation still increments source epoch (or detection fails
  // closed when the epoch was changed without a registered append).
  for(unsigned mode=0;mode<11;++mode){
   FakePort p=healthy();FakeObserver o=observer(p);
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));
   switch(mode){
     case 0:p.s.mediaTag++;break;
     case 1:p.s.mountEpoch++;break;
     case 2:p.s.ownerLeaseSerial++;break;
     case 3:p.s.closeSerial++;break;
     case 4:p.s.sourceEpoch++;break;
     case 5:o.revoked=true;break;
     case 6:p.s.writeInFlight=true;break;
     case 7:p.s.activeSOS=true;break;
     case 8:p.s.exportActive=true;break;
     case 9:o.w.registeredRetirementPort=false;break;
     case 10:o.w.soleMainLoopSDOwner=false;break;
   }
   unsigned calls=0;
   CHECK(!a.run(Lane::History,[&]{++calls;return true;}));
   CHECK(calls==0);CHECK(a.revoked());}
  // Owner loss, concurrent begin (reentrant), silent mutation, read failure.
  {FakePort p=healthy();FakeObserver o=observer(p);
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));unsigned calls=0;
   CHECK(!a.run(Lane::History,[&]{++calls;++p.s.sourceEpoch;return true;}));
   CHECK(calls==1);CHECK(a.revoked());}
  {FakePort p=healthy();FakeObserver o=observer(p);
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));unsigned calls=0;
   CHECK(!a.run(Lane::Export,[&]{++calls;return false;}));
   CHECK(calls==1);CHECK(a.revoked());}
  {FakePort p=healthy();FakeObserver o=observer(p);
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));unsigned calls=0;
   CHECK(!a.run(Lane::History,[&]{++calls;
     const bool inner=a.run(Lane::Export,[&]{++calls;return true;});
     return !inner;
   }));
   CHECK(calls==1);CHECK(a.revoked());}
  // Failed write, stale queue bookkeeping, source epoch jump, media swap.
  for(unsigned mode=0;mode<5;++mode){
   FakePort p=healthy();FakeObserver o=observer(p);
   if(mode==0)p.brokenAppend=true;
   if(mode==1)p.brokenQueue=true;
   if(mode==2)p.extraEpoch=true;
   if(mode==3)p.swapAfter=true;
   if(mode==4)p.allow=false;
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));p.s.urgentQueued=1;
   DTNF24H::Controller c{};
   if(mode==2){ // Extra epoch during quiesce is forbidden.
     CHECK(c.request(a,p.s.key)==DTNF24H::State::Failed);
   }else if(mode==4){
     CHECK(c.request(a,p.s.key)==DTNF24H::State::Failed);
   }else{
     CHECK(c.request(a,p.s.key)==DTNF24H::State::Draining);
     CHECK(c.poll(a)==DTNF24H::State::Failed);
   }
   CHECK(a.revoked());CHECK(!c.candidate().eligible);}
  {FakePort p=healthy();FakeObserver o=observer(p);
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));DTNF24H::Controller c{};
   CHECK(c.request(a,p.s.key)==DTNF24H::State::Draining);
   p.brokenFlush=true;CHECK(c.poll(a)==DTNF24H::State::Closing);
   CHECK(c.poll(a)==DTNF24H::State::Failed);
   CHECK(a.revoked());}
  {FakePort p=healthy();FakeObserver o=observer(p);
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));DTNF24H::Controller c{};
   CHECK(c.request(a,p.s.key)==DTNF24H::State::Draining);
   CHECK(c.poll(a)==DTNF24H::State::Closing);
   CHECK(c.poll(a)==DTNF24H::State::Checking);
   p.brokenInspect=true;CHECK(c.poll(a)==DTNF24H::State::Failed);
   CHECK(a.revoked());}
  // Fresh instance is unbound after a simulated reboot: no owner remembered.
  {FakePort p=healthy();FakeObserver o=observer(p);
   DTNF24I::GuardedPort<FakePort,FakeObserver> b(p,o);
   Coordinator<decltype(b),FakeObserver> a(b,o);
   CHECK(arm(p,o,b,a));
   DTNF24I::GuardedPort<FakePort,FakeObserver> afterReboot(p,o);
   Coordinator<decltype(afterReboot),FakeObserver> fresh(afterReboot,o);
   CHECK(fresh.state()==State::Unbound);
   CHECK(!fresh.snapshot().mounted);
   CHECK(!fresh.run(Lane::History,[]{return true;}));
   CHECK(fresh.revoked());}
  std::printf("F24J owner coordinator: PASS (%u checks; FAKE OWNER/PORT ONLY)\n",checks);
  return 0;
}
