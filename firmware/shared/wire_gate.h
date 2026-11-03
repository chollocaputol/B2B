#pragma once
// Phase 5.1.5 HOST-ONLY packet discrimination. Not wired into ESP32.
// Classification is NEVER authentication or permission to relay.
#include <cstddef>
#include <cstdint>
namespace Phase51 {
enum class Kind : uint8_t { Reject=0, LegacyV1Untrusted, PositionV2Untrusted, V3NeedsCrypto };
struct Classification { Kind kind; uint8_t type; };
inline Classification classify(const uint8_t* p, size_t n) {
  if (!p || n < 4 || p[0] != 0x45 || p[1] != 0x4d) return {Kind::Reject,0};
  const uint8_t v=p[2], t=p[3];
  if (v==1 && n==62 && t>=1 && t<=5) return {Kind::LegacyV1Untrusted,t};
  if (v==2 && n==25 && t==6) return {Kind::PositionV2Untrusted,t};
  if (v==3 && ((t==0x20 && n==93) || (t==0x21 && n==99) ||
               (t==0x22 && n==80))) return {Kind::V3NeedsCrypto,t};
  return {Kind::Reject,0};
}
// Fail-closed: no classifier result authorizes origin authenticity, SOS state
// mutation, relay permission, or transmission.
inline constexpr bool permitsRelayRadioTx(Kind) {return false;}
inline constexpr bool establishesOriginAuthentication(Kind) {return false;}
}
