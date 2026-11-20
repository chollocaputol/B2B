#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../communication/packet_protocol.h"
namespace F23DReadiness {
struct EventSnapshot {
  uint32_t self=0,next=0;
  bool provisioned=false,counterAvailable=false,active=false,pending=false;
  bool cancelPending=false,storageSafe=false,recoveryHold=false;
};
inline bool selectable(const EventSnapshot& s) {
  return s.provisioned && s.self==1U && s.counterAvailable && s.next!=0 &&
      s.next!=UINT32_MAX && !s.active && !s.pending && !s.cancelPending &&
      s.storageSafe && !s.recoveryHold;
}
inline bool eventRow(char* row,size_t capacity,const EventSnapshot& s,uint32_t now) {
  const int n=snprintf(row,capacity,
      "[F23D-NEXT] local=%lu ms=%lu provisioned=%u next=%lu counter=%u exhausted=%u active=%u pending=%u cancel=%u storage_safe=%u recovery_hold=%u assessment=%s SNAPSHOT_ONLY=1",
      (unsigned long)s.self,(unsigned long)now,unsigned(s.provisioned),
      (unsigned long)s.next,unsigned(s.counterAvailable),unsigned(s.next==UINT32_MAX),
      unsigned(s.active),unsigned(s.pending),unsigned(s.cancelPending),
      unsigned(s.storageSafe),unsigned(s.recoveryHold),selectable(s)?"READY":"BLOCKED");
  return n>=0 && size_t(n)<capacity;
}
// Non-cryptographic comparison fingerprints, never origin authentication.
// Canonical form substitutes hop=0 without changing or serializing the packet.
inline uint32_t fingerprint(const uint8_t* bytes,size_t length,bool canonical=false) {
  uint32_t result=2166136261UL;
  for(size_t i=0;i<length;++i) {
    const uint8_t value=canonical && i==offsetof(VesselPacket,hopCount)?0:bytes[i];
    result=(result^value)*16777619UL;
  }
  return result;
}
inline bool relevant(const uint8_t* bytes,size_t length) {
  return bytes && length>=4 && (bytes[3]==PACKET_SOS || bytes[3]==PACKET_SOS_CANCEL);
}
inline bool rxRow(char* row,size_t capacity,uint32_t self,uint32_t now,
    const uint8_t* bytes,size_t length,int16_t rssi,int snrX10,
    const char* validation,const char* outcome) {
  if(!relevant(bytes,length) || length>99)return false;
  VesselPacket p{};
  if(length==sizeof(p))memcpy(&p,bytes,sizeof(p));
  const int n=snprintf(row,capacity,
      "[F23D-RXOBS] local=%lu ms=%lu origin=%lu event=%lu kind=%u hop=%u bytes=%u rssi=%d snr_x10=%d validation=%s state=%s wire_fnv=%08lX canonical_fnv=%08lX AUTH=UNVERIFIED",
      (unsigned long)self,(unsigned long)now,(unsigned long)p.originNodeId,
      (unsigned long)p.sequenceNumber,unsigned(bytes[3]),unsigned(p.hopCount),
      unsigned(length),int(rssi),snrX10,validation,outcome,
      (unsigned long)fingerprint(bytes,length),
      (unsigned long)fingerprint(bytes,length,true));
  return n>=0 && size_t(n)<capacity;
}
}
