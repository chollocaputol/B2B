#pragma once
#include <stdint.h>
// Phase 5.2.2C-3G-4C: pure peer-relative timing heuristic; NO radio access.
namespace TelemetryPhaseGuard {
static constexpr uint32_t kGuardBeginMs = 3300UL;
static constexpr uint32_t kGuardEndMs = 6300UL;
static constexpr uint32_t kMaximumDeferMs = 3200UL;
inline uint32_t suggestedDelayMs(uint32_t nowMs, uint32_t peerLastRxMs) {
  const uint32_t age = uint32_t(nowMs - peerLastRxMs); // millis wrap-safe
  return age >= kGuardBeginMs && age < kGuardEndMs ? kGuardEndMs - age : 0UL;
}
inline uint32_t boundedDelayMs(uint32_t wantedMs, uint32_t alreadyDeferredMs) {
  if (!wantedMs || alreadyDeferredMs >= kMaximumDeferMs) return 0UL;
  const uint32_t remainingMs = kMaximumDeferMs - alreadyDeferredMs;
  return wantedMs < remainingMs ? wantedMs : remainingMs;
}
}
