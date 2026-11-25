// Included in dtn_f24y_target_backend.cpp to reuse its existing private SPI3 mount.
// This candidate deliberately has NO provisioned configuration or permission.
#include "study_sf_target.h"
#include "../../shared/storage/study_sf_journal.h"
#include <WiFi.h>
#include <esp_http_client.h>
#include <esp_random.h>
#include <freertos/queue.h>
#include <time.h>
#include <atomic>
namespace {
struct StudyConfig {bool authorized;const char* ssid;const char* password;const char* url;const char* bearer;const char* apikey;const char* ca;const char* ntp;};
// No runtime override, serial command, secrets or production URL. Separate approval required.
#if THESIS_STUDY_SF_SYNC_V003 == 1
#include "study_sf_local_config.generated.h"
#else
const StudyConfig studyConfig{false,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
#endif
bool usable(const StudyConfig& c){return c.authorized&&c.ssid&&*c.ssid&&c.password&&c.url&&!strncmp(c.url,"https://",8)&&c.bearer&&*c.bearer&&c.apikey&&*c.apikey&&c.ca&&*c.ca&&c.ntp&&*c.ntp;}
// This opt-in bench profile permits storage only; the original config stays empty.
bool storageUsable(const StudyConfig& c){
#if THESIS_STUDY_SF_OFFLINE_V003 == 1
 return true;
#else
 return usable(c);
#endif
}
StudySF::Encoder studyEncoder;
// Volatile manual recording mode; no persistence or new storage owner.
std::atomic<bool> studyContinuous{false};
std::atomic<bool> studyStorageReady{false};
QueueHandle_t studyQueue=nullptr;
uint32_t studyNode=0;char studyBoot[37]{};
void newUuid(char (&out)[37]){uint8_t bytes[16];esp_fill_random(bytes,sizeof(bytes));StudySF::uuidFromBytes(bytes,out);}
struct ResponseBuffer {char body[StudySF::MaxReply]{};size_t len=0;bool overflow=false;};
esp_err_t receiveResponse(esp_http_client_event_t* e){if(e->event_id==HTTP_EVENT_ON_DATA){auto* b=static_cast<ResponseBuffer*>(e->user_data);if(e->data_len<0||size_t(e->data_len)>=sizeof(b->body)-b->len||memchr(e->data,0,size_t(e->data_len))){b->overflow=true;return ESP_FAIL;}memcpy(b->body+b->len,e->data,size_t(e->data_len));b->len+=size_t(e->data_len);b->body[b->len]=0;}return ESP_OK;}
int upload(const StudyConfig& c,const StudySF::Batch& batch,ResponseBuffer& reply){if(!usable(c)||WiFi.status()!=WL_CONNECTED||time(nullptr)<1700000000)return 0;esp_http_client_config_t cfg{};cfg.url=c.url;cfg.cert_pem=c.ca;cfg.timeout_ms=5000;cfg.buffer_size_tx=4096;cfg.disable_auto_redirect=true;cfg.event_handler=receiveResponse;cfg.user_data=&reply;cfg.transport_type=HTTP_TRANSPORT_OVER_SSL;auto client=esp_http_client_init(&cfg);if(!client)return 0;char auth[2048];int n=snprintf(auth,sizeof(auth),"Bearer %s",c.bearer);int status=0;if(n>0&&size_t(n)<sizeof(auth)&&esp_http_client_set_method(client,HTTP_METHOD_POST)==ESP_OK&&esp_http_client_set_header(client,"Authorization",auth)==ESP_OK&&esp_http_client_set_header(client,"apikey",c.apikey)==ESP_OK&&esp_http_client_set_header(client,"Content-Type","application/json")==ESP_OK&&esp_http_client_set_post_field(client,batch.body,int(strlen(batch.body)))==ESP_OK&&esp_http_client_perform(client)==ESP_OK&&!reply.overflow)status=esp_http_client_get_status_code(client);esp_http_client_cleanup(client);return status;}
void studyWorker(void* opaque){
 const auto* config=static_cast<const StudyConfig*>(opaque);
 if(!config||!storageUsable(*config)){vTaskDelete(nullptr);return;}
 // One worker is the sole study-file owner, never shares File handles with the loop.
 const auto& c=*config;
 static EspMount mount;static StudySF::Journal journal;
 if(!mount.mount()||!journal.begin(mount.root(),studyNode,studyBoot)){Serial.println("[STUDY-SF] STORAGE_START_FAILED; no durability claim");vTaskDelete(nullptr);return;}
 studyStorageReady.store(true);
#if THESIS_STUDY_SF_OFFLINE_V003 == 1
 Serial.printf("[STUDY-SF] OFFLINE_READY node=%lu path=/sd/study_sf/v%04lu/%s.jsonl; WIFI=DISABLED\n",(unsigned long)studyNode,(unsigned long)studyNode,studyBoot);
 // Read at most one existing batch on boot, without acknowledging or uploading it.
 // This is evidence of a readable retained prefix, not a full-card audit.
 char auditId[37];newUuid(auditId);const auto prior=journal.prepare(auditId);
 if(prior==StudySF::Journal::Next::Ready){
   Serial.printf("[STUDY-SF] RETAINED_PREFIX count=%u\n",unsigned(journal.batch().count));
   for(size_t i=0;i<journal.batch().count;++i)Serial.printf("[STUDY-SF] RETAINED_ID %s\n",journal.batch().ids[i]);
 }else Serial.printf("[STUDY-SF] RETAINED_PREFIX result=%u (0=Ready,1=Idle,2=Advanced,3=Torn,4=Fault)\n",unsigned(prior));
#else
 bool netStarted=false,clockStarted=false,syncStopped=false,online=false,clockReported=false;uint32_t lastConnect=0,lastUpload=0;
 Serial.printf("[STUDY-SF] SYNC_READY node=%lu path=/sd/study_sf/v%04lu/%s.jsonl\n",(unsigned long)studyNode,(unsigned long)studyNode,studyBoot);
#endif
 for(;;){StudySF::Record record{};
  if(xQueueReceive(studyQueue,&record,0)==pdTRUE){if(!journal.append(record)){studyStorageReady.store(false);Serial.println("[STUDY-SF] STORAGE_WRITE_FAILED; STOP; review retained files");vTaskDelete(nullptr);return;}Serial.println("[STUDY-SF] APPEND_CHECKED");
#if THESIS_STUDY_SF_OFFLINE_V003 == 1 || THESIS_STUDY_SF_SYNC_V003 == 1
  // Exact appended/read-back-checked record, for local serial evidence only.
  Serial.printf("[STUDY-SF] STORED %s\n",record.line);
#endif
  }
#if THESIS_STUDY_SF_OFFLINE_V003 != 1
  const uint32_t now=millis();
  if(!syncStopped){
   if(WiFi.status()!=WL_CONNECTED){if(online){online=false;Serial.println("[STUDY-SF] WIFI_DISCONNECTED; retaining records");}if(!netStarted||uint32_t(now-lastConnect)>=30000U){netStarted=true;lastConnect=now;WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.begin(c.ssid,c.password);}}
   else {if(!online){online=true;Serial.printf("[STUDY-SF] WIFI_CONNECTED ip=%s\n",WiFi.localIP().toString().c_str());}if(!clockStarted){configTime(0,0,c.ntp);clockStarted=true;Serial.println("[STUDY-SF] SNTP_STARTED");}
    if(!clockReported&&time(nullptr)>=1700000000){clockReported=true;Serial.printf("[STUDY-SF] CLOCK_READY epoch=%lld\n",(long long)time(nullptr));}
    if(time(nullptr)>=1700000000&&uxQueueMessagesWaiting(studyQueue)==0&&uint32_t(now-lastUpload)>=1000U){lastUpload=now;char batchId[37];newUuid(batchId);auto next=journal.prepare(batchId);
     if(next==StudySF::Journal::Next::Ready){ResponseBuffer reply;int status=upload(c,journal.batch(),reply);if(journal.confirm(status,reply.body)){Serial.println("[STUDY-SF] BATCH_CONFIRMED");Serial.printf("[STUDY-SF] RECEIPT %s\n",reply.body);}else {Serial.printf("[STUDY-SF] UPLOAD_UNCONFIRMED status=%d; retained\n",status);if(status==400||status==401||status==403||status==409||status==422)syncStopped=true;}}
     else if(next==StudySF::Journal::Next::Torn)Serial.println("[STUDY-SF] TORN_PRIOR_TAIL; retained; trial incomplete");
     else if(next==StudySF::Journal::Next::Fault){syncStopped=true;Serial.println("[STUDY-SF] SYNC_READ_FAILED; retained; review required");}
    }
   }
  }
#endif
  vTaskDelay(pdMS_TO_TICKS(20));
 }
}
bool enqueue(const StudySF::Record& r,bool valid){if(!valid||!studyQueue||xQueueSend(studyQueue,&r,0)!=pdTRUE){Serial.println("[STUDY-SF] CAPTURE_NOT_QUEUED; trial loss");return false;}return true;}
} // namespace
// Keep the positive worker compiled and linked, but provide no runtime grant.
extern "C" {extern void (*const thesis_study_sf_link_anchor)(void*) __attribute__((used))=studyWorker;}
bool thesisStudySfConfigured(){return storageUsable(studyConfig);}
void thesisStudySfSetContinuous(bool continuous){if(thesisStudySfConfigured())studyContinuous.store(continuous);}
bool thesisStudySfIsContinuous(){return studyContinuous.load();}
bool thesisStudySfStorageReady(){return studyStorageReady.load();}
bool thesisStudySfStart(uint32_t node){
#if THESIS_STUDY_SF_OFFLINE_V003 == 1 || THESIS_STUDY_SF_SYNC_V003 == 1
 if(node!=3U)return false;
#endif
 if(!thesisStudySfConfigured()||studyQueue||node<1||node>9999)return false;newUuid(studyBoot);if(!studyEncoder.begin(node,studyBoot))return false;studyNode=node;studyQueue=xQueueCreate(8,sizeof(StudySF::Record));if(!studyQueue)return false;if(xTaskCreate(studyWorker,"study-sf",32768,const_cast<StudyConfig*>(&studyConfig),1,nullptr)!=pdPASS){vQueueDelete(studyQueue);studyQueue=nullptr;return false;}return true;}
bool thesisStudySfWeather(uint32_t ms,const char* utc,double t,double rh,double p,double trend,unsigned state,const char* micro,const char* pattern,bool valid){if(!thesisStudySfIsContinuous())return false;if(!studyQueue)return false;StudySF::Record r;return enqueue(r,studyEncoder.weather(r,ms,utc,t,rh,p,trend,state,micro,pattern,valid));}
bool thesisStudySfGps(uint32_t ms,const char* utc,bool fix,double lat,double lon){if(!thesisStudySfIsContinuous())return false;if(!studyQueue)return false;StudySF::Record r;return enqueue(r,studyEncoder.gps(r,ms,utc,fix,lat,lon));}
bool thesisStudySfControl(uint32_t ms,const char* utc,const VesselPacket& packet,uint8_t source){if(!studyQueue)return false;StudySF::Record r;return enqueue(r,studyEncoder.control(r,ms,utc,packet,source));}
