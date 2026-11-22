#pragma once
// F24B optional Arduino FS::File / SD read-only port. NOT INCLUDED by firmware.
// This header never mounts SD, changes pins, creates paths, writes, or uploads.
// A separately reviewed single-owner gate must authorize any future call.
// Do not include in an operational firmware profile until hardware/media
// permission, closed-session proof, digest verification and SOS priority exist.
#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#include <stdint.h>

namespace DTNF06ArduinoSDRead {
class ReadFile {
 public:
  bool isOpen() const {return static_cast<bool>(file_);}
  uint64_t size() const {
    return isOpen()?static_cast<uint64_t>(const_cast<File&>(file_).size()):0U;
  }
  bool readAt(uint64_t offset,uint8_t& value) const {
    if (!isOpen() || offset>=size() || offset>UINT32_MAX)return false;
    File& f=const_cast<File&>(file_);
    if (!f.seek(static_cast<uint32_t>(offset)))return false;
    const int next=f.read();
    if(next<0)return false;
    value=static_cast<uint8_t>(next);return true;
  }
  void close(){if(file_)file_.close();}
 private:
  friend class ReadVolume;
  File file_{};
};
class ReadVolume {
 public:
  bool openReadOnly(const char* exactPath,ReadFile& output) {
    output.close();
    if(!exactPath)return false;
    // The upstream reader creates the allowlisted F06 path and requires
    // an externally approved closed-session gate BEFORE invoking this.
    output.file_=SD.open(exactPath,FILE_READ);
    return output.isOpen();
  }
};
} // namespace DTNF06ArduinoSDRead
#endif
