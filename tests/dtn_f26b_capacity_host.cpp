// Host only, exact accepted packet codec, actual strict service, synthetic witnesses.
#define main f24o_fixture_main
#include "dtn_f24o_owned_port_e2e.cpp"
#undef main
#include "../firmware/shared/storage/dtn_f24y_checked_vfs.h"
#include "../firmware/shared/storage/dtn_f25b_session_lifecycle.h"
namespace M=DTNF24M; namespace N=DTNF24N; namespace B=DTNF25B; namespace W=DTNF24W; namespace Y=DTNF24Y; namespace Z=DTNF24Z; namespace F=DTNF26B;
using R=Z::Result; using Q=M::PendingQueue<8>;
static VesselPacket control(unsigned event,bool cancel=false){auto p=sos(2,event);if(cancel){p.packetType=PACKET_SOS_CANCEL;M::le32(reinterpret_cast<uint8_t*>(p.message),event);M::le32(reinterpret_cast<uint8_t*>(p.message)+4,2);}return p;}
static M::PacketEntry entry(const VesselPacket& p){M::PacketEntry e{};e.origin=p.originNodeId;e.event=p.sequenceNumber;e.source=0;memcpy(e.bytes,&p,62);return e;}
static N::Receipt receipt(unsigned event,bool cancel=false){auto e=entry(control(event,cancel));N::Receipt r{};r.origin=2;r.event=event;r.packetCrc=M::crc32(e.bytes,62);r.type=M::recordType(e);r.serverId=uint64_t(event)*2+unsigned(cancel);r.serverTag=13;return r;}
struct Verify{bool operator()(const N::Receipt& r,const M::PacketEntry& p)const{auto expected=entry(control(p.event,M::recordType(p)==PACKET_SOS_CANCEL));auto want=receipt(p.event,M::recordType(p)==PACKET_SOS_CANCEL);return N::matchesRecord(r,expected)&&r.packetCrc==want.packetCrc&&r.serverId==want.serverId&&r.serverTag==want.serverTag&&!memcmp(expected.bytes,p.bytes,62);}};
struct Driver{const std::string* root_;unsigned* calls;Driver(const std::string& p,unsigned& c):root_(&p),calls(&c){}const char* root()const{return root_->c_str();}bool mount(){++*calls;return fs::is_directory(*root_);}bool unmount(){++*calls;return true;}};
struct Witness:SimulatedWitness{const char* current=nullptr;size_t count=0;bool countValid=true;std::vector<std::pair<std::string,B::Closure>> claims;
 bool fresh(uint32_t v,const char* dir)const{return v==2&&current&&!strcmp(current,dir);}
 bool closedCount(size_t& n)const{n=count;return countValid;}
 bool closed(const char* dir,B::Closure& out)const{for(auto& c:claims)if(c.first==dir){out=c.second;return true;}return false;}
};
struct Mount {W::Identity id{}; W::Identity view()const{return id;}};
using Service=B::Service<Y::Backend<Driver>,Mount,Witness>;
struct Rig{std::string root;Witness w{};Mount m{};unsigned calls=0;Service svc;
 Rig(const fs::path& p,const char* cur):root(p.string()),svc(m,w,root,calls){w.e=witness().e;w.e.mountEpoch=20;w.e.ownerSerial=101;w.current=cur;m.id={w.e.mediaTag,w.e.mountEpoch,w.e.ownerSerial,true,true,true};}
 void claim(const char* dir,size_t packets,size_t receipts){w.claims.push_back({dir,{w.e.mediaTag,7,99,2,packets*88,receipts*40}});w.count=w.claims.size();}
 void ready(){CHECK(svc.start(2,w.current)==R::Done);CHECK(svc.discover()==R::Done);finish();}
 void finish(){for(unsigned i=0;i<800;++i){auto r=svc.poll(Verify{});if(r==R::Done)return;CHECK(r==R::Progress);}CHECK(false);}
 Z::Summary summary(){Z::Summary s{};CHECK(svc.summary(s)==R::Done&&s.completed);return s;}
};
static void fixture(const fs::path& p){CHECK(!fs::exists(p));CHECK(fs::create_directories(p/"THESIS/F06TEST/V002"));}
static void enqueue(Q& q,unsigned e,bool cancel=false){CHECK(q.capture(control(e,cancel),0)==M::CaptureResult::Queued);}
static void append(Rig& r,Q& q,unsigned e,bool cancel=false){enqueue(q,e,cancel);CHECK(r.svc.append(q)==R::Done&&q.pending()==0);}
static void seed(const fs::path& p,const char* dir,const std::vector<VesselPacket>& packets,const std::vector<N::Receipt>& receipts={}){
 CHECK(fs::create_directory(p/std::string(dir+1)));HostSD sd(p);std::vector<uint8_t> data;
 for(auto& pkt:packets){uint8_t f[M::kFrameBytes]{};CHECK(M::encode(entry(pkt),f));data.insert(data.end(),f,f+sizeof(f));}if(!data.empty())put(packetFile(sd,dir),data);data.clear();
 for(auto& rc:receipts){uint8_t f[N::kReceiptBytes]{};CHECK(N::encodeReceipt(rc,f));data.insert(data.end(),f,f+sizeof(f));}if(!data.empty())put(receiptFile(sd,dir),data);
}
static void admission(const fs::path& p){
 fixture(p);Rig r(p,S1);Q q;CHECK(r.svc.start(2,S1)==R::Done);enqueue(q,1);CHECK(r.svc.append(q)==R::Deferred&&q.pending()==1);HostSD sd(p);CHECK(!fs::exists(packetFile(sd,S1)));CHECK(r.svc.discover()==R::Done);r.finish();CHECK(r.svc.append(q)==R::Done&&q.pending()==0);
 for(unsigned i=2;i<=16;++i)append(r,q,i);
 auto s=r.summary();CHECK(s.pending==16&&s.capacity.occupied==16&&s.capacity.missingPartners==16&&!s.capacity.overBudget&&s.capacity.activeFrames==16);
 // Exact duplicate is already persisted; does not consume another frame.
 append(r,q,1);CHECK(r.summary().capacity.activeFrames==16);
 for(unsigned i=1;i<=16;++i)append(r,q,i,true);
 s=r.summary();CHECK(s.pending==32&&s.capacity.occupied==32&&s.capacity.missingPartners==0&&s.capacity.activeFrames==32);
 auto original=bytes(packetFile(sd,S1));CHECK(original.size()==32*88);enqueue(q,17);CHECK(r.svc.append(q)==R::CapacityBlocked&&q.pending()==1);CHECK(r.svc.append(q)==R::CapacityBlocked&&q.pending()==1);CHECK(bytes(packetFile(sd,S1))==original);CHECK(r.summary().pending==32);CHECK(r.svc.shutdown()==R::Done);
}
static void reserve(const fs::path& p){fixture(p);Rig r(p,S1);r.ready();Q q;for(unsigned i=1;i<=16;++i)append(r,q,i);enqueue(q,17);CHECK(r.svc.append(q)==R::CapacityBlocked&&q.pending()==1);CHECK(r.summary().capacity.missingPartners==16);CHECK(r.summary().capacity.activeFrames==16);CHECK(r.svc.shutdown()==R::Done);}
static void restartSeed(const fs::path& p){
 fixture(p);seed(p,S1,{control(1,true),control(1)},{receipt(1,true),receipt(1)});HostSD sd(p);auto old=bytes(packetFile(sd,S1));auto oldr=bytes(receiptFile(sd,S1));
 Rig r(p,S2);r.claim(S1,2,2);r.ready();auto s=r.summary();CHECK(s.pending==0&&s.delivered==2&&s.capacity.archive==F::ArchiveState::NeedsClosedArchive);Q q;append(r,q,1);CHECK(!fs::exists(packetFile(sd,S2)));append(r,q,2,true);CHECK(r.summary().capacity.missingPartners==1);CHECK(r.svc.receipt(receipt(2,true),Verify{})==R::Done);CHECK(r.summary().capacity.archive==F::ArchiveState::Unresolved);append(r,q,2);CHECK(r.svc.receipt(receipt(2),N::DenyReceiptVerifier{})==R::Rejected);CHECK(r.svc.receipt(receipt(2),Verify{})==R::Done);s=r.summary();CHECK(s.delivered==4&&s.pending==0&&s.capacity.activeFrames==2&&s.capacity.activeReceipts==2&&s.capacity.archive==F::ArchiveState::NeedsClosedArchive);CHECK(r.svc.shutdown()==R::Done);CHECK(bytes(packetFile(sd,S1))==old&&bytes(receiptFile(sd,S1))==oldr);
}
static void restartRead(const fs::path& p){
 HostSD sd(p);auto old=bytes(packetFile(sd,S1)),oldr=bytes(receiptFile(sd,S1)),newer=bytes(packetFile(sd,S2)),newerr=bytes(receiptFile(sd,S2));CHECK(old.size()==176&&oldr.size()==80&&newer.size()==176&&newerr.size()==80);
 // Fresh process: compare the exact expected frame bytes before invoking replay.
 for(unsigned i=1;i<=2;++i){const auto& pf=i==1?old:newer;const auto& rf=i==1?oldr:newerr;for(unsigned j=0;j<2;++j){bool cancel=j==0;uint8_t f[M::kFrameBytes]{},rc[N::kReceiptBytes]{};CHECK(M::encode(entry(control(i,cancel)),f));CHECK(N::encodeReceipt(receipt(i,cancel),rc));CHECK(!memcmp(pf.data()+j*88,f,88)&&!memcmp(rf.data()+j*40,rc,40));}}
 Rig r(p,S3);r.claim(S1,2,2);r.claim(S2,2,2);r.ready();auto s=r.summary();CHECK(s.delivered==4&&s.pending==0&&s.capacity.occupied==4&&s.capacity.activeFrames==0&&s.capacity.archive==F::ArchiveState::NeedsClosedArchive);Q q;append(r,q,2);CHECK(!fs::exists(packetFile(sd,S3)));CHECK(r.svc.shutdown()==R::Done);CHECK(bytes(packetFile(sd,S1))==old&&bytes(receiptFile(sd,S1))==oldr&&bytes(packetFile(sd,S2))==newer&&bytes(receiptFile(sd,S2))==newerr);
}
static void inventory(const fs::path& p){fixture(p);{Rig r(p,S1);r.w.count=12;CHECK(r.svc.start(2,S1)==R::Rejected&&r.calls==0);CHECK(!fs::exists(p/std::string(S1+1)));r.w.countValid=false;CHECK(r.svc.start(2,S1)==R::Rejected&&r.calls==0);}
 Rig r(p,S1);for(unsigned i=0;i<11;++i){char n[64];snprintf(n,sizeof(n),"/THESIS/F06TEST/V002/S%07X",i+100);seed(p,n,{});r.claim(n,0,0);}r.ready();CHECK(r.summary().sessions==11);CHECK(r.svc.shutdown()==R::Done);Rig after(p,S2);after.w.count=12;CHECK(after.svc.start(2,S2)==R::Rejected&&after.calls==0);CHECK(!fs::exists(p/std::string(S2+1)));}
