// A-05: disposable host files, independent synthetic witness, no physical SD.
#define main f24o_fixture_main
#include "dtn_f24o_owned_port_e2e.cpp"
#undef main
#include "../firmware/shared/storage/dtn_f24y_checked_vfs.h"
#include "../firmware/shared/storage/dtn_f25b_session_lifecycle.h"
namespace B=DTNF25B;namespace Z=DTNF24Z;namespace Y=DTNF24Y;namespace W=DTNF24W;using R=Z::Result;
struct MW {const SimulatedWitness* w;W::Identity view()const{auto e=w->view();return {e.mediaTag,e.mountEpoch,e.ownerSerial,e.mounted,e.exclusive,e.independentOwner};}};
struct Control {unsigned mount=0,unmount=0;};
struct Driver {const std::string* root_;Control* c;Driver(const std::string& r,Control& ctrl):root_(&r),c(&ctrl){}const char* root()const{return root_->c_str();}bool mount(){++c->mount;return fs::is_directory(*root_);}bool unmount(){++c->unmount;return true;}};
struct Independent:SimulatedWitness {
 const char* reserved=nullptr; bool allowClosed=false,wrongPath=false; B::Closure claim{};
 bool fresh(uint32_t v,const char* dir)const{return v==2 && reserved && !strcmp(reserved,dir);}
 bool closedCount(size_t& n)const{n=allowClosed?1:0;return true;}
 bool closed(const char* dir,B::Closure& out)const{if(!allowClosed || wrongPath || strcmp(dir,S1))return false;out=claim;return true;}
};
struct BSys:Y::NativeVfs {
 static unsigned creates;
 static bool createDirectory(const char* p){++creates;return Y::NativeVfs::createDirectory(p);}
};unsigned BSys::creates=0;
using BB=Y::Backend<Driver,BSys>;using BS=B::Service<BB,MW,Independent>;
struct BRig {
 std::string root; Independent w{}; MW m{&w};Control c{};BS service;
 explicit BRig(const fs::path& p,const char* current):root(p.string()),service(m,w,root,c){w.e=witness().e;w.reserved=current;BSys::creates=0;}
 void attest(){w.allowClosed=true;w.e.mountEpoch=8;w.e.ownerSerial=100;w.claim={w.e.mediaTag,7,99,2,88,0};}
};
static void base(const fs::path& p){CHECK(!fs::exists(p));CHECK(fs::create_directories(p/"THESIS/F06TEST/V002"));}
static void appendOne(BRig& r){DTNF24M::PendingQueue<8> q;CHECK(q.capture(sos(2,17),0)==DTNF24M::CaptureResult::Queued);CHECK(r.service.append(q)==R::Done);CHECK(q.pending()==0);}
static void initB(const fs::path& p){base(p);BRig r(p,S1);r.w.reserved=nullptr;CHECK(r.service.start(2,S1)==R::Rejected);CHECK(r.c.mount==0 && BSys::creates==0);r.w.reserved=S1;r.w.e.radioBusy=true;CHECK(r.service.start(2,S1)==R::Deferred);CHECK(r.c.mount==0);r.w.e.radioBusy=false;CHECK(r.service.start(2,S1)==R::Done);CHECK(BSys::creates==1 && fs::is_directory(p/std::string(S1+1)));CHECK(r.service.discover()==R::Done);appendOne(r);CHECK(r.service.shutdown()==R::Done);CHECK(r.c.unmount==1);HostSD sd(p);CHECK(bytes(packetFile(sd,S1)).size()==88);}
static R drain(BRig& r){for(unsigned i=0;i<80;++i){auto result=r.service.poll(DTNF24N::DenyReceiptVerifier{});if(result!=R::Progress)return result;}CHECK(false);return R::Fault;}
static void recovery(const fs::path& p){BRig r(p,S2);r.attest();HostSD sd(p);auto original=bytes(packetFile(sd,S1));CHECK(original.size()==88);CHECK(r.service.start(2,S2)==R::Done);r.w.e.urgentPending=true;CHECK(r.service.discover()==R::Deferred);r.w.e.urgentPending=false;CHECK(r.service.discover()==R::Done);CHECK(drain(r)==R::Done);Z::Summary s{};CHECK(r.service.summary(s)==R::Done);CHECK(s.completed && s.pending==1 && s.delivered==0 && s.sessions==1);CHECK(r.service.shutdown()==R::Done);CHECK(bytes(packetFile(sd,S1))==original);CHECK(!fs::exists(packetFile(sd,S2)));}
static void faultB(const fs::path& p,const std::string& phase){
 base(p);{BRig r(p,S1);CHECK(r.service.start(2,S1)==R::Done);appendOne(r);CHECK(r.service.shutdown()==R::Done);}
 HostSD sd(p);auto f=packetFile(sd,S1);auto original=bytes(f);BRig r(p,phase=="collision"?S1:S2);r.attest();
 if(phase=="collision"){CHECK(r.service.start(2,S1)==R::Fault);CHECK(BSys::creates==0);CHECK(bytes(f)==original);CHECK(r.service.shutdown()==R::Fault);return;}
 CHECK(r.service.start(2,S2)==R::Done);
 if(phase=="noattest")r.w.allowClosed=false;
 if(phase=="missingdir")fs::remove_all(p/std::string(S1+1));
 if(phase=="wrongmedia")++r.w.claim.media;
 if(phase=="wrongpath")r.w.wrongPath=true;
 if(phase=="sameepoch")r.w.claim.closedEpoch=r.w.e.mountEpoch;
 if(phase=="badlength")r.w.claim.packetBytes=87;
 if(phase=="overflow")for(unsigned i=0;i<12;++i){char n[16];snprintf(n,sizeof(n),"S%07X",100+i);fs::create_directory(p/"THESIS/F06TEST/V002"/n);}
 if(phase=="missingdir"||phase=="noattest"||phase=="wrongpath"||phase=="wrongmedia"||phase=="sameepoch"||phase=="badlength"||phase=="overflow"){CHECK(r.service.discover()==R::Fault);}
 else {
  CHECK(r.service.discover()==R::Done);
  if(phase=="missing")fs::remove(f);
  if(phase=="torn")fs::resize_file(f,44);
  if(phase=="corrupt"){std::fstream file(f,std::ios::in|std::ios::out|std::ios::binary);file.put('X');CHECK(bool(file));}
  if(phase=="unexpected"){std::ofstream file(receiptFile(sd,S1),std::ios::binary);file.put('X');CHECK(bool(file));}
  if(phase=="revoked")r.w.allowClosed=false;
  if(phase=="changed")++r.w.claim.closeSerial;
  CHECK(drain(r)==R::Fault);
 }
 Z::Summary s{};CHECK(r.service.summary(s)==R::Fault);CHECK(!s.completed);CHECK(r.service.shutdown()==R::Fault);CHECK(r.c.unmount==0);
 DTNF24M::PendingQueue<8> q;CHECK(q.capture(sos(2,18),0)==DTNF24M::CaptureResult::Queued);CHECK(r.service.append(q)==R::Fault);CHECK(q.pending()==1);
 if(phase!="missingdir"&&phase!="missing"&&phase!="torn"&&phase!="corrupt")CHECK(bytes(f)==original);
}
static void capB(const fs::path& p){base(p);BRig r(p,S1);CHECK(r.service.start(2,S1)==R::Done);DTNF24M::PendingQueue<8> q;for(unsigned i=1;i<=32;++i){CHECK(q.capture(sos(2,i),0)==DTNF24M::CaptureResult::Queued);CHECK(r.service.append(q)==R::Done);}CHECK(q.capture(sos(2,33),0)==DTNF24M::CaptureResult::Queued);CHECK(r.service.append(q)==R::Fault);CHECK(q.pending()==1);HostSD sd(p);CHECK(fs::file_size(packetFile(sd,S1))==32*88);}
static void denyB(const fs::path& p){base(p);std::string root=p.string();Control c{};const W::DenyWitness m{};const B::DenyWitness w{};B::Service<BB,W::DenyWitness,B::DenyWitness> service(m,w,root,c);DTNF24M::PendingQueue<8> q;CHECK(q.capture(sos(2,17),0)==DTNF24M::CaptureResult::Queued);CHECK(service.start(2,S1)==R::Rejected);CHECK(service.discover()==R::Rejected);CHECK(service.append(q)==R::Rejected);CHECK(service.poll(DTNF24N::DenyReceiptVerifier{})==R::Rejected);Z::Summary summary{};CHECK(service.summary(summary)==R::Rejected);CHECK(service.shutdown()==R::Rejected);CHECK(q.pending()==1 && c.mount==0 && c.unmount==0 && !fs::exists(p/std::string(S1+1)));}
int main(int argc,char** argv){if(argc!=3)return 2;std::string phase=argv[1];fs::path p=argv[2];if(phase=="init")initB(p);else if(phase=="recovery")recovery(p);else if(phase=="cap")capB(p);else if(phase=="deny")denyB(p);else if(phase=="collision"||phase=="missingdir"||phase=="noattest"||phase=="wrongpath"||phase=="wrongmedia"||phase=="sameepoch"||phase=="badlength"||phase=="overflow"||phase=="missing"||phase=="torn"||phase=="corrupt"||phase=="unexpected"||phase=="revoked"||phase=="changed")faultB(p,phase);else return 2;printf("F25B %s PASS %u checks; host files only\n",phase.c_str(),checks);return 0;}
