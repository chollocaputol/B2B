#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <initializer_list>
#include "firmware/shared/relay/f23d_lab_diagnostics.h"
#include "firmware/shared/relay/f23d_evidence_queue.h"
#include "firmware/shared/relay/f23d_run_evidence.h"
#include "firmware/shared/relay/f23d_permission_qualification.h"
#include "firmware/shared/communication/multi_peer_sos.h"
using namespace F23DReadiness;
static unsigned checks=0;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}++checks;}while(0)
struct Writer {
  int space=0;std::string output;size_t biggest=0;
  int availableForWrite(){return space;}
  size_t write(const uint8_t* p,size_t n){if(n>biggest){biggest=n;}output.append(reinterpret_cast<const char*>(p),n);return n;}
};
static VesselPacket packet(uint8_t kind=PACKET_SOS){
  VesselPacket p{};p.magic=PACKET_MAGIC;p.version=1;p.packetType=kind;
  p.originNodeId=1;p.sequenceNumber=42;p.timestamp=123;
  if(kind==PACKET_SOS_CANCEL){encodeSOSAck(p,42,1);}
  return p;
}
using F23DQualification::LocalEvidence;
static LocalEvidence reviewed(){
  LocalEvidence e;e.provisioned=true;e.boardMatches=true;e.operatorPresent=true;
  e.physicalContractReviewed=true;e.recoverySafe=true;e.self=2;return e;
}
struct Mock {
  F23DQualification::PermissionModel* owner=nullptr;unsigned calls=0;bool reservedBeforeCall=true;
  int submit(const VesselPacket& p){++calls;reservedBeforeCall &= owner->attempts()==calls && p.hopCount==1;return -7;}
};
int main(){
  EventSnapshot s;s.self=1;s.next=42;s.provisioned=true;s.counterAvailable=true;s.storageSafe=true;
  CHECK(selectable(s));char row[512];const auto original=s;
  CHECK(eventRow(row,sizeof(row),s,123));
  CHECK(std::strstr(row,"next=42") && std::strstr(row,"assessment=READY"));
  CHECK(s.next==original.next && s.counterAvailable==original.counterAvailable);
  auto bad=s;bad.self=2;CHECK(!selectable(bad));
  bad=s;bad.provisioned=false;CHECK(!selectable(bad));
  bad=s;bad.counterAvailable=false;CHECK(!selectable(bad));
  bad=s;bad.next=0;CHECK(!selectable(bad));
  bad=s;bad.next=UINT32_MAX;CHECK(!selectable(bad));
  bad=s;bad.active=true;CHECK(!selectable(bad));
  bad=s;bad.pending=true;CHECK(!selectable(bad));
  bad=s;bad.cancelPending=true;CHECK(!selectable(bad));
  bad=s;bad.storageSafe=false;CHECK(!selectable(bad));
  bad=s;bad.recoveryHold=true;CHECK(!selectable(bad));
  CHECK(eventRow(row,sizeof(row),bad,124) && std::strstr(row,"assessment=BLOCKED"));
  CHECK(!eventRow(row,8,s,123));

  auto p=packet();auto forwarded=p;forwarded.hopCount=1;
  const auto* bytes=reinterpret_cast<const uint8_t*>(&p);
  const auto* hopBytes=reinterpret_cast<const uint8_t*>(&forwarded);
  CHECK(fingerprint(bytes,62)!=fingerprint(hopBytes,62));
  CHECK(fingerprint(bytes,62,true)==fingerprint(hopBytes,62,true));
  CHECK(rxRow(row,sizeof(row),3,100,hopBytes,62,-89,-75,"LEGACY_UNAUTHENTICATED","NEW"));
  CHECK(std::strstr(row,"local=3") && std::strstr(row,"origin=1") && std::strstr(row,"event=42"));
  CHECK(std::strstr(row,"hop=1") && std::strstr(row,"rssi=-89") && std::strstr(row,"snr_x10=-75"));
  CHECK(p.hopCount==0 && forwarded.hopCount==1);
  for(const char* outcome:{"NEW","DUPLICATE","STALE","FULL","CANCELLED","CANCEL_DUPLICATE","CANCEL_MISMATCH","REJECTED"}){
    CHECK(rxRow(row,sizeof(row),3,100,bytes,62,-89,12,"LEGACY_UNAUTHENTICATED",outcome));
    CHECK(std::strstr(row,outcome)!=nullptr);
  }
  CHECK(rxRow(row,sizeof(row),3,100,bytes,4,-89,0,"BAD_FORMAT","REJECTED"));
  CHECK(!rxRow(row,sizeof(row),3,100,bytes,100,-89,0,"BAD_FORMAT","REJECTED"));
  CHECK(!rxRow(row,sizeof(row),3,100,bytes,3,-89,0,"BAD_FORMAT","REJECTED"));
  auto unrelated=p;unrelated.packetType=PACKET_TELEMETRY;
  CHECK(!rxRow(row,sizeof(row),3,100,reinterpret_cast<uint8_t*>(&unrelated),62,-89,0,"VALID","NEW"));
  CHECK(!rxRow(row,8,3,100,bytes,62,-89,0,"VALID","NEW"));

  EvidenceQueue q;Writer writer;
  CHECK(q.push("first") && q.push("second"));
  CHECK(q.drain(writer)==0 && q.size()==2 && writer.output.empty());
  writer.space=2;
  while(q.size())CHECK(q.drain(writer)<=2);
  CHECK(writer.output=="first\nsecond\n" && writer.biggest<=2);
  writer.output.clear();writer.space=1000;
  std::string longRow(500,'x');CHECK(q.push(longRow.c_str()));
  while(q.size())CHECK(q.drain(writer)<=64);
  CHECK(writer.output==longRow+"\n" && writer.biggest<=64);
  for(size_t i=0;i<EvidenceQueue::kDepth;++i)CHECK(q.push("bounded"));
  CHECK(!q.push("overflow") && q.lost()==1);
  CHECK(q.peak()==EvidenceQueue::kDepth);
  CHECK(!q.push(std::string(512,'z').c_str()) && q.lost()==2);
  while(q.size())q.drain(writer);
  CHECK(q.lost()==2); // loss is sticky evidence, never hidden by draining

  RunEvidence evidence;
  evidence.command("R23D SELECT 12 42",F23DBench::Control::Selected,100);
  CHECK(evidence.run()==12 && evidence.selectDeadline()==15100);
  evidence.command("R23D ARM 99",F23DBench::Control::Armed,200);
  CHECK(evidence.deadline()==15200 && evidence.remaining(201)==14999);
  CHECK(evidence.remaining(15200)==0);
  CHECK(evidence.deferralDue(1000) && !evidence.deferralDue(1500) && evidence.deferralDue(2000));
  CHECK(evidence.sosRemaining()==1 && evidence.cancelRemaining()==1);
  evidence.cancellationAccepted();CHECK(evidence.sosRemaining()==0 && evidence.cancelRemaining()==1);
  evidence.reserved(PACKET_SOS_CANCEL);CHECK(evidence.cancelRemaining()==0);
  CHECK(!std::strcmp(RunEvidence::controlName(F23DBench::Control::Denied),"DENIED"));
  CHECK(!std::strcmp(RunEvidence::admissionName(F23CLab::Admission::Duplicate),"DUPLICATE"));
  RunEvidence wrap;wrap.command("R23D ARM 99",F23DBench::Control::Armed,UINT32_MAX-100);
  CHECK(wrap.remaining(149)==14750);

  CHECK(!F23DQualification::PermissionModel::kGrantsFirmwareRadio);
  auto local=reviewed();auto cancel=packet(PACKET_SOS_CANCEL);
  F23DQualification::PermissionModel permission;Mock mock;mock.owner=&permission;int rc=0;
  CHECK(!permission.dispatch(local,0,mock,rc));
  CHECK(permission.command("R23D SELECT 1 42",local,0,99)==F23DBench::Control::Selected);
  CHECK(permission.command("R23D ARM 99",local,0,0)==F23DBench::Control::Armed);
  CHECK(permission.observe(p,local,true,0)==F23CLab::Admission::Accepted);
  CHECK(permission.dispatch(local,250,mock,rc) && rc==-7 && permission.attempts()==1);
  CHECK(!permission.dispatch(local,251,mock,rc));
  CHECK(permission.observe(cancel,local,false,300)==F23CLab::Admission::Accepted);
  CHECK(permission.dispatch(local,1250,mock,rc) && permission.attempts()==2);
  CHECK(mock.calls==2 && mock.reservedBeforeCall);
  CHECK(!permission.dispatch(local,5000,mock,rc));
  CHECK(permission.command("R23D DISARM",local,5001,0)==F23DBench::Control::Disarmed);
  CHECK(permission.command("R23D SELECT 2 42",local,5002,99)==F23DBench::Control::Locked);
  for(unsigned i=0;i<6;++i){
    auto denied=local;
    if(i==0)denied.provisioned=false;
    if(i==1)denied.self=3;
    if(i==2)denied.boardMatches=false;
    if(i==3)denied.operatorPresent=false;
    if(i==4)denied.physicalContractReviewed=false;
    if(i==5)denied.recoverySafe=false;
    F23DQualification::PermissionModel model;
    const auto selected=model.command("R23D SELECT 1 42",denied,0,99);
    if(selected==F23DBench::Control::Selected)
      CHECK(model.command("R23D ARM 99",denied,0,0)==F23DBench::Control::Denied);
    else CHECK(selected==F23DBench::Control::Denied);
    Mock none;none.owner=&model;CHECK(!model.dispatch(denied,250,none,rc) && none.calls==0);
  }
  F23DQualification::PermissionModel revoke;Mock blocked;blocked.owner=&revoke;
  CHECK(revoke.command("R23D SELECT 1 42",local,0,99)==F23DBench::Control::Selected);
  CHECK(revoke.command("R23D ARM 99",local,0,0)==F23DBench::Control::Armed);
  CHECK(revoke.observe(p,local,true,0)==F23CLab::Admission::Accepted);
  auto noOperator=local;noOperator.operatorPresent=false;
  CHECK(!revoke.dispatch(noOperator,250,blocked,rc) && !revoke.armed());
  CHECK(revoke.command("R23D ARM 99",local,251,0)==F23DBench::Control::Locked);
  F23DQualification::PermissionModel expired;
  CHECK(expired.command("R23D SELECT 1 42",local,0,99)==F23DBench::Control::Selected);
  CHECK(expired.command("R23D ARM 99",local,15000,0)==F23DBench::Control::Denied);
  F23DQualification::PermissionModel reboot;
  CHECK(!reboot.armed());
  CHECK(reboot.command("R23D ARM 99",local,0,0)==F23DBench::Control::Locked);
  std::printf("PASS: %u F23D readiness/query/RX/queue/permission checks; host-only grants\n",checks);
}
