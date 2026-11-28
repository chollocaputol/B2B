#pragma once
// F24C: fixed-wire, two-slot ACK sidecar Store for the F24A ledger.
// Not linked into firmware. Never opens source F06 journals. No mount, Wi-Fi,
// RF, credentials, Arduino calls or SD I/O in this portable header.
// The host/SD Port is responsible for *durable* flush/close/readback and
// must write ONLY the inactive sidecar slot; physical power-loss durability
// of a specific SD controller/filesystem requires independent qualification.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dtn_ack_ledger.h"

namespace DTNAckSidecar {
static constexpr size_t kWireBytes=94U;
static constexpr size_t kPathBytes=104U;

enum class Bind : uint8_t { Ready, BadStream, Denied, InvalidPath };

inline void put32(uint8_t* out,size_t& at,uint32_t n){
  for(unsigned i=0U;i<4U;++i)out[at++]=static_cast<uint8_t>(n>>(8U*i));
}
inline void put64(uint8_t* out,size_t& at,uint64_t n){
  put32(out,at,static_cast<uint32_t>(n));
  put32(out,at,static_cast<uint32_t>(n>>32U));
}
inline uint32_t get32(const uint8_t* in,size_t& at){
  uint32_t n=0;
  for(unsigned i=0U;i<4U;++i)n|=static_cast<uint32_t>(in[at++])<<(8U*i);
  return n;
}
inline uint64_t get64(const uint8_t* in,size_t& at){
  const uint64_t lo=get32(in,at),hi=get32(in,at);
  return lo|(hi<<32U);
}
// Explicit, platform-independent little-endian wire format. Never copy
// raw C++ Snapshot structs (alignment and padding vary by toolchain).
inline bool encode(const DTNAckLedger::Snapshot& snapshot,
                   uint8_t out[kWireBytes]){
  if(!out||!DTNAckLedger::valid(snapshot))return false;
  size_t at=0U;
  put32(out,at,snapshot.magic);put32(out,at,snapshot.version);
  for(uint8_t b:snapshot.stream.uploader.bytes)out[at++]=b;
  put32(out,at,snapshot.stream.vessel);
  for(char b:snapshot.stream.session)out[at++]=static_cast<uint8_t>(b);
  out[at++]=static_cast<uint8_t>(snapshot.stream.journal);
  for(uint8_t b:snapshot.stream.sha256)out[at++]=b;
  put32(out,at,snapshot.generation);
  put64(out,at,snapshot.firstPending);put64(out,at,snapshot.ackMask);
  put32(out,at,snapshot.check);
  return at==kWireBytes;
}
inline bool decode(const uint8_t* in,size_t length,DTNAckLedger::Snapshot& out){
  out=DTNAckLedger::Snapshot{};
  if(!in||length!=kWireBytes)return false;
  size_t at=0U;DTNAckLedger::Snapshot v{};
  v.magic=get32(in,at);v.version=get32(in,at);
  for(uint8_t& b:v.stream.uploader.bytes)b=in[at++];
  v.stream.vessel=get32(in,at);
  for(char& b:v.stream.session)b=static_cast<char>(in[at++]);
  v.stream.journal=static_cast<DTNAckLedger::Journal>(in[at++]);
  for(uint8_t& b:v.stream.sha256)b=in[at++];
  v.generation=get32(in,at);v.firstPending=get64(in,at);
  v.ackMask=get64(in,at);v.check=get32(in,at);
  if(at!=kWireBytes||!DTNAckLedger::valid(v))return false;
  out=v;return true;
}

inline bool sidecarPath(const DTNAckLedger::Stream& stream,unsigned slot,
                        char* out,size_t capacity){
  if(!out||!capacity||slot>1U||!DTNAckLedger::validStream(stream))return false;
  const char* suffix=stream.journal==DTNAckLedger::Journal::SosEvents?
                     "sos":"tlm";
  // Dedicated sidecar namespace; source /THESIS/F06TEST is never written.
  const int n=snprintf(out,capacity,"/THESIS/DTNSYNC/V%03lu/%s/%s_%u.ack",
      static_cast<unsigned long>(stream.vessel),stream.session,suffix,slot);
  return n>0 && static_cast<size_t>(n)<capacity;
}

// Gate: bool permit(const Stream&) const, checked at EVERY call.
// Port: bool readFile(path, byteBuffer, capacity, bytesRead, exists)
//       bool writeFile(path, exact94bytes, length)
// The Port must only access the exact allowlisted sidecar path. The sidecar
// directory must exist due to separately authorized provisioning, never
// created automatically by this class. Port write must flush and close,
// and may damage only the INACTIVE slot; readback is verified here.
// A false write / uncertain readback forces F24A to fail-stop and reload.
// On removable media, Gate must also recheck SD identity/ownership and
// closed-session proof on each I/O, not merely hold a cached bool.
template<class Port,class Gate> class Store {
 public:
  Bind bind(Port& port,const Gate& gate,const DTNAckLedger::Stream& stream){
    port_=nullptr;gate_=nullptr;stream_=DTNAckLedger::Stream{};
    if(!DTNAckLedger::validStream(stream))return Bind::BadStream;
    if(!gate.permit(stream))return Bind::Denied;
    char a[kPathBytes],b[kPathBytes];
    if(!sidecarPath(stream,0U,a,sizeof(a))||
       !sidecarPath(stream,1U,b,sizeof(b)))return Bind::InvalidPath;
    stream_=stream;port_=&port;gate_=&gate;
    return Bind::Ready;
  }
  DTNAckLedger::SlotRead read(unsigned slot,DTNAckLedger::Snapshot& out){
    out=DTNAckLedger::Snapshot{};
    if(!authorized()||slot>1U)return DTNAckLedger::SlotRead::Error;
    char path[kPathBytes]{};
    if(!sidecarPath(stream_,slot,path,sizeof(path)))return DTNAckLedger::SlotRead::Error;
    uint8_t bytes[kWireBytes]{};size_t length=0U;bool exists=false;
    if(!port_->readFile(path,bytes,sizeof(bytes),length,exists))
      return DTNAckLedger::SlotRead::Error;
    if(!exists)return DTNAckLedger::SlotRead::Missing;
    // Distinguish a malformed/torn EXISTING slot from a missing slot;
    // F24A can recover using the other valid slot, never initialize fresh.
    (void)decode(bytes,length,out);
    return DTNAckLedger::SlotRead::Present;
  }
  bool write(unsigned slot,const DTNAckLedger::Snapshot& next){
    if(!authorized()||slot>1U||!DTNAckLedger::valid(next)||
       !DTNAckLedger::sameStream(stream_,next.stream))return false;
    DTNAckLedger::Snapshot a{},b{};
    const auto x=read(0U,a),y=read(1U,b);
    if(x==DTNAckLedger::SlotRead::Error||y==DTNAckLedger::SlotRead::Error)return false;
    const bool av=x==DTNAckLedger::SlotRead::Present&&DTNAckLedger::valid(a);
    const bool bv=y==DTNAckLedger::SlotRead::Present&&DTNAckLedger::valid(b);
    if((av&&!DTNAckLedger::sameStream(a.stream,stream_))||
       (bv&&!DTNAckLedger::sameStream(b.stream,stream_)))return false;
    if(x==DTNAckLedger::SlotRead::Missing&&y==DTNAckLedger::SlotRead::Missing){
      // Genesis: only slot 0; no existing malformed/wrong-sidecar files.
      if(slot!=0U||next.generation!=1U||next.firstPending!=0U||
         next.ackMask!=0U)return false;
    }else{
      if(!av&&!bv)return false;
      if(av&&bv){
        if(a.generation==b.generation){if(!DTNAckLedger::same(a,b))return false;}
        else if(DTNAckLedger::newer(a.generation,b.generation)){
          if(!DTNAckLedger::monotonic(b,a))return false;
        }else if(DTNAckLedger::newer(b.generation,a.generation)){
          if(!DTNAckLedger::monotonic(a,b))return false;
        }else return false;
      }
      const bool activeB=bv&&(!av||DTNAckLedger::newer(b.generation,a.generation));
      const unsigned active=activeB?1U:0U;
      const DTNAckLedger::Snapshot& latest=activeB?b:a;
      if(slot==active||latest.generation==UINT32_MAX||
         next.generation!=latest.generation+1U||
         !DTNAckLedger::monotonic(latest,next))return false;
    }
    uint8_t encoded[kWireBytes]{};
    if(!encode(next,encoded))return false;
    char path[kPathBytes]{};
    if(!sidecarPath(stream_,slot,path,sizeof(path)))return false;
    if(!port_->writeFile(path,encoded,sizeof(encoded)))return false;
    // Reopen and verify exact logical state, not just file length or HTTP 200.
    DTNAckLedger::Snapshot readback{};
    return read(slot,readback)==DTNAckLedger::SlotRead::Present &&
           DTNAckLedger::same(readback,next);
  }
 private:
  bool authorized()const{return port_&&gate_&&gate_->permit(stream_);}
  Port* port_=nullptr;
  const Gate* gate_=nullptr;
  DTNAckLedger::Stream stream_{};
};
} // namespace DTNAckSidecar
