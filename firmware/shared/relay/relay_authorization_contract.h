#pragma once
#include <stdint.h>
#include "relay_tx_readiness.h"

// R2-C2: evidence requirements and diagnostics ONLY. No key material,
// cryptographic verifier, allowlist or RF transmission is provided here.
// A claimed numeric origin and a matching v1 ACK are NOT authentication.
namespace RelayContract {
static constexpr uint32_t kMaxEventRadioAttempts = 0;
static constexpr bool kCanAuthorizeRealTx = false;
static_assert(kMaxEventRadioAttempts == 0, "R2-C2 no relay airtime budget");
static_assert(!kCanAuthorizeRealTx, "R2-C2 cannot authorize TX");

enum class Proof : uint8_t { None, VerifiedByTrustedMechanism };
enum class ReceiptSemantics : uint8_t {
  Unspecified, NeighborOnlyClearlyIdentified, EndToEndVerified
};
// Evidence is a REQUIREMENTS MODEL, not trusted from packets or callers.
// Real firmware supplies only evidenceUnavailable(). There is no TX grant.
struct Evidence {
  Proof originProof = Proof::None;
  Proof relayPermission = Proof::None;
  ReceiptSemantics receipts = ReceiptSemantics::Unspecified;
  uint32_t relayNode = 0;
  uint32_t claimedOrigin = 0;
  uint32_t attemptBudget = 0;
};
enum class Status : uint8_t {
  InvalidIdentity, MissingOriginProof, MissingRelayPermission,
  UndefinedReceiptSemantics, InvalidBudget, RadioDisabled
};
inline const char* statusName(Status s) {
  switch(s) {
    case Status::InvalidIdentity: return "INVALID_IDENTITY";
    case Status::MissingOriginProof: return "ORIGIN_PROOF_MISSING";
    case Status::MissingRelayPermission: return "RELAY_PERMISSION_MISSING";
    case Status::UndefinedReceiptSemantics: return "RECEIPT_SEMANTICS_UNDEFINED";
    case Status::InvalidBudget: return "RADIO_BUDGET_UNAVAILABLE";
    case Status::RadioDisabled: return "RADIO_DISABLED";
  }
  return "UNKNOWN";
}
inline Evidence evidenceUnavailable(uint32_t relayNode, uint32_t origin) {
  Evidence e;
  e.relayNode = relayNode;
  e.claimedOrigin = origin;
  e.receipts = ReceiptSemantics::NeighborOnlyClearlyIdentified;
  return e;
}
inline Status assess(const Evidence& e) {
  if (e.relayNode < 1 || e.relayNode > 9999 ||
      e.claimedOrigin < 1 || e.claimedOrigin > 9999 ||
      e.relayNode == e.claimedOrigin) return Status::InvalidIdentity;
  if (e.originProof != Proof::VerifiedByTrustedMechanism)
    return Status::MissingOriginProof;
  if (e.relayPermission != Proof::VerifiedByTrustedMechanism)
    return Status::MissingRelayPermission;
  if (e.receipts == ReceiptSemantics::Unspecified)
    return Status::UndefinedReceiptSemantics;
  // A finite, nonzero budget is necessary for future radio eligibility,
  // but current firmware deliberately supports zero attempts only.
  if (e.attemptBudget == 0 || e.attemptBudget > kMaxEventRadioAttempts)
    return Status::InvalidBudget;
  return Status::RadioDisabled; // Unreachable in R2-C2; still fail-closed.
}
inline bool txForbidden(const Evidence& e) {
  (void)e;
  return !kCanAuthorizeRealTx && !RelayReadiness::kRelayRadioTxEnabled;
}
} // namespace RelayContract
