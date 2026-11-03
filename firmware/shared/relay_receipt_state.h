#pragma once
#include <stdint.h>

// R2-B3 diagnostic-only record. Local RAM; NO changes to the wire protocol.
// A matching legacy ACK is only a claim from a neighbor. This code neither
// authenticates senders nor establishes that a distant vessel received an SOS.
namespace RelayReceipt {
enum class Neighbor : uint8_t { None, Awaiting, Claimed, Exhausted };
enum class FarEnd : uint8_t { Unverified };
inline const char* neighborName(Neighbor v) {
  switch (v) {
    case Neighbor::None: return "NONE";
    case Neighbor::Awaiting: return "AWAITING_NEIGHBOR_ACK";
    case Neighbor::Claimed: return "NEIGHBOR_ACK_CLAIMED";
    case Neighbor::Exhausted: return "NO_NEIGHBOR_ACK_AFTER_RETRIES";
  }
  return "UNKNOWN";
}
inline const char* farEndName(FarEnd) { return "UNVERIFIED"; }
struct Snapshot {
  uint32_t origin=0, event=0, neighborAckSender=0, cancelAckSender=0;
  uint8_t attempts=0;
  bool active=false, cancelledLocally=false, anyRadioTxOK=false, cancelAckClaimed=false;
  Neighbor neighbor=Neighbor::None;
  FarEnd farEnd=FarEnd::Unverified;
};
class Tracker {
public:
  void begin(uint32_t origin, uint32_t event) {
    value_ = Snapshot{};
    value_.origin=origin;
    value_.event=event;
    value_.active=true;
    value_.neighbor=Neighbor::Awaiting;
  }
  void sosAttempt(uint32_t origin, uint32_t event, bool txOK) {
    if (!matches(origin,event) || value_.cancelledLocally) return;
    if (value_.attempts<255) ++value_.attempts;
    value_.anyRadioTxOK |= txOK;
  }
  bool neighborAck(uint32_t origin, uint32_t event, uint32_t sender) {
    if (!matches(origin,event) || value_.cancelledLocally ||
        value_.neighbor!=Neighbor::Awaiting || sender==0 || sender>9999 ||
        sender==origin) return false;
    value_.neighborAckSender=sender;
    value_.neighbor=Neighbor::Claimed;
    return true;
  }
  void retriesExhausted(uint32_t origin, uint32_t event) {
    if (matches(origin,event) && value_.neighbor==Neighbor::Awaiting)
      value_.neighbor=Neighbor::Exhausted;
  }
  void localCancel(uint32_t origin, uint32_t event) {
    if (!matches(origin,event)) return;
    value_.active=false;
    value_.cancelledLocally=true;
    // The application permits a newer SOS while this cancellation waits
    // for its own ACK. Keep that one in-flight cancellation separately.
    cancel_ = {origin, event, 0, true, false};
  }
  bool cancelAck(uint32_t origin, uint32_t event, uint32_t sender) {
    if (!cancel_.pending || cancel_.claimed || cancel_.origin!=origin ||
        cancel_.event!=event || sender==0 || sender>9999 || sender==origin)
      return false;
    cancel_.claimed=true;
    cancel_.pending=false;
    cancel_.sender=sender;
    // Only annotate the current SOS snapshot when it is the same event.
    if (value_.origin==origin && value_.event==event) {
      value_.cancelAckClaimed=true;
      value_.cancelAckSender=sender;
    }
    return true;
  }
  const Snapshot& snapshot() const { return value_; }
private:
  struct CancelPending {
    uint32_t origin=0, event=0, sender=0;
    bool pending=false, claimed=false;
  };
  CancelPending cancel_{};
  bool matches(uint32_t origin,uint32_t event) const {
    return value_.active && value_.origin==origin && value_.event==event;
  }
  Snapshot value_{};
};
} // namespace RelayReceipt
