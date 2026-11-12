#pragma once
#include <stdint.h>

// R2-C1 readiness assessment only. This module is not a transmit gate or
// permission grant; there is deliberately no function which can allow TX.
// IMPORTANT: never use the result to initiate a transmitter call or radio TX.
namespace RelayReadiness {
static constexpr bool kRelayRadioTxEnabled = false;
static constexpr bool kOriginAuthenticationImplemented = false;
static constexpr bool kRelayAuthorizationImplemented = false;
static constexpr bool kEndToEndReceiptImplemented = false;
static constexpr uint8_t kMaxRelayRadioAttempts = 0;
static_assert(!kRelayRadioTxEnabled, "R2-C1 relay RF TX must remain OFF");
static_assert(kMaxRelayRadioAttempts == 0, "R2-C1 has no relay radio budget");

enum class Blocker : uint8_t {
  RadioDisabled, OriginAuthenticationMissing, RelayAuthorizationMissing,
  EndToEndReceiptMissing, RadioBudgetZero
};
inline const char* blockerName(Blocker b) {
  switch (b) {
    case Blocker::RadioDisabled: return "RADIO_DISABLED";
    case Blocker::OriginAuthenticationMissing: return "ORIGIN_AUTH_MISSING";
    case Blocker::RelayAuthorizationMissing: return "RELAY_AUTHORIZATION_MISSING";
    case Blocker::EndToEndReceiptMissing: return "END_TO_END_RECEIPT_MISSING";
    case Blocker::RadioBudgetZero: return "RADIO_BUDGET_ZERO";
  }
  return "UNKNOWN";
}
struct Snapshot {
  bool radioEnabled;
  bool originAuthenticated;
  bool relayAuthorized;
  bool endToEndReceipt;
  uint8_t maxRadioAttempts;
};
inline Snapshot current() {
  return { kRelayRadioTxEnabled, kOriginAuthenticationImplemented,
           kRelayAuthorizationImplemented, kEndToEndReceiptImplemented,
           kMaxRelayRadioAttempts };
}
inline Blocker primaryBlocker() {
  const Snapshot s = current();
  if (!s.radioEnabled) return Blocker::RadioDisabled;
  if (!s.originAuthenticated) return Blocker::OriginAuthenticationMissing;
  if (!s.relayAuthorized) return Blocker::RelayAuthorizationMissing;
  if (!s.endToEndReceipt) return Blocker::EndToEndReceiptMissing;
  return Blocker::RadioBudgetZero;
}
} // namespace RelayReadiness
