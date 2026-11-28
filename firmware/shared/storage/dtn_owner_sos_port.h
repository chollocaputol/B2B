#pragma once
// F24X: actual F24M/N client API on F24W owner-held tokens. SOS only.
// Single caller, owner and witness outlive Port and handles. No raw File.
// A fault leaves uncertain resources in the owner inventory; never retry.
#include "dtn_owned_sos_port.h"
#include "dtn_mount_owner.h"
namespace DTNF24X {
template<class Owner,class Witness> class Port final {
 using Token=typename Owner::Token;
 public:
 class FileHandle final {
  friend class Port;
  FileHandle(Port& p,Token&& t,bool wr,bool old):p_(&p),token_(std::move(t)),write_(wr),old_(old){}
  Port* p_=nullptr;Token token_{};bool write_=false,old_=false;
  bool live(){return p_ && p_->live(old_);}
  public:
  FileHandle()=default;
  FileHandle(const FileHandle&)=delete;FileHandle& operator=(const FileHandle&)=delete;
  FileHandle(FileHandle&& o)noexcept:p_(o.p_),token_(std::move(o.token_)),write_(o.write_),old_(o.old_){o.p_=nullptr;}
  FileHandle& operator=(FileHandle&&)=delete;
  // Abandonment quarantines logical inventory, not backend close.
  ~FileHandle(){if(p_)p_->fail();}
  explicit operator bool(){return live();}
  size_t size(){size_t n=0;if(live() && !p_->owner_.size(token_,n))p_->fail();return n;}
  bool seek(size_t at){if(live() && p_->owner_.seek(token_,at))return true;if(p_)p_->fail();return false;}
  int read(uint8_t* dst,size_t n){
   if(live() && !write_ && dst && n && n<=DTNF24M::kFrameBytes && p_->owner_.io(token_,DTNF24W::Action::Read,dst,n))return int(n);
   if(p_)p_->fail();
   return 0;
  }
  size_t write(const uint8_t* src,size_t n){
   if(live() && write_ && src && n && n<=DTNF24M::kFrameBytes){
    uint8_t copy[DTNF24M::kFrameBytes]{};memcpy(copy,src,n);
    if(p_->owner_.io(token_,DTNF24W::Action::Write,copy,n))return n;
   }
   if(p_)p_->fail();
   return 0;
  }
  void flush(){if(p_ && (!live() || !write_ || !p_->owner_.flush(token_)))p_->fail();}
  int getWriteError(){return live() && write_?0:1;}
  void close(){
   if(!p_)return;
   if(!live() || !p_->owner_.close(token_) || !p_->live(old_))p_->fail();
   p_->open_=false;p_=nullptr;
  }
 };
 Port(Owner& o,const Witness& w):owner_(o),witness_(w){}
 Port(const Port&)=delete;Port& operator=(const Port&)=delete;
 bool bind(uint32_t vessel,const char* current){
  if(attempted_)return fail();
  attempted_=true;char check[96]{};
  const auto e=witness_.view();
  if(!DTNF24M::sandboxPath(check,sizeof(check),current,vessel) || !DTNF24O::validEvidence(e,vessel))return fail();
  vessel_=vessel;media_=e.mediaTag;epoch_=e.mountEpoch;serial_=e.ownerSerial;
  strncpy(current_,current,sizeof(current_)-1);bound_=true;return live(false);
 }
 bool storageHealthy(){return live(false);}
 bool bound(){return live(false);}
 bool faulted()const{return fault_;}
 bool hasHandle()const{return open_;}
 bool exists(const char* path){
  bool current=false,found=false;
  if(!live(false) || open_ || !DTNF24O::supportedPath(path,current_,vessel_,current) || !live(!current))return fail();
  if(!owner_.exists(path,found) || !live(!current))return fail();
  return found;
 }
 FileHandle open(const char* path,const char* mode){
  const bool wr=mode && strcmp(mode,"a")==0,rd=mode && strcmp(mode,"r")==0;
  bool current=false;
  if(!live(false) || open_ || (!wr && !rd) || !DTNF24O::supportedPath(path,current_,vessel_,current) || (wr && !current) || !live(!current)){fail();return {};}
  auto token=owner_.open(DTNF24W::Lane::Sos,path,wr?DTNF24W::Mode::Append:DTNF24W::Mode::Read);
  if(!token || !live(!current)){fail();return {};}
  open_=true;return FileHandle(*this,std::move(token),wr,!current);
 }
 // Positive directory discovery is NOT migrated. Never silently skip it.
 template<class Sink>bool scanClosedSessions(const char*,const char*,Sink&&){return fail();}
 private:
 Owner& owner_;const Witness& witness_;uint32_t vessel_=0;
 uint64_t media_=0,epoch_=0,serial_=0;char current_[64]{};
 bool attempted_=false,bound_=false,fault_=false,open_=false;
 bool fail(){fault_=true;bound_=false;owner_.quarantine();return false;}
 bool live(bool old){
  if(fault_ || !bound_)return false;
  const auto e=witness_.view();
  if(!DTNF24O::validEvidence(e,vessel_) || e.mediaTag!=media_ || e.mountEpoch!=epoch_ || e.ownerSerial!=serial_ || (old && e.urgentPending) || !owner_.checkedIdentity(media_,epoch_,serial_))return fail();
  return true;
 }
};
} // namespace DTNF24X
