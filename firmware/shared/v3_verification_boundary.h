#pragma once
// Phase 5.1.7: portable, receive-only v3 verification policy boundary.
// Caller MUST supply independently provisioned trusted key and genuine Ed25519 verifier.
// This header contains NO cryptographic implementation or device trust anchors.
// It NEVER authorizes RF TX, SOS state mutation, ACK, or relay.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "wire_gate.h"

namespace Phase517 {
static constexpr size_t kPublicKeyBytes=32;
static constexpr size_t kSignatureBytes=64;
static constexpr uint8_t kAlert=0x20,kDetail=0x21,kCancel=0x22;
enum class Verdict : uint8_t {Malformed, Unsupported, UntrustedKey, UnauthorizedEpoch, CryptoUnavailable, BadSignature, VerifiedHostOnly};
struct Identity {uint32_t origin=0,epoch=0,event=0; uint8_t kind=0;};
// Callbacks return false when an identity/epoch is not provisioned/authorized.
using TrustedKey = bool (*)(void*,uint32_t,uint32_t,uint8_t[32]);
using VerifySignature = bool (*)(void*,const uint8_t[32],const uint8_t*,size_t,const uint8_t[64]);
struct Callbacks {void* context=nullptr; TrustedKey trustedKey=nullptr; VerifySignature ed25519=nullptr;};
inline uint32_t rd32(const uint8_t* p) {return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline int32_t signed32(uint32_t v) {return v<=0x7fffffffU?int32_t(v):-1-int32_t(~v);}
inline int16_t signed16(uint16_t v) {return v<=32767U?int16_t(v):int16_t(-1-int32_t(uint16_t(~v)));}
inline uint16_t rd16(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline bool gpsOk(int32_t lat,int32_t lon){return lat>=-900000000&&lat<=900000000&&lon>=-1800000000&&lon<=1800000000&&!(lat==0&&lon==0);}
inline bool fieldsOk(const uint8_t* f,size_t n){
 const uint8_t k=f[3];
 if(k==kAlert){
   if(n!=93) return false;
   const uint8_t flags=f[20];
   if(flags&~uint8_t(3))return false;
   if(!(flags&2)&&rd32(f+16)!=0)return false;
   const int32_t lat=signed32(rd32(f+21)),lon=signed32(rd32(f+25));
   return (flags&1)?gpsOk(lat,lon):(lat==0&&lon==0);
 }
 if(k==kDetail){
   if(n!=99) return false;
   const uint8_t flags=f[34];
   if(flags&~uint8_t(3))return false;
   if(!(flags&2)&&rd32(f+16)!=0)return false;
   if(f[33]>100)return false;
   if(!(flags&1)){
     // signed weather measurement + classification fields at 22..32 (inclusive).
     for(size_t i=22;i<=31;i++)if(f[i]!=0)return false;
   }
   return true;
 }
 if(k==kCancel)return n==80;
 return false;
}
inline const char* domain(uint8_t kind) {
 switch(kind){case kAlert:return "THESIS-EXP-V3-SOS-ALERT";case kDetail:return "THESIS-EXP-V3-SOS-DETAIL";case kCancel:return "THESIS-EXP-V3-SOS-CANCEL";}
 return nullptr;
}
inline Verdict verify(const uint8_t* frame,size_t n,const Callbacks& cb,Identity& identity){
 identity=Identity{};
 if(Phase51::classify(frame,n).kind!=Phase51::Kind::V3NeedsCrypto) return Verdict::Unsupported;
 if(!frame||n<16||!fieldsOk(frame,n))return Verdict::Malformed;
 Identity candidate{rd32(frame+4),rd32(frame+8),rd32(frame+12),frame[3]};
 if(candidate.origin<1||candidate.origin>9999||candidate.epoch==0||candidate.event==0)return Verdict::Malformed;
 if(!cb.trustedKey)return Verdict::UntrustedKey;
 uint8_t key[32]={};
 if(!cb.trustedKey(cb.context,candidate.origin,candidate.epoch,key))return Verdict::UnauthorizedEpoch;
 if(!cb.ed25519)return Verdict::CryptoUnavailable;
 const char* d=domain(candidate.kind);
 if(!d)return Verdict::Unsupported;
 const size_t dlen=strlen(d)+1; // include domain terminator, required by v3 format
 const size_t bodylen=n-kSignatureBytes;
 // stack buffer: max domain 30 + 35 byte signed body, bounded well below 128.
 uint8_t message[128]={};
 if(dlen+bodylen>sizeof(message))return Verdict::Malformed;
 memcpy(message,d,dlen);memcpy(message+dlen,frame,bodylen);
 if(!cb.ed25519(cb.context,key,message,dlen+bodylen,frame+bodylen))return Verdict::BadSignature;
 identity=candidate;
 return Verdict::VerifiedHostOnly;
}
inline constexpr bool authorizesRadioTx(Verdict){return false;}
inline constexpr bool authorizesSosMutation(Verdict){return false;}
inline constexpr bool providesDestinationReceipt(Verdict){return false;}
} // namespace Phase517
