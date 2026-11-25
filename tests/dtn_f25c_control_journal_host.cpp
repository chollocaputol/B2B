// F25C host-only mixed legacy/typed journal, receipts, and deny runtime.
#define main f24o_fixture_main
#include "dtn_f24o_owned_port_e2e.cpp"
#undef main
#include "../firmware/shared/storage/dtn_f24y_checked_vfs.h"
#include "../firmware/shared/storage/dtn_f25b_session_lifecycle.h"
#include "../firmware/shared/storage/dtn_f25a_runtime_deny.h"
namespace M=DTNF24M;namespace N=DTNF24N;namespace B=DTNF25B;namespace Z=DTNF24Z;namespace W=DTNF24W;namespace Y=DTNF24Y;
using R=Z::Result;using C=M::CaptureResult;using L=N::Ledger<8>;
static VesselPacket cancelPacket(unsigned o,unsigned e){auto p=sos(o,e);p.packetType=PACKET_SOS_CANCEL;M::le32(reinterpret_cast<uint8_t*>(p.message),e);M::le32(reinterpret_cast<uint8_t*>(p.message)+4,o);return p;}
static M::PacketEntry entry(const VesselPacket& p){M::PacketEntry e{};e.origin=p.originNodeId;e.event=p.sequenceNumber;e.source=1;memcpy(e.bytes,&p,sizeof(p));return e;}
static N::Receipt receipt(const M::PacketEntry& p,uint64_t id){N::Receipt r{};r.origin=p.origin;r.event=p.event;r.packetCrc=M::crc32(p.bytes,62);r.serverId=id;r.serverTag=0x13;
#if THESIS_DTN_F25C_BUILD_ONLY == 1
 r.type=M::recordType(p);
#endif
 return r;}
