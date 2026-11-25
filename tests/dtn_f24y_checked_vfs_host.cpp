// F24Y host integration: same descriptor backend implementation, disposable
// local files; mount/unmount simulated, never an SD device or target driver.
#define main f24o_fixture_main
#include "dtn_f24o_owned_port_e2e.cpp"
#undef main
#include "../firmware/shared/storage/dtn_f24y_checked_vfs.h"
#include "../firmware/shared/storage/dtn_f24x_owner_sos_port.h"
namespace Y=DTNF24Y;namespace W=DTNF24W;
struct MW {const SimulatedWitness* w;W::Identity view()const{const auto e=w->view();return {e.mediaTag,e.mountEpoch,e.ownerSerial,e.mounted,e.exclusive,e.independentOwner};}};
enum class Fault {None,Stat,Open,Size,ShortWrite,ShortRead,Sync,Close,Seek,Mount,Unmount};
struct Control {Fault fault=Fault::None;unsigned mounts=0,unmounts=0,opens=0,closes=0,syncs=0,calls=0;};
struct HostMount {
 const std::string* path;Control* c;
 HostMount(const std::string& p,Control& control):path(&p),c(&control){}
 const char* root()const{return path->c_str();}
 bool mount(){++c->mounts;return c->fault!=Fault::Mount&&fs::is_directory(*path);}
 bool unmount(){++c->unmounts;return c->fault!=Fault::Unmount;}
};
// Fault injection wraps the actual OS operations; no mock successes for I/O.
struct Injected : Y::NativeVfs {
 static Control* c;
 static Y::FileKind kind(const char* p){++c->calls;return c->fault==Fault::Stat?Y::FileKind::Error:NativeVfs::kind(p);}
 static int open(const char* p,Y::Mode m){++c->calls;if(c->fault==Fault::Open)return -1;const int fd=NativeVfs::open(p,m);if(fd>=0)++c->opens;return fd;}
 static bool size(int fd,size_t& n){++c->calls;if(c->fault==Fault::Size)return false;return NativeVfs::size(fd,n);}
 static int64_t write(int fd,const uint8_t* p,size_t n){++c->calls;return NativeVfs::write(fd,p,c->fault==Fault::ShortWrite?n/2:n);}
 static int64_t read(int fd,uint8_t* p,size_t n){++c->calls;return NativeVfs::read(fd,p,c->fault==Fault::ShortRead?n/2:n);}
 static int64_t seek(int fd,size_t n){++c->calls;return c->fault==Fault::Seek?-1:NativeVfs::seek(fd,n);}
 static int sync(int fd){++c->calls;++c->syncs;return c->fault==Fault::Sync?-1:NativeVfs::sync(fd);}
 static int close(int fd){++c->calls;++c->closes;const int rc=NativeVfs::close(fd);return c->fault==Fault::Close?-1:rc;}
};
Control* Injected::c=nullptr;
using Backend=Y::Backend<HostMount,Injected>;
using Owner=W::MountOwner<Backend,MW>;
using YPort=DTNF24X::Port<Owner,SimulatedWitness>;
struct Rig {
 const std::string root;Control c{};SimulatedWitness e=witness();MW mw{&e};Owner owner;YPort port;
 Rig(const fs::path& p,Fault f=Fault::None):root(p.string()),owner(mw,root,c),port(owner,e){c.fault=f;Injected::c=&c;}
 void bind(const char* s){CHECK(owner.mount());CHECK(port.bind(2,s));}
 void failed(){CHECK(owner.state()==W::State::Faulted);const auto calls=c.calls;CHECK(!owner.shutdown());CHECK(!owner.mount());CHECK(c.calls==calls);CHECK(c.unmounts==0);}
 void finish(){CHECK(owner.outstanding()==0);CHECK(c.opens==c.closes);CHECK(port.storageHealthy());CHECK(owner.shutdown());CHECK(c.unmounts==1);CHECK(owner.state()==W::State::Detached);}
};
static void prepare(const fs::path& root){CHECK(!fs::exists(root));CHECK(fs::create_directories(root));sessions(root);}
static void init(const fs::path& root){
 prepare(root);Rig r(root);r.bind(S1);DTNF24M::SessionWriter wr;DTNF24M::PendingQueue<8> q;
 CHECK(q.capture(sos(2,17),DTNF24M::kLocal)==DTNF24M::CaptureResult::Queued);
 CHECK(wr.appendChecked(r.port,S1,2,*q.front(),"a","r"));CHECK(q.acknowledgeDurableAppend());CHECK(q.pending()==0);CHECK(wr.written()==88);CHECK(r.c.syncs>=2);
 HostSD sd(root);auto data=bytes(packetFile(sd,S1));CHECK(data.size()==88);DTNF24M::PacketEntry e{};CHECK(DTNF24M::decode(data.data(),data.size(),e));auto expected=sos(2,17);CHECK(!memcmp(&expected,e.bytes,62));r.finish();
}
static void replay(const fs::path& root){
 Rig r(root);r.bind(S2);DTNF24N::Replay<16,12> re;CHECK(re.begin(2,S2));CHECK(re.addClosed(S1));CHECK(re.lock());
 for(unsigned i=0;i<20&&!re.completed();++i){auto result=re.poll(r.port,"r",DTNF24N::DenyReceiptVerifier{},[]{return true;});CHECK(result==DTNF24N::Poll::Progress||result==DTNF24N::Poll::Completed);}
 CHECK(re.completed());CHECK(re.ledger().pending()==1);CHECK(re.ledger().delivered()==0);r.finish();
}
static void online(const fs::path& root){
 Rig r(root);r.bind(S2);DTNF24N::Replay<16,12> re;CHECK(re.begin(2,S2));CHECK(re.addClosed(S1));CHECK(re.lock());
 for(unsigned i=0;i<20&&!re.completed();++i){auto result=re.poll(r.port,"r",DTNF24N::DenyReceiptVerifier{},[]{return true;});CHECK(result==DTNF24N::Poll::Progress||result==DTNF24N::Poll::Completed);}
 CHECK(re.completed());CHECK(re.ledger().pending()==1);CHECK(re.ledger().nextPending());const auto receipt=serverAccept(root,*re.ledger().nextPending());DTNF24N::ReceiptWriter writer;
 CHECK(!writer.appendVerified(r.port,S2,2,re.ledger(),receipt,DTNF24N::DenyReceiptVerifier{},"a","r"));CHECK(re.ledger().pending()==1);
 CHECK(writer.appendVerified(r.port,S2,2,re.ledger(),receipt,FakeShoreVerifier{root},"a","r"));CHECK(writer.written()==40);CHECK(re.ledger().pending()==0);CHECK(re.ledger().delivered()==1);r.finish();
}
static void restart(const fs::path& root){
 Rig r(root);r.bind(S3);DTNF24N::Replay<16,12> re;CHECK(re.begin(2,S3));CHECK(re.addClosed(S1));CHECK(re.addClosed(S2));CHECK(re.lock());
 for(unsigned i=0;i<40&&!re.completed();++i){auto result=re.poll(r.port,"r",FakeShoreVerifier{root},[]{return true;});CHECK(result==DTNF24N::Poll::Progress||result==DTNF24N::Poll::Completed);}
 CHECK(re.completed());CHECK(re.ledger().pending()==0);CHECK(re.ledger().delivered()==1);r.finish();
}
static void torn(const fs::path& root){
 Rig r(root);r.bind(S2);DTNF24N::Replay<16,12> re;CHECK(re.begin(2,S2));CHECK(re.addClosed(S1));CHECK(re.lock());
 CHECK(re.poll(r.port,"r",DTNF24N::DenyReceiptVerifier{},[]{return true;})==DTNF24N::Poll::Failed);CHECK(re.ledger().faulted());CHECK(!re.ledger().nextPending());r.finish();
}
static void fault(const fs::path& root,Fault f){
 prepare(root);Rig r(root);r.bind(S1);r.c.fault=f;DTNF24M::SessionWriter wr;DTNF24M::PendingQueue<8> q;CHECK(q.capture(sos(2,19),DTNF24M::kLocal)==DTNF24M::CaptureResult::Queued);
 CHECK(!wr.appendChecked(r.port,S1,2,*q.front(),"a","r"));CHECK(wr.faulted());CHECK(wr.written()==0);CHECK(q.pending()==1);CHECK(r.port.faulted());r.failed();
 if(f==Fault::ShortWrite){HostSD sd(root);CHECK(bytes(packetFile(sd,S1)).size()==44);}
 if(f==Fault::Close){CHECK(r.c.closes==1);CHECK(r.owner.outstanding()==1);}
}
static void metadata(const fs::path& root){
 prepare(root);Rig r(root);CHECK(r.owner.mount());char p[96]{};CHECK(DTNF24M::sandboxPath(p,sizeof(p),S1,2));bool exists=true;CHECK(r.owner.exists(p,exists));CHECK(!exists);
 const auto calls=r.c.calls;W::DenyWitness deny;W::MountOwner<Backend,W::DenyWitness> no(deny,r.root,r.c);CHECK(!no.mount());CHECK(r.c.mounts==1);CHECK(r.c.calls==calls);
 CHECK(!Y::journalPath(nullptr));CHECK(!Y::journalPath("/THESIS/F06TEST/V002/SABC1001/../dtn_sos_packets.f24m"));CHECK(!Y::journalPath("/THESIS/F06TEST/V004/SABC1001/dtn_sos_packets.f24m"));CHECK(!Y::journalPath("/THESIS/F06TEST/V002/SABC1001/telemetry.jsonl"));CHECK(Y::journalPath(p));
 CHECK(!r.owner.exists("/THESIS/F06TEST/V002/S0000000/dtn_sos_packets.f24m",exists));CHECK(!exists);r.failed();
}
static void lifecycle(const fs::path& root){
 prepare(root);{Rig r(root,Fault::Mount);CHECK(!r.owner.mount());CHECK(r.c.mounts==1);r.failed();}
 {Rig r(root);CHECK(r.owner.mount());r.c.fault=Fault::Unmount;CHECK(!r.owner.shutdown());CHECK(r.owner.state()==W::State::Faulted);CHECK(!r.owner.shutdown());CHECK(!r.owner.mount());CHECK(r.c.unmounts==1);}
 {Rig r(root);CHECK(r.owner.mount());char p[96]{};CHECK(DTNF24M::sandboxPath(p,sizeof(p),S1,2));auto t=r.owner.open(W::Lane::Sos,p,W::Mode::Append);CHECK(bool(t));uint8_t b[]={1,2,3};CHECK(r.owner.io(t,W::Action::Write,b,3));CHECK(r.owner.shutdown());CHECK(r.c.syncs==1);CHECK(r.c.closes==1);CHECK(r.c.unmounts==1);CHECK(r.owner.outstanding()==0);}
}
int main(int argc,char** argv){
 if(argc!=3)return 2;
 const std::string phase=argv[1];const fs::path root=argv[2];
 if(phase=="init")init(root);else if(phase=="replay")replay(root);else if(phase=="online")online(root);else if(phase=="restart")restart(root);else if(phase=="torn")torn(root);else if(phase=="metadata")metadata(root);else if(phase=="lifecycle")lifecycle(root);
 else if(phase=="stat")fault(root,Fault::Stat);else if(phase=="open")fault(root,Fault::Open);else if(phase=="size")fault(root,Fault::Size);else if(phase=="short")fault(root,Fault::ShortWrite);else if(phase=="read")fault(root,Fault::ShortRead);else if(phase=="sync")fault(root,Fault::Sync);else if(phase=="close")fault(root,Fault::Close);else if(phase=="seek")fault(root,Fault::Seek);else return 2;
 std::cout<<"F24Y "<<phase<<" PASS "<<checks<<" checks; host descriptors only, simulated mount\n";return 0;
}
