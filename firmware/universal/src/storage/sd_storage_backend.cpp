// F24Y/Z retain compile/link evidence. F25A adds a separate permanent-deny runtime caller.
#ifndef THESIS_DTN_F24Y_BUILD_ONLY
#define THESIS_DTN_F24Y_BUILD_ONLY 0
#endif
#if THESIS_DTN_F24Y_BUILD_ONLY != 0 && THESIS_DTN_F24Y_BUILD_ONLY != 1
#error "Invalid F24Y flag"
#endif
#if THESIS_DTN_F24Y_BUILD_ONLY == 1
#if THESIS_DTN_F24X_BUILD_ONLY != 1 || THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1
#error "F24Y requires existing F24X/K permanent deny"
#endif
#include "../../../shared/storage/dtn_checked_vfs.h"
#include <esp_vfs_fat.h>
#include <driver/spi_master.h>
#include <driver/sdspi_host.h>
#include <esp_idf_version.h>
#include <esp32-hal-spi.h>
#if !defined(CONFIG_IDF_TARGET_ESP32S3) || !CONFIG_IDF_TARGET_ESP32S3
#error "F26A audited controller mapping requires ESP32-S3"
#endif
#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5,5,5)
#error "F24Y audited SDK is ESP-IDF 5.5.5"
#endif
namespace {
// Arduino bus indices are NOT ESP-IDF host enum values. On the pinned S3
// core FSPI routes to SPI2; HSPI routes to SPI3. Keep LoRa on SPI2 and
// reserve SPI3 for this dormant SD backend, subject to a future owner grant.
static_assert(FSPI==0 && HSPI==1,"Unexpected Arduino S3 SPI mapping");
constexpr spi_host_device_t kSdHost=SPI3_HOST;
static_assert(kSdHost!=SPI2_HOST,"SD must not claim the LoRa controller");
// This driver assumes externally qualified exclusive bus/power/card identity.
// No such grant exists in firmware. Not instantiated by the live logger.
// Do not mix this driver with Arduino SD/SPI on the same host.
class EspMount final {
 sdmmc_card_t* card_=nullptr;
 bool bus_=false, fault_=false;
 bool fail(){fault_=true;return false;}
 public:
 const char* root()const{return "/sd";}
 bool mount(){
  if(bus_||card_||fault_)return false;
  spi_bus_config_t bus{};
  bus.mosi_io_num=35;bus.miso_io_num=37;bus.sclk_io_num=36;
  bus.quadwp_io_num=-1;bus.quadhd_io_num=-1;
  bus.data4_io_num=-1;bus.data5_io_num=-1;bus.data6_io_num=-1;bus.data7_io_num=-1;
  bus.max_transfer_sz=4096;
  if(spi_bus_initialize(kSdHost,&bus,SPI_DMA_CH_AUTO)!=ESP_OK)return fail();
  bus_=true;
  sdmmc_host_t host=SDSPI_HOST_DEFAULT();host.slot=kSdHost;host.max_freq_khz=1000;
  sdspi_device_config_t device=SDSPI_DEVICE_CONFIG_DEFAULT();device.host_id=kSdHost;device.gpio_cs=GPIO_NUM_47;
  esp_vfs_fat_mount_config_t config{};
  config.format_if_mount_failed=false;config.max_files=2;config.disk_status_check_enable=true;
  // A failed mount can leave SDK state uncertain. Do not free/retry blindly.
  if(esp_vfs_fat_sdspi_mount(root(),&host,&device,&config,&card_)!=ESP_OK || !card_)return fail();
  return true;
 }
 bool unmount(){
  if(fault_||!bus_||!card_)return false;
  if(esp_vfs_fat_sdcard_unmount(root(),card_)!=ESP_OK)return fail();
  card_=nullptr;
  if(spi_bus_free(kSdHost)!=ESP_OK)return fail();
  bus_=false;return true;
 }
};
using Target=DTNF24Y::Backend<EspMount>;
using Mode=DTNF24Y::Mode;using Action=DTNF24Y::Action;
static_assert(std::is_trivially_destructible<Target>::value,"No implicit target teardown");
// Retain actual backend methods in the linked ELF via a data anchor, not a
// callable test command or startup probe. Nothing references this table at runtime.
struct LinkTable {
 bool (Target::*mount)();
 bool (Target::*exists)(const char*,bool&);
 bool (Target::*open)(uint8_t,const char*,Mode);
 bool (Target::*io)(uint8_t,Action,uint8_t*,size_t);
 bool (Target::*size)(uint8_t,size_t&);
 bool (Target::*seek)(uint8_t,size_t);
 bool (Target::*flush)(uint8_t);
 bool (Target::*close)(uint8_t);
 bool (Target::*unmount)();
};
} // namespace
extern "C" {
// --undefined retains this data and its member-function references under GC.
extern const LinkTable thesis_f24y_link_anchor __attribute__((used)) = {
 &Target::mount,&Target::exists,&Target::open,&Target::io,&Target::size,
 &Target::seek,&Target::flush,&Target::close,&Target::unmount};
}
// F24Z: retain real discovery and the composed service methods for target
// compilation only in F24Z. F25A below supplies the separate runtime path.
#if defined(THESIS_DTN_F24Z_BUILD_ONLY) && THESIS_DTN_F24Z_BUILD_ONLY == 1
#include "../../../shared/storage/dtn_session_service.h"
namespace {
using ZService=DTNF24Z::Service<Target,DTNF24W::DenyWitness,DTNF24O::DenyWitness>;
using ZResult=DTNF24Z::Result;
using ZQueue=DTNF24M::PendingQueue<8>;
using ZVerifier=DTNF24N::DenyReceiptVerifier;
using ZGuard=bool(*)();
struct ZLinkTable {
 bool (Target::*discover)(uint32_t,const char*,DTNF24W::SessionSnapshot&,const ZGuard&);
 ZResult (ZService::*start)(uint32_t,const char*);
 ZResult (ZService::*scan)();
 ZResult (ZService::*append)(ZQueue&);
 ZResult (ZService::*poll)(const ZVerifier&);
 ZResult (ZService::*receipt)(const DTNF24N::Receipt&,const ZVerifier&);
 ZResult (ZService::*shutdown)();
};
}
extern "C" {
extern const ZLinkTable thesis_f24z_link_anchor __attribute__((used))={
 &Target::discover<ZGuard>,&ZService::start,&ZService::discover,
 &ZService::append<ZQueue>,&ZService::poll<ZVerifier>,&ZService::receipt<ZVerifier>,&ZService::shutdown};
}
#if THESIS_DTN_F25A_BUILD_ONLY == 1
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../../../shared/storage/dtn_runtime_guard.h"
#if THESIS_DTN_F25B_BUILD_ONLY == 1
namespace {
using BService=DTNF25B::Service<Target,DTNF24W::DenyWitness,DTNF25B::DenyWitness>;
struct BLinkTable {
 ZResult (BService::*start)(uint32_t,const char*);
 ZResult (BService::*scan)();
 ZResult (BService::*append)(ZQueue&);
 ZResult (BService::*poll)(const ZVerifier&);
 ZResult (BService::*receipt)(const DTNF24N::Receipt&,const ZVerifier&);
 ZResult (BService::*shutdown)();
};
}
extern "C" {
extern const BLinkTable thesis_f25b_link_anchor __attribute__((used))={
 &BService::start,&BService::discover,&BService::append<ZQueue>,&BService::poll<ZVerifier>,&BService::receipt<ZVerifier>,&BService::shutdown};
}
#endif
namespace {
// Actual target adapter, private instance, immutable denial, no callable start.
DTNF25A::Runtime<Target> f25aRuntime;
uintptr_t f25aTaskToken(){
 if(xPortInIsrContext())return 0;
 return reinterpret_cast<uintptr_t>(xTaskGetCurrentTaskHandle());
}
}
bool thesisF25ABindLoop(){return f25aRuntime.bind(f25aTaskToken());}
DTNF24M::CaptureResult thesisF25ACapture(DTNF25A::Queue& queue,const VesselPacket& packet,uint8_t source){
 return f25aRuntime.capture(f25aTaskToken(),queue,packet,source);
}
void thesisF25APoll(DTNF25A::Queue& queue,bool radioBusy){
 DTNF25A::Report report{};
 const auto result=f25aRuntime.poll(f25aTaskToken(),queue,millis(),radioBusy,report);
 if(result==DTNF25A::Tick::Denied || result==DTNF25A::Tick::Unexpected)
  Serial.printf("[F25A] admission=%s pending=%u full_drop=%lu durable=NO physical=DENIED\n",
    result==DTNF25A::Tick::Denied?"REJECTED_NO_SESSION":"UNEXPECTED_STOP_REVIEW",
    unsigned(report.pending),(unsigned long)report.lost);
}
#endif
#endif
#endif

#if THESIS_STUDY_SF_BUILD_ONLY == 1
#if THESIS_DTN_F25A_BUILD_ONLY != 1 || THESIS_DTN_F25C_BUILD_ONLY != 1 || THESIS_DTN_F24Y_BUILD_ONLY != 1
#error "Study SF requires inherited deny profile and SPI3 mount implementation"
#endif
#include <WiFi.h> // explicit PlatformIO dependency discovery for study worker
#include "../synchronization/dtn_worker.inc"
#endif
