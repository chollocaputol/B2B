#pragma once
#include <map>
#include <memory>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
static unsigned storageOps=0;
static bool cardAbsent=false,failOpen=false,shortWrite=false,failFlush=false,failRead=false,corruptRead=false;
static constexpr int FILE_READ=0,FILE_APPEND=1;
struct MockBytes{std::string text;};
class File {
 std::shared_ptr<MockBytes> bytes_;size_t position_=0;bool open_=false,append_=false;
public:
 File()=default;
 File(std::shared_ptr<MockBytes> p,bool append):bytes_(p),open_(true),append_(append){}
 explicit operator bool()const{return open_;}
 size_t size(){++storageOps;return bytes_?bytes_->text.size():0;}
 bool seek(size_t n){++storageOps;position_=n;return open_ && bytes_ && n<=bytes_->text.size();}
 size_t write(const uint8_t* p,size_t n){++storageOps;if(!open_ || !append_)return 0;size_t w=shortWrite?n/2:n;bytes_->text.append(reinterpret_cast<const char*>(p),w);return w;}
 int read(uint8_t* p,size_t n){++storageOps;if(!open_ || failRead)return -1;size_t left=bytes_->text.size()-position_;if(n>left)n=left;memcpy(p,bytes_->text.data()+position_,n);if(n && corruptRead)p[0]^=1;position_+=n;return int(n);}
 void flush(){++storageOps;}
 int getWriteError(){return failFlush?1:0;}
 void close(){++storageOps;open_=false;}
};
struct MockSD {
 std::map<std::string,std::shared_ptr<MockBytes>> files;
 bool exists(const char* p){++storageOps;return files.count(p)!=0;}
 File open(const char* p,int mode){
  ++storageOps;if(cardAbsent || failOpen)return {};
  auto it=files.find(p);
  if(it==files.end()){
   if(mode==FILE_READ)return {};
   files[p]=std::make_shared<MockBytes>();it=files.find(p);
  }
  return File(it->second,mode==FILE_APPEND);
 }
 void reset(){files.clear();storageOps=0;cardAbsent=failOpen=shortWrite=failFlush=failRead=corruptRead=false;}
};
static MockSD SD;
