#pragma once
// Three-node bench: bounded forwarding of unmodified POSITION_V2 bytes.
// Pure routing queue: does NOT authorize any RF transmission.
// Only V002 may relay V001; no generic mesh or additional hop is implied.
// The 25-byte POSITION_V2 schema has no hop field.
#include "../communication/compact_position_v2.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace CompactPositionRelay {
static constexpr uint32_t kSelectedOrigin = 1;
static constexpr uint32_t kSelectedRelay = 2;
static constexpr size_t kQueueSize = 4;
static constexpr size_t kRecentSize = 32;

enum class Admission : uint8_t {
  Invalid, WrongRelay, OwnOrigin, WrongOrigin, Duplicate, Full, Queued
};
inline const char* name(Admission a) {
  switch (a) {
    case Admission::Invalid: return "INVALID";
    case Admission::WrongRelay: return "WRONG_RELAY";
    case Admission::OwnOrigin: return "OWN_ORIGIN";
    case Admission::WrongOrigin: return "WRONG_ORIGIN";
    case Admission::Duplicate: return "DUPLICATE";
    case Admission::Full: return "FULL";
    case Admission::Queued: return "QUEUED";
  }
  return "UNKNOWN";
}
struct Frame {
  uint8_t bytes[CompactPositionV2::kWireSize]{};
  uint32_t sequence=0;
  uint32_t uptimeSeconds=0;
};
struct Key { uint32_t sequence=0, uptimeSeconds=0; bool used=false; };

class Queue {
public:
  Admission offer(const uint8_t* bytes, size_t length, uint32_t self) {
    if (self != kSelectedRelay) return Admission::WrongRelay;
    if (!bytes || length != CompactPositionV2::kWireSize) return Admission::Invalid;
    CompactPositionV2::State decoded;
    if (!CompactPositionV2::decode(bytes, length, decoded)) return Admission::Invalid;
    if (decoded.originId == self) return Admission::OwnOrigin;
    if (decoded.originId != kSelectedOrigin) return Admission::WrongOrigin;
    for (size_t i=0; i<kRecentSize; ++i)
      if (recent_[i].used && recent_[i].sequence == decoded.sequence &&
          recent_[i].uptimeSeconds == decoded.uptimeSeconds) return Admission::Duplicate;
    // FULL must not consume a duplicate-cache slot.
    if (count_ == kQueueSize) return Admission::Full;
    Frame& slot = queue_[(head_+count_)%kQueueSize];
    memcpy(slot.bytes, bytes, sizeof(slot.bytes));
    slot.sequence = decoded.sequence;
    slot.uptimeSeconds = decoded.uptimeSeconds;
    ++count_;
    recent_[recentCursor_] = {decoded.sequence, decoded.uptimeSeconds, true};
    recentCursor_ = (recentCursor_+1)%kRecentSize;
    return Admission::Queued;
  }
  // Caller must separately verify identity, authorization, channel/timing
  // safety and SOS/CANCEL priority before invoking any radio adapter.
  bool peek(Frame& frame, bool emergencyPending) const {
    if (emergencyPending || !count_) return false;
    frame = queue_[head_];
    return true;
  }
  // Reserve denial is not an acknowledgment: keep the packet queued.
  bool acknowledge() {
    if (!count_) return false;
    head_ = (head_+1)%kQueueSize;
    --count_;
    return true;
  }
  bool take(Frame& frame, bool emergencyPending) {
    if (!peek(frame, emergencyPending)) return false;
    return acknowledge();
  }
  size_t pending() const { return count_; }
private:
  Frame queue_[kQueueSize]{};
  Key recent_[kRecentSize]{};
  size_t head_=0, count_=0, recentCursor_=0;
};
} // namespace CompactPositionRelay
