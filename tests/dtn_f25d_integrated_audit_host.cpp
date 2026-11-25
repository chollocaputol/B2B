// A-07 integrated host audit; uses actual F25B service and F25C codecs.
// All directories must be new private disposable host fixtures.
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
struct Driver{const std::string* root_;unsigned* calls;Driver(const std::string& root,unsigned& n):root_(&root),calls(&n){}const char* root()const{return root_->c_str();}bool mount(){++*calls;return fs::is_directory(*root_);}bool unmount(){++*calls;return true;}};
struct Witness:SimulatedWitness{const char* current=nullptr;size_t count=0;B::Closure a{},b{};
 bool fresh(uint32_t v,const char* dir)const{return v==2 && current && !strcmp(current,dir);}
 bool closedCount(size_t& n)const{n=count;return true;}
 bool closed(const char* dir,B::Closure& out)const{if(count>=1 && !strcmp(dir,S1)){out=a;return true;}if(count>=2 && !strcmp(dir,S2)){out=b;return true;}return false;}
};
using Backend=Y::Backend<Driver>;
struct SeparateMount { W::Identity id{}; W::Identity view() const {return id;} };
using Integrated=B::Service<Backend,SeparateMount,Witness>;
struct AuditRig {
 std::string root; Witness w{}; SeparateMount m{}; unsigned calls=0; Integrated svc;
 AuditRig(const fs::path& p,const char* current,size_t count):root(p.string()),svc(m,w,root,calls){
  w.e=witness().e;w.current=current;w.count=count;w.e.mountEpoch=9;w.e.ownerSerial=101;
  w.a={w.e.mediaTag,7,99,2,0,0};w.b={w.e.mediaTag,8,100,2,0,0};
  m.id={w.e.mediaTag,w.e.mountEpoch,w.e.ownerSerial,true,true,true};
 }
};
static void fixture(const fs::path& p){CHECK(!fs::exists(p));CHECK(fs::create_directories(p/"THESIS/F06TEST/V002"));}
static void frames(const fs::path& p,const char* session,const std::vector<VesselPacket>& packets){
 CHECK(fs::create_directory(p/std::string(session+1)));HostSD sd(p);std::vector<uint8_t> data;
 for(const auto& v:packets){uint8_t f[M::kFrameBytes]{};CHECK(M::encode(entry(v),f));data.insert(data.end(),f,f+sizeof(f));}
 put(packetFile(sd,session),data);
}
static R finish(Integrated& svc){for(unsigned i=0;i<160;++i){auto r=svc.poll(Verify{});if(r!=R::Progress)return r;}CHECK(false);return R::Fault;}
static void cross(const fs::path& p){
 fixture(p);auto s=sos(2,17),c=cancelPacket(2,17);frames(p,S1,{c});frames(p,S2,{s,c});
 HostSD sd(p);auto a=bytes(packetFile(sd,S1)),b=bytes(packetFile(sd,S2));AuditRig r(p,S3,2);r.w.a.packetBytes=a.size();r.w.b.packetBytes=b.size();
 CHECK(r.svc.start(2,S3)==R::Done);CHECK(r.svc.discover()==R::Done);CHECK(finish(r.svc)==R::Done);Z::Summary sum{};CHECK(r.svc.summary(sum)==R::Done && sum.completed && sum.pending==2 && sum.delivered==0 && sum.sessions==2);
 auto sr=receipt(entry(s),11),cr=receipt(entry(c),12);auto wrong=cr;wrong.type=PACKET_SOS;
 CHECK(r.svc.receipt(wrong,Verify{})==R::Rejected);CHECK(r.svc.receipt(cr,N::DenyReceiptVerifier{})==R::Rejected);CHECK(!fs::exists(receiptFile(sd,S3)));
 CHECK(r.svc.receipt(cr,Verify{})==R::Done);CHECK(r.svc.summary(sum)==R::Done && sum.pending==1 && sum.delivered==1);CHECK(r.svc.receipt(sr,Verify{})==R::Done);CHECK(r.svc.summary(sum)==R::Done && sum.pending==0 && sum.delivered==2);
 r.w.e.radioBusy=true;CHECK(r.svc.summary(sum)==R::Deferred && !sum.completed);r.w.e.radioBusy=false;CHECK(r.svc.summary(sum)==R::Done && sum.completed);CHECK(r.svc.shutdown()==R::Done);CHECK(r.svc.summary(sum)==R::Rejected && !sum.completed);CHECK(bytes(packetFile(sd,S1))==a && bytes(packetFile(sd,S2))==b);CHECK(bytes(receiptFile(sd,S3)).size()==80);
}
static void conflict(const fs::path& p){
 fixture(p);auto c=cancelPacket(2,17),other=c;other.timestamp++;frames(p,S1,{c});frames(p,S2,{other});AuditRig r(p,S3,2);r.w.a.packetBytes=r.w.b.packetBytes=88;
 CHECK(r.svc.start(2,S3)==R::Done);CHECK(r.svc.discover()==R::Done);CHECK(finish(r.svc)==R::Fault);Z::Summary sum{};CHECK(r.svc.summary(sum)==R::Fault && !sum.completed);CHECK(r.svc.shutdown()==R::Fault);CHECK(r.calls==1);
}
static void capacity(const fs::path& p){
 fixture(p);std::vector<VesselPacket> a,b;for(unsigned i=1;i<=32;++i)a.push_back(sos(2,i));b.push_back(cancelPacket(2,1));frames(p,S1,a);frames(p,S2,b);AuditRig r(p,S3,2);r.w.a.packetBytes=32*88;r.w.b.packetBytes=88;
 CHECK(r.svc.start(2,S3)==R::Done);CHECK(r.svc.discover()==R::Done);CHECK(finish(r.svc)==R::Fault);Z::Summary sum{};CHECK(r.svc.summary(sum)==R::Fault && !sum.completed);CHECK(r.calls==1);
}
static void revoked(const fs::path& p,bool reproduce){
 fixture(p);frames(p,S1,{sos(2,17)});AuditRig r(p,S2,1);r.w.a.packetBytes=88;CHECK(r.svc.start(2,S2)==R::Done);CHECK(r.svc.discover()==R::Done);CHECK(finish(r.svc)==R::Done);Z::Summary s{};CHECK(r.svc.summary(s)==R::Done && s.completed && s.pending==1);
 // Independent mount witness changes BETWEEN calls. Closure witness unchanged.
 r.m.id.authorized=false;auto result=r.svc.summary(s);
 if(reproduce){CHECK(result==R::Done && s.completed && s.pending==1);printf("REPRODUCED: stale completed summary after mount authority revocation\n");}
 else {CHECK(result==R::Fault && !s.completed && s.pending==0 && s.delivered==0);}
 // Restoring evidence must not clear a detected fault in the repaired version.
 if(!reproduce){r.m.id.authorized=true;CHECK(r.svc.summary(s)==R::Fault && !s.completed);}
 M::PendingQueue<8> q;CHECK(q.capture(cancelPacket(2,17),1)==C::Queued);CHECK(r.svc.append(q)==R::Fault && q.pending()==1);CHECK(r.svc.shutdown()==R::Fault);CHECK(r.calls==1);
}
static void mismatch(const fs::path& p,bool reproduce,unsigned kind=0){
 fixture(p);AuditRig r(p,S1,0);
 if(kind==0)++r.m.id.media;else if(kind==1)++r.m.id.epoch;else if(kind==2)++r.m.id.owner;else if(kind==3)r.m.id.authorized=false;else if(kind==4)r.m.id.exclusive=false;else r.m.id.allPathsInterposed=false;
 CHECK(r.svc.start(2,S1)==R::Fault);
 const bool created=fs::exists(p/std::string(S1+1));
 if(reproduce){CHECK(r.calls==1 && created);printf("REPRODUCED: mismatched mount identity creates a directory before rejection\n");}
 else {CHECK(r.calls==0 && !created);}
 CHECK(r.svc.shutdown()==R::Fault);
 CHECK(r.calls==(reproduce?1u:0u));
}
int main(int argc,char** argv){if(argc!=3)return 2;std::string mode=argv[1];fs::path p=argv[2];if(mode=="cross")cross(p);else if(mode=="conflict")conflict(p);else if(mode=="capacity")capacity(p);else if(mode=="reproduce")revoked(p,true);else if(mode=="revoked")revoked(p,false);else if(mode=="mismatch_reproduce")mismatch(p,true);else if(mode=="mismatch"){for(unsigned i=0;i<6;++i)mismatch(p/std::to_string(i),false,i);}else return 2;printf("A07 %s PASS %u checks; host only\n",mode.c_str(),checks);}
