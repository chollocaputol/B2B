// Reuse the actual F26B service fixtures; no alternate owner or synthetic ledger.
#define THESIS_F26C_FIXTURE
#include "dtn_f26b_capacity_host.cpp"
#undef THESIS_F26C_FIXTURE
// Maintenance grant is distinct from fresh writable-session reservation.
struct MaintenanceWitness:Witness {
 bool allowed=true;
 bool maintenance(uint32_t v,const char* d)const{return allowed && v==2 && current && !strcmp(current,d);}
};
using MaintenanceService=B::Service<Y::Backend<Driver>,Mount,MaintenanceWitness>;
struct MaintenanceRig {
 std::string root;MaintenanceWitness w{};Mount m{};unsigned calls=0;MaintenanceService svc;
 MaintenanceRig(const fs::path& p,const char* cur):root(p.string()),svc(m,w,root,calls){w.e=witness().e;w.e.mountEpoch=20;w.e.ownerSerial=101;w.current=cur;m.id={w.e.mediaTag,w.e.mountEpoch,w.e.ownerSerial,true,true,true};}
 void claim(const char* dir,size_t packets,size_t receipts){w.claims.push_back({dir,{w.e.mediaTag,7,99,2,packets*88,receipts*40}});w.count=w.claims.size();}
 void ready(){CHECK(svc.startMaintenance(2,w.current)==R::Done);CHECK(svc.discover()==R::Done);}
};
static void bypass(const fs::path& p){
 fixture(p);Rig r(p,S1);r.ready();Q q;
 for(unsigned i=1;i<=16;++i)append(r,q,i);
 enqueue(q,17);enqueue(q,18);enqueue(q,1,true);enqueue(q,19);enqueue(q,2,true);
 CHECK(q.pending()==5 && q.front()->event==17);CHECK(r.svc.append(q)==R::Done);
 CHECK(q.pending()==4 && q.at(0)->event==17 && q.at(1)->event==18 && q.at(2)->event==19 && q.at(3)->event==2);
 CHECK(r.svc.append(q)==R::Done && q.pending()==3 && q.at(0)->event==17 && q.at(1)->event==18 && q.at(2)->event==19);
 auto s=r.summary();CHECK(s.capacity.occupied==18 && s.capacity.missingPartners==14 && s.capacity.activeFrames==18);
 HostSD sd(p);auto raw=bytes(packetFile(sd,S1));CHECK(raw.size()==18*88);
 for(unsigned i=0;i<2;++i){M::PacketEntry pkt{};CHECK(M::decode(raw.data()+(16+i)*88,88,pkt));CHECK(pkt.event==i+1 && M::recordType(pkt)==PACKET_SOS_CANCEL);}
 CHECK(r.svc.append(q)==R::CapacityBlocked && q.pending()==3 && bytes(packetFile(sd,S1))==raw);
 enqueue(q,3);CHECK(r.svc.append(q)==R::Done && q.pending()==3 && bytes(packetFile(sd,S1))==raw);
 CHECK(q.lost()==0 && r.svc.shutdown()==R::Done);
}
static void wrap(const fs::path& p){
 fixture(p);Rig r(p,S1);r.ready();Q q;
 for(unsigned i=1;i<=15;++i)append(r,q,i);
 enqueue(q,16);enqueue(q,17);enqueue(q,18);enqueue(q,19);enqueue(q,16,true);enqueue(q,20);enqueue(q,1,true);enqueue(q,21);
 CHECK(r.svc.append(q)==R::Done && q.pending()==7 && q.front()->event==17);
 CHECK(r.svc.append(q)==R::Done && q.pending()==6 && q.at(0)->event==17 && q.at(3)->event==20);
 CHECK(r.svc.append(q)==R::Done && q.pending()==5 && q.at(0)->event==17 && q.at(1)->event==18 && q.at(2)->event==19 && q.at(3)->event==20 && q.at(4)->event==21 && !q.at(5));
 CHECK(r.svc.append(q)==R::CapacityBlocked && q.pending()==5);CHECK(r.svc.shutdown()==R::Done);
}
static void writeFailure(const fs::path& p){
 fixture(p);Rig r(p,S1);r.ready();Q q;for(unsigned i=1;i<=16;++i)append(r,q,i);
 enqueue(q,17);enqueue(q,1,true);HostSD sd(p);auto original=bytes(packetFile(sd,S1));original.push_back(42);put(packetFile(sd,S1),original);
 CHECK(r.svc.append(q)==R::Fault && q.pending()==2 && q.at(0)->event==17 && q.at(1)->event==1 && M::recordType(*q.at(1))==PACKET_SOS_CANCEL);
 CHECK(bytes(packetFile(sd,S1))==original);CHECK(r.svc.append(q)==R::Fault && q.pending()==2);
}
static void conflictBehind(const fs::path& p){
 fixture(p);Rig r(p,S1);r.ready();Q q;for(unsigned i=1;i<=16;++i)append(r,q,i);
 enqueue(q,17);auto bad=control(1);bad.timestamp++;CHECK(q.capture(bad,0)==M::CaptureResult::Queued);enqueue(q,2,true);
 HostSD sd(p);auto original=bytes(packetFile(sd,S1));CHECK(r.svc.append(q)==R::Fault&&q.pending()==3);CHECK(bytes(packetFile(sd,S1))==original);
}
static void fullInventory(const fs::path& p){
 fixture(p);MaintenanceRig r(p,S1);HostSD sd(p);
 std::vector<std::string> dirs;std::vector<std::vector<uint8_t>> originals,receipts;
 for(unsigned i=0;i<12;++i){char d[64];snprintf(d,sizeof(d),"/THESIS/F06TEST/V002/S%07X",i+100);dirs.emplace_back(d);
  std::vector<VesselPacket> ps;std::vector<N::Receipt> rs;
  for(unsigned j=0;j<3;++j){unsigned index=i*3+j;unsigned event=index/2+1;bool cancel=index%2;ps.push_back(control(event,cancel));rs.push_back(receipt(event,cancel));}
  seed(p,d,ps,rs);r.claim(d,3,3);originals.push_back(bytes(packetFile(sd,d)));receipts.push_back(bytes(receiptFile(sd,d)));
 }
 CHECK(r.svc.start(2,S1)==R::Rejected && r.calls==0);
 size_t count=999;CHECK(r.svc.archiveCount(count)==R::Rejected && count==0);
 r.ready();CHECK(r.calls==1 && !fs::exists(p/std::string(S1+1)));
 CHECK(r.svc.archiveCount(count)==R::Done&&count==12);
 Q q;enqueue(q,99);CHECK(r.svc.append(q)==R::Rejected&&q.pending()==1);CHECK(r.svc.receipt(receipt(1),Verify{})==R::Rejected);CHECK(r.svc.poll(Verify{})==R::Rejected);
 for(size_t i=0;i<count;++i){B::ArchiveSession meta{};CHECK(r.svc.archiveSession(i,meta)==R::Done&&meta.count==12&&meta.path==dirs[i]&&meta.closure.packetBytes==264&&meta.closure.receiptBytes==120&&meta.closure.closedEpoch==7&&meta.closure.closeSerial==99);
  for(bool rc:{false,true}){std::vector<uint8_t> copied;for(size_t j=0;j<3;++j){Z::ArchiveFrame f{};CHECK(r.svc.archiveFrame(i,rc,j,f)==R::Done&&f.length==(rc?40u:88u));copied.insert(copied.end(),f.bytes,f.bytes+f.length);}CHECK(copied==(rc?receipts[i]:originals[i]));
   Z::ArchiveFrame invalid{};invalid.length=123;CHECK(r.svc.archiveFrame(i,rc,3,invalid)==R::Rejected&&invalid.length==0);
  }
 }
 Z::ArchiveFrame f{};CHECK(r.svc.archiveFrame(12,false,0,f)==R::Rejected&&f.length==0);B::ArchiveSession m{};CHECK(r.svc.archiveSession(12,m)==R::Rejected&&m.count==0);
 CHECK(r.svc.shutdown()==R::Done&&r.calls==2);CHECK(r.svc.archiveFrame(0,false,0,f)==R::Rejected&&f.length==0);
 CHECK(!fs::exists(p/std::string(S1+1)));for(size_t i=0;i<dirs.size();++i){CHECK(bytes(packetFile(sd,dirs[i].c_str()))==originals[i] && bytes(receiptFile(sd,dirs[i].c_str()))==receipts[i]);}
}
static void absence(const fs::path& p){fixture(p);MaintenanceRig r(p,S2);seed(p,S1,{});r.claim(S1,0,0);r.ready();B::ArchiveSession m{};CHECK(r.svc.archiveSession(0,m)==R::Done&&m.closure.packetBytes==0&&m.closure.receiptBytes==0);Z::ArchiveFrame f{};CHECK(r.svc.archiveFrame(0,false,0,f)==R::Rejected&&f.length==0);CHECK(r.svc.shutdown()==R::Done);}
static void undeclared(const fs::path& p){fixture(p);MaintenanceRig r(p,S2);seed(p,S1,{control(1)});r.claim(S1,0,0);r.ready();B::ArchiveSession m{};CHECK(r.svc.archiveSession(0,m)==R::Fault&&m.count==0);CHECK(r.svc.shutdown()==R::Fault);}
static void corrupted(const fs::path& p){fixture(p);MaintenanceRig r(p,S2);seed(p,S1,{control(1)});HostSD sd(p);auto raw=bytes(packetFile(sd,S1));raw[30]^=1;put(packetFile(sd,S1),raw);r.claim(S1,1,0);r.ready();B::ArchiveSession m{};CHECK(r.svc.archiveSession(0,m)==R::Done);Z::ArchiveFrame f{};f.length=123;CHECK(r.svc.archiveFrame(0,false,0,f)==R::Fault&&f.length==0);CHECK(bytes(packetFile(sd,S1))==raw);}
static void emptyInventory(const fs::path& p){fixture(p);MaintenanceRig r(p,S1);r.ready();size_t count=99;CHECK(r.svc.archiveCount(count)==R::Done&&count==0);B::ArchiveSession m{};CHECK(r.svc.archiveSession(0,m)==R::Rejected&&m.count==0);CHECK(r.svc.shutdown()==R::Done);CHECK(!fs::exists(p/std::string(S1+1)));}
static void missing(const fs::path& p){fixture(p);MaintenanceRig r(p,S2);seed(p,S1,{});r.claim(S1,1,0);r.ready();B::ArchiveSession m{};CHECK(r.svc.archiveSession(0,m)==R::Fault&&m.count==0);}
static void wrongSize(const fs::path& p){fixture(p);MaintenanceRig r(p,S2);seed(p,S1,{control(1),control(1,true)});r.claim(S1,1,0);r.ready();B::ArchiveSession m{};CHECK(r.svc.archiveSession(0,m)==R::Fault&&m.count==0);}
static void wrongReceipt(const fs::path& p){fixture(p);MaintenanceRig r(p,S2);seed(p,S1,{control(1)},{receipt(1)});HostSD sd(p);auto raw=bytes(receiptFile(sd,S1));raw[20]^=1;put(receiptFile(sd,S1),raw);r.claim(S1,1,1);r.ready();Z::ArchiveFrame f{};CHECK(r.svc.archiveFrame(0,true,0,f)==R::Fault&&f.length==0);CHECK(bytes(receiptFile(sd,S1))==raw);}
static void busyAndDrift(const fs::path& p){fixture(p);MaintenanceRig r(p,S2);seed(p,S1,{control(1)});r.claim(S1,1,0);r.w.e.radioBusy=true;CHECK(r.svc.startMaintenance(2,S2)==R::Deferred&&r.calls==0);r.w.e.radioBusy=false;r.ready();r.w.e.urgentPending=true;Z::ArchiveFrame f{};CHECK(r.svc.archiveFrame(0,false,0,f)==R::Deferred&&f.length==0);r.w.e.urgentPending=false;CHECK(r.svc.archiveFrame(0,false,0,f)==R::Done&&f.length==88);r.w.claims[0].second.closeSerial++;CHECK(r.svc.archiveFrame(0,false,0,f)==R::Fault&&f.length==0);}
static void denied(const fs::path& p){fixture(p);MaintenanceRig r(p,S1);r.w.allowed=false;CHECK(r.svc.startMaintenance(2,S1)==R::Rejected&&r.calls==0);CHECK(!fs::exists(p/std::string(S1+1)));Rig old(p,S1);CHECK(old.svc.startMaintenance(2,S1)==R::Rejected&&old.calls==0);}
static void collision(const fs::path& p){fixture(p);MaintenanceRig r(p,S1);seed(p,S1,{control(1)});r.claim(S1,1,0);CHECK(r.svc.startMaintenance(2,S1)==R::Done);CHECK(r.svc.discover()==R::Fault);HostSD sd(p);CHECK(bytes(packetFile(sd,S1)).size()==88);}
static void revoked(const fs::path& p){fixture(p);MaintenanceRig r(p,S2);seed(p,S1,{control(1)});r.claim(S1,1,0);r.ready();r.w.allowed=false;Z::ArchiveFrame f{};f.length=1;CHECK(r.svc.archiveFrame(0,false,0,f)==R::Fault&&f.length==0);size_t n=99;CHECK(r.svc.archiveCount(n)==R::Fault&&n==0);}
int main(int argc,char** argv){if(argc!=3)return 2;std::string phase=argv[1];fs::path p=argv[2];if(phase=="bypass")bypass(p);else if(phase=="wrap")wrap(p);else if(phase=="write_failure")writeFailure(p);else if(phase=="conflict")conflictBehind(p);else if(phase=="full_inventory")fullInventory(p);else if(phase=="absence")absence(p);else if(phase=="undeclared")undeclared(p);else if(phase=="corrupt")corrupted(p);else if(phase=="empty_inventory")emptyInventory(p);else if(phase=="missing")missing(p);else if(phase=="wrong_size")wrongSize(p);else if(phase=="wrong_receipt")wrongReceipt(p);else if(phase=="busy_drift")busyAndDrift(p);else if(phase=="denied")denied(p);else if(phase=="collision")collision(p);else if(phase=="revoked")revoked(p);else return 2;printf("F26C %s PASS %u checks; host only\n",phase.c_str(),checks);}
