#pragma once
#include <stdint.h>
// Phase 5.2.2C-3G-4E2. Pure host-testable bounded contention policy.
// Radio operations and SOS priority are implemented separately.
namespace MacContention {
enum class Sense : uint8_t { Free, Busy, RxActive, Error, Timeout, Interrupted };
enum class Decision : uint8_t { Wait, Send, ForcedSend };
static constexpr uint32_t kMaxDeferralMs = 2500UL;
static constexpr uint32_t kMinBackoffMs = 180UL;
static constexpr uint32_t kMaxBackoffMs = 1100UL;

inline bool due(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}
class Gate {
  uint32_t startedMs_ = 0, nextTryMs_ = 0;
  uint8_t retries_ = 0;
  bool pending_ = false;
public:
  void begin(uint32_t now) { startedMs_ = nextTryMs_ = now; retries_ = 0; pending_ = true; }
  bool pending() const { return pending_; }
  bool ready(uint32_t now) const { return pending_ && due(now, nextTryMs_); }
  uint32_t elapsed(uint32_t now) const { return uint32_t(now - startedMs_); }
  uint32_t nextTryMs() const { return nextTryMs_; }
  uint8_t retries() const { return retries_; }
  void finish() { pending_ = false; }
  Decision report(uint32_t now, Sense sense, uint8_t mode, uint32_t randomness) {
    // mode 1=CAD OBSERVE, 2=CAD ENFORCE. 0 uses existing scheduler directly.
    if (!pending_) return Decision::Wait;
    if (sense == Sense::Free) return Decision::Send;
    // OBSERVE never blocks based on CAD busy/error alone. But protect an
    // ongoing RX briefly, with a hard deadline to prevent starvation.
    const bool needsWait = (sense == Sense::RxActive || sense == Sense::Interrupted) || (mode == 2);
    if (!needsWait) return Decision::Send;
    if (elapsed(now) >= kMaxDeferralMs) return Decision::ForcedSend;
    const uint32_t remaining = kMaxDeferralMs - elapsed(now);
    const uint32_t low = sense == Sense::RxActive ? 80UL : kMinBackoffMs;
    const uint32_t spread = sense == Sense::RxActive ? 160UL : (kMaxBackoffMs - kMinBackoffMs + 1UL);
    uint32_t delay = low + (randomness % spread);
    // Ensure a positive retry interval, bounded by the maximum wait.
    if (delay > remaining) delay = remaining;
    nextTryMs_ = now + delay;
    ++retries_;
    return Decision::Wait;
  }
};
} // namespace MacContention
