#pragma once
// F24C Arduino SD transport OPTIONAL and NOT INCLUDED in runtime firmware.
// NO mounting, formatting, mkdir, pin/PMU changes or F06 journal writes.
// Caller must own the existing SD mutex/main-loop owner, verify removable
// media continuity and only pass a checked Gate to DTNAckSidecar::Store.
// SD/FAT flush+readback is NOT proof of physical power-loss durability.
#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#include <stdint.h>
#include <string.h>
#include "dtn_ack_sidecar.h"
namespace DTNAckSidecarArduino {
class Port {
 public:
  bool readFile(const char* path,uint8_t* out,size_t capacity,
                size_t& readBytes,bool& exists){
    readBytes=0U;exists=false;
    if(!safePath(path)||!out||capacity<DTNAckSidecar::kWireBytes)return false;
    exists=SD.exists(path);
    if(!exists)return true;
    File file=SD.open(path,FILE_READ);
    if(!file)return false;
    const size_t length=static_cast<size_t>(file.size());
    // Oversize is an EXISTING invalid slot, never a fresh/missing slot.
    if(length>capacity){readBytes=capacity+1U;file.close();return true;}
    while(readBytes<length){
      const int ch=file.read();
      if(ch<0){file.close();return false;}
      out[readBytes++]=static_cast<uint8_t>(ch);
    }
    file.close();return true;
  }
  bool writeFile(const char* path,const uint8_t* bytes,size_t length){
    if(!safePath(path)||!bytes||length!=DTNAckSidecar::kWireBytes)return false;
    // Store::write checks the CURRENT active slot and refuses its path.
    // Any previous inactive slot may be discarded; the active remains.
    // Must NEVER remove any F06 source JSONL or the active ACK slot.
    if(SD.exists(path)&&!SD.remove(path))return false;
    File file=SD.open(path,FILE_WRITE);
    if(!file)return false;
    const size_t n=file.write(bytes,length);
    file.flush();
    const bool ok=(n==length && !file.getWriteError());
    file.close();
    return ok; // Store reopens and validates the full snapshot readback.
  }
 private:
  static bool safePath(const char* p){
    if(!p)return false;
    // Defensive only. Store builds the exact path from a validated Stream.
    const char prefix[]="/THESIS/DTNSYNC/V";
    return strncmp(p,prefix,sizeof(prefix)-1U)==0 &&
           strstr(p,"..") == nullptr && strlen(p)<DTNAckSidecar::kPathBytes;
  }
};
} // namespace DTNAckSidecarArduino
#endif
