#pragma once
// F24A: portable, bounded, OFFLINE-only DTN per-journal ACK ledger.
// No Arduino, SD, Wi-Fi, HTTPS, RF, secret material, journal writes or flash.
// A future adapter must make each sidecar slot durable (write, sync, readback)
// and MUST NEVER modify immutable F06 source JSONL journals.
// Response authentication, UUID generation, SHA256 calculation and strict
// backend schema/source authorization are EXTERNAL gates, NOT implemented here.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace DTNAckLedger {
static constexpr uint32_t kMagic=0x44414C32UL; // DAL2
static constexpr uint32_t kVersion=2U;
static constexpr size_t kAckWindow=64U;
static constexpr size_t kMaxBatch=32U;

enum class Journal : uint8_t { SosEvents=1U, Telemetry=2U, Diagnostics=3U };
enum class Reply : uint8_t { Accepted=1U, Duplicate=2U, Retry=3U, Rejected=4U };
enum class SlotRead : uint8_t { Missing, Present, Error };
enum class Load : uint8_t { Loaded, Fresh, Corrupt, WrongStream, IoError };
enum class Commit : uint8_t { Applied, NoNewAck, Invalid, StorageFailure };

struct Id128 { uint8_t bytes[16]{}; };
inline bool sameId(const Id128& a,const Id128& b){return memcmp(a.bytes,b.bytes,16U)==0;}
// A UUID-v4/v7 *format* check, NOT a check of randomness or authenticity.
inline bool uuidFormat(const Id128& id){
  const uint8_t version=static_cast<uint8_t>(id.bytes[6U]>>4U);
  return (version==4U || version==7U) && ((id.bytes[8U]&0xC0U)==0x80U);
}
struct Stream {
  Id128 uploader{};      // server-registered HTTP uploader, not LoRa origin
  uint32_t vessel=0U;
  char session[9]{};     // exact closed F06 S[0-9A-F]{7} identifier
  Journal journal=Journal::SosEvents;
  uint8_t sha256[32]{}; // SHA256 of immutable COMPLETE journal bytes
};
inline bool sameStream(const Stream& a,const Stream& b){
  return sameId(a.uploader,b.uploader) && a.vessel==b.vessel &&
         memcmp(a.session,b.session,9U)==0 && a.journal==b.journal &&
         memcmp(a.sha256,b.sha256,32U)==0;
}
inline bool validStream(const Stream& s){
  if(!uuidFormat(s.uploader) || s.vessel<1U || s.vessel>9999U ||
     (s.journal!=Journal::SosEvents && s.journal!=Journal::Telemetry) ||
     s.session[0]!='S' || s.session[8]!='\0') return false;
  for(unsigned i=1U;i<8U;++i){
    const char c=s.session[i];
    if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))return false;
  }
  uint8_t any=0U;
  for(size_t i=0;i<sizeof(s.sha256);++i)any|=s.sha256[i];
  return any!=0U; // not proof that SHA256 has been independently verified
}
struct Snapshot {
  uint32_t magic=0U,version=0U;
  Stream stream{};
  uint32_t generation=0U;
  uint64_t firstPending=0U; // first ordinal not in durable contiguous prefix
  uint64_t ackMask=0U;      // 64 ordinals from firstPending; bit0 is always 0
  uint32_t check=0U;       // CRC32 for accidental/torn write detection ONLY
};
// Checksum explicitly serializes logical fields; never hashes C++ padding.
inline void crcByte(uint32_t& crc,uint8_t b){
  crc^=b;
  for(unsigned i=0U;i<8U;++i)
    crc=(crc>>1U)^((crc&1U)?0xEDB88320UL:0U);
}
inline void crc32(uint32_t& crc,uint32_t n){
  for(unsigned i=0U;i<4U;++i)crcByte(crc,static_cast<uint8_t>(n>>(8U*i)));
}
inline void crc64(uint32_t& crc,uint64_t n){
  crc32(crc,static_cast<uint32_t>(n));crc32(crc,static_cast<uint32_t>(n>>32U));
}
inline uint32_t checksum(const Snapshot& s){
  uint32_t crc=0xFFFFFFFFUL;
  crc32(crc,s.magic);crc32(crc,s.version);
  for(uint8_t v:s.stream.uploader.bytes)crcByte(crc,v);
  crc32(crc,s.stream.vessel);
  for(char v:s.stream.session)crcByte(crc,static_cast<uint8_t>(v));
  crcByte(crc,static_cast<uint8_t>(s.stream.journal));
  for(uint8_t v:s.stream.sha256)crcByte(crc,v);
  crc32(crc,s.generation);crc64(crc,s.firstPending);crc64(crc,s.ackMask);
  return crc^0xFFFFFFFFUL;
}
inline Snapshot make(const Stream& s,uint32_t generation,uint64_t firstPending,uint64_t mask){
  Snapshot v{};v.magic=kMagic;v.version=kVersion;v.stream=s;
  v.generation=generation;v.firstPending=firstPending;v.ackMask=mask;
  v.check=checksum(v);return v;
}
inline bool valid(const Snapshot& s){
  return s.magic==kMagic && s.version==kVersion && validStream(s.stream) &&
         s.generation!=0U && s.firstPending<=UINT64_MAX-kAckWindow &&
         (s.ackMask&1U)==0U && s.check==checksum(s);
}
inline bool same(const Snapshot& a,const Snapshot& b){
  return a.magic==b.magic && a.version==b.version &&
    sameStream(a.stream,b.stream) && a.generation==b.generation &&
    a.firstPending==b.firstPending && a.ackMask==b.ackMask &&
    a.check==b.check;
}
inline bool newer(uint32_t a,uint32_t b){
  const uint32_t delta=a-b;
  return delta!=0U && delta<0x80000000UL; // defined for uint32 rollover
}
inline bool monotonic(const Snapshot& earlier,const Snapshot& later){
  if(later.firstPending<earlier.firstPending)return false;
  const uint64_t distance=later.firstPending-earlier.firstPending;
  if(distance>kAckWindow)return false; // one bounded commit cannot jump farther
  const uint64_t inherited=distance==kAckWindow?0U:(earlier.ackMask>>distance);
  return (later.ackMask&inherited)==inherited;
}

