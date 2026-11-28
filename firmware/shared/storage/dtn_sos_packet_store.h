#pragma once
// F24M: SOS-only complete-packet capture / framed owner-journal candidate.
// C++17 host-testable. No physical authorization or SD singleton in this file.
// Caller must provide an independently authorized *single* SD owner; F24M
// firmware preview is hard-locked to the F24K permanently denied facade.
#include <stdint.h>
#ifndef THESIS_DTN_F25C_BUILD_ONLY
#define THESIS_DTN_F25C_BUILD_ONLY 0
#endif
#if THESIS_DTN_F25C_BUILD_ONLY != 0 && THESIS_DTN_F25C_BUILD_ONLY != 1
#error "Invalid F25C flag"
#endif
#if THESIS_DTN_F25C_BUILD_ONLY == 1 && (defined(ARDUINO) || defined(ESP_PLATFORM))
#if THESIS_DTN_F25B_BUILD_ONLY != 1 || THESIS_DTN_F25A_BUILD_ONLY != 1 || THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1
#error "F25C target requires inherited permanent denial"
#endif
#endif
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../communication/packet_protocol.h"

namespace DTNF24M {
static constexpr size_t kWireBytes = sizeof(VesselPacket);
static constexpr size_t kFrameBytes = 88;
static constexpr unsigned kLocal = 0, kReceived = 1;
static_assert(kWireBytes == 62, "F24M must preserve the existing 62-byte SOS wire ABI");

// Integrity against torn/corrupted host records, NOT sender authentication.
inline uint32_t crc32(const uint8_t* bytes,size_t n) {
 uint32_t v=0xFFFFFFFFUL;
 for(size_t i=0;i<n;++i) {
  v ^= bytes[i];
  for(unsigned k=0;k<8;++k) v=(v>>1)^(0xEDB88320UL & (0U-(v&1U)));
 }
 return ~v;
}
inline void le16(uint8_t* p,uint16_t v){p[0]=uint8_t(v);p[1]=uint8_t(v>>8);}
inline void le32(uint8_t* p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8U*i));}
inline uint16_t rd16(const uint8_t* p){return uint16_t(p[0]) | uint16_t(uint16_t(p[1])<<8U);}
inline uint32_t rd32(const uint8_t* p){return uint32_t(p[0]) | (uint32_t(p[1])<<8U) | (uint32_t(p[2])<<16U) | (uint32_t(p[3])<<24U);}
inline bool validSOS(const VesselPacket& p){
 return p.magic==PACKET_MAGIC && p.version==PACKET_VERSION &&
        p.packetType==PACKET_SOS && p.originNodeId>=1U &&
        p.originNodeId<=9999U && p.sequenceNumber!=0U;
}
// F25C only: retain the legacy SOS wire and disk encoding; CANCEL uses v2.
inline bool validControl(const VesselPacket& p){
 if(validSOS(p))return true;
#if THESIS_DTN_F25C_BUILD_ONLY == 1
 return p.magic==PACKET_MAGIC && p.version==PACKET_VERSION &&
  p.packetType==PACKET_SOS_CANCEL && p.originNodeId>=1U && p.originNodeId<=9999U &&
  p.sequenceNumber!=0U && rd32(reinterpret_cast<const uint8_t*>(p.message))==p.sequenceNumber &&
  rd32(reinterpret_cast<const uint8_t*>(p.message)+4)==p.originNodeId;
#else
 return false;
#endif
}
struct PacketEntry {
 uint8_t bytes[kWireBytes]{};
 uint32_t origin=0, event=0;
 uint8_t source=0;
};
inline uint8_t recordType(const PacketEntry& p){return p.bytes[offsetof(VesselPacket,packetType)];}
enum class CaptureResult : uint8_t {Queued,Duplicate,Invalid,Conflicting,Full,Faulted};

