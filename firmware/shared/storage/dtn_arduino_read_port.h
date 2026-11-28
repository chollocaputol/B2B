#pragma once
// F24D device-side SD read-only transport. No mount, mkdir, formatting or writes.
// Must be owned by ThesisSDLogger main-loop and wrapped in a reviewed closure gate.
#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#include <stdint.h>
#include "dtn_closed_sha256_job.h"
namespace DTNF24DArduino {
class ReadFile {
public:
  bool isOpen()const{return static_cast<bool>(file_);}
  uint64_t size()const{return isOpen()?uint64_t(const_cast<File&>(file_).size()):0U;}
  bool readBlockAt(uint64_t offset,uint8_t* out,size_t wanted)const{
    if(!isOpen()||!out||!wanted||wanted>DTNF24D::kChunkBytes||
       offset>UINT32_MAX||offset>size()||wanted>size()-offset)return false;
    File& f=const_cast<File&>(file_);
    if(!f.seek(static_cast<uint32_t>(offset)))return false;
    return static_cast<size_t>(f.read(out,wanted))==wanted;
  }
  void close(){if(file_)file_.close();}
private:
  friend class ReadVolume;File file_{};
};
class ReadVolume {
public:
  bool openReadOnly(const char* exactPath,ReadFile& dest){
    dest.close();
    if(!exactPath||strncmp(exactPath,"/THESIS/F06TEST/V",17U)!=0||
       strstr(exactPath,"..")!=nullptr)return false;
    // Job::begin builds the exact allowlisted path after the external Gate.
    dest.file_=SD.open(exactPath,FILE_READ);
    return dest.isOpen();
  }
};
} // DTNF24DArduino
#endif
