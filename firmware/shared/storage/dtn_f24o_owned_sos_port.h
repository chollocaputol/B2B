#pragma once
// THESIS F24O: session-bound, narrow SOS-only guarded SD port.
// This is an adapter for F24M SessionWriter and F24N Replay/ReceiptWriter.
// NO physical authority is implemented here. A caller-supplied independent
// witness must authenticate a storage owner; firmware wires DenyWitness only.
// Host tests inject simulated witness and disposable files; this is not a FAT
// power-fail durability guarantee, media CID check, RLS, or radio authorization.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <utility>
#include "dtn_f24m_sos_packet_store.h"

namespace DTNF24O {
struct Evidence {
 uint32_t vessel=0;
 uint64_t mediaTag=0,mountEpoch=0,ownerSerial=0;
 bool mounted=false,independentOwner=false,exclusive=false;
 bool f23dClosed=false,otherWriterClosed=false,closedSessionsVerified=false;
 bool radioBusy=false,urgentPending=false,exportActive=false,historyActive=false;
};
struct DenyWitness { Evidence view()const{return {};}};
inline bool validEvidence(const Evidence& e,uint32_t vessel){
 return vessel>=1 && vessel<=3 && e.vessel==vessel && e.mediaTag && e.mountEpoch &&
        e.ownerSerial && e.mounted && e.independentOwner && e.exclusive &&
        e.f23dClosed && e.otherWriterClosed && e.closedSessionsVerified &&
        !e.radioBusy && !e.exportActive && !e.historyActive;
}
inline bool supportedPath(const char* path,const char* current,uint32_t vessel,
                          bool& currentSession){
 currentSession=false;
 if(!path || !current || vessel<1 || vessel>3)return false;
 char root[40]{};
 const int n=snprintf(root,sizeof(root),"/THESIS/F06TEST/V%03lu/",(unsigned long)vessel);
 if(n<=0 || size_t(n)>=sizeof(root))return false;
 if(strncmp(path,root,size_t(n)))return false;
 const char* session=path+size_t(n);
 char sid[9]{};
 for(size_t i=0;i<8;++i){if(!session[i])return false;sid[i]=session[i];}
 if(!DTNF24M::validSession(sid) || session[8]!='/')return false;
 const char* file=session+9;
 if(strcmp(file,"dtn_sos_packets.f24m") && strcmp(file,"dtn_shore_receipts.f24n"))return false;
 char fullSession[64]{};
 if(size_t(n)+8>=sizeof(fullSession))return false;
 memcpy(fullSession,root,size_t(n));memcpy(fullSession+n,sid,8);
 currentSession=strcmp(fullSession,current)==0;
 return true;
}

template<class Backend,class Witness>class Port final {
 public:
 using RawFile=decltype(std::declval<Backend&>().open((const char*)nullptr,(const char*)nullptr));
 class FileHandle final {
  public:
  FileHandle()=default;
  FileHandle(Port* owner,RawFile&& file,bool write)
      :owner_(owner),file_(std::move(file)),write_(write){}
  FileHandle(const FileHandle&)=delete;
  FileHandle& operator=(const FileHandle&)=delete;
  FileHandle(FileHandle&& other)noexcept
      :owner_(other.owner_),file_(std::move(other.file_)),write_(other.write_){other.owner_=nullptr;}
  FileHandle& operator=(FileHandle&& other)noexcept{
   if(this!=&other){close();owner_=other.owner_;file_=std::move(other.file_);
    write_=other.write_;other.owner_=nullptr;}return *this;
  }
  ~FileHandle(){close();}
  explicit operator bool()const{return owner_!=nullptr && bool(file_) && owner_->live(false);}
  size_t size(){return permitted()?file_.size():0U;}
  bool seek(size_t pos){return permitted() && file_.seek(pos);}
  int read(uint8_t* out,size_t n){return permitted() && out?file_.read(out,n):0;}
  size_t write(const uint8_t* data,size_t n){return permitted() && write_ && data?file_.write(data,n):0U;}
  void flush(){if(permitted() && write_)file_.flush();else if(owner_)owner_->revoke();}
  int getWriteError(){return permitted() && write_?file_.getWriteError():1;}
  void close(){
   if(owner_){file_.close();owner_->open_=false;owner_=nullptr;}
  }
  private:
  bool permitted(){return owner_ && owner_->live(false) && bool(file_);}
  Port* owner_=nullptr;
  RawFile file_{};
  bool write_=false;
 };
 Port(Backend& backend,const Witness& witness):backend_(backend),witness_(witness){}
 Port(const Port&)=delete;Port& operator=(const Port&)=delete;
 bool bind(uint32_t vessel,const char* current){
  if(attempted_){revoke();return false;}
  attempted_=true;
  char check[96]{};
  if(!DTNF24M::sandboxPath(check,sizeof(check),current,vessel)){revoke();return false;}
  const Evidence e=witness_.view();
  if(!validEvidence(e,vessel)){revoke();return false;}
  vessel_=vessel;media_=e.mediaTag;mount_=e.mountEpoch;lease_=e.ownerSerial;
  strncpy(current_,current,sizeof(current_)-1);
  bound_=true;return true;
 }
 bool exists(const char* path){
  if(!live(false) || open_){revoke();return false;}
  bool isCurrent=false;
  if(!supportedPath(path,current_,vessel_,isCurrent)){revoke();return false;}
  if(!isCurrent && !live(true))return false;
  const bool result=backend_.exists(path);
  if(!live(false))return false;
  return result;
 }
 FileHandle open(const char* path,const char* mode){
  if(!live(false) || open_){revoke();return {};}
  const bool read=mode && strcmp(mode,"r")==0;
  const bool write=mode && strcmp(mode,"a")==0;
  bool isCurrent=false;
  if((!read && !write) || !supportedPath(path,current_,vessel_,isCurrent) ||
     (write && !isCurrent) || (read && !isCurrent && !live(true)) ||
     (read && !isCurrent && witness_.view().urgentPending)){
   revoke();return {};
  }
  auto handle=backend_.open(path,mode);
  if(!handle || !live(false)){
   if(handle)handle.close();
   revoke();return {};
  }
  open_=true;
  return FileHandle(this,std::move(handle),write);
 }
 // F24P: bounded, owner-checked directory discovery through THIS SAME port.
 // Only exact vessel F06 root; handles remain internal (parent + one child).
 // Never accepts a guessed closure, changes journal files, or opens for write.
 // Failure is latched; enumeration requires an independently trustworthy
 // physical owner not supplied by the build-only firmware profile.
 template<class Sink>
 bool scanClosedSessions(const char* root,const char* readMode,Sink&& sink){
  if(!live(true) || open_ || !readMode || strcmp(readMode,"r")!=0){revoke();return false;}
  char expected[48]{};
  const int n=snprintf(expected,sizeof(expected),"/THESIS/F06TEST/V%03lu",
                       (unsigned long)vessel_);
  if(n<=0 || size_t(n)>=sizeof(expected) || !root || strcmp(root,expected)){
   revoke();return false;
  }
  open_=true; // Exclude EVERY other port operation throughout directory scan.
  auto folder=backend_.open(root,readMode);
  if(!folder){open_=false;revoke();return false;}
  bool good=live(true) && folder.isDirectory();
  bool exhausted=false;
  for(unsigned visited=0;good && visited<64;++visited){
   if(!live(true)){good=false;break;}
   auto child=folder.openNextFile();
   if(!live(true)){child.close();good=false;break;}
   if(!child){exhausted=true;break;}
   if(!live(true)){child.close();good=false;break;}
   const bool isDir=child.isDirectory();
   if(!live(true)){child.close();good=false;break;}
   const char* raw=child.name();
   const char* slash=raw?strrchr(raw,'/'):nullptr;
   const char* name=slash?slash+1:raw;
   char candidate[64]{};
   if(!raw || !name || !*name){good=false;}
   // FS::File::name may be a basename or the precise full path.
   // Reject a plausible-looking session from some OTHER directory.
   else if(slash && (size_t(slash-raw)!=size_t(n) ||
                         strncmp(raw,root,size_t(n))!=0)){good=false;}
   else if(isDir && !DTNF24M::validSession(name)){good=false;}
   else if(isDir){
    const int k=snprintf(candidate,sizeof(candidate),"%s/%s",root,name);
    if(k<=0 || size_t(k)>=sizeof(candidate))good=false;
   }
   child.close();
   if(!live(true)){good=false;break;}
   if(good && isDir && strcmp(candidate,current_)!=0){
    if(!sink(candidate)){good=false;break;}
    if(!live(true)){good=false;break;}
   }
  }
  // F24O already treats close as mandatory cleanup, even on revoked lease.
  // Physical handle-close scheduling remains UNPROVEN / NOT AUTHORIZED.
  folder.close();open_=false;
  if(!good || !exhausted || !live(true)){revoke();return false;}
  return true;
 }
 bool faulted()const{return fault_;}
 bool bound()const{return bound_ && !fault_;}
 bool hasHandle()const{return open_;}
 void revoke(){fault_=true;bound_=false;}
 private:
 bool live(bool closedSource)const{
  if(fault_ || !bound_)return false;
  const Evidence e=witness_.view();
  if(!validEvidence(e,vessel_) || e.mediaTag!=media_ ||
     e.mountEpoch!=mount_ || e.ownerSerial!=lease_ ||
     (closedSource && (!e.closedSessionsVerified || e.urgentPending))){
   const_cast<Port*>(this)->revoke();return false;
  }
  return true;
 }
 Backend& backend_;const Witness& witness_;
 uint32_t vessel_=0;uint64_t media_=0,mount_=0,lease_=0;
 char current_[64]{};
 bool attempted_=false,bound_=false,fault_=false,open_=false;
};
} // namespace DTNF24O