template<size_t Capacity> class PendingQueue final {
 static_assert(Capacity>=1 && Capacity<=16,"Bound RAM-only capture queue");
 public:
 CaptureResult capture(const VesselPacket& p,uint8_t source){
  if(fault_)return CaptureResult::Faulted;
  if(!validControl(p) || source>kReceived)return CaptureResult::Invalid;
  for(size_t i=0;i<size_;++i){
   const PacketEntry& previous=entries_[(read_+i)%Capacity];
   if(previous.origin==p.originNodeId && previous.event==p.sequenceNumber && recordType(previous)==p.packetType){
    return memcmp(previous.bytes,&p,kWireBytes)==0
       ? CaptureResult::Duplicate:CaptureResult::Conflicting;
   }
  }
  if(size_==Capacity){if(lost_!=UINT32_MAX)++lost_;return CaptureResult::Full;}
  PacketEntry& item=entries_[(read_+size_)%Capacity];
  memcpy(item.bytes,&p,kWireBytes);
  item.origin=p.originNodeId;item.event=p.sequenceNumber;item.source=source;
  ++size_;return CaptureResult::Queued;
 }
 const PacketEntry* front()const {return size_ ? &entries_[read_]:nullptr;}
 bool acknowledgeDurableAppend(){
  if(!size_ || fault_)return false;
  read_=(read_+1U)%Capacity;--size_;return true;
 }
#if THESIS_DTN_F26C_BUILD_ONLY == 1
 // Single serialized caller: only remove the exact selected, persisted item.
 const PacketEntry* at(size_t index)const{return index<size_?&entries_[(read_+index)%Capacity]:nullptr;}
 bool acknowledgeDurableAppendAt(size_t index){
  if(fault_ || index>=size_)return false;
  if(index==0)return acknowledgeDurableAppend();
  for(size_t i=index;i+1<size_;++i)entries_[(read_+i)%Capacity]=entries_[(read_+i+1)%Capacity];
  --size_;return true;
 }
#endif
 void latchStorageFault(){fault_=true;}
 bool faulted()const{return fault_;}
 size_t pending()const{return size_;}
 uint32_t lost()const{return lost_;}
 private:
 PacketEntry entries_[Capacity]{};
 size_t read_=0,size_=0;uint32_t lost_=0;bool fault_=false;
};

