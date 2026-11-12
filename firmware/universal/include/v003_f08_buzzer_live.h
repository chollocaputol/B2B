#pragma once
#include <stdint.h>
#include <string.h>

// Dedicated Vessel-003 bench buzzer ACTUATION profile (not field firmware).
// Only use after verifying B-LDO2, transistor wiring, and SFM-20B current.
#ifndef THESIS_V003_F08_BUZZER_LIVE_PROFILE
#define THESIS_V003_F08_BUZZER_LIVE_PROFILE 0
#endif
#ifndef THESIS_V003_F08_BUZZER_LIVE_EXPLICIT_OPT_IN
#define THESIS_V003_F08_BUZZER_LIVE_EXPLICIT_OPT_IN 0
#endif
#if THESIS_V003_F08_BUZZER_LIVE_PROFILE != 0 && THESIS_V003_F08_BUZZER_LIVE_PROFILE != 1
#error "Invalid buzzer-live profile flag"
#endif
#if THESIS_V003_F08_BUZZER_LIVE_EXPLICIT_OPT_IN != 0 && THESIS_V003_F08_BUZZER_LIVE_EXPLICIT_OPT_IN != 1
#error "Invalid buzzer live opt-in flag"
#endif
#if THESIS_V003_F08_BUZZER_LIVE_PROFILE == 1 && THESIS_V003_F08_BUZZER_LIVE_EXPLICIT_OPT_IN != 1
#error "Buzzer live output requires explicit actuation opt-in"
#endif
#if THESIS_V003_F08_BUZZER_LIVE_PROFILE == 1
#include <Arduino.h>
#endif

namespace V003F08BuzzerLive {
static constexpr int kBuzzerPin = 38;
static constexpr uint32_t kPulseMs = 250U;
static constexpr bool kOutputsEnabled = THESIS_V003_F08_BUZZER_LIVE_PROFILE == 1 &&
                                        THESIS_V003_F08_BUZZER_LIVE_EXPLICIT_OPT_IN == 1;
enum class Request : uint8_t { NotBuzzer, Help, Status, Off, Test, Invalid };

inline Request parse(const char* line) {
  if (!line || strncmp(line, "F08 BUZZER ", 11) != 0) return Request::NotBuzzer;
  const char* arg = line + 11;
  if (strcmp(arg, "TEST") == 0) return Request::Test;
  if (strcmp(arg, "OFF") == 0) return Request::Off;
  if (strcmp(arg, "STATUS") == 0) return Request::Status;
  if (strcmp(arg, "HELP") == 0) return Request::Help;
  return Request::Invalid;
}

inline const char* requestName(Request req) {
  switch (req) {
    case Request::Test: return "TEST";
    case Request::Off: return "OFF";
    case Request::Status: return "STATUS";
    case Request::Help: return "HELP";
    case Request::NotBuzzer: return "NOT_BUZZER";
    case Request::Invalid: return "INVALID";
  }
  return "INVALID";
}

class Controller {
 public:
  // Call ONLY after PMU setup and VesselIdentity::load(). Startup output LOW.
  // Neither an incorrect identity nor a disabled PMU rail may touch GPIO38.
  void begin(bool vessel003, bool pmuRailEnabled) {
    authorized_ = vessel003 && pmuRailEnabled;
    active_ = false;
    startMs_ = 0;
    if (authorized_) {
#if THESIS_V003_F08_BUZZER_LIVE_PROFILE == 1
      // Arduino-ESP32 3.x requires GPIO mode before digitalWrite().
      // External 12k base pulldown holds the transistor OFF before setup.
      pinMode(kBuzzerPin, OUTPUT);
      digitalWrite(kBuzzerPin, LOW);
#endif
    }
  }

  bool authorized() const { return authorized_; }
  bool active() const { return active_; }

  // Reject repeated TEST while active, preventing infinite extension by spam.
  bool command(Request req, uint32_t nowMs, bool sosActive) {
    if (!authorized_) return false;
    if (req == Request::Off) {
      active_ = false;
      apply();
      return true;
    }
    if (req != Request::Test || sosActive || active_) return false;
    active_ = true;
    startMs_ = nowMs;
    apply();
    return true;
  }

  void tick(uint32_t nowMs, bool sosActive) {
    if (!authorized_ || !active_) return;
    if (sosActive || uint32_t(nowMs - startMs_) >= kPulseMs) {
      active_ = false;
      apply();
    }
  }

 private:
  void apply() {
#if THESIS_V003_F08_BUZZER_LIVE_PROFILE == 1
    if (authorized_) digitalWrite(kBuzzerPin, active_ ? HIGH : LOW);
#endif
  }
  bool authorized_ = false;
  bool active_ = false;
  uint32_t startMs_ = 0;
};

inline Controller& instance() {
  static Controller controller;
  return controller;
}
}  // namespace V003F08BuzzerLive
