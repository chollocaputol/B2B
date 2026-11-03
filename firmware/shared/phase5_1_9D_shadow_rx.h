#pragma once
// Phase 5.1.9D: ESP32 receive-only shadow integration, HARD LOCKED without keys.
// No trust anchor, persisted replay state, NVS access, RF ACK, operational SOS or relay.
#include "phase5_1_9_shadow.h"
#include <stdint.h>
#include <stddef.h>

extern "C" {
int sodium_init(void);
int crypto_sign_verify_detached(const unsigned char*, const unsigned char*,
                                unsigned long long, const unsigned char*);
}
namespace Phase519D {
static constexpr bool kDeviceKeysProvisioned=false;
static constexpr bool kDurableRollbackProtection=false;
static constexpr bool kOperationalV3=false;
static constexpr bool kAutomaticAck=false;
static constexpr bool kRelayTransmission=false;

// Intentional hard lock, independent of any bytes supplied over LoRa.
inline bool trustedKey(void*,uint32_t,uint32_t,uint8_t key[32]) {
  if(key) for(size_t i=0;i<32;++i) key[i]=0;
  return false;
}
// Genuine verified device backend, already tested with RFC 8032 vectors on ESP32-S3.
// This is NEVER called until the independent trust key callback authorizes a key.
inline bool verifyEd25519(void*,const uint8_t key[32],const uint8_t* msg,size_t n,const uint8_t sig[64]) {
  if (!key||!sig||(!msg&&n)) return false;
  if(sodium_init()<0) return false;
  return crypto_sign_verify_detached(sig,msg,static_cast<unsigned long long>(n),key)==0;
}
inline Phase519::Result inspect(const uint8_t* wire,size_t length,Phase519::Ledger& ledger) {
  const Phase517::Callbacks cb{nullptr, trustedKey, verifyEd25519};
  return Phase519::inspect(wire,length,cb,ledger);
}
inline constexpr bool mayUpdateOperationalSos(Phase519::Outcome){return false;}
inline constexpr bool maySendAck(Phase519::Outcome){return false;}
inline constexpr bool mayRelay(Phase519::Outcome){return false;}
}
static_assert(!Phase519D::kDeviceKeysProvisioned,"No device key provisioning");
static_assert(!Phase519D::kDurableRollbackProtection,"No durable replay authority");
static_assert(!Phase519D::kOperationalV3,"No v3 operational state");
static_assert(!Phase519D::kAutomaticAck,"No v3 ACK");
static_assert(!Phase519D::kRelayTransmission,"No v3 relay");
