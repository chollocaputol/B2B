#pragma once
#include <stdint.h>
#include <stddef.h>
// Ten origin-scoped, RAM-only distress event records. No storage or forwarding.
namespace MultiPeerSOS {
static constexpr size_t kCapacity=10;
enum class Result : uint8_t { New, Duplicate, Stale, Cancelled, CancelDuplicate, CancelMismatch, Full };
inline const char* name(Result r) {
 switch(r) {case Result::New:return "NEW";case Result::Duplicate:return "DUPLICATE";
 case Result::Stale:return "STALE";case Result::Cancelled:return "CANCELLED";
 case Result::CancelDuplicate:return "CANCEL_DUPLICATE";
 case Result::CancelMismatch:return "CANCEL_MISMATCH";case Result::Full:return "FULL";}
 return "UNKNOWN";
}
struct Slot {uint32_t origin=0,event=0,lastSeenMs=0;bool used=false,cancelled=false;};
class Table {
 public:
 Slot* find(uint32_t origin) {for(auto& x: slots_) if(x.used&&x.origin==origin)return &x;return nullptr;}
 Result receiveSOS(uint32_t origin,uint32_t event,uint32_t now) {
  Slot* x=find(origin);
  if(x){if(event<x->event)return Result::Stale;
   if(event==x->event){x->lastSeenMs=now;return Result::Duplicate;}
   x->event=event;x->cancelled=false;x->lastSeenMs=now;return Result::New;}
  for(auto& e:slots_)if(!e.used){e.used=true;e.origin=origin;e.event=event;e.lastSeenMs=now;return Result::New;}
  return Result::Full;
 }
 Result cancel(uint32_t origin,uint32_t event,uint32_t now) {
  Slot* x=find(origin);if(!x||x->event!=event)return Result::CancelMismatch;
  x->lastSeenMs=now;if(x->cancelled)return Result::CancelDuplicate;
  x->cancelled=true;return Result::Cancelled;
 }
 private:Slot slots_[kCapacity]{};
};
}
