#pragma once
#include <stdint.h>

// Phase 5.2.2C-3G-4E1: pure, node-independent airtime-aware scheduling.
// No CAD, RF switching, SOS queue, or slot leader implemented here.
// Airtime value is from prior 62-byte SF9/BW125/CR4/7 bench measurement.
namespace DecentralizedMac {
static constexpr uint32_t kMeasuredAirtimeMs = 492UL;
// Trial 3G4E3B: peer-present telemetry load target reduced from 25% to 8%.
// The 492ms model is the historical 62-byte frame; current POSITION_V2 is
// 25 bytes (~262ms logged). This deliberately errs toward longer intervals.
static constexpr uint32_t kTargetLoadPercent = 8UL;
static constexpr uint32_t kMinIntervalMs = 5000UL;
static constexpr uint32_t kMaxBaseIntervalMs = 60000UL;
static constexpr uint32_t kMaxJitterMs = 6000UL;
static constexpr uint32_t kRecentPeerHorizonMs = 120000UL;
static constexpr uint32_t kMaxCountedNodes = 32UL; // airtime calculation cap, not peers-table capacity

// Integer-only approximation of mean channel utilization assuming each node
// uses the same report interval. This is a TARGET, not an RF measurement.
inline uint32_t baseIntervalMs(uint32_t recentlyHeardPeers) {
  // Preserve the established 5s base for a vessel with no recent peers.
  if (recentlyHeardPeers == 0) return kMinIntervalMs;
  uint32_t nodes = recentlyHeardPeers >= kMaxCountedNodes ? kMaxCountedNodes : recentlyHeardPeers + 1UL;
  uint64_t needed = (uint64_t(nodes) * kMeasuredAirtimeMs * 100ULL +
                     (kTargetLoadPercent - 1UL)) / kTargetLoadPercent;
  if (needed < kMinIntervalMs) return kMinIntervalMs;
  if (needed > kMaxBaseIntervalMs) return kMaxBaseIntervalMs;
  return static_cast<uint32_t>(needed);
}

inline uint32_t jitterHalfWidthMs(uint32_t baseMs) {
  const uint32_t q = baseMs / 4UL;
  return q < kMaxJitterMs ? q : kMaxJitterMs;
}

inline uint32_t elapsedSince(uint32_t now, uint32_t before) {
  return uint32_t(now - before);  // safe for millis rollover
}

class Scheduler {
  uint32_t nodeId_ = 0;
  uint32_t randomState_ = 0xC3A5C85CUL;
  uint32_t nextDueMs_ = 0;
  uint32_t lastBaseMs_ = kMinIntervalMs;
  uint32_t lastChosenMs_ = kMinIntervalMs;
  uint32_t txOpportunities_ = 0;
  bool started_ = false;

  uint32_t nextRandom() {
    // Xorshift32 with a non-zero seed, independent of Arduino random() state.
    uint32_t x = randomState_;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    randomState_ = x;
    return x;
  }
public:
  void begin(uint32_t nodeId, uint32_t nowMs, uint32_t bootNonce) {
    nodeId_ = nodeId;
    randomState_ = (bootNonce ^ (nodeId * 0x9E3779B9UL) ^ 0xA341316CUL);
    if (randomState_ == 0) randomState_ = 0xC3A5C85CUL;
    lastBaseMs_ = lastChosenMs_ = kMinIntervalMs;
    txOpportunities_ = 0;
    nextDueMs_ = nowMs + 1000UL + (nextRandom() % 4001UL);
    started_ = true;
  }
  bool due(uint32_t nowMs) const {
    return started_ && static_cast<int32_t>(nowMs - nextDueMs_) >= 0;
  }
  // Call ONCE per scheduled ordinary telemetry transmission, before the
  // blocking radio.transmit. Does not skip packets or affect SOS/ACK paths.
  uint32_t scheduleNext(uint32_t nowMs, uint32_t recentlyHeardPeers) {
    const uint32_t base = baseIntervalMs(recentlyHeardPeers);
    const uint32_t half = jitterHalfWidthMs(base);
    const uint32_t width = half * 2UL + 1UL;
    const uint32_t chosen = base - half + (nextRandom() % width);
    lastBaseMs_ = base;
    lastChosenMs_ = chosen;
    nextDueMs_ = nowMs + chosen; // never catch up in a burst
    ++txOpportunities_;
    return chosen;
  }
  uint32_t nextDueMs() const { return nextDueMs_; }
  uint32_t baseMs() const { return lastBaseMs_; }
  uint32_t intervalMs() const { return lastChosenMs_; }
  uint32_t txOpportunities() const { return txOpportunities_; }
  uint32_t nodeId() const { return nodeId_; }
};
} // namespace DecentralizedMac
