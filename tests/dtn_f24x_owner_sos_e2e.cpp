// Real host files, existing F24M/N clients through F24W owner-held tokens.
// Reuse F24O test fixtures only (packet bytes and mock shore, not its port).
#define main f24o_fixture_main
#include "dtn_f24o_owned_port_e2e.cpp"
#undef main
#include "../firmware/shared/storage/dtn_f24x_owner_sos_port.h"
#include <climits>
#include <system_error>
namespace X=DTNF24X; namespace W=DTNF24W;
struct MountWitness {
 const SimulatedWitness* w;
 W::Identity view()const{const auto e=w->view();return {e.mediaTag,e.mountEpoch,e.ownerSerial,e.mounted,e.exclusive,e.independentOwner};}
};
enum class Fault {None,Stat,Size,ShortWrite,Flush,FinalReadClose,Read,Close,Unmount,SwapDuringWrite};
struct Control {Fault fault=Fault::None;unsigned calls=0,opens=0,closes=0,unmounts=0;SimulatedWitness* w=nullptr;};
// This is a checked HOST stdio backend, not Arduino VFS/FAT or power-loss proof.
// No implicit resource destructor. Faulted open resources remain until host
// process exits. Never expose FILE* to clients. No production mount permission.
class CheckedHostBackend {
 const fs::path* root_;Control* c_;FILE* files_[2]{};
 fs::path name(const char* p)const{return *root_/std::string(p+1);}
 public:
 CheckedHostBackend(const fs::path& r,Control& c):root_(&r),c_(&c){}
 bool mount(){++c_->calls;return fs::is_directory(*root_);}
 bool exists(const char* p,bool& found){
  ++c_->calls;found=false;if(c_->fault==Fault::Stat)return false;
  std::error_code ec;const auto target=name(p);
  if(!fs::is_directory(target.parent_path(),ec) || ec)return false;
  const auto status=fs::status(target,ec);
  if(ec && ec!=std::errc::no_such_file_or_directory)return false;
  if(!fs::exists(status))return status.type()==fs::file_type::not_found;
  found=fs::is_regular_file(status);return found;
 }
 bool open(uint8_t slot,const char* p,W::Mode mode){
  ++c_->calls;if(files_[slot])return false;
  files_[slot]=std::fopen(name(p).string().c_str(),mode==W::Mode::Append?"ab+":"rb");
  if(files_[slot])++c_->opens;
  return files_[slot]!=nullptr;
 }
 bool child(uint8_t,uint8_t){++c_->calls;return false;}
 bool size(uint8_t s,size_t& n){
  ++c_->calls;n=0;if(c_->fault==Fault::Size || !files_[s])return false;
  const long old=std::ftell(files_[s]);if(old<0 || std::fseek(files_[s],0,SEEK_END))return false;
  const long end=std::ftell(files_[s]);if(end<0 || std::fseek(files_[s],old,SEEK_SET))return false;
  n=size_t(end);return true;
 }
 bool seek(uint8_t s,size_t n){++c_->calls;return files_[s] && n<=size_t(LONG_MAX) && std::fseek(files_[s],long(n),SEEK_SET)==0;}
 bool io(uint8_t s,W::Action a,uint8_t* p,size_t n){
  ++c_->calls;if(!files_[s])return false;
  if(a==W::Action::Read){if(c_->fault==Fault::Read)return false;return std::fread(p,1,n,files_[s])==n && !std::ferror(files_[s]);}
  if(a!=W::Action::Write)return false;
  if(c_->fault==Fault::ShortWrite){(void)std::fwrite(p,1,n/2,files_[s]);(void)std::fflush(files_[s]);return false;}
  const bool ok=std::fwrite(p,1,n,files_[s])==n && !std::ferror(files_[s]);
  if(c_->fault==Fault::SwapDuringWrite)++c_->w->e.mediaTag;
  return ok;
 }
 bool flush(uint8_t s){++c_->calls;return !(c_->fault==Fault::Flush && c_->opens>=2) && files_[s] && std::fflush(files_[s])==0;}
 bool close(uint8_t s){
  ++c_->calls;if(!files_[s])return false;
  const int rc=std::fclose(files_[s]);files_[s]=nullptr;++c_->closes;
  return rc==0 && c_->fault!=Fault::Close && !(c_->fault==Fault::FinalReadClose && c_->closes==3);
 }
 bool unmount(){++c_->calls;++c_->unmounts;return !files_[0] && !files_[1] && c_->fault!=Fault::Unmount;}
};
using XOwner=W::MountOwner<CheckedHostBackend,MountWitness>;
using XPort=X::Port<XOwner,SimulatedWitness>;
struct Rig {
 SimulatedWitness e=witness();MountWitness mw{&e};Control c{};
 XOwner owner;XPort port;
 Rig(const fs::path& root,const char* session):owner(mw,root,c),port(owner,e){c.w=&e;CHECK(owner.mount());CHECK(port.bind(2,session));}
 void finish(){CHECK(port.storageHealthy());CHECK(!port.hasHandle());CHECK(owner.outstanding()==0);CHECK(c.opens==c.closes);CHECK(owner.shutdown());CHECK(c.unmounts==1);}
 void failed(){CHECK(port.faulted());CHECK(owner.state()==W::State::Faulted);const auto calls=c.calls;CHECK(!owner.shutdown());CHECK(!port.storageHealthy());CHECK(c.calls==calls);CHECK(c.unmounts==0);}
};
struct ReadAllowed {bool operator()()const{return true;}};
static DTNF24N::Replay<16,12> xReplay(Rig& rig,const fs::path& root,const char* current,std::initializer_list<const char*> olds,bool trusted){
 DTNF24N::Replay<16,12> replay;CHECK(replay.begin(2,current));for(auto s:olds)CHECK(replay.addClosed(s));CHECK(replay.lock());
 for(unsigned i=0;i<80;++i){
  auto result=trusted?replay.poll(rig.port,"r",FakeShoreVerifier{root},ReadAllowed{}):replay.poll(rig.port,"r",DTNF24N::DenyReceiptVerifier{},ReadAllowed{});
  CHECK(rig.owner.outstanding()==0);CHECK(!rig.port.hasHandle());
  if(result==DTNF24N::Poll::Completed){CHECK(replay.completed());return replay;}
  CHECK(result==DTNF24N::Poll::Progress);
 }
 CHECK(false);return replay;
}
static void xInit(const fs::path& root){
 CHECK(!fs::exists(root));CHECK(fs::create_directories(root));sessions(root);Rig rig(root,S1);
 DTNF24M::PendingQueue<8> queue;DTNF24M::SessionWriter writer;
 auto cancel=sos(2,70);cancel.packetType=PACKET_SOS_CANCEL;
 CHECK(queue.capture(cancel,DTNF24M::kLocal)==DTNF24M::CaptureResult::Invalid); // CANCEL not implemented.
 for(uint32_t i=0;i<3;++i){auto p=sos(i+1,70+i);CHECK(queue.capture(p,i==1?DTNF24M::kLocal:DTNF24M::kReceived)==DTNF24M::CaptureResult::Queued);}
 while(queue.pending()){CHECK(queue.front());CHECK(writer.appendChecked(rig.port,S1,2,*queue.front(),"a","r"));CHECK(rig.owner.outstanding()==0);CHECK(queue.acknowledgeDurableAppend());}
 CHECK(writer.written()==264);HostSD sd(root);const auto b=bytes(packetFile(sd,S1));CHECK(b.size()==264);
 for(uint32_t i=0;i<3;++i){DTNF24M::PacketEntry p{};CHECK(DTNF24M::decode(b.data()+88*i,88,p));auto expected=sos(i+1,70+i);CHECK(!memcmp(&expected,p.bytes,62));}
 put(root/"research_only.bin",{'S','A','F','E'});rig.finish();
}
static void xOffline(const fs::path& root){Rig rig(root,S2);auto r=xReplay(rig,root,S2,{S1},false);CHECK(r.ledger().pending()==3);CHECK(r.ledger().delivered()==0);rig.finish();}
static void xOnline(const fs::path& root){
 Rig rig(root,S2);auto r=xReplay(rig,root,S2,{S1},false);DTNF24N::ReceiptWriter writer;
 auto receipt=serverAccept(root,*r.ledger().nextPending());CHECK(!writer.appendVerified(rig.port,S2,2,r.ledger(),receipt,DTNF24N::DenyReceiptVerifier{},"a","r"));CHECK(r.ledger().pending()==3);
 CHECK(writer.appendVerified(rig.port,S2,2,r.ledger(),receipt,FakeShoreVerifier{root},"a","r"));CHECK(r.ledger().pending()==2);CHECK(writer.written()==40);rig.finish();
}
static void xRestart(const fs::path& root){Rig rig(root,S3);auto r=xReplay(rig,root,S3,{S1,S2},true);CHECK(r.ledger().pending()==2);CHECK(r.ledger().delivered()==1);rig.finish();}
static void xAudit(const fs::path& root){Rig rig(root,S4);auto r=xReplay(rig,root,S4,{S1,S2},false);CHECK(r.ledger().pending()==3);CHECK(r.ledger().delivered()==0);CHECK(r.unverified()==1);CHECK(bytes(root/"research_only.bin")==std::vector<uint8_t>({'S','A','F','E'}));rig.finish();}
static void xFault(const fs::path& root,Fault fault){
 CHECK(!fs::exists(root));CHECK(fs::create_directories(root));sessions(root);Rig rig(root,S1);rig.c.fault=fault;
 DTNF24M::PendingQueue<8> queue;DTNF24M::SessionWriter writer;CHECK(queue.capture(sos(2,70),DTNF24M::kLocal)==DTNF24M::CaptureResult::Queued);
 CHECK(!writer.appendChecked(rig.port,S1,2,*queue.front(),"a","r"));CHECK(writer.faulted());CHECK(writer.written()==0);CHECK(queue.pending()==1);
 const auto calls=rig.c.calls;CHECK(!writer.appendChecked(rig.port,S1,2,*queue.front(),"a","r"));CHECK(calls==rig.c.calls);rig.failed();
 if(fault==Fault::Stat)CHECK(rig.c.opens==0);
 if(fault==Fault::FinalReadClose){CHECK(rig.c.closes==3);CHECK(rig.owner.outstanding()==1);}
 if(fault==Fault::ShortWrite){HostSD sd(root);CHECK(bytes(packetFile(sd,S1)).size()==44);}
}
static void xReplayStat(const fs::path& root){Rig rig(root,S4);rig.c.fault=Fault::Stat;DTNF24N::Replay<16,12> re;CHECK(re.begin(2,S4));CHECK(re.addClosed(S1));CHECK(re.lock());CHECK(re.poll(rig.port,"r",DTNF24N::DenyReceiptVerifier{},ReadAllowed{})==DTNF24N::Poll::Failed);CHECK(re.ledger().faulted());rig.failed();}
static void xReceiptClose(const fs::path& root){Rig rig(root,S5);auto re=xReplay(rig,root,S5,{S1},false);rig.c.closes=0;rig.c.fault=Fault::FinalReadClose;DTNF24N::ReceiptWriter writer;const auto receipt=serverAccept(root,*re.ledger().nextPending());CHECK(!writer.appendVerified(rig.port,S5,2,re.ledger(),receipt,FakeShoreVerifier{root},"a","r"));CHECK(writer.written()==0);CHECK(writer.faulted());CHECK(re.ledger().pending()==3);rig.failed();}
static void xAbandon(const fs::path& root){Rig rig(root,S4);char p[96]{};CHECK(DTNF24M::sandboxPath(p,sizeof(p),S1,2));{auto h=rig.port.open(p,"r");CHECK(bool(h));auto h2=std::move(h);CHECK(!bool(h));CHECK(bool(h2));}CHECK(rig.c.closes==0);CHECK(rig.owner.outstanding()==1);rig.failed();}
static void xPath(const fs::path& root){Rig rig(root,S4);char p[96]{};CHECK(DTNF24M::sandboxPath(p,sizeof(p),S1,2));CHECK(!rig.port.open(p,"a"));CHECK(rig.c.opens==0);rig.failed();}
static void xTorn(const fs::path& root){
 // Previous independent process intentionally left a 44-byte torn frame.
 Rig rig(root,S2);DTNF24N::Replay<16,12> re;CHECK(re.begin(2,S2));CHECK(re.addClosed(S1));CHECK(re.lock());
 CHECK(re.poll(rig.port,"r",DTNF24N::DenyReceiptVerifier{},ReadAllowed{})==DTNF24N::Poll::Failed);
 CHECK(re.ledger().faulted());CHECK(re.ledger().nextPending()==nullptr);CHECK(rig.owner.outstanding()==0);rig.finish();
}
static void xUrgent(const fs::path& root){
 Rig rig(root,S4);char p[96]{};CHECK(DTNF24M::sandboxPath(p,sizeof(p),S1,2));auto h=rig.port.open(p,"r");CHECK(bool(h));
 rig.e.e.urgentPending=true;const auto calls=rig.c.calls;uint8_t b[10]{};CHECK(h.read(b,sizeof(b))==0);h.close();CHECK(rig.c.calls==calls);CHECK(rig.owner.outstanding()==1);rig.failed();
}
static void xDiscovery(const fs::path& root){
 Rig rig(root,S4);bool called=false;const auto calls=rig.c.calls;
 CHECK(!rig.port.scanClosedSessions("/THESIS/F06TEST/V002","r",[&](const char*){called=true;return true;}));CHECK(!called);CHECK(rig.c.calls==calls);rig.failed();
}
static void xDeny(){W::DenyMountFacade<int> f;DTNF24O::DenyWitness witness;X::Port<W::DenyMountFacade<int>::OwnerType,DTNF24O::DenyWitness> port(f.ownerForSos(),witness);CHECK(!f.begin());CHECK(!port.bind(2,S1));CHECK(!port.storageHealthy());CHECK(port.faulted());CHECK(!port.open("/anything","a"));CHECK(!f.end());CHECK(f.state()==W::State::Faulted);}
int main(int argc,char** argv){
 if(argc!=3)return 2;
 const std::string phase=argv[1];const fs::path root=argv[2];
 if(phase=="init")xInit(root);else if(phase=="offline")xOffline(root);else if(phase=="online")xOnline(root);else if(phase=="restart")xRestart(root);else if(phase=="audit")xAudit(root);
 else if(phase=="stat")xFault(root,Fault::Stat);else if(phase=="size")xFault(root,Fault::Size);else if(phase=="short")xFault(root,Fault::ShortWrite);else if(phase=="flush")xFault(root,Fault::Flush);else if(phase=="close")xFault(root,Fault::Close);else if(phase=="readclose")xFault(root,Fault::FinalReadClose);else if(phase=="read")xFault(root,Fault::Read);else if(phase=="swap")xFault(root,Fault::SwapDuringWrite);
 else if(phase=="replaystat")xReplayStat(root);else if(phase=="receiptclose")xReceiptClose(root);else if(phase=="abandon")xAbandon(root);else if(phase=="path")xPath(root);else if(phase=="deny")xDeny();else if(phase=="torn")xTorn(root);else if(phase=="urgent")xUrgent(root);else if(phase=="discovery")xDiscovery(root);else return 2;
 std::cout<<"F24X "<<phase<<" PASS "<<checks<<" checks; host files only; no physical grant\n";return 0;
}
