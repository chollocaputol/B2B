#pragma once
// PHASE 5.1.8A: ESP32 verification BACKEND CONTRACT, not an implementation.
// The Arduino-ESP32 toolchain's Ed25519 capability is unverified.
// Intentionally returns unavailable: no keys, no crypto, no authorization.
// DO NOT change these defaults to accept without reviewed device crypto,
// root-approved key provisioning, replay-safe lifecycle and hardware tests.
#include "v3_verification_boundary.h"

namespace Phase518 {
enum class DeviceCryptoState : uint8_t {Unavailable=0};
static constexpr DeviceCryptoState kDeviceCryptoState=DeviceCryptoState::Unavailable;
static constexpr bool kDeviceEd25519Available=false;
static constexpr bool kV3SosStateMutationsEnabled=false;
static constexpr bool kV3RadioAcksEnabled=false;
static constexpr bool kRelayRadioTxEnabled=false;

inline Phase517::Callbacks deviceCallbacks() { return Phase517::Callbacks{}; }
inline Phase517::Verdict inspectReceiveOnly(const uint8_t* p,size_t n,Phase517::Identity& out) {
  return Phase517::verify(p,n,deviceCallbacks(),out);
}
inline constexpr bool mayAcceptAsAuthenticated(Phase517::Verdict) {return false;}
inline constexpr bool mayMutateSosState(Phase517::Verdict) {return false;}
inline constexpr bool maySendAck(Phase517::Verdict) {return false;}
inline constexpr bool mayRelay(Phase517::Verdict) {return false;}
}
static_assert(!Phase518::kDeviceEd25519Available, "ESP Ed25519 NOT VERIFIED");
static_assert(!Phase518::kV3SosStateMutationsEnabled, "v3 SOS mutation forbidden");
static_assert(!Phase518::kV3RadioAcksEnabled, "v3 ACK forbidden");
static_assert(!Phase518::kRelayRadioTxEnabled, "relay TX forbidden");