inline bool encode(const PacketEntry& item,uint8_t (&frame)[kFrameBytes]){
 VesselPacket p{};memcpy(&p,item.bytes,kWireBytes);
 if(!validControl(p) || p.originNodeId!=item.origin || p.sequenceNumber!=item.event ||
    item.source>kReceived)return false;
 memset(frame,0,kFrameBytes);
 frame[0]='F';frame[1]='2';frame[2]='4';frame[3]='M';
 frame[4]=1;frame[5]=item.source;
#if THESIS_DTN_F25C_BUILD_ONLY == 1
 if(p.packetType==PACKET_SOS_CANCEL){frame[4]=2;frame[6]=PACKET_SOS_CANCEL;}
#endif
 le32(frame+8,item.origin);le32(frame+12,item.event);
 le16(frame+16,uint16_t(kWireBytes));
 le32(frame+18,crc32(item.bytes,kWireBytes));
 memcpy(frame+22,item.bytes,kWireBytes);
 le32(frame+84,crc32(frame,84));return true;
}
inline bool decode(const uint8_t* frame,size_t size,PacketEntry& out){
 if(frame==nullptr || size!=kFrameBytes || frame[0]!='F' || frame[1]!='2' ||
    frame[2]!='4' || frame[3]!='M' || frame[5]>kReceived || frame[7]!=0 || rd16(frame+16)!=kWireBytes ||
    rd32(frame+84)!=crc32(frame,84) ||
    rd32(frame+18)!=crc32(frame+22,kWireBytes))return false;
 VesselPacket p{};memcpy(&p,frame+22,kWireBytes);
 if(!validControl(p) || p.originNodeId!=rd32(frame+8) || p.sequenceNumber!=rd32(frame+12))return false;
 const bool legacy=frame[4]==1 && frame[6]==0 && p.packetType==PACKET_SOS;
#if THESIS_DTN_F25C_BUILD_ONLY == 1
 const bool cancel=frame[4]==2 && frame[6]==PACKET_SOS_CANCEL && p.packetType==PACKET_SOS_CANCEL;
 if(!legacy && !cancel)return false;
#else
 if(!legacy)return false;
#endif
 PacketEntry candidate{};memcpy(candidate.bytes,frame+22,kWireBytes);
 candidate.origin=p.originNodeId;candidate.event=p.sequenceNumber;
 candidate.source=frame[5];out=candidate;return true;
}
inline bool validSession(const char* s){
 if(!s || strlen(s)!=8 || s[0]!='S')return false;
 for(unsigned i=1;i<8;++i)
  if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='A'&&s[i]<='F')))return false;
 return true;
}
inline bool sandboxPath(char* out,size_t cap,const char* dir,uint32_t vessel){
 if(!out || cap==0 || !dir || vessel<1 || vessel>3)return false;
 char root[40]{};
 const int n=snprintf(root,sizeof(root),"/THESIS/F06TEST/V%03lu/",(unsigned long)vessel);
 if(n<0 || size_t(n)>=sizeof(root) || strncmp(dir,root,size_t(n)) ||
    !validSession(dir+size_t(n)))return false;
 const int path=snprintf(out,cap,"%s/dtn_sos_packets.f24m",dir);
 return path>0 && size_t(path)<cap;
}
// F24X checked ports expose storageHealthy(); legacy ports retain their prior
// behavior. This is not a retroactive claim that raw File close is checked.
template<class SDLike>auto storageHealthyImpl(SDLike& sd,int)->decltype(bool(sd.storageHealthy())){return bool(sd.storageHealthy());}
template<class SDLike>bool storageHealthyImpl(SDLike&,long){return true;}
template<class SDLike>bool storageHealthy(SDLike& sd){return storageHealthyImpl(sd,0);}
// Writer: one new per-boot F06 session file, never truncate or reopen an old
// file for writing. One packet appended + flushed + handle closed + read back.
// On any uncertain write/readback, latch failure: DO NOT retry an ambiguous
// append or claim durability. Does not grant a physical owner or reader lease.
class SessionWriter final {
 public:
 template<class SDLike>bool appendChecked(SDLike& sd,const char* dir,
    uint32_t vessel,const PacketEntry& packet,const char* appendFlag,const char* readFlag){
  if(fault_ || !storageHealthy(sd)){fault_=true;return false;}
  uint8_t frame[kFrameBytes]{};
  if(!encode(packet,frame)){fault_=true;return false;}
  if(!initialized_){
   if(!sandboxPath(path_,sizeof(path_),dir,vessel) || sd.exists(path_) || !storageHealthy(sd)){
    fault_=true;return false;
   }
   // No persistent writer handle; creates only a *new* session file.
   auto initial=sd.open(path_,appendFlag);
   if(!initial){fault_=true;return false;}
   const bool empty=initial.size()==0;
   initial.close();
   if(!empty || !storageHealthy(sd)){fault_=true;return false;}
   initialized_=true;
  }
  auto handle=sd.open(path_,appendFlag);
  if(!handle){fault_=true;return false;}
  if(handle.size()!=written_){handle.close();fault_=true;return false;}
  const bool lengthOK=handle.write(frame,kFrameBytes)==kFrameBytes;
  handle.flush();const bool wrote=lengthOK && handle.getWriteError()==0;
  handle.close();
  if(!wrote || !storageHealthy(sd)){fault_=true;return false;}
  auto check=sd.open(path_,readFlag);
  if(!check){fault_=true;return false;}
  bool equal=check.size()==written_+kFrameBytes && check.seek(written_);
  uint8_t actual[32]{};
  size_t at=0;
  while(equal && at<kFrameBytes){
   const size_t step=(kFrameBytes-at)<sizeof(actual)?kFrameBytes-at:sizeof(actual);
   equal=check.read(actual,step)==int(step) && memcmp(actual,frame+at,step)==0;
   at+=step;
  }
  check.close();
  if(!equal || !storageHealthy(sd)){fault_=true;return false;}
  written_+=kFrameBytes;return true;
 }
 bool faulted()const{return fault_;}
 uint32_t written()const{return written_;}
 const char* path()const{return path_;}
 private:
 char path_[96]{};uint32_t written_=0;
 bool initialized_=false,fault_=false;
};
} // namespace DTNF24M
