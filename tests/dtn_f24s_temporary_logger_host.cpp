// F24S: real F24R File method adapter through F06 short-lived allowlist.
// Synthetic RAM backend, never Arduino/physical SD; lease truth is mocked.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include "../firmware/shared/storage/dtn_f24r_guarded_file_port.h"
#include "../firmware/shared/storage/dtn_f24s_temporary_logger_files.h"
static unsigned tests=0;
#define CHECK(x) do{++tests;if(!(x)){fprintf(stderr,"F24S FAIL line %d: %s\n",__LINE__,#x);exit(18);}}while(0)
static const char* READ="r";
static const char* APPEND="a";
static const char* JOURNAL="/THESIS/F06TEST/V002/S00ABC12/diagnostics.jsonl";
static const char* DIRECTORY="/THESIS/F06TEST/V002/S00ABC12";
struct Store;
struct RawFile {
 Store* store=nullptr;size_t cursor=0;bool writing=false;
 RawFile()=default;RawFile(Store* s,bool w);
 RawFile(const RawFile&)=delete; RawFile& operator=(const RawFile&)=delete;
 RawFile(RawFile&& other)noexcept:store(other.store),cursor(other.cursor),writing(other.writing){other.store=nullptr;}
 RawFile& operator=(RawFile&& other)noexcept{if(this!=&other){close();store=other.store;cursor=other.cursor;writing=other.writing;other.store=nullptr;}return *this;}
 ~RawFile(){close();}
 explicit operator bool()const{return store!=nullptr;}
 size_t size()const;
 int read();int available()const;
 size_t read(uint8_t* b,size_t n);
 size_t print(const char*);size_t print(char);
 bool seek(size_t at);bool isDirectory()const{return false;}
 void flush(){};int getWriteError()const{return 0;}
 void close();
};
struct Store {
 std::string data{};int opens=0,checks=0,reads=0,writes=0,closes=0,live=0;
 bool exists(const char*){++checks;return true;}
 RawFile open(const char*,const char* mode){++opens;return RawFile(this,strcmp(mode,"a")==0);}
};
RawFile::RawFile(Store* s,bool w):store(s),cursor(w?s->data.size():0),writing(w){++s->live;}
void RawFile::close(){if(store){--store->live;++store->closes;store=nullptr;}}
size_t RawFile::size()const{return store?store->data.size():0;}
int RawFile::available()const{return store && !writing?int(store->data.size()-cursor):0;}
int RawFile::read(){if(!available())return -1;++store->reads;return (unsigned char)store->data[cursor++];}
size_t RawFile::read(uint8_t* b,size_t n){if(!store || writing || !b)return 0;size_t m=store->data.size()-cursor;if(n<m)m=n;memcpy(b,store->data.data()+cursor,m);cursor+=m;store->reads+=int(m);return m;}
size_t RawFile::print(const char* s){if(!store || !writing || !s)return 0;const size_t n=strlen(s);store->data.append(s);store->writes+=int(n);return n;}
size_t RawFile::print(char c){if(!store || !writing)return 0;store->data.push_back(c);++store->writes;return 1;}
bool RawFile::seek(size_t p){if(!store || p>store->data.size())return false;cursor=p;return true;}
struct Witness {DTNF24Q::Facts facts{};DTNF24Q::Facts view()const{return facts;}};
static DTNF24Q::Facts good(){DTNF24Q::Facts f{};f.vessel=2;f.mediaTag=101;f.mountEpoch=102;f.ownerSerial=103;
 f.independentlyAuthorized=f.soleOwner=f.allPathsInterposed=f.legacyCoordinated=f.f23dCoordinated=f.otherHandlesClosed=true;return f;}