// Synthetic, deterministic independent verifier: matches exact known packet bytes,
// type, identity, CRC, id and tag. No production authentication claim.
struct Verify {bool operator()(const N::Receipt& r,const M::PacketEntry& p)const{
 const auto expected=entry(M::recordType(p)==PACKET_SOS_CANCEL?cancelPacket(2,17):sos(2,17));
 const auto want=receipt(expected,M::recordType(expected)==PACKET_SOS_CANCEL?12:11);
 return !memcmp(expected.bytes,p.bytes,62) && N::matchesRecord(r,expected) && r.packetCrc==want.packetCrc && r.serverId==want.serverId && r.serverTag==want.serverTag;
}};
#if THESIS_DTN_F25C_BUILD_ONLY == 1
static void unit(){
 const auto s=sos(2,17),c=cancelPacket(2,17);const auto sp=entry(s),cp=entry(c);
 M::PendingQueue<8> q;CHECK(q.capture(s,0)==C::Queued);CHECK(q.capture(c,0)==C::Queued);CHECK(q.capture(c,1)==C::Duplicate);CHECK(q.capture(s,1)==C::Duplicate);
 auto bad=c;bad.message[0]^=1;CHECK(q.capture(bad,0)==C::Invalid);bad=c;bad.message[4]^=1;CHECK(q.capture(bad,0)==C::Invalid);bad=c;bad.timestamp++;CHECK(q.capture(bad,0)==C::Conflicting);bad=c;bad.packetType=PACKET_SOS_CANCEL_ACK;CHECK(q.capture(bad,0)==C::Invalid);bad=c;bad.packetType=PACKET_SOS_ACK;CHECK(q.capture(bad,0)==C::Invalid);bad=c;bad.packetType=PACKET_TELEMETRY;CHECK(q.capture(bad,0)==C::Invalid);CHECK(q.capture(c,2)==C::Invalid);
 CHECK(q.pending()==2);CHECK(!memcmp(q.front()->bytes,&s,62));CHECK(q.acknowledgeDurableAppend());CHECK(!memcmp(q.front()->bytes,&c,62));CHECK(q.acknowledgeDurableAppend());CHECK(!q.front());
 M::PendingQueue<2> full;CHECK(full.capture(s,0)==C::Queued);CHECK(full.capture(c,0)==C::Queued);CHECK(full.capture(cancelPacket(3,17),1)==C::Full);CHECK(full.lost()==1 && full.pending()==2);
 uint8_t sf[M::kFrameBytes]{},cf[M::kFrameBytes]{};CHECK(M::encode(sp,sf));CHECK(M::encode(cp,cf));CHECK(sf[4]==1 && sf[6]==0 && cf[4]==2 && cf[6]==PACKET_SOS_CANCEL);M::PacketEntry out{};CHECK(M::decode(sf,sizeof(sf),out));CHECK(!memcmp(out.bytes,&s,62));CHECK(M::decode(cf,sizeof(cf),out));CHECK(!memcmp(out.bytes,&c,62));
 cf[4]=1;cf[6]=0;M::le32(cf+84,M::crc32(cf,84));CHECK(!M::decode(cf,sizeof(cf),out));CHECK(M::encode(cp,cf));cf[6]=PACKET_SOS;M::le32(cf+84,M::crc32(cf,84));CHECK(!M::decode(cf,sizeof(cf),out));CHECK(M::encode(cp,cf));cf[23]^=1;CHECK(!M::decode(cf,sizeof(cf),out));CHECK(M::encode(cp,cf));CHECK(!M::decode(cf,sizeof(cf)-1,out));
 auto sr=receipt(sp,11),cr=receipt(cp,12);uint8_t srbuf[N::kReceiptBytes]{},crbuf[N::kReceiptBytes]{};CHECK(N::encodeReceipt(sr,srbuf));CHECK(N::encodeReceipt(cr,crbuf));CHECK(srbuf[4]==1 && srbuf[6]==0 && crbuf[4]==2 && crbuf[6]==PACKET_SOS_CANCEL);N::Receipt rr{};CHECK(N::decodeReceipt(srbuf,sizeof(srbuf),rr));CHECK(N::receiptType(rr)==PACKET_SOS);CHECK(N::decodeReceipt(crbuf,sizeof(crbuf),rr));CHECK(N::receiptType(rr)==PACKET_SOS_CANCEL);crbuf[6]=PACKET_SOS;M::le32(crbuf+36,M::crc32(crbuf,36));CHECK(!N::decodeReceipt(crbuf,sizeof(crbuf),rr));
 L ledger;CHECK(ledger.eventState(2,17)==L::EventState::Unknown);CHECK(ledger.accept(cp)==N::Add::Inserted);CHECK(ledger.eventState(2,17)==L::EventState::CancelSeen);CHECK(ledger.accept(sp)==N::Add::Inserted);CHECK(ledger.eventState(2,17)==L::EventState::CancelSeen);CHECK(ledger.accept(cp)==N::Add::Duplicate);CHECK(ledger.pending()==2);CHECK(M::recordType(*ledger.nextPending())==PACKET_SOS);CHECK(ledger.apply(cr,N::DenyReceiptVerifier{})==N::ReceiptResult::Unverified);CHECK(ledger.pending()==2);CHECK(ledger.apply(sr,Verify{})==N::ReceiptResult::Verified);CHECK(ledger.pending()==1 && ledger.delivered()==1);CHECK(M::recordType(*ledger.nextPending())==PACKET_SOS_CANCEL);CHECK(ledger.apply(sr,Verify{})==N::ReceiptResult::Duplicate);CHECK(ledger.apply(cr,Verify{})==N::ReceiptResult::Verified);CHECK(ledger.delivered()==2 && !ledger.nextPending());
 L late;CHECK(late.accept(cp)==N::Add::Inserted);CHECK(late.apply(cr,Verify{})==N::ReceiptResult::Verified);CHECK(late.accept(sp)==N::Add::Inserted);CHECK(late.eventState(2,17)==L::EventState::CancelSeen);CHECK(late.pending()==1);CHECK(late.eventState(3,17)==L::EventState::Unknown);CHECK(late.accept(entry(sos(3,17)))==N::Add::Inserted);CHECK(late.eventState(3,17)==L::EventState::SOSSeen);
 L mismatch;CHECK(mismatch.accept(sp)==N::Add::Inserted);CHECK(mismatch.accept(cp)==N::Add::Inserted);auto wrong=cr;wrong.type=PACKET_SOS;CHECK(mismatch.apply(wrong,Verify{})==N::ReceiptResult::Conflict);CHECK(mismatch.faulted());L conflict;CHECK(conflict.accept(cp)==N::Add::Inserted);bad=c;bad.timestamp++;CHECK(conflict.accept(entry(bad))==N::Add::Conflict);CHECK(conflict.eventState(2,17)==L::EventState::Fault);
}
struct MW{const SimulatedWitness* w;W::Identity view()const{auto e=w->view();return {e.mediaTag,e.mountEpoch,e.ownerSerial,e.mounted,e.exclusive,e.independentOwner};}};
struct Driver{const std::string* root_;unsigned* calls;Driver(const std::string& root,unsigned& n):root_(&root),calls(&n){}const char* root()const{return root_->c_str();}bool mount(){++*calls;return fs::is_directory(*root_);}bool unmount(){++*calls;return true;}};
struct Witness:SimulatedWitness{const char* current=nullptr;size_t count=0;B::Closure a{},b{};
 bool fresh(uint32_t v,const char* dir)const{return v==2 && current && !strcmp(current,dir);}
 bool closedCount(size_t& n)const{n=count;return true;}
 bool closed(const char* dir,B::Closure& out)const{if(count>=1 && !strcmp(dir,S1)){out=a;return true;}if(count>=2 && !strcmp(dir,S2)){out=b;return true;}return false;}
};
using Backend=Y::Backend<Driver>;using Service=B::Service<Backend,MW,Witness>;
struct Rig{std::string root;Witness w{};MW m{&w};unsigned calls=0;Service svc;Rig(const fs::path& p,const char* current,size_t n):root(p.string()),svc(m,w,root,calls){w.e=witness().e;w.current=current;w.count=n;w.e.mountEpoch=7+n;w.e.ownerSerial=99+n;w.a={w.e.mediaTag,7,99,2,176,0};w.b={w.e.mediaTag,8,100,2,0,80};}};
static void drain(Service& svc){for(unsigned i=0;i<100;++i){auto r=svc.poll(Verify{});if(r==R::Done)return;CHECK(r==R::Progress);}CHECK(false);}
static void append(Service& svc,const VesselPacket& p){M::PendingQueue<8> q;CHECK(q.capture(p,1)==C::Queued);CHECK(svc.append(q)==R::Done);CHECK(!q.pending());}
static void init(const fs::path& p){CHECK(!fs::exists(p));CHECK(fs::create_directories(p/"THESIS/F06TEST/V002"));Rig r(p,S1,0);CHECK(r.svc.start(2,S1)==R::Done);CHECK(r.svc.discover()==R::Done);append(r.svc,cancelPacket(2,17));append(r.svc,sos(2,17));CHECK(r.svc.shutdown()==R::Done);HostSD sd(p);CHECK(bytes(packetFile(sd,S1)).size()==176);}
static void replayMixed(const fs::path& p){Rig r(p,S2,1);HostSD sd(p);const auto old=bytes(packetFile(sd,S1));CHECK(old.size()==176);CHECK(r.svc.start(2,S2)==R::Done);CHECK(r.svc.discover()==R::Done);drain(r.svc);Z::Summary s{};CHECK(r.svc.summary(s)==R::Done && s.pending==2 && s.delivered==0);
 auto sr=receipt(entry(sos(2,17)),11),cr=receipt(entry(cancelPacket(2,17)),12);auto bad=cr;bad.type=PACKET_SOS;CHECK(r.svc.receipt(bad,Verify{})==R::Rejected);CHECK(r.svc.receipt(cr,N::DenyReceiptVerifier{})==R::Rejected);CHECK(!fs::exists(receiptFile(sd,S2)));CHECK(r.svc.receipt(sr,Verify{})==R::Done);CHECK(r.svc.summary(s)==R::Done && s.pending==1 && s.delivered==1);CHECK(r.svc.receipt(cr,Verify{})==R::Done);CHECK(r.svc.summary(s)==R::Done && s.pending==0 && s.delivered==2);CHECK(r.svc.receipt(cr,Verify{})==R::Rejected);CHECK(r.svc.shutdown()==R::Done);CHECK(bytes(packetFile(sd,S1))==old);CHECK(bytes(receiptFile(sd,S2)).size()==80);}
