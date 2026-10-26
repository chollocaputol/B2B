#pragma once
#include <stdint.h>
// Phase 3.5.4 preliminary shared cancellation scheduler.
// Stagger node slots to reduce repeated simultaneous TX collisions.
// Timings are experimental, not a guarantee of channel access.
namespace ControlRetry
{
    static constexpr uint32_t kRetryMs = 4500UL;
    static inline uint32_t slotDelayMs(uint32_t nodeId, uint32_t eventId, uint8_t attemptIndex)
    {
        const uint32_t nodeSlot = (nodeId == 0xA1UL) ? 100UL : 650UL;
        const uint32_t variation = ((eventId * 17UL + (uint32_t)attemptIndex * 29UL) % 5UL) * 37UL;
        return nodeSlot + variation;
    }
    // Distinct SOS windows; experimental phase offsets in milliseconds.
    // This does NOT provide guaranteed collision avoidance or MAC coordination.
    static inline uint32_t sosSlotDelayMs(uint32_t nodeId, uint32_t eventId, uint8_t attemptIndex)
    {
        const uint32_t base = (nodeId == 0xA1UL) ? 250UL : 1500UL;
        const uint32_t variation = ((eventId * 13UL + (uint32_t)attemptIndex * 37UL) % 7UL) * 43UL;
        return base + variation;
    }
    static inline bool due(uint32_t now, uint32_t deadline)
    {
        return (int32_t)(now - deadline) >= 0;
    }
}