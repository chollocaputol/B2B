#include <cassert>
#include <cstdio>
#include <string>
#include "tests/f23d_sd_mock_storage.h"
#include "firmware/shared/relay/f23d_sd_journal.h"
#include "firmware/shared/relay/f23d_evidence_queue.h"
#include "firmware/shared/relay/f23d_lab_diagnostics.h"
#include "firmware/shared/storage/f23d_sd_file_writer.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;assert(x);}while(0)
static F23DSD::Record rec(const char* kind,uint32_t ms=10){F23DSD::Record r{};CHECK(F23DSD::token(r.record,sizeof(r.record),kind));r.ms=ms;return r;}
static void boot(F23DSD::Journal& j,F23DSDFileWriter& w){
 CHECK(j.initialize(1,"S0000001",true,0));CHECK(w.begin("/THESIS/F06TEST/V001/S0000001",1));
 j.poll(w,0,false);CHECK(j.readyToStart());CHECK(j.written()==1);
}
static void seal(F23DSD::Journal& j,F23DSDFileWriter& w){CHECK(j.requestSeal());for(unsigned i=0;i<1500 && !j.sealed() && !j.failed();++i)j.poll(w,500,false);}
static void tests(){
 CHECK(sizeof(F23DSD::Record)<=128);CHECK(sizeof(F23DSD::Journal)<6500);
 {F23DSD::Journal j;CHECK(!j.initialize(1,"S0000001",false,0));CHECK(j.health()&F23DSD::Identity);}
 {F23DSD::Journal j;CHECK(!j.initialize(4,"S0000001",true,0));}
 {F23DSD::Journal j;CHECK(!j.initialize(1,"OLD",true,0));CHECK(j.health()&F23DSD::Session);}
 for(const char* path:{"/THESIS/VESSEL_001","/THESIS/F06TEST/V002/S0000001","/THESIS/F06TEST/V001/S0000001/EXTRA","/THESIS/F06TEST/V001/S12345678"}){
  SD.reset();F23DSDFileWriter w;CHECK(!w.begin(path,1));CHECK(SD.files.empty());
 }
 SD.reset();{F23DSDFileWriter w;cardAbsent=true;CHECK(!w.begin("/THESIS/F06TEST/V001/S0000001",1));}
 SD.reset();{F23DSDFileWriter w;CHECK(w.begin("/THESIS/F06TEST/V001/S0000001",1));F23DSDFileWriter again;CHECK(!again.begin("/THESIS/F06TEST/V001/S0000001",1));CHECK(SD.files.size()==1);}
 for(unsigned fault=0;fault<5;++fault){
  SD.reset();F23DSD::Journal j;F23DSDFileWriter w;boot(j,w);CHECK(j.start(1));CHECK(j.capture(rec("TEST")));
  CHECK(j.requestSeal());shortWrite=fault==0;failFlush=fault==1;failRead=fault==2;corruptRead=fault==3;failOpen=fault==4;
  const auto pending=j.pending();j.poll(w,20,false);CHECK(j.failed());CHECK(j.health()&F23DSD::Storage);CHECK(j.pending()==pending);CHECK(!j.sealed());
 }
 SD.reset();{
  F23DSD::Journal j;F23DSDFileWriter w;boot(j,w);CHECK(j.start(1));CHECK(j.permissionHealthy());
  const auto before=storageOps;CHECK(j.capture(rec("RX")));j.poll(w,10,false);CHECK(storageOps==before);
  CHECK(j.requestSeal());CHECK(!j.permissionHealthy());j.poll(w,20,true);CHECK(storageOps==before);
  seal(j,w);CHECK(j.sealed());CHECK(!j.start(500));CHECK(!j.initialize(1,"S0000002",true,0));
 }
 SD.reset();{
  F23DSD::Journal j;F23DSDFileWriter w;boot(j,w);CHECK(j.start(1));
  for(unsigned i=1;i<F23DSD::Journal::kCapacity;++i)CHECK(j.capture(rec("RX",i+1)));
  CHECK(!j.capture(rec("RX",100)));CHECK(j.lost()==1 && j.health()&F23DSD::Loss);CHECK(!j.permissionHealthy());
  seal(j,w);CHECK(j.sealed());CHECK(SD.files.begin()->second->text.find("\"lost\":1")!=std::string::npos);
 }
 SD.reset();{
  F23DSD::Journal j;F23DSDFileWriter w;boot(j,w);CHECK(j.start(1));j.formatLost();j.operationalLoss(1);
  seal(j,w);CHECK(j.sealed());CHECK(j.health()&F23DSD::Format);CHECK(j.health()&F23DSD::OperationalLoss);
 }
 SD.reset();{
  F23DSD::Journal j;F23DSDFileWriter w;boot(j,w);CHECK(j.start(1));CHECK(j.capture(rec("RX")));
  F23DReadiness::EvidenceQueue serial;
  for(unsigned i=0;i<13;++i)serial.push("STALLED");
  CHECK(serial.lost()==1);
  struct Stalled{int availableForWrite(){return 0;}size_t write(const uint8_t*,size_t){return 0;}} stalled;
  serial.drain(stalled);seal(j,w);CHECK(j.sealed() && j.lost()==0 && j.health()==0);
 }
 SD.reset();{
  F23DSD::Journal j;F23DSDFileWriter w;boot(j,w);CHECK(j.start(1));CHECK(j.capture(rec("RX")));
  F23DSD::Journal reboot;CHECK(!reboot.sealed());CHECK(!reboot.start(2));
  CHECK(SD.files.begin()->second->text.find("\"END\"")==std::string::npos);
 }
 SD.reset();{
  F23DSD::Journal j;F23DSDFileWriter w;boot(j,w);CHECK(j.start(1));CHECK(!j.start(2));
  auto invalid=rec("RX");memset(invalid.record,'A',sizeof(invalid.record));CHECK(!j.capture(invalid));
  CHECK(j.health()&F23DSD::Format);seal(j,w);CHECK(j.sealed() && !j.permissionHealthy());
 }
 SD.reset();{
  F23DSD::Journal j;F23DSDFileWriter w;boot(j,w);CHECK(j.start(1));CHECK(j.capture(rec("RX")));
  CHECK(j.requestSeal());cardAbsent=true;j.poll(w,500,false);CHECK(j.failed());CHECK(!j.sealed() && j.pending()>0);
 }
 SD.reset();{
  F23DSD::Journal j;F23DSDFileWriter w;boot(j,w);CHECK(j.start(1));CHECK(j.requestSeal());
  for(unsigned i=0;i<1500 && j.written()<3;++i)j.poll(w,500,false);
  CHECK(j.written()==3);failRead=true;j.poll(w,500,false);
  CHECK(j.failed() && !j.sealed() && j.pending()==0);
 }
 char small[8],large[F23DSD::Journal::kLine];auto maximal=rec("LOCAL_CONTROL_DEFERRED");
 maximal.ms=maximal.run=maximal.origin=maximal.event=maximal.wire=maximal.canonical=UINT32_MAX;
 maximal.rc=maximal.rx_rc=INT32_MIN;maximal.rssi=maximal.snr_x10=INT16_MIN;
 CHECK(!F23DSD::Journal::row(small,sizeof(small),maximal,3,"SFFFFFFF",UINT32_MAX,UINT32_MAX,48,UINT32_MAX,UINT32_MAX));
 CHECK(F23DSD::Journal::row(large,sizeof(large),maximal,3,"SFFFFFFF",UINT32_MAX,UINT32_MAX,48,UINT32_MAX,UINT32_MAX));
 CHECK(!F23DSD::token(maximal.state,sizeof(maximal.state),"BAD\"JSON"));
 printf("PASS: %u actual journal/File-writer/mock-storage checks; Record=%zu Journal=%zu max_line=%zu\n",checks,sizeof(F23DSD::Record),sizeof(F23DSD::Journal),strlen(large));
}
static VesselPacket packet(uint32_t event,bool cancel,bool relay){
 VesselPacket p{};p.magic=PACKET_MAGIC;p.version=PACKET_VERSION;p.originNodeId=1;p.sequenceNumber=event;p.packetType=cancel?PACKET_SOS_CANCEL:PACKET_SOS;p.hopCount=relay?1:0;
 if(cancel){uint32_t origin=1;memcpy(p.message,&event,4);memcpy(p.message+4,&origin,4);}else strcpy(p.message,"SOS");
 return p;
}
static F23DSD::Record eventRow(const char* stage,unsigned local,uint32_t event,bool cancel=false,bool relay=false){
 auto r=rec(stage);auto p=packet(event,cancel,relay);
 r.origin=1;r.event=event;r.type=p.packetType;r.hop=p.hopCount;r.bytes=62;
 r.canonical=F23DReadiness::fingerprint(reinterpret_cast<const uint8_t*>(&p),62,true);
 r.wire=F23DReadiness::fingerprint(reinterpret_cast<const uint8_t*>(&p),62);
 r.run=local==2?7:0;r.permit=local==2;return r;
}
static void journals(){
 for(unsigned local=1;local<=3;++local){
  SD.reset();F23DSD::Journal j;F23DSDFileWriter w;char session[9],directory[64];
  snprintf(session,sizeof(session),"S%07X",local);snprintf(directory,sizeof(directory),"/THESIS/F06TEST/V%03u/%s",local,session);
  CHECK(j.initialize(local,session,true,0));CHECK(w.begin(directory,local));j.poll(w,0,false);CHECK(j.start(1));
  uint32_t clock=10;
  auto push=[&](F23DSD::Record r){r.ms=clock++;CHECK(j.capture(r));};
  // Same-capture, relay-disabled event 41; event 42 is the positive trial.
  if(local==1){
   for(uint32_t event:{41U,42U}){
    push(eventRow("ALLOCATED",local,event));push(eventRow("LOCAL_SOS",local,event));
    auto begin=eventRow("TX_BEGIN",local,event);begin.used=1;push(begin);
    auto result=eventRow("TX_RESULT",local,event);result.used=1;result.called=true;push(result);
    push(eventRow("LOCAL_CANCEL",local,event,true));
    begin=eventRow("TX_BEGIN",local,event,true);begin.used=1;push(begin);
    result=eventRow("TX_RESULT",local,event,true);result.used=1;result.called=true;push(result);
   }
  }
  if(local==2){
   auto negative=eventRow("RX",local,41);negative.run=0;F23DSD::token(negative.state,sizeof(negative.state),"NEW");push(negative);
   auto selection=rec("SELECTED");selection.run=7;selection.event=42;push(selection);
   auto arm=selection;F23DSD::token(arm.record,sizeof(arm.record),"ARMED");arm.permit=true;push(arm);
   for(bool cancel:{false,true}){
    auto rx=eventRow("RX",local,42,cancel);F23DSD::token(rx.state,sizeof(rx.state),cancel?"CANCELLED":"NEW");F23DSD::token(rx.validation,sizeof(rx.validation),"LEGACY_UNAUTHENTICATED");push(rx);
    push(eventRow("ACCEPTED",local,42,cancel));
    for(const char* stage:{"RESERVED","RADIO_INVOKE","RADIO_RESULT"}){
     auto row=eventRow(stage,local,42,cancel,true);row.used=cancel?2:1;row.sos_left=0;row.cancel_left=cancel?0:1;row.called=!strcmp(stage,"RADIO_RESULT");push(row);
    }
   }
  }
  if(local==3){
   for(bool cancel:{false,true}){
    auto row=eventRow("RX",local,42,cancel,true);
    F23DSD::token(row.validation,sizeof(row.validation),"LEGACY_UNAUTHENTICATED");
    F23DSD::token(row.state,sizeof(row.state),cancel?"CANCELLED":"NEW");row.rssi=-90;row.snr_x10=20;push(row);
   }
  }
  seal(j,w);CHECK(j.sealed());printf("JOURNAL_%u\n%s",local,SD.files.begin()->second->text.c_str());
 }
}
int main(int argc,char**){if(argc>1)journals();else tests();}
