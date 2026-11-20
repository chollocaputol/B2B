#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace F23DReadiness {
// RAM only; no Serial dependency or filesystem. Callbacks only enqueue.
class EvidenceQueue {
public:
  static constexpr size_t kDepth=12,kRow=512,kChunk=64;
  bool push(const char* text) {
    size_t length=0;while(length<kRow && text[length])++length;
    if(length>kRow-2 || count_==kDepth) { lose();return false; }
    auto& slot=rows_[(cursor_+count_)%kDepth];
    memcpy(slot.text,text,length);slot.text[length]='\n';slot.length=length+1;
    ++count_;if(count_>peak_)peak_=count_;
    return true;
  }
  template<class Writer> size_t drain(Writer& writer) {
    if(!count_)return 0;
    const int available=writer.availableForWrite();
    if(available<=0)return 0;
    auto& slot=rows_[cursor_];
    size_t n=slot.length-offset_;
    if(n>kChunk)n=kChunk;
    if(n>size_t(available))n=size_t(available);
    size_t written=writer.write(reinterpret_cast<const uint8_t*>(slot.text+offset_),n);
    if(written>n){lose();written=n;}
    offset_+=written;
    if(offset_==slot.length){offset_=0;cursor_=(cursor_+1)%kDepth;--count_;}
    return written;
  }
  size_t size() const{return count_;}
  size_t peak() const{return peak_;}
  uint32_t lost() const{return lost_;}
private:
  void lose(){if(lost_!=UINT32_MAX)++lost_;}
  struct Row {char text[kRow]{};size_t length=0;};
  Row rows_[kDepth]{};
  size_t cursor_=0,count_=0,offset_=0,peak_=0;
  uint32_t lost_=0;
};
}
