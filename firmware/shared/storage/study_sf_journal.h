#pragma once
// Finite-study JSONL store and upload cursor. One caller owns all files.
// No deletion, historical RAM ledger, or durable cursor. Reboot replays retained IDs.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cerrno>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "dtn_jsonl_snapshot.h"
#include "dtn_f24m_sos_packet_store.h"
#if defined(ARDUINO)
#include <cJSON.h>
#else
// Host tests link the system cJSON library; only its public opaque C API is used.
extern "C" {
struct cJSON;
cJSON* cJSON_ParseWithLengthOpts(const char*,size_t,const char**,int);
void cJSON_Delete(cJSON*);
cJSON* cJSON_GetObjectItemCaseSensitive(const cJSON*,const char*);
cJSON* cJSON_GetArrayItem(const cJSON*,int);
int cJSON_GetArraySize(const cJSON*);
int cJSON_IsObject(const cJSON*); int cJSON_IsArray(const cJSON*); int cJSON_IsString(const cJSON*); int cJSON_IsNumber(const cJSON*);
char* cJSON_GetStringValue(const cJSON*); double cJSON_GetNumberValue(const cJSON*);
}
#endif
namespace StudySF {
constexpr size_t MaxLine=768, MaxBatch=8, MaxBody=8192, MaxReply=2048;
struct Record { char line[MaxLine]{}; };
inline bool uuid(const char* s){if(!s||strlen(s)!=36||s[14]!='4'||!strchr("89ab",s[19]))return false;for(unsigned i=0;i<36;++i){if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}else if(!strchr("0123456789abcdef",s[i]))return false;}return true;}
inline void uuidFromBytes(uint8_t (&b)[16],char (&s)[37]){b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;size_t j=0;for(size_t i=0;i<16;++i){if(i==4||i==6||i==8||i==10)s[j++]='-';s[j++]="0123456789abcdef"[b[i]>>4];s[j++]="0123456789abcdef"[b[i]&15];}s[j]=0;}
inline bool utcValid(const char* s){if(!s||strlen(s)!=20)return false;for(unsigned i=0;i<20;++i){const char expected=i==4||i==7?'-':i==10?'T':i==13||i==16?':':i==19?'Z':0;if(expected?s[i]!=expected:s[i]<'0'||s[i]>'9')return false;}unsigned y,m,d,h,n,sec;if(sscanf(s,"%4u-%2u-%2uT%2u:%2u:%2uZ",&y,&m,&d,&h,&n,&sec)!=6||m<1||m>12||h>23||n>59||sec>59)return false;const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};return d>=1&&d<=days[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0));}
inline void number(char (&out)[32],double x,double lo,double hi){if(!std::isfinite(x)||x<lo||x>hi)strcpy(out,"null");else snprintf(out,sizeof(out),"%.9g",x);}
inline bool label(const char* s){if(!s||strlen(s)>64)return false;for(;*s;++s)if(!strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_ -",*s))return false;return true;}
class Encoder {char boot_[37]{};uint32_t node_=0,counter_=0;
 bool emit(Record& r,const char* kind,uint32_t ms,const char* utc,const char* payload){if(!node_||counter_==UINT32_MAX)return false;char stamp[24]="null";if(utcValid(utc))snprintf(stamp,sizeof(stamp),"\"%s\"",utc);const int n=snprintf(r.line,sizeof(r.line),"{\"record_id\":\"%s:%lu\",\"kind\":\"%s\",\"recorder_node_id\":%lu,\"captured_at\":%s,\"uptime_ms\":%lu,\"payload\":%s}",boot_,(unsigned long)++counter_,kind,(unsigned long)node_,stamp,(unsigned long)ms,payload);return n>0&&size_t(n)<sizeof(r.line);}
 public:
 bool begin(uint32_t node,const char* boot){if(node_||node<1||node>9999||!uuid(boot))return false;node_=node;strcpy(boot_,boot);return true;}
 bool weather(Record& r,uint32_t ms,const char* utc,double t,double rh,double p,double trend,unsigned state,const char* microState=nullptr,const char* pattern=nullptr,bool valid=false){if(state>3)return false;char a[32],b[32],c[32],d[32],micro[240]="null",payload[480];number(a,t,-100,100);number(b,rh,0,100);number(c,p,100,1200);number(d,trend,-200,200);if(microState||pattern){if(!label(microState)||!label(pattern))return false;snprintf(micro,sizeof(micro),"{\"input_valid\":%s,\"state\":\"%s\",\"pattern\":\"%s\"}",valid?"true":"false",microState,pattern);}snprintf(payload,sizeof(payload),"{\"temperature_c\":%s,\"humidity_percent\":%s,\"pressure_hpa\":%s,\"pressure_trend_hpa\":%s,\"weather_state\":%u,\"micro\":%s}",a,b,c,d,state,micro);return emit(r,"weather",ms,utc,payload);}
 bool gps(Record& r,uint32_t ms,const char* utc,bool fix,double lat,double lon){char a[32],b[32],p[160];number(a,fix?lat:NAN,-90,90);number(b,fix?lon:NAN,-180,180);fix=fix&&strcmp(a,"null")&&strcmp(b,"null");if(!fix){strcpy(a,"null");strcpy(b,"null");}snprintf(p,sizeof(p),"{\"gps_fix\":%s,\"latitude\":%s,\"longitude\":%s}",fix?"true":"false",a,b);return emit(r,"gps",ms,utc,p);}
 bool control(Record& r,uint32_t ms,const char* utc,const VesselPacket& packet,uint8_t source){if(!DTNF24M::validControl(packet)||source>1||(source==0&&packet.originNodeId!=node_))return false;char hex[125],p[240];const auto* b=reinterpret_cast<const uint8_t*>(&packet);for(size_t i=0;i<62;++i){hex[2*i]="0123456789abcdef"[b[i]>>4];hex[2*i+1]="0123456789abcdef"[b[i]&15];}hex[124]=0;snprintf(p,sizeof(p),"{\"origin_node_id\":%lu,\"event_id\":%lu,\"source\":\"%s\",\"packet_hex\":\"%s\"}",(unsigned long)packet.originNodeId,(unsigned long)packet.sequenceNumber,source?"received":"local",hex);return emit(r,packet.packetType==PACKET_SOS?"sos":"cancel",ms,utc,p);}
};
struct Json {cJSON* p;explicit Json(const char* s):p(cJSON_ParseWithLengthOpts(s,strlen(s)+1,nullptr,1)){} ~Json(){cJSON_Delete(p);} Json(const Json&)=delete;};
inline const cJSON* field(const cJSON* p,const char* k){return cJSON_GetObjectItemCaseSensitive(p,k);}
inline bool strEq(const cJSON* p,const char* s){return cJSON_IsString(p)&&strcmp(cJSON_GetStringValue(p),s)==0;}
inline bool numEq(const cJSON* p,uint32_t n){return cJSON_IsNumber(p)&&cJSON_GetNumberValue(p)==double(n);}
inline bool recordId(const char* line,const char* boot,uint32_t node,char (&id)[48]){Json j(line);const auto* v=field(j.p,"record_id");if(!cJSON_IsObject(j.p)||cJSON_GetArraySize(j.p)!=6||!numEq(field(j.p,"recorder_node_id"),node)||!cJSON_IsString(v))return false;const char* s=cJSON_GetStringValue(v);const size_t len=strlen(s);if(len<38||len>47||strncmp(s,boot,36)||s[36]!=':'||s[37]<'1'||s[37]>'9')return false;uint64_t n=0;for(size_t i=37;i<len;++i){if(s[i]<'0'||s[i]>'9')return false;n=n*10+unsigned(s[i]-'0');}if(n>UINT32_MAX)return false;strcpy(id,s);return true;}
struct Batch {char body[MaxBody]{};char ids[MaxBatch][48]{};char id[37]{};size_t count=0;long end=0;};
inline bool confirmed(const Batch& b,uint32_t node,int http,const char* reply){if(http!=200||!b.count||!reply||strlen(reply)>=MaxReply)return false;Json j(reply);const auto* arr=field(j.p,"records");if(!cJSON_IsObject(j.p)||cJSON_GetArraySize(j.p)!=4||!numEq(field(j.p,"schema_version"),1)||!numEq(field(j.p,"recorder_node_id"),node)||!strEq(field(j.p,"batch_id"),b.id)||!cJSON_IsArray(arr)||cJSON_GetArraySize(arr)!=int(b.count))return false;for(size_t i=0;i<b.count;++i){const auto* r=cJSON_GetArrayItem(arr,int(i));const auto* status=field(r,"status");if(!cJSON_IsObject(r)||cJSON_GetArraySize(r)!=2||!strEq(field(r,"record_id"),b.ids[i])||!(strEq(status,"accepted")||strEq(status,"duplicate")))return false;}return true;}
// Each boot has one exclusive file. Replay prior files by name, then current file.
// Offsets are RAM only; loss of a confirmation or restart results in safe retries.
class Journal {char dir_[160]{},boot_[37]{},current_[48]{},selected_[48]{},previous_[48]{};uint32_t node_=0;long offset_=0;bool active_=false,storageFault_=false,syncFault_=false;unsigned torn_=0;Batch pending_{};
 bool path(char (&p)[224],const char* name)const {int n=snprintf(p,sizeof(p),"%s/%s",dir_,name);return n>0&&size_t(n)<sizeof(p);}
 static bool mkdirChecked(const char* p){if(mkdir(p,0700)==0)return true;struct stat st{};return errno==EEXIST&&stat(p,&st)==0&&S_ISDIR(st.st_mode);}
 bool fail(){storageFault_=true;return false;}
 bool select(){if(selected_[0])return true;DIR* d=opendir(dir_);if(!d)return false;char best[48]{};int err=0;while(true){errno=0;auto* e=readdir(d);if(!e){err=errno;break;}const char* n=e->d_name;if(strlen(n)!=42||strcmp(n+36,".jsonl")||!strcmp(n,current_)||strcmp(n,previous_)<=0)continue;char u[37]{};memcpy(u,n,36);if(!uuid(u))continue;if(!best[0]||strcmp(n,best)<0)strcpy(best,n);}if(closedir(d)!=0||err)return false;active_=!best[0];strcpy(selected_,active_?current_:best);offset_=0;return true;}
 public:
 bool begin(const char* root,uint32_t node,const char* boot){if(node_||!root||node<1||node>9999||!uuid(boot))return false;char base[128];int n=snprintf(base,sizeof(base),"%s/study_sf",root);if(n<0||size_t(n)>=sizeof(base)||!mkdirChecked(base))return fail();n=snprintf(dir_,sizeof(dir_),"%s/v%04lu",base,(unsigned long)node);if(n<0||size_t(n)>=sizeof(dir_)||!mkdirChecked(dir_))return fail();strcpy(boot_,boot);snprintf(current_,sizeof(current_),"%s.jsonl",boot);char p[224];if(!path(p,current_))return fail();int fd=open(p,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return fail();bool ok=fsync(fd)==0;if(close(fd)!=0)ok=false;if(!ok)return fail();node_=node;return true;}
 bool append(const Record& r){if(!node_||storageFault_)return false;char id[48],p[224];if(!recordId(r.line,boot_,node_,id)||!path(p,current_))return fail();FILE* f=fopen(p,"r+b");if(!f)return fail();bool ok=fseek(f,0,SEEK_END)==0;long start=ok?ftell(f):-1;const size_t len=strlen(r.line);if(start<0)ok=false;if(ok&&start>0){ok=fseek(f,-1,SEEK_END)==0&&fgetc(f)=='\n'&&fseek(f,0,SEEK_END)==0;}if(ok)ok=fwrite(r.line,1,len,f)==len&&fputc('\n',f)!=EOF&&fflush(f)==0&&fsync(fileno(f))==0;char read[MaxLine]{};if(ok)ok=fseek(f,start,SEEK_SET)==0&&fread(read,1,len,f)==len&&memcmp(read,r.line,len)==0&&fgetc(f)=='\n';if(fclose(f)!=0)ok=false;return ok?true:fail();}
 enum class Next{Ready,Idle,Advanced,Torn,Fault};
 Next prepare(const char* batchId){if(!node_||storageFault_||syncFault_)return Next::Fault;if(pending_.count)return Next::Ready;if(!uuid(batchId)||!select()){syncFault_=true;return Next::Fault;}char p[224],session[37]{};memcpy(session,selected_,36);if(!path(p,selected_)){syncFault_=true;return Next::Fault;}FILE* f=fopen(p,"rb");if(!f){syncFault_=true;return Next::Fault;}bool ok=fseek(f,offset_,SEEK_SET)==0;bool eof=false,torn=false;pending_=Batch{};strcpy(pending_.id,batchId);size_t used=size_t(snprintf(pending_.body,sizeof(pending_.body),"{\"schema_version\":1,\"batch_id\":\"%s\",\"records\":[",batchId));while(ok&&pending_.count<MaxBatch){char line[MaxLine]{};size_t len=0;int c;while((c=fgetc(f))!=EOF&&c!='\n'){if(len>=sizeof(line)-1||c==0){ok=false;break;}line[len++]=char(c);}if(!ok)break;if(c==EOF){if(ferror(f))ok=false;else{eof=true;torn=len!=0;}break;}if(!DTNJsonlSnapshot::framedObject(line,len)||!recordId(line,session,node_,pending_.ids[pending_.count])){ok=false;break;}if(used+len+4>=sizeof(pending_.body)){ok=false;break;}if(pending_.count)pending_.body[used++]=',';memcpy(pending_.body+used,line,len);used+=len;pending_.end=ftell(f);if(pending_.end<0){ok=false;break;}++pending_.count;}if(fclose(f)!=0)ok=false;if(!ok){pending_=Batch{};syncFault_=true;return Next::Fault;}if(pending_.count){strcpy(pending_.body+used,"]}");return Next::Ready;}if(!eof){syncFault_=true;return Next::Fault;}if(torn&&active_){syncFault_=true;return Next::Fault;}if(active_)return Next::Idle;strcpy(previous_,selected_);selected_[0]=0;if(torn){++torn_;return Next::Torn;}return Next::Advanced;}
 const Batch& batch()const{return pending_;}
 bool confirm(int status,const char* body){if(!confirmed(pending_,node_,status,body))return false;offset_=pending_.end;pending_=Batch{};return true;}
 unsigned tornFiles()const{return torn_;} bool storageFault()const{return storageFault_;} bool syncFault()const{return syncFault_;}
};
} // namespace StudySF
