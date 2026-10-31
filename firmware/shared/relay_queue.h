#pragma once
#include <stdint.h>
#include <stddef.h>
#include "relay_policy.h"

// Phase 5.2.1: bounded, RAM-only forward preparation; NO radio transmission.
// A queued packet retains the original origin/event and increments hop count.
// Duplicate suppression is per (origin, sequence, packet type) and is RAM-only.
namespace Relay52 {
static constexpr size_t kQueueDepth = 4;
enum class Admit : uint8_t { Invalid, OwnOrigin, NotRelayable, HopLimit, Duplicate, Queued, Full };
inline const char* name(Admit x) {
  switch(x) {
  case Admit::Invalid:return "INVALID";
  case Admit::OwnOrigin:return "OWN_ORIGIN";
  case Admit::NotRelayable:return "NOT_RELAYABLE";
  case Admit::HopLimit:return "HOP_LIMIT";
  case Admit::Duplicate:return "DUPLICATE";
  case Admit::Queued:return "QUEUED";
  case Admit::Full:return "QUEUE_FULL";
  }
  return "UNKNOWN";
}
template<typename Packet> class Queue {
public:
 Queue():count_(0),cursor_(0){}
 Admit receive(const Packet& original,uint32_t self) {
  // Verify before inserting into the duplicate cache, to avoid poisoning
  // the cache with malformed packet frames.
  if (original.magic!=0x4D45 || original.version!=1 || original.originNodeId==0 ||
      original.packetType<1 || original.packetType>5) return Admit::Invalid;
  if (original.originNodeId==self) return Admit::OwnOrigin;
  if (!RelayPolicy::eligible(original.packetType)) return Admit::NotRelayable;
  if (original.hopCount>=RelayPolicy::kMaxHops) return Admit::HopLimit;
  if (cache_.contains(original.originNodeId,original.sequenceNumber,original.packetType)) return Admit::Duplicate;
  if (count_==kQueueDepth) return Admit::Full;
  Packet forwarded=original;
  ++forwarded.hopCount;
  slots_[(cursor_+count_)%kQueueDepth]=forwarded;
  ++count_;
  cache_.remember(original.originNodeId,original.sequenceNumber,original.packetType);
  return Admit::Queued;
 }
 size_t size() const {return count_;}
 bool pop(Packet& packet) {
  if (!count_) return false;
  packet=slots_[cursor_];
  cursor_=(cursor_+1)%kQueueDepth;
  --count_;
  return true;
 }
private:
 Packet slots_[kQueueDepth]{};
 size_t count_,cursor_;
 RelayPolicy::Cache cache_;
};
} // namespace Relay52
