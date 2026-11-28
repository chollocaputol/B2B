#pragma once
// Vessel HOME headers are display-only: never change the provisioned identity.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

namespace FleetHomeTitle {
inline bool valid(uint32_t id, bool ready) {
  return ready && id >= 1U && id <= 9999U;
}
inline void shortTitle(uint32_t id, bool ready, char* out, size_t capacity) {
  if (!out || capacity == 0U) return;
  if (!valid(id, ready)) { snprintf(out, capacity, "NO ID"); return; }
  snprintf(out, capacity, "V%03lu", static_cast<unsigned long>(id));
}
inline void landscapeTitle(uint32_t id, bool ready, char* out, size_t capacity) {
  if (!out || capacity == 0U) return;
  char vessel[8]; shortTitle(id, ready, vessel, sizeof(vessel));
  snprintf(out, capacity, "%s HOME", vessel);
}
inline bool portraitBatteryIconFits(uint32_t id, bool ready) {
  // Portrait 64 px status bar: four-character V001..V999 title leaves
  // x26..36 for battery glyph and x38..62 for the percentage text.
  // Longer titles must hide only the glyph, not the battery percentage.
  return valid(id, ready) && id <= 999U;
}
}
