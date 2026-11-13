#pragma once
#include <stdint.h>
#include <string.h>
#include "weather_manager.h"

// F14 Vessel-003 supervised *local* probe. This module NEVER touches GPIO,
// sensors, LoRa, SD, emergency state, or stored macro/micro history.
// Its selected synthetic state is consumed ONLY by the F13 hardware adapter.
#ifndef THESIS_V003_F14_INDICATOR_PROBE
#define THESIS_V003_F14_INDICATOR_PROBE 0
#endif
#ifndef THESIS_V003_F14_EXPLICIT_OPT_IN
#define THESIS_V003_F14_EXPLICIT_OPT_IN 0
#endif
#if THESIS_V003_F14_INDICATOR_PROBE != 0 && THESIS_V003_F14_INDICATOR_PROBE != 1
#error "Invalid F14 indicator probe flag"
#endif
#if THESIS_V003_F14_EXPLICIT_OPT_IN != 0 && THESIS_V003_F14_EXPLICIT_OPT_IN != 1
#error "Invalid F14 explicit opt-in flag"
#endif
#if THESIS_V003_F14_INDICATOR_PROBE == 1 && \
    (THESIS_V003_F14_EXPLICIT_OPT_IN != 1 || THESIS_V003_F13_SUPERVISED_DRIVER != 1 || \
     THESIS_V003_F13_LIVE_OUTPUTS != 1 || THESIS_V003_F13_EXPLICIT_OPT_IN != 1 || \
     THESIS_V003_F13_HARDWARE_REVIEWED != 1 || THESIS_V003_F12_INDICATOR_PREVIEW != 1 || \
     THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1 || THESIS_V003_F10_MICRO_NOWCAST != 1 || \
     THESIS_V003_F09_SHARED_SENSOR_BUS != 1 || WEATHER_FAST_TEST != 0 || \
     THESIS_SD_LOGGER != 0 || THESIS_LAB_RELAY_TX != 0)
#error "F14 requires F13 live Vessel-003 opt-in; real timing, SD/relay OFF"
#endif

namespace V003F14Probe {
static constexpr uint32_t kMaxDemoMs = 6000U;
enum class Command : uint8_t { Invalid, Help, Status, Off, Front, Squall };
inline Command parse(const char* line) {
  if (!line || strncmp(line, "F14 IND ", 8) != 0) return Command::Invalid;
  const char* cmd = line + 8;
  if (strcmp(cmd, "HELP") == 0) return Command::Help;
  if (strcmp(cmd, "STATUS") == 0) return Command::Status;
  if (strcmp(cmd, "OFF") == 0) return Command::Off;
  if (strcmp(cmd, "FRONT") == 0) return Command::Front;
  if (strcmp(cmd, "SQUALL") == 0) return Command::Squall;
  return Command::Invalid;
}
inline const char* name(Command c) {
  switch(c) {
    case Command::Help: return "HELP";
    case Command::Status: return "STATUS";
    case Command::Off: return "OFF";
    case Command::Front: return "FRONT";
    case Command::Squall: return "SQUALL";
    case Command::Invalid: break;
  }
  return "INVALID";
}
inline bool actualWarning(uint8_t state, bool valid) {
  return valid && (state == WEATHER_FRONT || state == WEATHER_SQUALL);
}
struct Selected {
  uint8_t state = WEATHER_CALIBRATING;
  bool valid = false;
  bool synthetic = false;
};
class Controller {
public:
  void begin(bool authorizedV003Live) {
    authorized_ = authorizedV003Live;
    active_ = Command::Off;
    beganMs_ = 0;
  }
  bool authorized() const { return authorized_; }
  bool active() const { return active_ == Command::Front || active_ == Command::Squall; }
  Command mode() const { return active_; }
  bool request(Command c, uint32_t now, bool realSOS, bool railGood, bool actualAlert) {
    if (!authorized_) return false;
    if (c == Command::Off) { active_ = Command::Off; return true; }
    if (c != Command::Front && c != Command::Squall) return false;
    // No re-arm/extension while a demo is active. SOS and a real weather
    // warning must NEVER be masked by operator simulation.
    if (active() || realSOS || !railGood || actualAlert) return false;
    active_ = c;
    beganMs_ = now;
    return true;
  }
  Selected select(uint32_t now, bool realSOS, bool railGood,
                  uint8_t actualState, bool actualValid) {
    Selected result{actualState, actualValid, false};
    if (!authorized_) return result;
    if (realSOS || !railGood || actualWarning(actualState, actualValid) ||
        uint32_t(now - beganMs_) >= kMaxDemoMs) {
      active_ = Command::Off;
    }
    if (active()) {
      result.state = active_ == Command::Front ? WEATHER_FRONT : WEATHER_SQUALL;
      result.valid = true;
      result.synthetic = true;
    }
    return result;
  }
private:
  bool authorized_ = false;
  Command active_ = Command::Off;
  uint32_t beganMs_ = 0;
};
}  // namespace V003F14Probe
