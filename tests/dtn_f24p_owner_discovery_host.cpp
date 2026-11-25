// F24P host mock only: never accesses a physical SD, radio or cloud.
#include "../firmware/shared/storage/dtn_f24o_owned_sos_port.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"F24P FAIL line "<<__LINE__<<": "<<#x<<"\n";std::exit(61);}}while(0)
static constexpr const char* CURRENT="/THESIS/F06TEST/V002/S0000001";
static constexpr const char* ROOT="/THESIS/F06TEST/V002";
struct Entry {std::string name;bool directory;};
struct MockVolume;
struct MockFile {
 MockVolume* v=nullptr;int kind=0;size_t index=0;
 MockFile()=default;MockFile(MockVolume* vv,int k,size_t i=0);
 MockFile(const MockFile&)=delete;MockFile& operator=(const MockFile&)=delete;
 MockFile(MockFile&& other)noexcept:v(other.v),kind(other.kind),index(other.index){other.v=nullptr;}
 MockFile& operator=(MockFile&& other)noexcept{
  if(this!=&other){close();v=other.v;kind=other.kind;index=other.index;other.v=nullptr;}return *this;
 }
 ~MockFile(){close();}
 explicit operator bool()const{return v!=nullptr;}
 bool isDirectory()const;
 const char* name()const;
 MockFile openNextFile();
 size_t size()const{return 0;}
 bool seek(size_t){return false;}
 int read(uint8_t*,size_t){return 0;}
 size_t write(const uint8_t*,size_t){return 0;}
 void flush(){}
 int getWriteError()const{return 1;}
 void close();
};
struct MockVolume {
 std::vector<Entry> entries;size_t cursor=0;int opens=0,handles=0,maxHandles=0,nexts=0;
 bool allowed=true;std::function<void(int)> afterNext;
 bool exists(const char*)const{return false;}
 MockFile open(const char* path,const char* mode){
  ++opens;if(!allowed || !path || !mode || strcmp(path,ROOT) || strcmp(mode,"r"))return {};
  cursor=0;return MockFile(this,1);
 }
};
MockFile::MockFile(MockVolume* vv,int k,size_t i):v(vv),kind(k),index(i){
 if(v){++v->handles;if(v->handles>v->maxHandles)v->maxHandles=v->handles;}
}
void MockFile::close(){if(v){--v->handles;v=nullptr;}}
bool MockFile::isDirectory()const{
 return v && (kind==1 || (kind==2 && index<v->entries.size() && v->entries[index].directory));
}
const char* MockFile::name()const{
 return v && kind==2 && index<v->entries.size()?v->entries[index].name.c_str():nullptr;
}
MockFile MockFile::openNextFile(){
 if(!v || kind!=1)return {};
 MockVolume* volume=v;++volume->nexts;
 if(volume->afterNext)volume->afterNext(volume->nexts);
 if(volume->cursor>=volume->entries.size())return {};
 return MockFile(volume,2,volume->cursor++);
}
struct Witness {DTNF24O::Evidence e{};DTNF24O::Evidence view()const{return e;}};
Witness approved(){
 Witness w{};auto& e=w.e;e.vessel=2;e.mediaTag=42;e.mountEpoch=7;e.ownerSerial=99;
 e.mounted=true;e.independentOwner=true;e.exclusive=true;
 e.f23dClosed=true;e.otherWriterClosed=true;e.closedSessionsVerified=true;
 return w;
}
using Port=DTNF24O::Port<MockVolume,Witness>;
using DenyPort=DTNF24O::Port<MockVolume,DTNF24O::DenyWitness>;
static void success(){
 MockVolume v;v.entries={{"S0000001",true},{"S0000002",true},{"notes.txt",false},
 {"/THESIS/F06TEST/V002/S0000003",true}};
 Witness w=approved();Port port(v,w);CHECK(port.bind(2,CURRENT));
 std::vector<std::string> found;
 CHECK(port.scanClosedSessions(ROOT,"r",[&](const char* s){
  CHECK(port.hasHandle());CHECK(v.handles==1);found.emplace_back(s);return true;
 }));
 CHECK(!port.faulted());CHECK(!port.hasHandle());CHECK(v.handles==0);
 CHECK(v.maxHandles==2);CHECK(v.opens==1);CHECK(v.nexts==5);
 CHECK(found.size()==2);CHECK(found[0]=="/THESIS/F06TEST/V002/S0000002");
 CHECK(found[1]=="/THESIS/F06TEST/V002/S0000003");
}
static void denial(){
 {
  MockVolume v;DTNF24O::DenyWitness deny;DenyPort p(v,deny);
  CHECK(!p.bind(2,CURRENT));CHECK(!p.scanClosedSessions(ROOT,"r",[](const char*){return true;}));
  CHECK(v.opens==0);CHECK(v.handles==0);
 }
 for(int i=0;i<5;++i){
  MockVolume v;Witness w=approved();Port p(v,w);CHECK(p.bind(2,CURRENT));
  if(i==0)w.e.urgentPending=true;
  if(i==1)w.e.mediaTag++;
  if(i==2)w.e.f23dClosed=false;
  if(i==3)w.e.exportActive=true;
  if(i==4)w.e.otherWriterClosed=false;
  CHECK(!p.scanClosedSessions(ROOT,"r",[](const char*){return true;}));
  CHECK(p.faulted());CHECK(v.opens==0);CHECK(v.handles==0);
 }
 for(const char* wrong:{"/THESIS/F06TEST/V003","/THESIS/F06TEST/V002/S0000002",
                        "/THESIS/F06TEST/V002/../V003"}){
  MockVolume v;Witness w=approved();Port p(v,w);CHECK(p.bind(2,CURRENT));
  CHECK(!p.scanClosedSessions(wrong,"r",[](const char*){return true;}));
  CHECK(p.faulted());CHECK(v.opens==0);CHECK(v.handles==0);
 }
 {MockVolume v;Witness w=approved();Port p(v,w);CHECK(p.bind(2,CURRENT));
  CHECK(!p.scanClosedSessions(ROOT,"a",[](const char*){return true;}));CHECK(v.opens==0);}
}
static void faults(){
 for(const std::string bad:{"S000000G","S00000","/THESIS/F06TEST/V003/S0000002",
                            "S0000002/../S0000003",""}){
  MockVolume v;v.entries={{bad,true}};Witness w=approved();Port p(v,w);
  CHECK(p.bind(2,CURRENT));
  CHECK(!p.scanClosedSessions(ROOT,"r",[](const char*){return true;}));
  CHECK(p.faulted());CHECK(!p.hasHandle());CHECK(v.handles==0);
 }
 {MockVolume v;v.allowed=false;Witness w=approved();Port p(v,w);CHECK(p.bind(2,CURRENT));
  CHECK(!p.scanClosedSessions(ROOT,"r",[](const char*){return true;}));CHECK(v.handles==0);}
 {MockVolume v;v.entries={{"S0000002",true}};Witness w=approved();Port p(v,w);
  CHECK(p.bind(2,CURRENT));
  CHECK(!p.scanClosedSessions(ROOT,"r",[](const char*){return false;}));
  CHECK(p.faulted());CHECK(v.handles==0);}
 {MockVolume v;v.entries={{"S0000002",true}};Witness w=approved();Port p(v,w);
  CHECK(p.bind(2,CURRENT));
  CHECK(!p.scanClosedSessions(ROOT,"r",[&](const char*){
   CHECK(!p.exists("/THESIS/F06TEST/V002/S0000002/dtn_sos_packets.f24m"));
   return true;
  }));CHECK(p.faulted());CHECK(v.handles==0);}
}
static void changesAndBounds(){
 for(int i=0;i<5;++i){
  MockVolume v;v.entries={{"S0000002",true},{"S0000003",true}};
  Witness w=approved();Port p(v,w);CHECK(p.bind(2,CURRENT));
  v.afterNext=[&](int step){if(step==1){
   if(i==0)++w.e.mountEpoch;
   if(i==1)++w.e.ownerSerial;
   if(i==2)w.e.radioBusy=true;
   if(i==3)w.e.historyActive=true;
   if(i==4)w.e.closedSessionsVerified=false;
  }};
  int accepted=0;
  CHECK(!p.scanClosedSessions(ROOT,"r",[&](const char*){++accepted;return true;}));
  CHECK(accepted==0);CHECK(p.faulted());CHECK(v.handles==0);
 }
 {MockVolume v;
  for(unsigned i=2;i<=65;++i){char buf[12]{};snprintf(buf,sizeof(buf),"S%07X",i);v.entries.push_back({buf,true});}
  Witness w=approved();Port p(v,w);CHECK(p.bind(2,CURRENT));unsigned n=0;
  CHECK(!p.scanClosedSessions(ROOT,"r",[&](const char*){++n;return true;}));
  CHECK(n==64);CHECK(p.faulted());CHECK(v.handles==0);}
 {MockVolume v;Witness w=approved();Port p(v,w);CHECK(p.bind(2,CURRENT));
  CHECK(p.scanClosedSessions(ROOT,"r",[](const char*){return true;}));
  CHECK(!p.faulted());CHECK(v.handles==0);}
}
int main(){
 success();denial();faults();changesAndBounds();
 std::cout<<"F24P owner-bound session discovery PASS: "<<checks
          <<" host checks; physical owner denied, no SD/RF/cloud access\n";
}
