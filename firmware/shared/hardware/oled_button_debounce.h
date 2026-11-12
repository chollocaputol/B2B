#pragma once
#include <stdint.h>

// Phase 5.2.2B-4: shared GPIO0 OLED-button debounce mechanism.
// Caller retains its own behavior on a stable press or release.
namespace OLEDButton {
inline bool stableEdge(int reading, int &lastReading, int &stableState,
                       unsigned long &lastChangeMs, unsigned long nowMs,
                       unsigned long debounceMs) {
  if (reading != lastReading) {
    lastChangeMs = nowMs;
    lastReading = reading;
  }
  if (nowMs - lastChangeMs >= debounceMs && reading != stableState) {
    stableState = reading;
    return true;
  }
  return false;
}
} // namespace OLEDButton
