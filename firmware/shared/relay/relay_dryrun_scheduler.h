#pragma once
#include <stdint.h>
#include <stddef.h>
#include "relay_queue.h"

// R2-B1: RAM-only bounded scheduling of already-authorized SOS/CANCEL relay
// candidates. poll() returns a prepared packet for LOGGING ONLY; the universal
// firmware deliberately does not pass it to any radio transmit function.
namespace Relay52 {
static constexpr uint32_t kDryRunDelayMs = 250UL;

template <typename Packet> class DryRunScheduler {
public:
  Admit offer(const Packet& p, uint32_t self, bool stateApproved) {
    if (!stateApproved) return Admit::StateRejected;
    // The staged packet counts against the same four-slot total capacity.
    // Checking this first avoids consuming the duplicate-cache slot on FULL.
    if (size() >= kQueueDepth) return Admit::Full;
    return queue_.receive(p, self);
  }

  // Single-slot time gate, rollover safe. Nothing is transmitted here.
  bool poll(uint32_t nowMs, Packet& prepared) {
    if (!staged_) {
      if (!queue_.pop(stagedPacket_)) return false;
      readyAtMs_ = nowMs + kDryRunDelayMs;
      staged_ = true;
    }
    if (static_cast<int32_t>(nowMs - readyAtMs_) < 0) return false;
    prepared = stagedPacket_;
    staged_ = false;
    return true;
  }

  size_t size() const { return queue_.size() + (staged_ ? 1U : 0U); }
private:
  Queue<Packet> queue_;
  Packet stagedPacket_{};
  uint32_t readyAtMs_ = 0;
  bool staged_ = false;
};
}  // namespace Relay52
