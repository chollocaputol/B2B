#pragma once
// Offline-only DTN sync cursor contract. NO Wi-Fi, SD, flash or RF operations.
// The source journal is immutable. A server-ACK'd record becomes checkpointed
// only after an independent storage adapter persists and reads back the cursor.
#include <stddef.h>
#include <stdint.h>

namespace DTNSyncCheckpoint {
static constexpr uint32_t kMagic = 0x44544E31UL; // DTN1
static constexpr uint32_t kVersion = 1UL;
static constexpr size_t kMaxBatch = 32U;

enum class Reply : uint8_t { Accepted, Duplicate, Retry, Rejected };
enum class SlotRead : uint8_t { Missing, Present, Error };
enum class Load : uint8_t { Loaded, Fresh, Corrupt, WrongSession, IoError };

struct Snapshot {
  uint32_t magic = 0;
  uint32_t version = 0;
  uint64_t session = 0;
  uint32_t generation = 0;
  uint64_t nextRecord = 0; // first index NOT durably accepted, zero-based
  uint32_t check = 0;
};

inline void feed32(uint32_t& h, uint32_t x) {
  for (unsigned i=0; i<4U; ++i) {
    h = (h ^ static_cast<uint8_t>((x >> (8U*i)) & 0xFFU)) * 16777619UL;
  }
}
inline void feed64(uint32_t& h, uint64_t x) {
  feed32(h,static_cast<uint32_t>(x));
  feed32(h,static_cast<uint32_t>(x >> 32U));
}
inline uint32_t checksum(const Snapshot& s) {
  uint32_t h=2166136261UL;
  feed32(h,s.magic);feed32(h,s.version);feed64(h,s.session);
  feed32(h,s.generation);feed64(h,s.nextRecord);
  return h; // logical fields, not compiler padding
}
inline Snapshot make(uint64_t session, uint32_t generation, uint64_t nextRecord) {
  Snapshot s{};
  s.magic=kMagic;s.version=kVersion;s.session=session;
  s.generation=generation;s.nextRecord=nextRecord;
  s.check=checksum(s);
  return s;
}
inline bool valid(const Snapshot& s) {
  return s.magic==kMagic && s.version==kVersion &&
         s.session!=0U && s.generation!=0U && s.check==checksum(s);
}
inline bool newer(uint32_t a,uint32_t b) {
  return static_cast<int32_t>(a-b)>0; // half-window rollover comparison
}
inline bool same(const Snapshot& a,const Snapshot& b) {
  return a.magic==b.magic && a.version==b.version &&
         a.session==b.session && a.generation==b.generation &&
         a.nextRecord==b.nextRecord && a.check==b.check;
}

// Required storage adapter: SlotRead read(unsigned slot, Snapshot&);
// bool write(unsigned slot, const Snapshot&);
// The adapter MUST persist a complete record to its own verified sidecar,
// never modify the source journal, and implement an interrupted-write-safe
// two-slot scheme. This class does not implement SD durability by itself.
template<class Storage> class Cursor {
 public:
  Load load(Storage& store, uint64_t expectedSession) {
    usable_=false;faulted_=false;active_=0;state_=Snapshot{};
    if(!expectedSession) return Load::WrongSession;
    Snapshot a{},b{};
    const SlotRead x=store.read(0U,a), y=store.read(1U,b);
    if(x==SlotRead::Error || y==SlotRead::Error)return Load::IoError;
    if(x==SlotRead::Missing && y==SlotRead::Missing)return Load::Fresh;
    const bool av=x==SlotRead::Present && valid(a);
    const bool bv=y==SlotRead::Present && valid(b);
    if(!av && !bv)return Load::Corrupt;
    if((av && a.session!=expectedSession) || (bv && b.session!=expectedSession))
      return Load::WrongSession;
    if(av && bv && a.generation==b.generation && !same(a,b))
      return Load::Corrupt;
    if(av && bv && a.generation!=b.generation &&
       (a.nextRecord>b.nextRecord ? newer(b.generation,a.generation)
                                  : newer(a.generation,b.generation)))
      return Load::Corrupt; // newer generation cannot move cursor backward
    if(bv && (!av || newer(b.generation,a.generation))){state_=b;active_=1U;}
    else {state_=a;active_=0U;}
    usable_=true;
    return Load::Loaded;
  }
  bool initializeFresh(Storage& store,uint64_t session) {
    if(usable_||faulted_||!session)return false;
    Snapshot a{},b{};
    if(store.read(0U,a)!=SlotRead::Missing ||
       store.read(1U,b)!=SlotRead::Missing)return false;
    const Snapshot start=make(session,1U,0U);
    if(!writeVerify(store,0U,start)){faulted_=true;return false;}
    state_=start;active_=0U;usable_=true;return true;
  }
  bool ready()const{return usable_&&!faulted_;}
  uint64_t nextRecord()const{return ready()?state_.nextRecord:0U;}
  const Snapshot& state()const{return state_;}
  // Only explicit trusted per-record Accepted or Duplicate results advance the
  // contiguous prefix. HTTP 200, Wi-Fi connectivity, and relay ACK alone do not.
  bool commitReply(Storage& store,uint64_t session,uint64_t batchStart,
                   const Reply* replies,size_t count,
                   bool authenticatedResponse,bool matchingBatch) {
    if(!ready()||!authenticatedResponse||!matchingBatch||!replies||
       !count||count>kMaxBatch||session!=state_.session||
       batchStart!=state_.nextRecord||state_.generation==UINT32_MAX)return false;
    size_t prefix=0;
    while(prefix<count && (replies[prefix]==Reply::Accepted ||
                           replies[prefix]==Reply::Duplicate))++prefix;
    if(!prefix)return false;
    if(UINT64_MAX-state_.nextRecord<prefix)return false;
    const Snapshot next=make(session,state_.generation+1U,
                             state_.nextRecord+prefix);
    const unsigned inactive=1U-active_;
    if(!writeVerify(store,inactive,next)){
      // A write may have reached storage before we lost its ACK; require
      // recovery before any new cursor operation to avoid uncertain state.
      faulted_=true;return false;
    }
    state_=next;active_=inactive;return true;
  }
 private:
  bool writeVerify(Storage& store,unsigned slot,const Snapshot& next) {
    if(!store.write(slot,next))return false;
    Snapshot readback{};
    return store.read(slot,readback)==SlotRead::Present &&
           valid(readback) && same(readback,next);
  }
  Snapshot state_{};
  unsigned active_=0U;
  bool usable_=false,faulted_=false;
};
} // namespace DTNSyncCheckpoint
