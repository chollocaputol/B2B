#pragma once
// Included inside ThesisSDLogger: reuses its authorized, mounted F06 owner.
// Actual implementation is also compiled against a mock SD/File API.
// F24Q names this writer's four SD volume entrypoints through the owning
// logger facade; ordinary/F23D-only host builds retain the SD default.
// File-handle methods are NOT globally interposed by this macro.
#ifndef F23D_SD_ACCESS
#define F23D_SD_ACCESS SD
#define F23D_SD_ACCESS_DEFAULTED 1
#endif
class F23DSDFileWriter {
public:
 bool begin(const char* directory,uint32_t local) {
  if(local<1 || local>3)return false;
  char root[40];snprintf(root,sizeof(root),"/THESIS/F06TEST/V%03lu/",(unsigned long)local);
  const size_t prefix=strlen(root);
  if(strncmp(directory,root,prefix) || !F23DSD::sessionValid(directory+prefix))return false;
  const int n=snprintf(path_,sizeof(path_),"%s/f23d_multihop.jsonl",directory);
  if(n<=0 || size_t(n)>=sizeof(path_) || F23D_SD_ACCESS.exists(path_))return false;
  // This file belongs exclusively to the newly allocated boot session.
  // Never reopen an existing file for writing, truncate, repair or retry.
  file_=F23D_SD_ACCESS.open(path_,FILE_APPEND);
  if(!file_ || file_.size()!=0){if(file_)file_.close();return false;}
  return true;
 }
 bool appendChecked(const char* bytes,size_t length) {
  if(!file_ || length>F23DSD::Journal::kLine || verifying_)return false;
  if(file_.write(reinterpret_cast<const uint8_t*>(bytes),length)!=length)return false;
  file_.flush();if(file_.getWriteError())return false;
  File readback=F23D_SD_ACCESS.open(path_,FILE_READ);
  if(!readback || readback.size()!=written_+length || !readback.seek(written_)){if(readback)readback.close();return false;}
  char chunk[64];size_t at=0;
  while(at<length){
   size_t n=length-at;if(n>sizeof(chunk))n=sizeof(chunk);
   if(size_t(readback.read(reinterpret_cast<uint8_t*>(chunk),n))!=n || memcmp(chunk,bytes+at,n)){readback.close();return false;}
   at+=n;
  }
  readback.close();written_+=length;
  for(size_t i=0;i<length;++i)expectedHash_=(expectedHash_^uint8_t(bytes[i]))*16777619UL;
  return true;
 }
 F23DSD::Finish finish() {
  if(!verifying_){
   if(!file_)return F23DSD::Finish::Failed;
   file_.flush();if(file_.getWriteError())return F23DSD::Finish::Failed;
   file_.close();verification_=F23D_SD_ACCESS.open(path_,FILE_READ);verifying_=true;
   if(!verification_ || verification_.size()!=written_)return F23DSD::Finish::Failed;
  }
  uint8_t chunk[64];size_t n=written_-checked_;if(n>sizeof(chunk))n=sizeof(chunk);
  if(n){
   if(size_t(verification_.read(chunk,n))!=n)return F23DSD::Finish::Failed;
   for(size_t i=0;i<n;++i)actualHash_=(actualHash_^chunk[i])*16777619UL;
   checked_+=n;return F23DSD::Finish::Pending;
  }
  verification_.close();
  return actualHash_==expectedHash_?F23DSD::Finish::OK:F23DSD::Finish::Failed;
 }
 const char* path()const{return path_;}
private:
 File file_,verification_;
 char path_[F23DSD::Journal::kPath]{};
 uint32_t written_=0,checked_=0,expectedHash_=2166136261UL,actualHash_=2166136261UL;
 bool verifying_=false;
};

#ifdef F23D_SD_ACCESS_DEFAULTED
#undef F23D_SD_ACCESS
#undef F23D_SD_ACCESS_DEFAULTED
#endif