static void unverified(const fs::path& p){fixture(p);auto bad=receipt(1);bad.serverTag=14;seed(p,S1,{control(1),control(1,true)},{bad,receipt(1),receipt(1,true)});Rig r(p,S2);r.claim(S1,2,3);r.ready();auto s=r.summary();CHECK(s.pending==0&&s.delivered==2&&s.capacity.archive==F::ArchiveState::Unverified);CHECK(r.svc.shutdown()==R::Done);}
static void conflict(const fs::path& p){fixture(p);seed(p,S1,{control(1)});Rig r(p,S2);r.claim(S1,1,0);r.ready();Q q;auto pkt=control(1);pkt.timestamp++;CHECK(q.capture(pkt,0)==M::CaptureResult::Queued);CHECK(r.svc.append(q)==R::Fault&&q.pending()==1);HostSD sd(p);CHECK(!fs::exists(packetFile(sd,S2)));Z::Summary s{};CHECK(r.svc.summary(s)==R::Fault&&!s.completed);}
static void overbudget(const fs::path& p){fixture(p);std::vector<VesselPacket> old;for(unsigned i=1;i<=17;++i)old.push_back(control(i));seed(p,S1,old);Rig r(p,S2);r.claim(S1,17,0);r.ready();CHECK(r.summary().capacity.overBudget);Q q;append(r,q,1,true);CHECK(r.summary().capacity.overBudget);enqueue(q,18);CHECK(r.svc.append(q)==R::CapacityBlocked&&q.pending()==1);CHECK(r.svc.shutdown()==R::Done);}
static void fullDelivered(const fs::path& p){fixture(p);std::vector<VesselPacket> packets;std::vector<N::Receipt> receipts;for(unsigned i=1;i<=16;++i){packets.push_back(control(i));packets.push_back(control(i,true));receipts.push_back(receipt(i));receipts.push_back(receipt(i,true));}seed(p,S1,packets,receipts);Rig r(p,S2);r.claim(S1,32,32);r.ready();auto s=r.summary();CHECK(s.delivered==32&&s.pending==0&&s.capacity.occupied==32&&s.capacity.archive==F::ArchiveState::NeedsClosedArchive);Q q;enqueue(q,17);CHECK(r.svc.append(q)==R::CapacityBlocked&&q.pending()==1);HostSD sd(p);CHECK(!fs::exists(packetFile(sd,S2)));CHECK(bytes(packetFile(sd,S1)).size()==32*88&&bytes(receiptFile(sd,S1)).size()==32*40);CHECK(r.svc.shutdown()==R::Done);}
static void drift(const fs::path& p){fixture(p);Rig r(p,S1);CHECK(r.svc.start(2,S1)==R::Done);r.w.count=1;CHECK(r.svc.discover()==R::Fault);Q q;enqueue(q,1);CHECK(r.svc.append(q)==R::Fault&&q.pending()==1);HostSD sd(p);CHECK(!fs::exists(packetFile(sd,S1)));CHECK(r.svc.shutdown()==R::Fault);}
#ifdef THESIS_F26C_FIXTURE
int f26b_fixture_main(int argc,char** argv)
#else
int main(int argc,char** argv)
#endif
{if(argc!=3)return 2;std::string phase=argv[1];fs::path p=argv[2];if(phase=="admission")admission(p);else if(phase=="reserve")reserve(p);else if(phase=="restart_seed")restartSeed(p);else if(phase=="restart_read")restartRead(p);else if(phase=="inventory")inventory(p);else if(phase=="unverified")unverified(p);else if(phase=="conflict")conflict(p);else if(phase=="overbudget")overbudget(p);else if(phase=="full_delivered")fullDelivered(p);else if(phase=="drift")drift(p);else return 2;printf("F26B %s PASS %u checks; host only\n",phase.c_str(),checks);return 0;}
