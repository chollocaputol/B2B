#pragma once
// Pure C++ F06 development-only SD recording policy. Does not include Arduino,
// talk to SD, alter radio traffic, or touch SOS state.
#include <stdint.h>

namespace SDHybrid {
static constexpr uint32_t kTailMs = 600000UL; // 10 minutes after emergency ends
struct State {
    bool continuous = false;      // safe startup: SOS ARMED, no routine telemetry
    bool activeSOS = false;       // current local/remote unresolved emergency
    bool urgentSeen = false;      // tail timer valid
    uint32_t lastUrgentMs = 0;
    uint32_t skippedRoutine = 0;  // intentional skips, never queue drops

    void noteUrgent(uint32_t nowMs) {
        urgentSeen = true;
        lastUrgentMs = nowMs;
    }
    void observeSOS(bool active, uint32_t nowMs) {
        if (active != activeSOS) {
            activeSOS = active;
            // Rising edge starts capture; falling edge starts post-SOS tail.
            noteUrgent(nowMs);
        }
    }
    bool sosOverride(uint32_t nowMs) const {
        return activeSOS ||
               (urgentSeen && uint32_t(nowMs - lastUrgentMs) < kTailMs);
    }
    bool routineEnabled(uint32_t nowMs) const {
        return continuous || sosOverride(nowMs);
    }
    bool shouldRecord(bool urgent, uint32_t nowMs) {
        if (urgent) {
            noteUrgent(nowMs);
            return true; // SOS, ACK, cancel and urgent diagnostics always protected
        }
        if (routineEnabled(nowMs)) return true;
        ++skippedRoutine;
        return false;
    }
};
} // namespace SDHybrid
