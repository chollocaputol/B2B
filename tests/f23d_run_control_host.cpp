#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "firmware/shared/relay/f23d_run_control.h"
#include "firmware/shared/relay/relay_queue.h"
#include "firmware/shared/communication/multi_peer_sos.h"
using namespace F23DBench;
static unsigned checks=0;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}++checks;}while(0)
struct MockRadio {
  unsigned calls=0; int rc=-7; VesselPacket packet{};
  int submit(const VesselPacket& p){++calls;packet=p;return rc;}
};
static Context context(){Context c;c.identityReady=true;c.self=2;c.physicalPermission=true;c.recoverySafe=true;return c;}
static VesselPacket frame(uint32_t event=42,uint8_t kind=PACKET_SOS){
  VesselPacket p{};p.magic=PACKET_MAGIC;p.version=PACKET_VERSION;p.originNodeId=1;
  p.sequenceNumber=event;p.packetType=kind;p.timestamp=123;p.latitude_e7=-12345;
  p.flags=FLAG_GPS_FIX;std::strcpy(p.nodeName,"Vessel-001");
  if(kind==PACKET_SOS_CANCEL)encodeSOSAck(p,event,1);else std::strcpy(p.message,"SOS");
  return p;
}
int main(){
  auto c=context();auto p=frame();auto cancel=frame(42,PACKET_SOS_CANCEL);
  MockRadio radio;int rc=0;RunControl run;
  CHECK(!kPhysicalPermissionInstalled);
  CHECK(sizeof(VesselPacket)==62);
  CHECK(!run.armed());
  CHECK(run.command("TEST ALL",c,0,99)==Control::NotCommand);
  CHECK(run.command("FLEET STATUS",c,0,99)==Control::NotCommand);
  CHECK(run.command("R23D SELECT 0 42",c,0,99)==Control::Invalid);
  CHECK(run.command("R23D SELECT 1 0",c,0,99)==Control::Invalid);
  CHECK(run.command("R23D SELECT 4294967296 42",c,0,99)==Control::Invalid);
  CHECK(run.command("R23D SELECT 1 42 EXTRA",c,0,99)==Control::Invalid);
  CHECK(run.command("R23D ARM 99",c,0,99)==Control::Locked);
  CHECK(!run.dispatch(c,0,radio,rc));
  for(uint32_t self:{0U,1U,3U,9999U}){
    RunControl invalid;auto bad=c;bad.self=self;
    CHECK(invalid.command("R23D SELECT 1 42",bad,0,99)==Control::Denied);
  }
  auto missing=c;missing.identityReady=false;RunControl identity;
  CHECK(identity.command("R23D SELECT 1 42",missing,0,99)==Control::Denied);
  CHECK(run.command("R23D SELECT 1 42",c,0,99)==Control::Selected);
  CHECK(run.challenge()==99);
  CHECK(!run.dispatch(c,0,radio,rc) && run.challenge()==99);
  CHECK(run.observe(p,c,true,0)==F23CLab::Admission::Disarmed);
  CHECK(run.challenge()==99);
  CHECK(run.command("R23D SELECT 2 43",c,0,88)==Control::Locked); // capacity one
  CHECK(run.command("R23D ARM 99",c,0,0)==Control::Armed);
  CHECK(run.command("R23D ARM 99",c,1,0)==Control::Locked);
  CHECK(run.observe(frame(41),c,true,0)==F23CLab::Admission::WrongEvent);
  CHECK(run.observe(frame(43),c,true,0)==F23CLab::Admission::WrongEvent);
  for(uint8_t hop:{uint8_t(1),uint8_t(2),uint8_t(255)}){
    auto bad=p;bad.hopCount=hop;
    CHECK(run.observe(bad,c,true,0)==F23CLab::Admission::WrongRoute);
  }
  auto wrong=p;wrong.originNodeId=3;
  CHECK(run.observe(wrong,c,true,0)==F23CLab::Admission::WrongRoute);
  wrong=p;wrong.magic=0;
  CHECK(run.observe(wrong,c,true,0)==F23CLab::Admission::Invalid);
  CHECK(run.observe(cancel,c,false,0)==F23CLab::Admission::StateRejected);
  CHECK(run.observe(p,c,false,0)==F23CLab::Admission::StateRejected);
  CHECK(run.observe(p,c,true,0)==F23CLab::Admission::Accepted);
  CHECK(run.observe(p,c,true,1)==F23CLab::Admission::Duplicate);
  auto busy=c;busy.localControlBusy=true;
  CHECK(!run.dispatch(busy,250,radio,rc));
  CHECK(run.attempts()==0 && radio.calls==0);
  CHECK(run.dispatch(c,250,radio,rc));
  CHECK(rc==-7 && radio.calls==1 && run.attempts()==1); // failed call consumed
  auto expected=p;expected.hopCount=1;
  CHECK(!std::memcmp(&expected,&radio.packet,sizeof(p)));
  CHECK(!run.dispatch(c,500,radio,rc));
  MultiPeerSOS::Table peers;
  CHECK(peers.receiveSOS(1,42,0)==MultiPeerSOS::Result::New);
  CHECK(peers.cancel(1,42,1)==MultiPeerSOS::Result::Cancelled);
  CHECK(peers.receiveSOS(1,43,2)==MultiPeerSOS::Result::New);
  // Operational CANCEL_MISMATCH does not erase the independently accepted event.
  CHECK(run.observe(cancel,c,false,600)==F23CLab::Admission::Accepted);
  CHECK(run.observe(cancel,c,false,601)==F23CLab::Admission::Duplicate);
  CHECK(run.observe(p,c,true,602)==F23CLab::Admission::Duplicate);
  CHECK(!run.dispatch(c,1249,radio,rc));
  CHECK(run.dispatch(c,1250,radio,rc));
  expected=cancel;expected.hopCount=1;
  CHECK(!std::memcmp(&expected,&radio.packet,sizeof(p)));
  CHECK(radio.calls==2 && run.attempts()==2);
  CHECK(!run.dispatch(c,5000,radio,rc));
  CHECK(run.command("R23D DISARM",c,5001,0)==Control::Disarmed);
  CHECK(run.command("R23D SELECT 2 43",c,5002,99)==Control::Locked);

  RunControl suppressed;
  CHECK(suppressed.command("R23D SELECT 1 42",c,0,99)==Control::Selected);
  CHECK(suppressed.command("R23D ARM 99",c,0,0)==Control::Armed);
  CHECK(suppressed.observe(p,c,true,0)==F23CLab::Admission::Accepted);
  auto malformed=cancel;malformed.message[4]=3;
  CHECK(suppressed.observe(malformed,c,false,100)==F23CLab::Admission::Invalid);
  CHECK(suppressed.observe(cancel,c,false,100)==F23CLab::Admission::Accepted);
  MockRadio cancelRadio;
  CHECK(suppressed.dispatch(c,350,cancelRadio,rc));
  CHECK(cancelRadio.packet.packetType==PACKET_SOS_CANCEL && suppressed.attempts()==1);
  CHECK(!suppressed.dispatch(c,2000,cancelRadio,rc));

  RunControl denied;auto noPermission=c;noPermission.physicalPermission=false;
  CHECK(denied.command("R23D SELECT 1 42",noPermission,0,99)==Control::Selected);
  CHECK(denied.command("R23D ARM 99",noPermission,0,0)==Control::Denied);
  CHECK(denied.command("R23D ARM 99",c,1,0)==Control::Locked);
  CHECK(!denied.dispatch(c,1000,radio,rc));
  RunControl wrongNonce;
  CHECK(wrongNonce.command("R23D SELECT 1 42",c,0,99)==Control::Selected);
  CHECK(wrongNonce.command("R23D ARM 98",c,0,0)==Control::Denied);
  RunControl late;
  CHECK(late.command("R23D SELECT 1 42",c,0,99)==Control::Selected);
  CHECK(late.command("R23D ARM 99",c,15000,0)==Control::Denied);
  RunControl expired;
  CHECK(expired.command("R23D SELECT 1 42",c,0,99)==Control::Selected);
  CHECK(expired.command("R23D ARM 99",c,0,0)==Control::Armed);
  CHECK(expired.observe(p,c,true,0)==F23CLab::Admission::Accepted);
  CHECK(!expired.dispatch(c,15000,radio,rc) && !expired.armed());
  RunControl lost;
  CHECK(lost.command("R23D SELECT 1 42",c,0,99)==Control::Selected);
  CHECK(lost.command("R23D ARM 99",c,0,0)==Control::Armed);
  CHECK(!lost.dispatch(missing,250,radio,rc) && !lost.armed());
  CHECK(lost.command("R23D SELECT 2 42",c,251,99)==Control::Locked);
  RunControl unsafe;
  CHECK(unsafe.command("R23D SELECT 1 42",c,0,99)==Control::Selected);
  CHECK(unsafe.command("R23D ARM 99",c,0,0)==Control::Armed);
  auto broken=c;broken.recoverySafe=false;
  CHECK(!unsafe.dispatch(broken,250,radio,rc) && !unsafe.armed());
  RunControl wrapped;const uint32_t start=UINT32_MAX-100;
  CHECK(wrapped.command("R23D SELECT 1 42",c,start,99)==Control::Selected);
  CHECK(wrapped.command("R23D ARM 99",c,start,0)==Control::Armed);
  CHECK(wrapped.observe(p,c,true,start)==F23CLab::Admission::Accepted);
  CHECK(!wrapped.dispatch(c,uint32_t(start+249U),radio,rc));
  CHECK(wrapped.dispatch(c,uint32_t(start+250U),radio,rc));
  RunControl reboot;
  CHECK(!reboot.armed() && !reboot.dispatch(c,1000,radio,rc));
  CHECK(reboot.command("R23D ARM 99",c,1000,0)==Control::Locked);
  MultiPeerSOS::Table endpoint;
  CHECK(endpoint.cancel(1,42,0)==MultiPeerSOS::Result::CancelMismatch);
  CHECK(endpoint.receiveSOS(1,42,1)==MultiPeerSOS::Result::New);
  Relay52::Queue<VesselPacket> queue;
  for(uint32_t event=1;event<=4;++event)
    CHECK(queue.receive(frame(event),2)==Relay52::Admit::Queued);
  CHECK(queue.receive(frame(5),2)==Relay52::Admit::Full);
  CHECK(run.attempts()==2); // diagnostic queue fullness never refills budgets
  std::printf("PASS: %u F23D controller/adapter mock checks; no physical RF\n",checks);
}
