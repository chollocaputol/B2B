#pragma once
#include <stdint.h>
#include "relay_policy.h"
#include "../communication/multi_peer_sos.h"

// R2-B2: final state gate for a queued SOS/CANCEL frame. This is a
// RAM-state consistency check, NOT origin authentication or a TX permission.
// A passed frame may ONLY be logged by the current universal firmware.
namespace Relay52 {
enum class Preflight : uint8_t {
  Ready, BadHeader, BadOrigin, BadHop, BadType, BadPayload,
  UnknownOrigin, ObsoleteEvent, SosCancelled, CancelNotConfirmed
};
inline const char* preflightName(Preflight result) {
  switch (result) {
    case Preflight::Ready: return "READY";
    case Preflight::BadHeader: return "BAD_HEADER";
    case Preflight::BadOrigin: return "BAD_ORIGIN";
    case Preflight::BadHop: return "BAD_HOP";
    case Preflight::BadType: return "BAD_TYPE";
    case Preflight::BadPayload: return "BAD_PAYLOAD";
    case Preflight::UnknownOrigin: return "UNKNOWN_ORIGIN";
    case Preflight::ObsoleteEvent: return "OBSOLETE_EVENT";
    case Preflight::SosCancelled: return "SOS_CANCELLED";
    case Preflight::CancelNotConfirmed: return "CANCEL_NOT_CONFIRMED";
  }
  return "UNKNOWN";
}

template <typename Packet>
inline Preflight preflightPrepared(const Packet& p, const MultiPeerSOS::Table& table) {
  if (p.magic != 0x4D45 || p.version != 1) return Preflight::BadHeader;
  if (p.originNodeId == 0 || p.originNodeId > 9999) return Preflight::BadOrigin;
  // Prepared relay frames must already have had their hop count incremented.
  if (p.hopCount == 0 || p.hopCount > RelayPolicy::kMaxHops) return Preflight::BadHop;
  if (!RelayPolicy::eligible(p.packetType)) return Preflight::BadType;
  if (!RelayPolicy::validCancelPayload(p)) return Preflight::BadPayload;
  const MultiPeerSOS::Slot* current = table.get(p.originNodeId);
  if (!current) return Preflight::UnknownOrigin;
  if (current->event != p.sequenceNumber) return Preflight::ObsoleteEvent;
  if (p.packetType == 2 && current->cancelled) return Preflight::SosCancelled;
  if (p.packetType == 4 && !current->cancelled) return Preflight::CancelNotConfirmed;
  return Preflight::Ready;
}
}  // namespace Relay52
