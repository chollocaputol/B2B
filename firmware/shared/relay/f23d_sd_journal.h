#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
namespace F23DSD {
enum Health : uint32_t { Loss=1,Format=2,Storage=4,Identity=8,Session=16,Index=32,OperationalLoss=64 };
enum class Finish { Pending,OK,Failed };
struct Record {
 char record[24]{},validation[24]{},state[20]{};
 uint32_t i=0,ms=0,run=0,origin=0,event=0,wire=0,canonical=0;
 int32_t rc=0,rx_rc=0;
 int16_t rssi=0,snr_x10=0;
 uint16_t bytes=0;
 uint8_t type=0,hop=0,used=0,sos_left=0,cancel_left=0;
 bool permit=false,called=false;
};
static_assert(sizeof(Record)<=128,"compact capture; never queue 512-byte serial lines");
inline bool token(char* dst,size_t cap,const char* text) {
 if(!text)return false;
 size_t i=0;
 for(;text[i];++i){
  if(i+1>=cap || !((text[i]>='A' && text[i]<='Z') || (text[i]>='0' && text[i]<='9') || text[i]=='_'))return false;
  dst[i]=text[i];
 }
 dst[i]=0;return true;
}
inline bool sessionValid(const char* s) {
 if(!s || strlen(s)!=8 || s[0]!='S')return false;
 for(unsigned i=1;i<8;++i)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='A'&&s[i]<='F')))return false;
 return true;
}
inline bool validToken(const char* text,size_t cap,bool empty=true) {
 if(!memchr(text,0,cap) || (!empty && !text[0]))return false;
 for(size_t i=0;text[i];++i)if(!((text[i]>='A' && text[i]<='Z') || (text[i]>='0' && text[i]<='9') || text[i]=='_'))return false;
 return true;
}
inline bool validRecord(const Record& r){return validToken(r.record,sizeof(r.record),false) && validToken(r.validation,sizeof(r.validation)) && validToken(r.state,sizeof(r.state));}
class Journal {
public:
 static constexpr size_t kCapacity=48,kLine=768,kPath=96;
 bool initialize(uint32_t local,const char* session,bool authorized,uint32_t now) {
  if(initialized_)return false;
  initialized_=true;local_=local;
  if(!authorized || local<1 || local>3){fail(Identity);return false;}
  if(!sessionValid(session)){fail(Session);return false;}
  memcpy(session_,session,9);initializedOK_=true;
  Record boot{};token(boot.record,sizeof(boot.record),"BOOT");boot.ms=now;return enqueue(boot);
 }
 bool start(uint32_t now) {
  if(!readyToStart() || started_)return false;
  started_=true;capturing_=true;
  Record row{};token(row.record,sizeof(row.record),"START");row.ms=now;return enqueue(row);
 }
 bool capture(Record row) {return capturing_ && !closing_ && !failed_ ? enqueue(row):false;}
 void formatLost(){if(capturing_){lose();health_|=Format;}}
 bool requestSeal(){if(!capturing_ || closing_ || failed_)return false;sealRequested_=true;return true;}
 void operationalLoss(uint32_t n){if(n){health_|=OperationalLoss;urgentLost_=n;}}
 void fail(uint32_t reason){health_|=reason;failed_=true;}
 bool readyToStart()const{return initializedOK_ && !failed_ && written_==1 && !count_ && !started_;}
 bool defersStorage()const{return capturing_ && !sealRequested_;}
 bool captureActive()const{return capturing_;}
 bool permissionHealthy()const{return initializedOK_ && capturing_ && !sealRequested_ && !failed_ && !health_;}
 bool sealed()const{return sealed_;}
 bool failed()const{return failed_;}
 bool initialized()const{return initialized_;}
 uint32_t health()const{return health_;}
 uint32_t lost()const{return lost_;}
 size_t pending()const{return count_;}
 uint32_t written()const{return written_;}
 const char* session()const{return session_;}
 static bool row(char* out,size_t cap,const Record& r,uint32_t local,const char* session,
                 uint32_t lost,uint32_t health,uint32_t pending,uint32_t written,uint32_t urgentLost) {
  if(!validRecord(r))return false;
  const int n=snprintf(out,cap,
   "{\"v\":1,\"local\":%lu,\"session\":\"%s\",\"i\":%lu,\"ms\":%lu,\"record\":\"%s\","
   "\"run\":%lu,\"origin\":%lu,\"event\":%lu,\"type\":%u,\"hop\":%u,\"bytes\":%u,"
   "\"wire\":%lu,\"canonical\":%lu,\"validation\":\"%s\",\"state\":\"%s\","
   "\"rc\":%ld,\"rx_rc\":%ld,\"rssi\":%d,\"snr_x10\":%d,\"used\":%u,"
   "\"sos_left\":%u,\"cancel_left\":%u,\"permit\":%s,\"called\":%s,"
   "\"lost\":%lu,\"health\":%lu,\"pending\":%lu,\"written\":%lu,\"urgent_lost\":%lu}\n",
   (unsigned long)local,session,(unsigned long)r.i,(unsigned long)r.ms,r.record,
   (unsigned long)r.run,(unsigned long)r.origin,(unsigned long)r.event,
   unsigned(r.type),unsigned(r.hop),unsigned(r.bytes),(unsigned long)r.wire,(unsigned long)r.canonical,
   r.validation,r.state,(long)r.rc,(long)r.rx_rc,int(r.rssi),int(r.snr_x10),unsigned(r.used),
   unsigned(r.sos_left),unsigned(r.cancel_left),r.permit?"true":"false",r.called?"true":"false",
   (unsigned long)lost,(unsigned long)health,(unsigned long)pending,(unsigned long)written,(unsigned long)urgentLost);
  return n>0 && size_t(n)<cap;
 }
 // Only called by the existing main-loop SD owner. Writer mocks exercise this
 // exact firmware state machine. No filesystem/Serial API exists in capture().
 template<class Writer> void poll(Writer& writer,uint32_t now,bool radioBusy) {
  if(failed_ || sealed_ || !initializedOK_ || radioBusy || defersStorage())return;
  if(sealRequested_ && !closing_){closing_=true;capturing_=false;}
  if(count_){
   const Record& r=queue_[read_];char line[kLine];
   if(!row(line,sizeof(line),r,local_,session_,lost_,health_,uint32_t(count_-1),written_,urgentLost_)){
    health_|=Format;lose();failed_=true;return;
   }
   if(!writer.appendChecked(line,strlen(line))){fail(Storage);return;}
   ++written_;read_=(read_+1)%kCapacity;--count_;
   if(!strcmp(r.record,"END"))endWritten_=true;
   return;
  }
  if(closing_ && !endQueued_){
   Record end{};token(end.record,sizeof(end.record),"END");end.ms=now;
   endQueued_=enqueue(end);return;
  }
  if(endWritten_){
   const Finish result=writer.finish();
   if(result==Finish::Failed)fail(Storage);
   if(result==Finish::OK)sealed_=true;
  }
 }
private:
 bool enqueue(Record row) {
  if(next_==UINT32_MAX){fail(Index);return false;}
  row.i=++next_;
  if(!validRecord(row)){lose();health_|=Format;return false;}
  if(count_==kCapacity){lose();return false;}
  queue_[(read_+count_)%kCapacity]=row;++count_;return true;
 }
 void lose(){health_|=Loss;if(lost_!=UINT32_MAX)++lost_;}
 Record queue_[kCapacity]{};
 char session_[9]{};
 size_t read_=0,count_=0;
 uint32_t local_=0,next_=0,written_=0,lost_=0,health_=0,urgentLost_=0;
 bool initialized_=false,initializedOK_=false,started_=false,capturing_=false;
 bool sealRequested_=false,closing_=false,endQueued_=false,endWritten_=false;
 bool failed_=false,sealed_=false;
};
}