static void restartMixed(const fs::path& p){Rig r(p,S3,2);HostSD sd(p);const auto old=bytes(packetFile(sd,S1)),receipts=bytes(receiptFile(sd,S2));CHECK(r.svc.start(2,S3)==R::Done);CHECK(r.svc.discover()==R::Done);drain(r.svc);Z::Summary s{};CHECK(r.svc.summary(s)==R::Done && s.pending==0 && s.delivered==2 && s.sessions==2);CHECK(r.svc.shutdown()==R::Done);CHECK(bytes(packetFile(sd,S1))==old && bytes(receiptFile(sd,S2))==receipts);CHECK(!fs::exists(packetFile(sd,S3)) && !fs::exists(receiptFile(sd,S3)));}
static unsigned ioCalls=0;
struct Counting {bool mount(){++ioCalls;return false;}bool unmount(){++ioCalls;return false;}bool exists(const char*,bool&){++ioCalls;return false;}bool open(uint8_t,const char*,W::Mode){++ioCalls;return false;}bool child(uint8_t,uint8_t){++ioCalls;return false;}bool io(uint8_t,W::Action,uint8_t*,size_t){++ioCalls;return false;}bool size(uint8_t,size_t&){++ioCalls;return false;}bool seek(uint8_t,size_t){++ioCalls;return false;}bool flush(uint8_t){++ioCalls;return false;}bool close(uint8_t){++ioCalls;return false;}};
static void runtime(){DTNF25A::Runtime<Counting> r;DTNF25A::Queue q;DTNF25A::Report report{};auto s=sos(2,17),c=cancelPacket(2,17);CHECK(!r.bind(0));CHECK(r.capture(0,q,c,1)==C::Faulted);CHECK(r.bind(1));CHECK(r.capture(2,q,c,1)==C::Faulted);CHECK(r.capture(1,q,s,0)==C::Queued);CHECK(r.capture(1,q,c,1)==C::Queued);CHECK(r.poll(1,q,0,true,report)==DTNF25A::Tick::Busy);CHECK(r.poll(1,q,0,false,report)==DTNF25A::Tick::Denied);CHECK(report.pending==2 && q.pending()==2);CHECK(r.poll(1,q,30000,false,report)==DTNF25A::Tick::Denied);CHECK(report.pending==2 && q.pending()==2 && !memcmp(q.front()->bytes,&s,62));CHECK(ioCalls==0);}
#else
static void legacy(const fs::path& p){HostSD sd(p);auto packets=bytes(packetFile(sd,S1)),receipts=bytes(receiptFile(sd,S2));CHECK(packets.size()==176 && receipts.size()==80);M::PacketEntry row{};CHECK(!M::decode(packets.data(),88,row));CHECK(M::decode(packets.data()+88,88,row));CHECK(M::recordType(row)==PACKET_SOS);N::Receipt r{};CHECK(N::decodeReceipt(receipts.data(),40,r));CHECK(N::receiptType(r)==PACKET_SOS);CHECK(!N::decodeReceipt(receipts.data()+40,40,r));M::PendingQueue<8> q;CHECK(q.capture(cancelPacket(2,17),1)==C::Invalid);uint8_t frame[88]{};CHECK(M::encode(entry(sos(2,17)),frame));CHECK(!memcmp(frame,packets.data()+88,88));uint8_t rf[40]{};CHECK(N::encodeReceipt(receipt(entry(sos(2,17)),11),rf));CHECK(!memcmp(rf,receipts.data(),40));CHECK(!Verify{}(receipt(entry(sos(2,17)),12),entry(sos(2,17))));}
#endif
int main(int argc,char** argv){if(argc!=3)return 2;std::string phase=argv[1];fs::path p=argv[2];
#if THESIS_DTN_F25C_BUILD_ONLY == 1
 if(phase=="unit")unit();else if(phase=="init")init(p);else if(phase=="replay")replayMixed(p);else if(phase=="restart")restartMixed(p);else if(phase=="runtime")runtime();else return 2;
#else
 if(phase=="legacy")legacy(p);else return 2;
#endif
 printf("F25C %s PASS %u checks; host only\n",phase.c_str(),checks);return 0;}
