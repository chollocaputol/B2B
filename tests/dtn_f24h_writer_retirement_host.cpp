// F24H synthetic, no SD/Arduino/network. All success mocks are untrusted.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "dtn_f24h_writer_retirement.h"
using namespace DTNF24H;
static unsigned checks=0;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"F24H FAIL line %d: %s\n",__LINE__,#x);return 1;}++checks;}while(false)
static SessionKey makeKey(){
  SessionKey s{};s.uploader.bytes[0]=8U;s.uploader.bytes[6]=0x40U;s.uploader.bytes[8]=0x80U;
  s.vessel=2U;std::memcpy(s.session,"S58BAFE2",9);
  s.journal=DTNAckLedger::Journal::SosEvents;
  return s;
}
struct Port {
 Snapshot x{};
 bool canQuiesce=true,canAppend=true,canFlush=true,canInspect=true,canProtect=true;
 bool wrongCount=false,noEpochAdvance=false,inflightAfterAppend=false;
 bool mutateDuringInspect=false,mutateDuringQuiesce=false;
 uint64_t bytes=18U;bool newline=true;
 unsigned quiesces=0,urgentWrites=0,researchWrites=0,flushes=0,inspections=0,protections=0;
 Snapshot snapshot()const{return x;}
 bool quiesceProducers(){++quiesces;if(!canQuiesce)return false;x.producerQuiesced=true;
  if(mutateDuringQuiesce)++x.ownerLeaseSerial;
  return true;}
 bool append(bool urgent){
  if(!canAppend || !x.producerQuiesced || x.writeProtected)return false;
  if(urgent){++urgentWrites;if(!wrongCount)--x.urgentQueued;}
  else {++researchWrites;if(!wrongCount)--x.researchQueued;}
  if(!noEpochAdvance)++x.sourceEpoch;
  x.writeInFlight=inflightAfterAppend;
  return true;
 }
 bool appendNextUrgent(){return append(true);}
 bool appendNextResearch(){return append(false);}
 bool flushAndCloseAll(){++flushes;if(!canFlush)return false;
  x.writerHandles=0U;x.flushSucceeded=true;return true;}
 bool inspectSOSLastByte(uint64_t& out,bool& complete){
  ++inspections;if(!canInspect)return false;
  out=bytes;complete=newline;
  if(mutateDuringInspect)++x.sourceEpoch;
  return true;
 }
 bool protectClosedSource(){++protections;if(!canProtect)return false;
  x.writeProtected=true;return true;}
};
static Port healthy(SessionKey s){
 Port p{};p.x.key=s;p.x.vessel=s.vessel;
 p.x.mediaTag=22U;p.x.mountEpoch=3U;p.x.sourceEpoch=9U;
 p.x.closeSerial=7U;p.x.ownerLeaseSerial=4U;
 p.x.mounted=true;p.x.mediaVerified=true;p.x.soleMainLoopOwner=true;
 return p;
}
static bool complete(Port& p,Controller& c){
 if(c.request(p,p.x.key)!=State::Draining)return false;
 for(unsigned i=0;i<70 && c.state()!=State::RetiredCandidate && c.state()!=State::Failed;++i)
  c.poll(p);
 return c.state()==State::RetiredCandidate;
}
int main(){
 const SessionKey stream=makeKey();
 CHECK(validKey(stream));
 Controller baseline; CHECK(baseline.state()==State::Untrusted);
 CHECK(!baseline.candidate().eligible);
 CHECK(!baseline.stillCandidate(healthy(stream).x));
 { Port p=healthy(stream);p.x.urgentQueued=2;p.x.researchQueued=2;
   Controller c;CHECK(complete(p,c));CHECK(c.candidate().eligible);
   CHECK(c.candidate().facts.retiredSession);CHECK(c.candidate().facts.appendQueueDrained);
   CHECK(c.candidate().facts.flushSucceeded);CHECK(c.candidate().facts.allWriterHandlesClosed);
   CHECK(c.candidate().facts.finalNewlineVerified);CHECK(c.candidate().facts.protectedAgainstWrites);
   CHECK(c.candidate().facts.fileBytes==18U);CHECK(c.candidate().facts.sourceEpoch==13U);
   Stream postHash{};postHash.uploader=stream.uploader;postHash.vessel=stream.vessel;
   std::memcpy(postHash.session,stream.session,9);postHash.journal=stream.journal;
   DTNF24G::CloseFacts bound{};
   CHECK(!bindPostHash(c.candidate(),postHash,bound)); // digest absent before hashing
   postHash.sha256[0]=1U;
   CHECK(bindPostHash(c.candidate(),postHash,bound));
   CHECK(DTNAckLedger::sameStream(bound.stream,postHash));
   CHECK(bound.closeSerial==7U && bound.fileBytes==18U);
   postHash.vessel=3U;CHECK(!bindPostHash(c.candidate(),postHash,bound));
   CHECK(bound.mediaTag==0U);
   postHash.vessel=stream.vessel;
   CHECK(p.urgentWrites==2U);CHECK(p.researchWrites==2U);CHECK(p.flushes==1U);
   CHECK(p.inspections==1U);CHECK(p.protections==1U);
   CHECK(c.stillCandidate(p.snapshot()));CHECK(c.request(p,stream)==State::Failed);
   CHECK(!c.candidate().eligible);CHECK(!c.stillCandidate(p.snapshot())); }
 { Port p=healthy(stream);p.x.urgentQueued=1;p.x.researchQueued=1;
   Controller c;CHECK(c.request(p,stream)==State::Draining);
   CHECK(c.poll(p)==State::Draining);CHECK(p.urgentWrites==1U);CHECK(p.researchWrites==0U);
   CHECK(c.poll(p)==State::Draining);CHECK(p.researchWrites==1U);
   CHECK(c.poll(p)==State::Closing);CHECK(c.poll(p)==State::Checking);
   CHECK(c.poll(p)==State::RetiredCandidate);CHECK(c.stillCandidate(p.x));
   p.x.sourceEpoch++;CHECK(!c.stillCandidate(p.x)); }
 {Port p=healthy(stream);Controller c;CHECK(complete(p,c));CHECK(p.urgentWrites==0);
  CHECK(p.researchWrites==0);CHECK(c.candidate().facts.fileBytes==18U);
  c.losePower();CHECK(c.state()==State::Untrusted);CHECK(!c.candidate().eligible);
  CHECK(!c.stillCandidate(p.x));}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  p.x.activeSOS=true;CHECK(c.poll(p)==State::Failed);CHECK(!c.candidate().eligible);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  p.x.urgentOrRadio=true;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  p.x.producerQuiesced=false;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  p.x.mountEpoch++;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  p.x.mediaTag++;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  p.x.ownerLeaseSerial++;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  p.x.closeSerial++;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  p.x.sourceEpoch++;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);p.x.urgentQueued=1;Controller c;
  CHECK(c.request(p,stream)==State::Draining);p.canAppend=false;
  CHECK(c.poll(p)==State::Failed);CHECK(p.x.urgentQueued==1U);}
 {Port p=healthy(stream);p.x.urgentQueued=1;Controller c;
  CHECK(c.request(p,stream)==State::Draining);p.wrongCount=true;
  CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);p.x.urgentQueued=1;Controller c;
  CHECK(c.request(p,stream)==State::Draining);p.noEpochAdvance=true;
  CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);p.x.urgentQueued=1;Controller c;
  CHECK(c.request(p,stream)==State::Draining);p.inflightAfterAppend=true;
  CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  CHECK(c.poll(p)==State::Closing);p.canFlush=false;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  CHECK(c.poll(p)==State::Closing);CHECK(c.poll(p)==State::Checking);
  p.canInspect=false;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  CHECK(c.poll(p)==State::Closing);CHECK(c.poll(p)==State::Checking);
  p.newline=false;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  CHECK(c.poll(p)==State::Closing);CHECK(c.poll(p)==State::Checking);
  p.mutateDuringInspect=true;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(c.request(p,stream)==State::Draining);
  CHECK(c.poll(p)==State::Closing);CHECK(c.poll(p)==State::Checking);
  p.canProtect=false;CHECK(c.poll(p)==State::Failed);}
 {Port p=healthy(stream);p.mutateDuringQuiesce=true;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.canQuiesce=false;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.urgentQueued=25;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.researchQueued=25;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.activeSOS=true;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.urgentOrRadio=true;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.historyActive=true;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.exportActive=true;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.mediaVerified=false;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.soleMainLoopOwner=false;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.mounted=false;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.mediaTag=0;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.mountEpoch=0;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.sourceEpoch=0;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.ownerLeaseSerial=0;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.closeSerial=0;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.producerQuiesced=true;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);p.x.writeInFlight=true;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);SessionKey other=stream;other.journal=DTNAckLedger::Journal::Telemetry;
  p.x.key=other;Controller c;CHECK(c.request(p,other)==State::Failed);}
 {Port p=healthy(stream);p.x.vessel=3;Controller c;
  CHECK(c.request(p,stream)==State::Failed);}
 {Port p=healthy(stream);SessionKey other=stream;other.session[3]='1';
  Controller c;CHECK(c.request(p,other)==State::Failed);}
 {Port p=healthy(stream);Controller c;CHECK(complete(p,c));
  p.x.urgentQueued=1;CHECK(!c.stillCandidate(p.x));}
 {Port p=healthy(stream);Controller c;CHECK(complete(p,c));
  p.x.writeProtected=false;CHECK(!c.stillCandidate(p.x));}
 {Port p=healthy(stream);Controller c;CHECK(complete(p,c));
  p.x.writerHandles=1;CHECK(!c.stillCandidate(p.x));}
 {Port p=healthy(stream);Controller c;CHECK(complete(p,c));
  p.x.mediaVerified=false;CHECK(!c.stillCandidate(p.x));}
 std::printf("F24H writer retirement: PASS (%u checks; MOCK PORT ONLY)\n",checks);
 return 0;
}
