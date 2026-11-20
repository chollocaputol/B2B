#include <cassert>
#include <cstdio>
#include "firmware/shared/relay/f23d_physical_permission.h"
using namespace F23DPhysical;
static unsigned checks=0;
#define CHECK(x) do{++checks;assert(x);}while(0)
static Evidence evidence;
static Evidence readEvidence(uint32_t now,uint32_t run,uint32_t event){
 Evidence e=evidence;if(e.run==UINT32_MAX)e.run=run;if(e.event==UINT32_MAX)e.event=event;
 e.sampledAt=now;return e;
}
static F23DBench::Context base(){F23DBench::Context c;c.identityReady=true;c.self=2;c.recoverySafe=true;return c;}
static void good(){evidence={};evidence.boardBound=true;evidence.operatorAuthorized=true;evidence.timingApproved=true;evidence.run=UINT32_MAX;evidence.event=UINT32_MAX;evidence.callEntryBoundMs=10;evidence.radioToRxBoundMs=1000;}
struct Rig{
 Permission p{readEvidence};F23DBench::RunControl c;uint32_t now=0;int calls=0,result=0,restore=0;bool advance=false,lose=false;
 F23DBench::Context context(){auto x=base();x.physicalPermission=p.evaluate(x,now);return x;}
 void arm(){auto x=context();CHECK(c.command("R23D SELECT 7 42",x,now,99)==F23DBench::Control::Selected);p.selected(7,42,now);x=context();CHECK(c.command("R23D ARM 99",x,now,0)==F23DBench::Control::Armed);p.armed(now);}
 int submit(const VesselPacket& v){
  CHECK(c.attempts()>0);if(advance)now=15000;
  auto x=context();if(!c.maySubmit(v,x) || !p.beforeRadio(x,now,lose?1:0)){c.revoke();return -997;}
  ++calls;if(result || restore){p.revoke();c.revoke();}return result;
 }
 bool dispatch(){int rc=0;return c.dispatch(context(),now,*this,rc);}
};
static VesselPacket packet(bool cancel=false){VesselPacket v{};v.magic=PACKET_MAGIC;v.version=PACKET_VERSION;v.originNodeId=1;v.sequenceNumber=42;v.packetType=cancel?PACKET_SOS_CANCEL:PACKET_SOS;if(cancel){uint32_t e=42,o=1;memcpy(v.message,&e,4);memcpy(v.message+4,&o,4);}return v;}
int main(){
 static_assert(sizeof(VesselPacket)==62,"control layout");
 auto b=base();Permission missing;missing.selected(7,42,0);CHECK(!missing.evaluate(b,0));CHECK(missing.reason()==Reason::Binding);
 Permission start{readEvidence};CHECK(!start.beforeRadio(b,0,0));CHECK(start.terminal());
 for(int bad=0;bad<12;++bad){good();Permission p{readEvidence};p.selected(7,42,0);b=base();
  switch(bad){case 0:b.identityReady=false;break;case 1:b.self=3;break;case 2:b.recoverySafe=false;break;
   case 3:evidence.boardBound=false;break;case 4:evidence.operatorAuthorized=false;break;case 5:evidence.run=8;break;
   case 6:evidence.event=43;break;case 7:evidence.timingApproved=false;break;case 8:evidence.callEntryBoundMs=0;break;
   case 9:evidence.radioToRxBoundMs=0;break;case 10:evidence.radioToRxBoundMs=UINT32_MAX;break;case 11:evidence.callEntryBoundMs=14000;break;}
  CHECK(!p.evaluate(b,0));CHECK(p.terminal());good();CHECK(!p.evaluate(base(),0));
 }
 good();{Rig r;r.arm();auto s=packet(),cancel=packet(true);CHECK(r.c.observe(s,r.context(),true,0)==F23CLab::Admission::Accepted);
 CHECK(r.c.observe(s,r.context(),true,0)==F23CLab::Admission::Duplicate);r.now=250;CHECK(r.dispatch());CHECK(r.calls==1 && r.c.attempts()==1);
 CHECK(r.c.observe(cancel,r.context(),true,r.now)==F23CLab::Admission::Accepted);r.now=1250;CHECK(r.dispatch());CHECK(r.calls==2 && r.c.attempts()==2);
 CHECK(r.c.observe(cancel,r.context(),true,r.now)==F23CLab::Admission::Duplicate);
 CHECK(!r.dispatch());CHECK(r.c.command("R23D ARM 99",r.context(),r.now,0)==F23DBench::Control::Locked);}
 good();{Rig r;r.arm();auto s=packet(),cancel=packet(true);CHECK(r.c.observe(cancel,r.context(),true,0)==F23CLab::Admission::UnknownCancel);
 CHECK(r.c.observe(s,r.context(),true,0)==F23CLab::Admission::Accepted);CHECK(r.c.observe(cancel,r.context(),true,0)==F23CLab::Admission::Accepted);
 r.now=250;CHECK(r.dispatch());CHECK(r.calls==1 && r.c.attempts()==1);r.now=1250;CHECK(!r.dispatch());}
 for(int fault=0;fault<4;++fault){good();Rig r;r.arm();CHECK(r.c.observe(packet(),r.context(),true,0)==F23CLab::Admission::Accepted);
 r.now=250;r.advance=fault==0;r.result=fault==1?-7:0;r.restore=fault==2?-8:0;r.lose=fault==3;
 CHECK(r.dispatch());CHECK(r.c.attempts()==1);CHECK(r.calls==(fault==0 || fault==3?0:1));CHECK(!r.c.armed());CHECK(!r.dispatch());}
 good();{Rig r;auto x=r.context();CHECK(r.c.command("R23D SELECT 7 42",x,0,99)==F23DBench::Control::Selected);r.p.selected(7,42,0);
 CHECK(r.c.command("R23D ARM 98",r.context(),0,0)==F23DBench::Control::Denied);CHECK(!r.c.armed());}
 good();{Rig r;r.arm();auto s=packet();s.sequenceNumber=43;CHECK(r.c.observe(s,r.context(),true,0)==F23CLab::Admission::WrongEvent);CHECK(r.c.attempts()==0);}
 good();{Rig r;r.arm();r.now=13990;CHECK(!r.p.beforeRadio(base(),r.now,0));CHECK(r.p.reason()==Reason::InsufficientTime);}
 good();{Permission p{readEvidence};p.selected(7,42,UINT32_MAX-100);p.armed(UINT32_MAX-100);CHECK(p.evaluate(base(),149));CHECK(!p.evaluate(base(),14900));}
 good();{Rig r;r.arm();Rig reboot;CHECK(!reboot.c.armed());CHECK(!reboot.p.beforeRadio(base(),0,0));}
 good();{Rig r;r.arm();auto busy=base();busy.localControlBusy=true;CHECK(!r.p.beforeRadio(busy,0,0));CHECK(r.p.reason()==Reason::LocalBusy);}
 good();{Rig r;r.arm();evidence.operatorAuthorized=false;CHECK(!r.p.evaluate(base(),100));CHECK(r.p.terminal());}
 good();{Permission p{readEvidence};p.selected(7,42,0);CHECK(!p.evaluate(base(),15000));CHECK(p.reason()==Reason::Expired);}
 good();{Permission p{+[](uint32_t,uint32_t,uint32_t){auto e=evidence;e.run=7;e.event=42;e.sampledAt=0;return e;}};p.selected(7,42,0);CHECK(!p.evaluate(base(),1));CHECK(p.reason()==Reason::Scope);}
 good();{Rig r;r.arm();auto s=packet();s.hopCount=1;CHECK(r.c.observe(s,r.context(),true,0)==F23CLab::Admission::WrongRoute);CHECK(r.c.attempts()==0);}
 CHECK(!F23DBench::kPhysicalPermissionInstalled);CHECK(!kPlatformEvidenceInstalled);
 printf("PASS: %u actual firmware permission/controller checks\n",checks);
}
