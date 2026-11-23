// F24I synthetic witness and port tests; NO SD / firmware / network.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "dtn_f24i_sd_owner_bridge.h"
using namespace DTNF24I;
static unsigned checks=0;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"F24I FAIL line %d: %s\n",__LINE__,#x);return 1;}++checks;}while(false)
static SessionKey key(){
 SessionKey k{};k.uploader.bytes[0]=8;k.uploader.bytes[6]=0x40;k.uploader.bytes[8]=0x80;
 k.vessel=2;std::memcpy(k.session,"S58BAFE2",9);return k;
}
struct FakePort {
 Snapshot s{};
 bool allowAction=true, failAppend=false, failFlush=false;
 bool wrongQueue=false, wrongEpoch=false, changedOwner=false;
 bool changedSourceOtherAction=false, revokedAfterAction=false;
 unsigned quiesceCount=0,urgentCount=0,researchCount=0,flushCount=0,inspectCount=0,protectCount=0;
 uint64_t bytes=16;bool newline=true;
 Snapshot snapshot()const{return s;}
 bool quiesceProducers(){++quiesceCount;if(!allowAction)return false;
   s.producerQuiesced=true;if(changedOwner)++s.ownerLeaseSerial;
   if(changedSourceOtherAction)++s.sourceEpoch;
   return true;}
 bool append(bool urgent){if(!allowAction || failAppend)return false;
   if(urgent){++urgentCount;if(!wrongQueue)--s.urgentQueued;}
   else {++researchCount;if(!wrongQueue)--s.researchQueued;}
   if(!wrongEpoch)++s.sourceEpoch;
   if(changedOwner)++s.mediaTag;
   return true;}
 bool appendNextUrgent(){return append(true);}
 bool appendNextResearch(){return append(false);}
 bool flushAndCloseAll(){++flushCount;if(!allowAction || failFlush)return false;
   s.flushSucceeded=true;s.writerHandles=0;
   if(changedSourceOtherAction)++s.sourceEpoch;
   return true;}
 bool inspectSOSLastByte(uint64_t& n,bool& done){++inspectCount;
   if(!allowAction)return false;
   n=bytes;done=newline;
   if(changedOwner)++s.mountEpoch;
   return true;}
 bool protectClosedSource(){++protectCount;if(!allowAction)return false;
   s.writeProtected=true;if(changedSourceOtherAction)++s.sourceEpoch;
   return true;}
};
struct FakeObserver {
 FakePort* p=nullptr;
 Witness w{};
 bool revoked=false,staleEpoch=false;
 Witness view()const{
  Witness x=w;
  if(p && !staleEpoch)x.sourceEpoch=p->s.sourceEpoch;
  if(revoked)x.explicitPhysicalApproval=false;
  return x;
 }
};
static FakePort healthyPort(){
 FakePort p{};p.s.key=key();p.s.vessel=2;
 p.s.mediaTag=33;p.s.mountEpoch=4;p.s.sourceEpoch=9;
 p.s.ownerLeaseSerial=8;p.s.closeSerial=5;
 p.s.mounted=true;p.s.mediaVerified=true;p.s.soleMainLoopOwner=true;
 return p;
}
static FakeObserver healthyObserver(FakePort& p){
 FakeObserver o{};o.p=&p;
 o.w.key=p.s.key;o.w.vessel=p.s.vessel;
 o.w.mediaTag=p.s.mediaTag;o.w.mountEpoch=p.s.mountEpoch;
 o.w.sourceEpoch=p.s.sourceEpoch;
 o.w.ownerLeaseSerial=p.s.ownerLeaseSerial;o.w.closeSerial=p.s.closeSerial;
 o.w.explicitPhysicalApproval=true;o.w.mediaIdentityIndependentlyVerified=true;
 o.w.soleMainLoopSDOwner=true;o.w.allWritesInterposed=true;
 o.w.sourceWriteExclusionEnforceable=true;o.w.registeredRetirementPort=true;
 return o;
}
static bool finish(FakePort& p,GuardedPort<FakePort,FakeObserver>& b){
 DTNF24H::Controller controller{};
 if(controller.request(b,p.s.key)!=DTNF24H::State::Draining)return false;
 for(unsigned i=0;i<60 && controller.state()!=DTNF24H::State::RetiredCandidate &&
   controller.state()!=DTNF24H::State::Failed;++i)controller.poll(b);
 return controller.state()==DTNF24H::State::RetiredCandidate &&
   controller.stillCandidate(b.snapshot()) &&
   controller.candidate().facts.fileBytes==p.bytes;
}
int main(){
 const SessionKey k=key();
 CHECK(DTNF24H::validKey(k));
 CHECK(!matches(Witness{},Snapshot{},k));
 {NoPhysicalPort p{};p.observedUrgent=3;p.observedResearch=4;
  NoPhysicalObserver o{};GuardedPort<NoPhysicalPort,NoPhysicalObserver> bridge(p,o);
  CHECK(p.snapshot().urgentQueued==3);CHECK(p.snapshot().researchQueued==4);
  CHECK(bridge.status()==Status::Unbound);CHECK(!bridge.arm(k));
  CHECK(bridge.revoked());CHECK(bridge.snapshot().mounted==false);
  CHECK(!bridge.arm(k));CHECK(!bridge.quiesceProducers());
  CHECK(!bridge.appendNextUrgent());CHECK(!bridge.appendNextResearch());
  CHECK(!bridge.flushAndCloseAll());uint64_t n=7;bool l=true;
  CHECK(!bridge.inspectSOSLastByte(n,l));CHECK(n==0 && !l);
  CHECK(!bridge.protectClosedSource());}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> bridge(p,o);
  CHECK(bridge.arm(k));CHECK(bridge.status()==Status::Armed);
  CHECK(!bridge.arm(k));CHECK(bridge.status()==Status::Armed);
  CHECK(bridge.snapshot().mediaTag==33);
  p.s.urgentQueued=2;p.s.researchQueued=2;
  CHECK(finish(p,bridge));CHECK(bridge.status()==Status::Armed);
  CHECK(p.quiesceCount==1);CHECK(p.urgentCount==2);
  CHECK(p.researchCount==2);CHECK(p.flushCount==1);
  CHECK(p.inspectCount==1);CHECK(p.protectCount==1);
  CHECK(p.s.writeProtected);CHECK(p.s.sourceEpoch==13);
  CHECK(!bridge.appendNextUrgent());CHECK(bridge.revoked());}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> bridge(p,o);CHECK(bridge.arm(k));
  p.s.urgentQueued=1;CHECK(!bridge.appendNextUrgent());CHECK(p.urgentCount==0);
  CHECK(bridge.revoked());}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> bridge(p,o);CHECK(bridge.arm(k));
  CHECK(bridge.quiesceProducers());CHECK(!bridge.appendNextResearch());
  CHECK(p.researchCount==0);CHECK(bridge.revoked());}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> bridge(p,o);CHECK(bridge.arm(k));
  CHECK(bridge.quiesceProducers());p.s.urgentQueued=1;p.s.researchQueued=1;
  CHECK(!bridge.appendNextResearch());CHECK(p.researchCount==0);}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> bridge(p,o);CHECK(bridge.arm(k));
  CHECK(bridge.quiesceProducers());p.s.urgentQueued=1;
  CHECK(!bridge.flushAndCloseAll());CHECK(p.flushCount==0);}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> bridge(p,o);CHECK(bridge.arm(k));
  CHECK(bridge.quiesceProducers());
  uint64_t n=123;bool l=true;CHECK(!bridge.inspectSOSLastByte(n,l));
  CHECK(n==0 && !l);CHECK(p.inspectCount==0);}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> bridge(p,o);CHECK(bridge.arm(k));
  CHECK(bridge.quiesceProducers());CHECK(!bridge.protectClosedSource());
  CHECK(p.protectCount==0);}
 // Every missing independent authority component MUST deny initial bind.
 for(unsigned selector=0;selector<17;++selector){
  FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  switch(selector){
   case 0:o.w.explicitPhysicalApproval=false;break;
   case 1:o.w.mediaIdentityIndependentlyVerified=false;break;
   case 2:o.w.soleMainLoopSDOwner=false;break;
   case 3:o.w.allWritesInterposed=false;break;
   case 4:o.w.sourceWriteExclusionEnforceable=false;break;
   case 5:o.w.registeredRetirementPort=false;break;
   case 6:o.w.mediaTag=0;break;
   case 7:o.w.mountEpoch=0;break;
   case 8:o.w.sourceEpoch=0;o.staleEpoch=true;break;
   case 9:o.w.ownerLeaseSerial=0;break;
   case 10:o.w.closeSerial=0;break;
   case 11:o.w.activeSOS=true;break;
   case 12:o.w.urgentOrRadio=true;break;
   case 13:o.w.concurrentExport=true;break;
   case 14:o.w.concurrentHistory=true;break;
   case 15:o.w.vessel=3;break;
   case 16:o.w.key.session[3]='1';break;
  }
  GuardedPort<FakePort,FakeObserver> b(p,o);
  CHECK(!b.arm(k));CHECK(b.revoked());CHECK(p.quiesceCount==0);
 }
 // Any mismatch in the Port snapshot also denies; no physical operations.
 for(unsigned selector=0;selector<19;++selector){
  FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  switch(selector){
   case 0:p.s.mediaTag=0;break;
   case 1:p.s.mountEpoch=0;break;
   case 2:p.s.sourceEpoch=0;break;
   case 3:p.s.ownerLeaseSerial=0;break;
   case 4:p.s.closeSerial=0;break;
   case 5:p.s.mounted=false;break;
   case 6:p.s.mediaVerified=false;break;
   case 7:p.s.soleMainLoopOwner=false;break;
   case 8:p.s.activeSOS=true;break;
   case 9:p.s.urgentOrRadio=true;break;
   case 10:p.s.exportActive=true;break;
   case 11:p.s.historyActive=true;break;
   case 12:p.s.writeInFlight=true;break;
   case 13:p.s.urgentQueued=25;break;
   case 14:p.s.researchQueued=25;break;
   case 15:p.s.vessel=3;break;
   case 16:p.s.key.vessel=3;break;
   case 17:p.s.sourceEpoch++;o.staleEpoch=true;break;
   case 18:p.s.key.session[1]='A';break;
  }
  GuardedPort<FakePort,FakeObserver> b(p,o);
  CHECK(!b.arm(k));CHECK(b.revoked());CHECK(p.flushCount==0);
 }
 // Revocations during service must latch, even though the first bind passed.
 for(unsigned selector=0;selector<13;++selector){
  FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  switch(selector){
   case 0:o.revoked=true;break;
   case 1:p.s.activeSOS=true;break;
   case 2:p.s.urgentOrRadio=true;break;
   case 3:p.s.exportActive=true;break;
   case 4:p.s.historyActive=true;break;
   case 5:p.s.mediaTag++;break;
   case 6:p.s.mountEpoch++;break;
   case 7:p.s.ownerLeaseSerial++;break;
   case 8:p.s.sourceEpoch++;break;
   case 9:p.s.closeSerial++;break;
   case 10:o.w.soleMainLoopSDOwner=false;break;
   case 11:o.w.allWritesInterposed=false;break;
   case 12:p.s.writeInFlight=true;break;
  }
  CHECK(!b.quiesceProducers());CHECK(b.revoked());CHECK(p.quiesceCount==0);
 }
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  p.changedOwner=true;CHECK(!b.quiesceProducers());CHECK(b.revoked());
  CHECK(p.quiesceCount==1);CHECK(!b.flushAndCloseAll());}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  p.changedSourceOtherAction=true;CHECK(!b.quiesceProducers());
  CHECK(b.revoked());}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  p.allowAction=false;CHECK(!b.quiesceProducers());CHECK(b.revoked());}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  CHECK(b.quiesceProducers());p.s.urgentQueued=1;p.wrongQueue=true;
  CHECK(!b.appendNextUrgent());CHECK(b.revoked());}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  CHECK(b.quiesceProducers());p.s.urgentQueued=1;p.wrongEpoch=true;
  CHECK(!b.appendNextUrgent());CHECK(b.revoked());}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  CHECK(b.quiesceProducers());p.s.urgentQueued=1;p.failAppend=true;
  CHECK(!b.appendNextUrgent());CHECK(b.revoked());CHECK(p.s.urgentQueued==1);}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  CHECK(b.quiesceProducers());p.failFlush=true;
  CHECK(!b.flushAndCloseAll());CHECK(b.revoked());}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  CHECK(b.quiesceProducers());CHECK(b.flushAndCloseAll());
  p.bytes=DTNF06ClosedReplay::kMaxJournalBytes+1;
  uint64_t n=123;bool l=true;
  CHECK(!b.inspectSOSLastByte(n,l));CHECK(b.revoked());
  CHECK(n==0 && !l);}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  CHECK(b.quiesceProducers());CHECK(b.flushAndCloseAll());
  p.changedOwner=true;uint64_t n=123;bool l=true;
  CHECK(!b.inspectSOSLastByte(n,l));CHECK(b.revoked());
  CHECK(n==0 && !l);}
 {FakePort p=healthyPort();FakeObserver o=healthyObserver(p);
  GuardedPort<FakePort,FakeObserver> b(p,o);CHECK(b.arm(k));
  CHECK(b.quiesceProducers());CHECK(b.flushAndCloseAll());
  p.changedSourceOtherAction=true;CHECK(!b.protectClosedSource());
  CHECK(b.revoked());}
 std::printf("F24I SD-owner bridge: PASS (%u checks; SYNTHETIC WITNESS / PORT ONLY)\n",checks);
 return 0;
}
