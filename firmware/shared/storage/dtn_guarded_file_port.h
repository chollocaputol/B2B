#pragma once
// F24R: explicit per-File operation interception for existing long-lived consumers.
// This is a portable, BUILD-ONLY adapter, not a physical SD owner or a file teardown
// implementation. Firmware binds an ungrantable F24Q DenyWitness, so NO real File
// can be opened here. Synthetic host witnesses validate call-site mechanics only.
// A raw Arduino File's destructor/close after a revoked lease remains UNSOLVED.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utility>
#include "dtn_cross_writer_arbiter.h"

namespace DTNF24R {
template<class Volume,class Witness> class GuardedVolume final {
 public:
 using RawFile=decltype(std::declval<Volume&>().open((const char*)nullptr,(const char*)nullptr));
 using Arbiter=DTNF24Q::Arbiter<Witness>;
 using Lease=DTNF24Q::Lease;
 using Lane=DTNF24Q::Lane;
 using Mode=DTNF24Q::Mode;
 using Action=DTNF24Q::Action;
 class FileHandle final {
  public:
   FileHandle()=default;
   FileHandle(GuardedVolume* port,RawFile&& raw,uint8_t slot,bool writer)
     :port_(port),raw_(std::move(raw)),slot_(slot),writer_(writer){}
   FileHandle(const FileHandle&)=delete;
   FileHandle& operator=(const FileHandle&)=delete;
   FileHandle(FileHandle&& other) noexcept
     :port_(other.port_),raw_(std::move(other.raw_)),slot_(other.slot_),writer_(other.writer_){other.port_=nullptr;}
   FileHandle& operator=(FileHandle&& other) noexcept {
     if(this!=&other){close();port_=other.port_;raw_=std::move(other.raw_);
       slot_=other.slot_;writer_=other.writer_;other.port_=nullptr;}
     return *this;
   }
   // NO physical authority may construct this type with an open File.
   // If a mock lease is revoked, this destructor cannot guarantee safe
   // cleanup of a real Arduino File: physical integration is prohibited.
   ~FileHandle(){close();}
   explicit operator bool()const{return port_ && port_->liveHandle(slot_) && bool(raw_);}
   size_t size(){return allow(Action::Size)?raw_.size():0U;}
   bool isDirectory(){return allow(Action::Size)?raw_.isDirectory():false;}
   const char* name(){return allow(Action::Size)?raw_.name():nullptr;}
   FileHandle openNextFile(){
     if(slot_!=0 || writer_ || !allow(Action::Enumerate))return {};
     return port_->openNextChild(raw_);
   }
   bool seek(size_t pos){return allow(Action::Seek)?raw_.seek(pos):false;}
   int read(){return allow(Action::Read)?raw_.read():-1;}
   int available(){return allow(Action::Read)?raw_.available():0;}
   size_t print(const char* text){return writer_ && text && allow(Action::Write)?raw_.print(text):0U;}
   size_t print(char ch){return writer_ && allow(Action::Write)?raw_.print(ch):0U;}
   size_t read(uint8_t* bytes,size_t length){return bytes && allow(Action::Read)?raw_.read(bytes,length):0U;}
   size_t write(const uint8_t* bytes,size_t length){return writer_ && bytes && allow(Action::Write)?raw_.write(bytes,length):0U;}
   void flush(){if(writer_ && allow(Action::Flush))raw_.flush();}
   int getWriteError(){return writer_ && allow(Action::Size)?raw_.getWriteError():1;}
   void close(){
     if(!port_)return;
#if defined(THESIS_DTN_F24V_BUILD_ONLY) && THESIS_DTN_F24V_BUILD_ONLY == 1
     // F24V: never make a slot look drained BEFORE backend close. A fresh
     // owner/media witness is checked again when releasing the slot.
     // This is a deny-build guard, NOT proof of successful fsync/fclose or
     // that a physical owner remains valid throughout raw_.close().
     if(port_->permitCloseSlot(slot_)){
       raw_.close();
       if(port_->closeSlot(slot_))port_=nullptr;
     }
#else
     if(port_->closeSlot(slot_)){raw_.close();port_=nullptr;}
#endif
     // Revoked: leave tracked handle quarantined; do not call raw_.close().
     // WARNING: destruction of the raw File is an additional unresolved risk.
   }
  private:
   bool allow(Action a){return port_ && port_->stepSlot(slot_,a) && bool(raw_);}
   GuardedVolume* port_=nullptr;
   RawFile raw_{};
   uint8_t slot_=0;
   bool writer_=false;
 };
 GuardedVolume(Volume& volume,Arbiter& arbiter,const Lease& lease)
   :volume_(volume),arbiter_(arbiter),lease_(lease){}
 GuardedVolume(const GuardedVolume&)=delete;
 GuardedVolume& operator=(const GuardedVolume&)=delete;
 bool exists(const char* path){
   if(!path || arbiter_.retiring() || !admitted())return false;
   const bool result=volume_.exists(path);
   return admitted()?result:false;
 }
 FileHandle open(const char* path,const char* mode){
   if(!path || !mode || arbiter_.retiring() || !admitted())return {};
   const bool append=strcmp(mode,"a")==0;
   const bool read=strcmp(mode,"r")==0;
   if(!append && !read)return {};
   const Lane lane=arbiter_.activeLane();
   uint8_t slot=0;
   if(lane==Lane::F23D && read)slot=1;
   if(!arbiter_.open(lease_,slot,append?Mode::Append:Mode::Read))return {};
   RawFile raw=volume_.open(path,mode);
   if(!raw){(void)arbiter_.close(lease_,slot);return {};}
   if(!arbiter_.step(lease_,slot,Action::Size)){
     // Failed/revoked during backend open: no physical cleanup authority.
     // This path is host-model-only; firmware never passes admitted().
     return {};
   }
   return FileHandle(this,std::move(raw),slot,append);
 }
 // F24T: reserve and check slot 1 BEFORE the raw directory iterator call.
 // The caller must close each child before requesting another. In a revoked
 // mock lease the raw object's implicit destructor may still perform I/O:
 // this implementation is NOT authority for physical-media use.
 FileHandle openNextChild(RawFile& parent){
   if(arbiter_.activeLane()!=Lane::History || !arbiter_.hasHandle(0) ||
      !arbiter_.openHistoryChild(lease_))return {};
   RawFile child=parent.openNextFile();
   if(!child){(void)arbiter_.close(lease_,1);return {};}
   if(!arbiter_.step(lease_,1,Action::Size))return {};
   return FileHandle(this,std::move(child),1,false);
 }
 // F24U: strictly cooperative pre-revocation drain. The caller retains and
 // explicitly closes every FileHandle before finishRetirement(). No surprise
 // ownership-loss cleanup, auto-close, forced unmount or regrant is attempted.
 bool verifyRetirementOwner(){return arbiter_.retiring() && arbiter_.probe(lease_);}
 bool beginRetirement(){return arbiter_.beginRetirement(lease_);}
 bool finishRetirement(){return arbiter_.finishRetirement(lease_);}
 bool retiring()const{return arbiter_.retiring();}
 bool hasOpenHandle()const{return arbiter_.hasOpenHandle();}
 bool release(){return arbiter_.release(lease_);}
 bool faulted()const{return arbiter_.faulted();}
 bool quarantined()const{return arbiter_.quarantined();}
 private:
 bool admitted(){
   return lease_.serial!=0 && arbiter_.probe(lease_);
 }
 bool liveHandle(uint8_t slot){
   return admitted() && arbiter_.hasHandle(slot);
 }
 bool stepSlot(uint8_t slot,Action a){return arbiter_.step(lease_,slot,a);}
 bool permitCloseSlot(uint8_t slot){return arbiter_.permitClose(lease_,slot);}
 bool closeSlot(uint8_t slot){return arbiter_.close(lease_,slot);}
 Volume& volume_;
 Arbiter& arbiter_;
 Lease lease_;
 };
} // namespace DTNF24R
