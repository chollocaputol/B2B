#pragma once
#include <stdint.h>
// C1: numeric-ID-aware control retry slots for the universal vessel firmware.
// Scheduling reduces repeated half-duplex collisions; it does not provide
// guaranteed channel access or reliable emergency delivery.
namespace ControlRetry {
  static constexpr uint32_t kRetryMs = 4500UL;

  // Four coarse first-attempt slots. Actual provisioned vessel IDs are 1..9999;
  // the historical 0xA1-versus-everyone-else split gave IDs 1 and 3 the same
  // base slot. Use numeric identity, with short event-specific desynchronization.
  static inline uint32_t firstSlot(uint32_t nodeId) {
    return (nodeId * 3UL + nodeId / 4UL) % 4UL;
  }

  // Retry offsets are intentionally small: the first-attempt phase separation
  // should persist across bounded retries rather than rotating into collision.
  static inline uint32_t retryJitterMs(uint32_t nodeId, uint32_t eventId,
                                       uint8_t attemptIndex) {
    return (nodeId * 31UL + eventId * 17UL +
            uint32_t(attemptIndex) * 53UL + nodeId / 4UL) % 181UL;
  }

  static inline uint32_t slotDelayMs(uint32_t nodeId, uint32_t eventId,
                                     uint8_t attemptIndex) {
    if (attemptIndex == 0)
      return 300UL + firstSlot(nodeId) * 980UL +
             (eventId * 17UL % 5UL) * 37UL;
    return 140UL + retryJitterMs(nodeId, eventId, attemptIndex);
  }

  static inline uint32_t sosSlotDelayMs(uint32_t nodeId, uint32_t eventId,
                                        uint8_t attemptIndex) {
    if (attemptIndex == 0)
      return 350UL + firstSlot(nodeId) * 980UL +
             (eventId * 13UL % 5UL) * 37UL;
    return 150UL + retryJitterMs(nodeId, eventId, attemptIndex);
  }

  static inline bool due(uint32_t now, uint32_t deadline) {
    return int32_t(now - deadline) >= 0;
  }
}
