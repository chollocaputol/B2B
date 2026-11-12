#pragma once
// Phase 5.1.6 RX-only format gate. This header does NOT verify Ed25519
// signatures and must never be interpreted as authenticated reception.
// Classification remains separate from authentication, authorization and RF TX.
#include "wire_gate.h"
namespace Phase516 {
static constexpr size_t kMaxReceiveWireBytes = 99; // v3 DETAIL only; no transmit
enum class Decision : unsigned char { Reject, LegacyUntrusted, PositionUntrusted, QuarantineV3 };
inline Decision decide(const unsigned char* data, size_t length) {
  const Phase51::Classification c = Phase51::classify(data,length);
  if (c.kind == Phase51::Kind::LegacyV1Untrusted) return Decision::LegacyUntrusted;
  if (c.kind == Phase51::Kind::PositionV2Untrusted) return Decision::PositionUntrusted;
  if (c.kind == Phase51::Kind::V3NeedsCrypto) return Decision::QuarantineV3;
  return Decision::Reject;
}
inline constexpr bool establishesSignatureProof(Decision) {return false;}
inline constexpr bool authorizesRelay(Decision) {return false;}
inline constexpr bool authorizesAutomaticV3Ack(Decision) {return false;}
}