struct RecordRef {uint64_t ordinal=0U;Id128 recordId{};};
struct Batch {
  Id128 batchId{},uploader{};
  const RecordRef* records=nullptr;
  size_t count=0U;
};
struct ItemReply {Id128 recordId{};Reply status=Reply::Retry;};
struct Response {
  uint32_t schemaVersion=0U;
  Id128 batchId{},uploader{};
  const ItemReply* items=nullptr;
  size_t count=0U;
};
inline bool approved(Reply r){return r==Reply::Accepted||r==Reply::Duplicate;}
inline bool recognized(Reply r){return approved(r)||r==Reply::Retry||r==Reply::Rejected;}

// Store interface:
// SlotRead read(unsigned slot,Snapshot& out);
// bool write(unsigned slot,const Snapshot& next);
// Sidecar must be scoped to a single Stream and must verify durability before
// acknowledging writes. This class uses two alternate slots + readback, but
// does not itself implement crash-safe SD hardware I/O.
template<class Store> class Ledger {
 public:
  Load load(Store& store,const Stream& expected){
    ready_=false;fault_=false;state_=Snapshot{};active_=0U;
    if(!validStream(expected))return Load::WrongStream;
    Snapshot a{},b{};
    const SlotRead x=store.read(0U,a),y=store.read(1U,b);
    if(x==SlotRead::Error||y==SlotRead::Error)return Load::IoError;
    if(x==SlotRead::Missing&&y==SlotRead::Missing)return Load::Fresh;
    const bool av=x==SlotRead::Present&&valid(a);
    const bool bv=y==SlotRead::Present&&valid(b);
    if(!av&&!bv)return Load::Corrupt;
    if((av&&!sameStream(a.stream,expected))||(bv&&!sameStream(b.stream,expected)))
      return Load::WrongStream;
    if(av&&bv){
      if(a.generation==b.generation && !same(a,b))return Load::Corrupt;
      if(a.generation!=b.generation){
        if(newer(a.generation,b.generation)){
          if(!monotonic(b,a))return Load::Corrupt;
        }else if(newer(b.generation,a.generation)){
          if(!monotonic(a,b))return Load::Corrupt;
        }else return Load::Corrupt;
      }
    }
    if(bv&&(!av||newer(b.generation,a.generation))){state_=b;active_=1U;}
    else {state_=a;active_=0U;}
    ready_=true;return Load::Loaded;
  }
  bool initializeFresh(Store& store,const Stream& stream){
    if(ready_||fault_||!validStream(stream))return false;
    Snapshot a{},b{};
    if(store.read(0U,a)!=SlotRead::Missing||store.read(1U,b)!=SlotRead::Missing)
      return false;
    const Snapshot start=make(stream,1U,0U,0U);
    if(!writeVerify(store,0U,start)){fault_=true;return false;}
    state_=start;active_=0U;ready_=true;return true;
  }
  bool ready()const{return ready_&&!fault_;}
  uint64_t firstPending()const{return state_.firstPending;}
  uint64_t ackMask()const{return state_.ackMask;}
  const Snapshot& state()const{return state_;}
  bool acknowledged(uint64_t ordinal)const{
    if(!ready())return false;
    if(ordinal<state_.firstPending)return true;
    const uint64_t d=ordinal-state_.firstPending;
    return d<kAckWindow && ((state_.ackMask>>d)&1U)!=0U;
  }
  bool eligible(uint64_t ordinal)const{
    return ready()&&ordinal>=state_.firstPending &&
      ordinal-state_.firstPending<kAckWindow&&!acknowledged(ordinal);
  }
  // The caller must verify TLS, server/device authentication, strict parsing,
  // exact payload identity, source eligibility and the immutable journal seal.
  // It must pass true ONLY for an authenticated, verified backend response.
  // Neither HTTP 200 nor LoRa peer ACK is sufficient.
  Commit apply(Store& store,const Batch& sent,const Response& reply,
               bool verifiedBackendResponse){
    if(!ready()||!verifiedBackendResponse||state_.generation==UINT32_MAX||
       reply.schemaVersion!=1U||!uuidFormat(sent.batchId)||
       !sameId(sent.batchId,reply.batchId)||
       !sameId(sent.uploader,state_.stream.uploader)||
       !sameId(reply.uploader,state_.stream.uploader)||
       !sent.records||!reply.items||sent.count<1U||sent.count>kMaxBatch||
       reply.count!=sent.count)return Commit::Invalid;
    for(size_t i=0U;i<sent.count;++i){
      const RecordRef& r=sent.records[i];
      if(!uuidFormat(r.recordId)||!eligible(r.ordinal)||
         (i&&r.ordinal<=sent.records[i-1U].ordinal))return Commit::Invalid;
      for(size_t j=0U;j<i;++j)
        if(sameId(r.recordId,sent.records[j].recordId))return Commit::Invalid;
    }
    uint64_t newMask=state_.ackMask;
    for(size_t i=0U;i<reply.count;++i){
      const ItemReply& ack=reply.items[i];
      if(!recognized(ack.status))return Commit::Invalid;
      size_t found=sent.count;
      for(size_t j=0U;j<sent.count;++j){
        if(sameId(ack.recordId,sent.records[j].recordId)){found=j;break;}
      }
      if(found==sent.count)return Commit::Invalid; // unsolicited record ID
      for(size_t j=0U;j<i;++j)
        if(sameId(ack.recordId,reply.items[j].recordId))return Commit::Invalid;
      if(approved(ack.status)){
        const uint64_t delta=sent.records[found].ordinal-state_.firstPending;
        newMask|=(UINT64_C(1)<<delta);
      }
    }
    if(newMask==state_.ackMask)return Commit::NoNewAck;
    uint64_t nextPending=state_.firstPending;
    while((newMask&1U)!=0U){newMask>>=1U;++nextPending;}
    if(nextPending>UINT64_MAX-kAckWindow)return Commit::Invalid;
    const Snapshot updated=make(state_.stream,state_.generation+1U,
                                nextPending,newMask);
    const unsigned inactive=1U-active_;
    if(!writeVerify(store,inactive,updated)){
      fault_=true;return Commit::StorageFailure;
    }
    state_=updated;active_=inactive;return Commit::Applied;
  }
 private:
  bool writeVerify(Store& store,unsigned slot,const Snapshot& next){
    if(!store.write(slot,next))return false;
    Snapshot actual{};
    return store.read(slot,actual)==SlotRead::Present &&
           valid(actual)&&same(actual,next);
  }
  Snapshot state_{};
  unsigned active_=0U;
  bool ready_=false,fault_=false;
};
} // namespace DTNAckLedger
