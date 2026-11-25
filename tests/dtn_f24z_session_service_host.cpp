// Real host files + simulated mount/identity; no board or card.
#define main f24o_fixture_main
#include "dtn_f24o_owned_port_e2e.cpp"
#undef main
#include "../firmware/shared/storage/dtn_f24y_checked_vfs.h"
#include "../firmware/shared/storage/dtn_f24z_session_service.h"
namespace Z=DTNF24Z;namespace Y=DTNF24Y;namespace W=DTNF24W;
using R=Z::Result;
struct MW {const SimulatedWitness* w;W::Identity view()const{auto e=w->view();return {e.mediaTag,e.mountEpoch,e.ownerSerial,e.mounted,e.exclusive,e.independentOwner};}};
enum class Fault {None,Open,Next,Close,Loss};
struct Control {Fault f=Fault::None;unsigned mount=0,unmount=0,opens=0,reads=0,closes=0;SimulatedWitness* w=nullptr;void (*duringRead)(void*)=nullptr;void* ctx=nullptr;};
struct Driver {const std::string* root_;Control* c;Driver(const std::string& r,Control& ctrl):root_(&r),c(&ctrl){}const char* root()const{return root_->c_str();}bool mount(){++c->mount;return fs::is_directory(*root_);}bool unmount(){++c->unmount;return true;}};
struct Sys:Y::NativeVfs {static Control* c;
 static DIR* dirOpen(const char* p){++c->opens;return c->f==Fault::Open?nullptr:NativeVfs::dirOpen(p);}
 static struct dirent* dirNext(DIR* d){++c->reads;if(c->duringRead)c->duringRead(c->ctx);if(c->f==Fault::Next){errno=EIO;return nullptr;}if(c->f==Fault::Loss)++c->w->e.mountEpoch;return NativeVfs::dirNext(d);}
 static int dirClose(DIR* d){++c->closes;int rc=NativeVfs::dirClose(d);return c->f==Fault::Close?-1:rc;}
};Control* Sys::c=nullptr;
using Backend=Y::Backend<Driver,Sys>;using Service=Z::Service<Backend,MW,SimulatedWitness>;
struct Rig{std::string root;SimulatedWitness w=witness();MW m{&w};Control c{};Service service;
 explicit Rig(const fs::path& p):root(p.string()),service(m,w,root,c){c.w=&w;Sys::c=&c;}
};
static void prep(const fs::path& p){CHECK(!fs::exists(p));CHECK(fs::create_directories(p));sessions(p);}
template<class Verifier> static void replayAll(Rig& r,const Verifier& verifier){for(unsigned i=0;i<60;++i){auto res=r.service.poll(verifier);CHECK(res==R::Done||res==R::Progress);if(res==R::Done)return;}CHECK(false);}
static void init(const fs::path& p){prep(p);Rig r(p);DTNF24M::PendingQueue<8> q;CHECK(r.service.start(2,S1)==R::Done);CHECK(r.service.discover()==R::Done);CHECK(r.c.opens==1&&r.c.closes==1);CHECK(r.service.append(q)==R::Empty);CHECK(q.capture(sos(2,17),DTNF24M::kLocal)==DTNF24M::CaptureResult::Queued);CHECK(r.service.append(q)==R::Done);CHECK(q.pending()==0);CHECK(r.service.shutdown()==R::Done);CHECK(r.c.unmount==1);}
static void reentrant(void* ptr){auto& r=*static_cast<Rig*>(ptr);Z::Summary s{};CHECK(r.service.summary(s)==R::Deferred);CHECK(r.service.shutdown()==R::Deferred);r.c.duringRead=nullptr;}
static void replay(const fs::path& p){Rig r(p);CHECK(r.service.start(2,S2)==R::Done);r.w.e.radioBusy=true;CHECK(r.service.discover()==R::Deferred);CHECK(r.c.opens==0);r.w.e.radioBusy=false;r.w.e.urgentPending=true;CHECK(r.service.discover()==R::Deferred);CHECK(r.c.opens==0);r.w.e.urgentPending=false;r.c.ctx=&r;r.c.duringRead=reentrant;CHECK(r.service.discover()==R::Done);CHECK(r.c.closes==1);CHECK(r.service.discover()==R::Rejected);r.w.e.radioBusy=true;CHECK(r.service.poll(DTNF24N::DenyReceiptVerifier{})==R::Deferred);r.w.e.radioBusy=false;replayAll(r,DTNF24N::DenyReceiptVerifier{});Z::Summary s{};CHECK(r.service.summary(s)==R::Done);CHECK(s.completed&&s.sessions==6&&s.pending==1&&s.delivered==0);DTNF24N::Receipt receipt{};CHECK(r.service.receipt(receipt,DTNF24N::DenyReceiptVerifier{})==R::Rejected);
 HostSD sd(p);const auto data=bytes(packetFile(sd,S1));DTNF24M::PacketEntry e{};CHECK(DTNF24M::decode(data.data(),data.size(),e));
 const auto accepted=serverAccept(p,e);CHECK(r.service.receipt(accepted,DTNF24N::DenyReceiptVerifier{})==R::Rejected);CHECK(r.service.receipt(accepted,FakeShoreVerifier{p})==R::Done);CHECK(r.service.summary(s)==R::Done);CHECK(s.pending==0&&s.delivered==1);CHECK(r.service.shutdown()==R::Done);}