using Arb=DTNF24Q::Arbiter<Witness>;
using Guard=DTNF24R::GuardedVolume<Store,Witness>;
using Temp=DTNF24S::TemporaryLoggerFiles<Guard>;
static void pathRules(){
 for(const char* name:{DIRECTORY,JOURNAL,"/THESIS/F06TEST/V001/S1234567/sos_events.jsonl","/THESIS/F06TEST/V003/SABCDEF0/telemetry.jsonl"})CHECK(DTNF24S::allowedF06Path(name,false));
 CHECK(!DTNF24S::allowedF06Path(DIRECTORY,true));
 CHECK(DTNF24S::allowedF06Path(JOURNAL,true));
 for(const char* name:{"/THESIS/VESSEL_002/diagnostics.jsonl","/THESIS/F06TEST/V004/S1234567/diagnostics.jsonl",
  "/THESIS/F06TEST/V002/S123456g/diagnostics.jsonl","/THESIS/F06TEST/V002/S1234567/../secrets.txt",
  "/THESIS/F06TEST/V002/S1234567/diagnostics.jsonl/extra","/THESIS/F06TEST/V002/S123456/diagnostics.jsonl",
  "/THESIS/F06TEST/V002/S1234567/dtn_shore_receipts.f24n","/THESIS/F06TEST/V002/sos_events.jsonl",
  "/THESIS/F06TEST/V002/S1234567/DIAGNOSTICS.JSONL","/THESIS/F06TEST/V002/S1234567/f23d_multihop.jsonl"}){
  CHECK(!DTNF24S::allowedF06Path(name,false));CHECK(!DTNF24S::allowedF06Path(name,true));
 }
 CHECK(!DTNF24S::allowedF06Path(nullptr,false));
}
static void appendAndVerify(){
 Store disk; Witness witness{good()};Arb arb(witness);auto lease=arb.acquire(2,DTNF24Q::Lane::LegacyAppend);
 Guard guard(disk,arb,lease);Temp volume(guard);CHECK(lease.serial==103);
 CHECK(!volume.open(DIRECTORY,APPEND));CHECK(!volume.open(JOURNAL,"w"));CHECK(!volume.open("/OTHER",READ));CHECK(disk.opens==0);
 CHECK(volume.exists(JOURNAL));CHECK(disk.checks==1);
 auto f=volume.open(JOURNAL,APPEND);CHECK(bool(f));CHECK(disk.opens==1);
 CHECK(f.print("{\"kind\":\"boot\"}")==15);CHECK(f.print('\n')==1);CHECK(f.size()==16);CHECK(f.available()==0);
 CHECK(f.read()==-1);CHECK(f.getWriteError()==0);f.flush();CHECK(disk.data=="{\"kind\":\"boot\"}\n");
 CHECK(!volume.open(JOURNAL,APPEND));f.close();CHECK(disk.live==0);CHECK(guard.release());
 Witness w{good()};Arb a(w);auto l=a.acquire(2,DTNF24Q::Lane::Audit);Guard g(disk,a,l);Temp t(g);
 auto r=t.open(JOURNAL,READ);CHECK(bool(r));CHECK(r.available()==16);CHECK(r.read()=='{');uint8_t b[4]{};CHECK(r.read(b,4)==4);CHECK(memcmp(b,"\"kin",4)==0);CHECK(r.seek(0));CHECK(r.size()==16);
 CHECK(r.print('x')==0);CHECK(r.print("x")==0);r.close();CHECK(g.release());
}
static void denyAndRevoke(){
 Store disk;DTNF24Q::DenyWitness none{};DTNF24Q::Arbiter<DTNF24Q::DenyWitness> a(none);
 auto lease=a.acquire(2,DTNF24Q::Lane::Audit);DTNF24R::GuardedVolume<Store,DTNF24Q::DenyWitness> guard(disk,a,lease);
 DTNF24S::TemporaryLoggerFiles<decltype(guard)> temp(guard);
 CHECK(!temp.exists(JOURNAL));CHECK(!temp.open(JOURNAL,READ));CHECK(!temp.open(JOURNAL,APPEND));CHECK(disk.opens==0);CHECK(disk.checks==0);
 Witness w{good()};Arb active(w);auto l=active.acquire(2,DTNF24Q::Lane::LegacyAppend);Guard g(disk,active,l);Temp p(g);
 auto f=p.open(JOURNAL,APPEND);CHECK(bool(f));const int n=disk.writes,c=disk.closes;
 w.facts.mountEpoch++;
 CHECK(f.print('x')==0);CHECK(f.print("x")==0);CHECK(!f);CHECK(active.quarantined());f.close();
 CHECK(disk.writes==n);CHECK(disk.closes==c);CHECK(!p.open(JOURNAL,APPEND));CHECK(!g.release());
 // RAM mock's RawFile destructor may close later: not SD or Arduino destructor proof.
}
int main(){pathRules();appendAndVerify();denyAndRevoke();
 printf("F24S guarded short-lived F06 logger methods PASS: %u host checks; deny-only firmware, physical teardown unresolved\n",tests);}
