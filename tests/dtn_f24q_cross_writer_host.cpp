#include <stdio.h>
#include <initializer_list>
#include <stdlib.h>
#include <string.h>
#include "../firmware/shared/storage/dtn_f24q_cross_writer_arbiter.h"
using namespace DTNF24Q;
static unsigned checks=0;
#define EXPECT(x) do{++checks;if(!(x)){fprintf(stderr,"F24Q FAIL line=%d expr=%s\n",__LINE__,#x);exit(2);}}while(0)
struct Witness { Facts facts{}; Facts view()const{return facts;} };
static Facts approved(){ Facts f{};f.vessel=2;f.mediaTag=10;f.mountEpoch=20;f.ownerSerial=30;
 f.independentlyAuthorized=f.soleOwner=f.allPathsInterposed=true;
 f.legacyCoordinated=f.f23dCoordinated=f.otherHandlesClosed=true;return f; }
static void lanes(){
 for(Lane lane:{Lane::Sos,Lane::LegacyAppend,Lane::Export,Lane::History,Lane::Audit,Lane::F23D}){
  Witness w{approved()}; Arbiter<Witness> a(w);auto lease=a.acquire(2,lane);
  EXPECT(lease.serial==30);EXPECT(lease.generation==1);EXPECT(a.activeLane()==lane);
  EXPECT(!a.acquire(2,Lane::Sos).serial);
  const bool writable=lane==Lane::Sos||lane==Lane::LegacyAppend||lane==Lane::F23D;
  const Mode initial=writable?Mode::Append:Mode::Read;
  EXPECT(a.open(lease,0,initial));EXPECT(a.hasOpenHandle());
  EXPECT(!a.open(lease,0,initial));
  EXPECT(a.step(lease,0,Action::Size));
  EXPECT(a.step(lease,0,writable?Action::Write:Action::Read));
  EXPECT(a.step(lease,0,writable?Action::Flush:Action::Seek));
  EXPECT(!a.step(lease,0,writable?Action::Read:Action::Write));
  EXPECT(!a.release(lease));
  EXPECT(a.close(lease,0));EXPECT(!a.hasOpenHandle());
  EXPECT(a.release(lease));EXPECT(a.activeLane()==Lane::None);
  EXPECT(!a.step(lease,0,Action::Size));
  EXPECT(!a.faulted());
 }
}
static void f23d(){
 Witness w{approved()};Arbiter<Witness> a(w);auto l=a.acquire(2,Lane::F23D);
 EXPECT(!a.open(l,1,Mode::Read)); // cannot verify before opening append
 EXPECT(a.open(l,0,Mode::Append));
 EXPECT(a.step(l,0,Action::Write));EXPECT(a.step(l,0,Action::Flush));
 EXPECT(a.open(l,1,Mode::Read));EXPECT(a.step(l,1,Action::Seek));
 EXPECT(a.step(l,1,Action::Read));
 EXPECT(!a.close(l,0)); // verification must retire first
 EXPECT(!a.acquire(2,Lane::Sos).serial);
 EXPECT(a.close(l,1));
 EXPECT(a.close(l,0));
 // End-of-run verification may reopen slot 1 after append handle closed.
 EXPECT(a.open(l,1,Mode::Read));EXPECT(a.step(l,1,Action::Size));
 EXPECT(a.step(l,1,Action::Read));EXPECT(!a.open(l,0,Mode::Append)); // this would be a dangerous reopen!
 EXPECT(a.close(l,1));EXPECT(a.release(l));
}
static void denial(){
 for(int i=0;i<13;++i){
  Witness w{approved()};
  switch(i){
   case 0:w.facts.vessel=1;break;
   case 1:w.facts.mediaTag=0;break;
   case 2:w.facts.mountEpoch=0;break;
   case 3:w.facts.ownerSerial=0;break;
   case 4:w.facts.independentlyAuthorized=false;break;
   case 5:w.facts.soleOwner=false;break;
   case 6:w.facts.allPathsInterposed=false;break;
   case 7:w.facts.legacyCoordinated=false;break;
   case 8:w.facts.f23dCoordinated=false;break;
   case 9:w.facts.otherHandlesClosed=false;break;
   case 10:w.facts.radioBusy=true;break;
   case 11:w.facts.urgentPending=true;break;
   case 12:w.facts.mediaTag=0;w.facts.otherHandlesClosed=false;break;
  }
  Arbiter<Witness> a(w);EXPECT(!a.acquire(2,Lane::Sos).serial);
  EXPECT(a.activeLane()==Lane::None);EXPECT(!a.hasOpenHandle());
 }
 const DenyWitness deny{};Arbiter<DenyWitness> always(deny);
 for(int i=0;i<10;++i){EXPECT(!always.acquire(2,Lane::F23D).serial);}
 EXPECT(!always.faulted());
 Witness w{approved()};Arbiter<Witness> a(w);
 EXPECT(!a.acquire(0,Lane::Sos).serial);
 EXPECT(!a.acquire(4,Lane::Sos).serial);
 EXPECT(!a.acquire(2,Lane::None).serial);
}
static void revocations(){
 for(int condition=0;condition<14;++condition){
  Witness w{approved()}; Arbiter<Witness> a(w);const auto l=a.acquire(2,Lane::F23D);
  EXPECT(a.open(l,0,Mode::Append));
  switch(condition){
   case 0:w.facts.vessel=1;break;
   case 1:w.facts.mediaTag=9;break;
   case 2:w.facts.mountEpoch=21;break;
   case 3:w.facts.ownerSerial=31;break;
   case 4:w.facts.independentlyAuthorized=false;break;
   case 5:w.facts.soleOwner=false;break;
   case 6:w.facts.allPathsInterposed=false;break;
   case 7:w.facts.legacyCoordinated=false;break;
   case 8:w.facts.f23dCoordinated=false;break;
   case 9:w.facts.otherHandlesClosed=false;break;
   case 10:w.facts.radioBusy=true;break;
   case 11:w.facts.urgentPending=true;break;
   case 12:w.facts.mountEpoch=0;break;
   case 13:w.facts.mediaTag=0;break;
  }
  EXPECT(!a.step(l,0,Action::Write));EXPECT(a.faulted());
  EXPECT(a.quarantined());EXPECT(!a.close(l,0));
  w.facts=approved();
  EXPECT(!a.close(l,0));EXPECT(!a.release(l));
  EXPECT(!a.acquire(2,Lane::Sos).serial);
 }
}
static void staleAndContention(){
 Witness w{approved()};Arbiter<Witness> a(w);
 auto l=a.acquire(2,Lane::Sos);EXPECT(l.serial!=0);
 auto bad=l; bad.generation++;EXPECT(!a.step(bad,0,Action::Read));
 EXPECT(a.faulted());EXPECT(!a.acquire(2,Lane::Sos).serial);
 // Fresh independent instance may obtain permission only from its live witness.
 Arbiter<Witness> b(w);auto v=b.acquire(2,Lane::History);EXPECT(v.serial!=0);
 EXPECT(b.open(v,0,Mode::Read));EXPECT(!b.open(v,1,Mode::Read));
 EXPECT(b.step(v,0,Action::Enumerate)); // history may enumerate directories
 EXPECT(b.close(v,0));EXPECT(b.release(v));
 auto again=b.acquire(2,Lane::LegacyAppend);EXPECT(again.generation==2);
 EXPECT(!b.open(again,0,Mode::Read));EXPECT(b.open(again,0,Mode::Append));
 EXPECT(!b.open(again,1,Mode::Read));
 EXPECT(b.close(again,0));EXPECT(b.release(again));
 EXPECT(!b.step(v,0,Action::Read));
}
int main(){lanes();f23d();denial();revocations();staleAndContention();
 printf("F24Q cross-writer lease model PASS: %u host checks; synthetic witness only, no real SD owner\n",checks);return 0;}
