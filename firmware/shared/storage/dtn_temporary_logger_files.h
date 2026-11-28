#pragma once
// F24S: build-only source-scoped F06 short-lived File entrypoint seam.
// Calls enter F24R's witness-checked GuardedVolume; no media/grant is supplied.
// The F24S firmware environment uses F24Q::DenyWitness + F24K denied SD.
// No physical File may be opened by the firmware configuration.
#include <stddef.h>
#include <string.h>
#include <utility>

namespace DTNF24S {
inline bool allowedF06Path(const char* path, bool writing) {
  if (!path) return false;
  static const char prefix[] = "/THESIS/F06TEST/V";
  static const size_t base = sizeof(prefix)-1U;
  if (strlen(path)<base+4U+1U+7U || strncmp(path,prefix,base)!=0) return false;
  const char* p=path+base;
  if (p[0]!='0' || p[1]!='0' || (p[2]!='1' && p[2]!='2' && p[2]!='3') || p[3]!='/') return false;
  p+=4;
  if (*p++!='S') return false;
  for (unsigned i=0;i<7;++i) {
    const char ch=p[i];
    if (!((ch>='0' && ch<='9') || (ch>='A' && ch<='F')))return false;
  }
  p+=7;
  if (*p=='\0')return !writing;  // directory read-only
  if (*p++!='/')return false;
  return strcmp(p,"sos_events.jsonl")==0 ||
         strcmp(p,"telemetry.jsonl")==0 ||
         strcmp(p,"diagnostics.jsonl")==0;
}

template<class GuardedVolume> class TemporaryLoggerFiles final {
 public:
  using FileHandle=decltype(std::declval<GuardedVolume&>().open((const char*)nullptr,(const char*)nullptr));
  explicit TemporaryLoggerFiles(GuardedVolume& guarded):guarded_(guarded){}
  TemporaryLoggerFiles(const TemporaryLoggerFiles&)=delete;
  TemporaryLoggerFiles& operator=(const TemporaryLoggerFiles&)=delete;
  bool exists(const char* path) {
    return allowedF06Path(path,false) && guarded_.exists(path);
  }
  FileHandle open(const char* path,const char* mode) {
    if (!mode)return {};
    const bool append=strcmp(mode,"a")==0;
    const bool read=strcmp(mode,"r")==0;
    if ((!append && !read) || !allowedF06Path(path,append))return {};
    return guarded_.open(path,mode);
  }
 private:
  GuardedVolume& guarded_;
};
} // namespace DTNF24S