static void restart(const fs::path& p){Rig r(p);CHECK(r.service.start(2,S3)==R::Done);CHECK(r.service.discover()==R::Done);replayAll(r,FakeShoreVerifier{p});Z::Summary s{};CHECK(r.service.summary(s)==R::Done);CHECK(s.pending==0&&s.delivered==1);CHECK(r.service.shutdown()==R::Done);}
static void failure(const fs::path& p,const std::string& phase){prep(p);Rig r(p);CHECK(r.service.start(2,S2)==R::Done);if(phase=="badname")fs::create_directory(p/"THESIS/F06TEST/V002/INVALID");if(phase=="missing")fs::remove(p/std::string(S2+1));if(phase=="overflow")for(int i=0;i<13;++i){char n[16];snprintf(n,sizeof(n),"S%07X",unsigned(i+100));fs::create_directory(p/"THESIS/F06TEST/V002"/n);}if(phase=="open")r.c.f=Fault::Open;if(phase=="next")r.c.f=Fault::Next;if(phase=="close")r.c.f=Fault::Close;if(phase=="loss")r.c.f=Fault::Loss;CHECK(r.service.discover()==R::Fault);Z::Summary s{};CHECK(r.service.summary(s)==R::Fault);CHECK(r.service.shutdown()==R::Fault);CHECK(r.c.unmount==0);CHECK(r.c.opens==1);CHECK(r.c.closes==((phase=="loss"||phase=="open")?0u:1u));}
static void denial(const fs::path& p){prep(p);Rig r(p);r.w.e.exclusive=false;CHECK(r.service.start(2,S1)==R::Rejected);CHECK(r.c.mount==0);r.w.e.exclusive=true;r.w.e.radioBusy=true;CHECK(r.service.start(2,S1)==R::Deferred);CHECK(r.c.mount==0);r.w.e.radioBusy=false;CHECK(r.service.start(2,S1)==R::Done);DTNF24M::PendingQueue<8> q;CHECK(q.capture(sos(2,17),DTNF24M::kLocal)==DTNF24M::CaptureResult::Queued);r.w.e.radioBusy=true;CHECK(r.service.append(q)==R::Deferred);CHECK(q.pending()==1);r.w.e.radioBusy=false;CHECK(r.service.append(q)==R::Done);CHECK(q.pending()==0);r.w.e.mediaTag++;CHECK(r.service.shutdown()==R::Fault);CHECK(r.c.unmount==0);}
int main(int argc,char** argv){if(argc!=3)return 2;std::string phase=argv[1];fs::path p=argv[2];if(phase=="init")init(p);else if(phase=="replay")replay(p);else if(phase=="restart")restart(p);else if(phase=="deny")denial(p);else if(phase=="badname"||phase=="missing"||phase=="overflow"||phase=="open"||phase=="next"||phase=="close"||phase=="loss")failure(p,phase);else return 2;printf("F24Z %s PASS %u checks; host files only\n",phase.c_str(),checks);}
