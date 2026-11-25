// Compiles the ACTUAL existing F23D writer against disposable in-memory File/SD.
// Tests BOTH ordinary 'SD' and F24Q logger 'interposedSD' inclusion routes.
// This proves call-site naming/compatibility, not exclusive physical File custody.
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include "../firmware/shared/relay/f23d_sd_journal.h"
static unsigned checks=0;
#define EXPECT(x) do{++checks;if(!(x)){fprintf(stderr,"F24Q F23D FAIL line %d: %s\n",__LINE__,#x);exit(3);}}while(0)
constexpr const char* FILE_APPEND="a";
constexpr const char* FILE_READ="r";
struct FakeVolume;
struct File {
 FakeVolume* volume=nullptr;bool append=false;size_t pos=0;
 File()=default;
 File(FakeVolume* v,bool a);
 File(const File&)=delete;File& operator=(const File&)=delete;
 File(File&& o)noexcept:volume(o.volume),append(o.append),pos(o.pos){o.volume=nullptr;}
 File& operator=(File&& o)noexcept{
  if(this!=&o){close();volume=o.volume;append=o.append;pos=o.pos;o.volume=nullptr;}return *this;
 }
 ~File(){close();}
 explicit operator bool()const{return volume!=nullptr;}
 size_t size()const;
 size_t write(const uint8_t*,size_t);
 size_t read(uint8_t*,size_t);
 bool seek(size_t);
 void flush(){}
 int getWriteError()const{return 0;}
 void close();
};
struct FakeVolume {
 std::vector<uint8_t> content;
 bool created=false;
 int existsCalls=0,openCalls=0,openHandles=0,peakHandles=0;
 bool exists(const char* path){++existsCalls;return created && strstr(path,"f23d_multihop.jsonl")!=nullptr;}
 File open(const char* path,const char* mode){
  ++openCalls;
  if(!path || !strstr(path,"f23d_multihop.jsonl"))return {};
  if(strcmp(mode,FILE_APPEND)==0){created=true;return File(this,true);}
  if(strcmp(mode,FILE_READ)==0 && created)return File(this,false);
  return {};
 }
};
File::File(FakeVolume* v,bool a):volume(v),append(a),pos(a?v->content.size():0){
 if(v){++v->openHandles;if(v->openHandles>v->peakHandles)v->peakHandles=v->openHandles;}
}
size_t File::size()const{return volume?volume->content.size():0;}
size_t File::write(const uint8_t* data,size_t n){
 if(!volume || !append || pos!=volume->content.size())return 0;
 volume->content.insert(volume->content.end(),data,data+n);pos+=n;return n;
}
size_t File::read(uint8_t* data,size_t n){
 if(!volume || append || pos+n>volume->content.size())return 0;
 memcpy(data,volume->content.data()+pos,n);pos+=n;return n;
}
bool File::seek(size_t p){if(!volume || p>volume->content.size())return false;pos=p;return true;}
void File::close(){if(volume){--volume->openHandles;volume=nullptr;}}
static FakeVolume SD;
static FakeVolume interposedSD;
#if defined(F24Q_USE_NAMED_FACADE)
#define F23D_SD_ACCESS interposedSD
#endif
#include "../firmware/shared/storage/f23d_sd_file_writer.h"
#if defined(F24Q_USE_NAMED_FACADE)
#undef F23D_SD_ACCESS
#endif
int main(){
#ifdef F24Q_USE_NAMED_FACADE
 FakeVolume& chosen=interposedSD;FakeVolume& unchosen=SD;
#else
 FakeVolume& chosen=SD;FakeVolume& unchosen=interposedSD;
#endif
 const char* directory="/THESIS/F06TEST/V002/S0000001";
 F23DSDFileWriter writer;
 EXPECT(writer.begin(directory,2));
 EXPECT(chosen.existsCalls==1);EXPECT(chosen.openCalls==1);EXPECT(chosen.openHandles==1);
 EXPECT(unchosen.existsCalls==0);EXPECT(unchosen.openCalls==0);
 const char row[]="{\"record\":\"START\"}\n";
 EXPECT(writer.appendChecked(row,sizeof(row)-1));
 EXPECT(chosen.openCalls==2);EXPECT(chosen.openHandles==1);EXPECT(chosen.peakHandles==2);
 EXPECT(chosen.content.size()==sizeof(row)-1);
 EXPECT(writer.finish()==F23DSD::Finish::Pending);
 EXPECT(chosen.openCalls==3);EXPECT(chosen.openHandles==1);
 EXPECT(writer.finish()==F23DSD::Finish::OK);
 EXPECT(chosen.openHandles==0);
 EXPECT(!memcmp(chosen.content.data(),row,sizeof(row)-1));
 F23DSDFileWriter duplicate;EXPECT(!duplicate.begin(directory,2));
 EXPECT(unchosen.existsCalls==0);EXPECT(unchosen.openCalls==0);
 printf("F24Q actual F23D writer %s PASS: %u checks; peak simultaneous File handles=%d\n",
#ifdef F24Q_USE_NAMED_FACADE
 "named-facade",
#else
 "legacy-default",
#endif
 checks,chosen.peakHandles);
 return 0;
}
