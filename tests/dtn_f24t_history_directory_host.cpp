// F24T: real F24R directory methods + F24Q lease against synthetic in-RAM
// File handles. Host-only; no Arduino, SD, hardware or usable owner grant.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include "../firmware/shared/storage/dtn_f24r_guarded_file_port.h"
#include "../firmware/shared/storage/dtn_f24t_history_directory.h"
static unsigned checks=0;
#define CHECK(e) do{++checks;if(!(e)){std::fprintf(stderr,"F24T CHECK FAIL line %d: %s\n",__LINE__,#e);std::exit(19);}}while(0)
static constexpr const char* ROOT="/THESIS/F06TEST/V002";
struct FakeVolume;
struct File {
 FakeVolume* v=nullptr;bool directory=false;bool parent=false;size_t next=0;std::string filename;
 File()=default;File(FakeVolume*,bool,bool,std::string);
 File(const File&)=delete;File& operator=(const File&)=delete;
 File(File&& rhs)noexcept:v(rhs.v),directory(rhs.directory),parent(rhs.parent),next(rhs.next),filename(std::move(rhs.filename)){rhs.v=nullptr;}
 File& operator=(File&& rhs)noexcept{
  if(this!=&rhs){close();v=rhs.v;directory=rhs.directory;parent=rhs.parent;next=rhs.next;filename=std::move(rhs.filename);rhs.v=nullptr;}
  return *this;
 }
 ~File(){close();}
 explicit operator bool()const{return v!=nullptr;}
 void close();bool isDirectory()const{return directory;}size_t size()const{return 0;}
 const char* name()const{return filename.c_str();}
 File openNextFile();
 bool seek(size_t){return false;}int read(){return -1;}int available(){return 0;}
 size_t read(uint8_t*,size_t){return 0;}size_t write(const uint8_t*,size_t){return 0;}
 size_t print(const char*){return 0;}size_t print(char){return 0;}
 void flush(){}int getWriteError()const{return 0;}
};
struct FakeVolume {
 std::vector<std::pair<std::string,bool>> children;
 int rootOpens=0,iterCalls=0,active=0,peak=0,rawCloses=0,existsCalls=0;
 bool returnFile=true;
 bool exists(const char*){++existsCalls;return true;}
 File open(const char* path,const char* mode){
  ++rootOpens;
  if(!returnFile || !path || std::strcmp(path,ROOT) || !mode || std::strcmp(mode,"r"))return {};
  return File(this,true,true,path);
 }
};
File::File(FakeVolume* x,bool d,bool p,std::string n):v(x),directory(d),parent(p),filename(std::move(n)){
 if(v){++v->active;if(v->active>v->peak)v->peak=v->active;}
}
void File::close(){if(v){++v->rawCloses;--v->active;v=nullptr;}}
File File::openNextFile(){
 if(!v || !parent || !directory)return {};
 ++v->iterCalls;
 if(next>=v->children.size())return {};
 auto child=v->children[next++];return File(v,child.second,false,child.first);
}
struct Witness {DTNF24Q::Facts facts{};DTNF24Q::Facts view()const{return facts;}};
static DTNF24Q::Facts granted(){DTNF24Q::Facts f{};f.vessel=2;f.mediaTag=91;f.mountEpoch=92;f.ownerSerial=93;
 f.independentlyAuthorized=f.soleOwner=f.allPathsInterposed=true;
 f.legacyCoordinated=f.f23dCoordinated=f.otherHandlesClosed=true;return f;
}
using Arb=DTNF24Q::Arbiter<Witness>;
using Port=DTNF24R::GuardedVolume<FakeVolume,Witness>;
using Lister=DTNF24T::HistoryDirectory<Port>;
static std::string full(const char* session){return std::string(ROOT)+"/"+session;}
static void rootsAndNames(){
 CHECK(DTNF24T::validRoot(ROOT,2));CHECK(!DTNF24T::validRoot(ROOT,1));
 CHECK(!DTNF24T::validRoot("/THESIS/F06TEST/V002/other",2));
 CHECK(!DTNF24T::validRoot("/THESIS/F06TEST/V001",2));
 CHECK(!DTNF24T::validRoot("/THESIS/F06TEST/V003",2));
 CHECK(!DTNF24T::validRoot("/THESIS/F06TEST/V002/../V003",2));
 CHECK(!DTNF24T::validRoot("/THESIS/F06TEST/V002",4));
 CHECK(!DTNF24T::validRoot(nullptr,2));
 CHECK(DTNF24T::validSession("S123ABCD"));
 CHECK(!DTNF24T::validSession("S123abcd"));
 CHECK(!DTNF24T::validSession("S123ABC"));
 CHECK(!DTNF24T::validSession("S123ABCDE"));
 CHECK(!DTNF24T::validSession("NOTSESSION"));
 char name[9]{};
 CHECK(DTNF24T::childName("S123ABCD",ROOT,name));CHECK(std::strcmp(name,"S123ABCD")==0);
 CHECK(DTNF24T::childName(full("S0000001").c_str(),ROOT,name));CHECK(std::strcmp(name,"S0000001")==0);
 CHECK(!DTNF24T::childName("/THESIS/F06TEST/V003/S0000001",ROOT,name));
 CHECK(!DTNF24T::childName("/THESIS/F06TEST/V002/other/S0000001",ROOT,name));
 CHECK(!DTNF24T::childName("S0000001/extra",ROOT,name));
 CHECK(!DTNF24T::childName("",ROOT,name));
 CHECK(!DTNF24T::childName(nullptr,ROOT,name));
}
static void normalAndCompeting(){
 FakeVolume disk{};
 disk.children={{full("S0000001"),true},{"telemetry.jsonl",false},{"S0000002",true},
   {full("S0000003"),false},{"S0000004",true},{"S00000ZZ",true}};
 Witness w{granted()};Arb arb(w);auto lease=arb.acquire(2,DTNF24Q::Lane::History);
 CHECK(lease.serial==93);Port port(disk,arb,lease);Lister lister(port);
 std::vector<std::string> shown;
 auto out=lister.scan(ROOT,2,[&](const char* name){shown.push_back(name);});
 CHECK(out.opened);CHECK(out.complete);CHECK(!out.truncated);CHECK(!out.faulted);
 CHECK(out.visited==6);CHECK(out.shown==3);CHECK(shown.size()==3);
 CHECK(shown[0]=="S0000001");CHECK(shown[1]=="S0000002");CHECK(shown[2]=="S0000004");
 CHECK(disk.peak==2);CHECK(disk.active==0);CHECK(disk.rootOpens==1);CHECK(disk.iterCalls==7);
 CHECK(disk.rawCloses==7);CHECK(port.release());CHECK(arb.activeLane()==DTNF24Q::Lane::None);
}
static void parentChildAndContention(){
 FakeVolume disk{};disk.children={{"S0000001",true}};
 Witness w{granted()};Arb arb(w);auto lease=arb.acquire(2,DTNF24Q::Lane::History);
 Port port(disk,arb,lease);
 auto dir=port.open(ROOT,"r");CHECK(bool(dir));CHECK(dir.isDirectory());
 CHECK(!port.release());CHECK(!arb.acquire(2,DTNF24Q::Lane::F23D).serial);
 CHECK(!arb.open(lease,1,DTNF24Q::Mode::Read));
 auto child=dir.openNextFile();CHECK(bool(child));CHECK(child.isDirectory());
 CHECK(std::strcmp(child.name(),"S0000001")==0);
 CHECK(!port.release());CHECK(arb.hasHandle(0));CHECK(arb.hasHandle(1));
 dir.close();CHECK(bool(dir));CHECK(disk.active==2); // parent close refused while child open
 auto another=dir.openNextFile();CHECK(!bool(another));CHECK(disk.iterCalls==1);
 child.close();CHECK(!bool(child));dir.close();CHECK(!bool(dir));
 CHECK(disk.active==0);CHECK(port.release());
}
static void caps(){
 FakeVolume disk{};for(unsigned i=0;i<80;++i){char b[12];std::snprintf(b,sizeof(b),"S%07X",i);disk.children.push_back({b,true});}
 Witness w{granted()};Arb arb(w);auto lease=arb.acquire(2,DTNF24Q::Lane::History);Port p(disk,arb,lease);Lister scan(p);
 unsigned emitted=0;auto out=scan.scan(ROOT,2,[&](const char*){++emitted;});
 CHECK(out.opened);CHECK(!out.complete);CHECK(out.truncated);CHECK(!out.faulted);
 CHECK(out.visited==64);CHECK(out.shown==48);CHECK(emitted==48);
 CHECK(disk.iterCalls==65);CHECK(disk.peak==2);CHECK(disk.active==0);CHECK(p.release());
 FakeVolume disk2{};for(unsigned i=0;i<50;++i){char b[12];std::snprintf(b,sizeof(b),"S%07X",i);disk2.children.push_back({b,true});}
 Witness w2{granted()};Arb a2(w2);auto l2=a2.acquire(2,DTNF24Q::Lane::History);Port p2(disk2,a2,l2);Lister sc2(p2);
 auto r2=sc2.scan(ROOT,2,[&](const char*){});
 CHECK(r2.opened);CHECK(r2.complete);CHECK(r2.truncated);CHECK(r2.visited==50);
 CHECK(r2.shown==48);CHECK(disk2.iterCalls==51);CHECK(p2.release());
}
static void revocationAndDenial(){
 FakeVolume disk{};disk.children={{"S0000001",true},{"S0000002",true}};
 Witness w{granted()};Arb arb(w);auto lease=arb.acquire(2,DTNF24Q::Lane::History);Port p(disk,arb,lease);Lister sc(p);
 unsigned seen=0;auto out=sc.scan(ROOT,2,[&](const char*){++seen;w.facts.mediaTag++;});
 CHECK(out.opened);CHECK(!out.complete);CHECK(out.faulted);CHECK(seen==1);
 CHECK(disk.iterCalls==1);CHECK(arb.quarantined());CHECK(!p.release());
 // This mock closes raw parent at destructor (uncontrolled after revocation).
 // The exact same behavior is a PHYSICAL RELEASE BLOCKER for Arduino File.
 CHECK(disk.active==0);CHECK(disk.rawCloses==2);
 DTNF24Q::DenyWitness deny{};DTNF24Q::Arbiter<DTNF24Q::DenyWitness> forbidden(deny);
 auto noLease=forbidden.acquire(2,DTNF24Q::Lane::History);
 DTNF24R::GuardedVolume<FakeVolume,DTNF24Q::DenyWitness> denied(disk,forbidden,noLease);
 DTNF24T::HistoryDirectory<decltype(denied)> deniedScanner(denied);
 const int before=disk.rootOpens;
 auto r=deniedScanner.scan(ROOT,2,[&](const char*){});
 CHECK(!r.opened);CHECK(!r.complete);CHECK(!r.faulted);CHECK(disk.rootOpens==before);
 auto invalid=deniedScanner.scan("/THESIS/VESSEL_002",2,[&](const char*){});
 CHECK(!invalid.opened);CHECK(disk.rootOpens==before);
}
int main(){rootsAndNames();normalAndCompeting();parentChildAndContention();caps();revocationAndDenial();
 std::printf("F24T bounded deny-only history iterator PASS: %u host checks; synthetic owner; File destruction after revocation NOT SAFE\n",checks);
 return 0;
}
