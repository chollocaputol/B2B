#pragma once
#include <stdint.h>
#include <string.h>

// Dedicated Vessel-003 *bench* test. Simulated weather/SOS indication only;
// does not change sensor history, radio messages, SOS ownership or OLED demo.
#ifndef THESIS_V003_F08_COMBINED_PROFILE
#define THESIS_V003_F08_COMBINED_PROFILE 0
#endif
#ifndef THESIS_V003_F08_COMBINED_EXPLICIT_OPT_IN
#define THESIS_V003_F08_COMBINED_EXPLICIT_OPT_IN 0
#endif
#if (THESIS_V003_F08_COMBINED_PROFILE != 0 && THESIS_V003_F08_COMBINED_PROFILE != 1) || \
    (THESIS_V003_F08_COMBINED_EXPLICIT_OPT_IN != 0 && THESIS_V003_F08_COMBINED_EXPLICIT_OPT_IN != 1)
#error "Invalid F08 combined bench build gate"
#endif
#if THESIS_V003_F08_COMBINED_PROFILE == 1 && THESIS_V003_F08_COMBINED_EXPLICIT_OPT_IN != 1
#error "F08 combined GPIO outputs require explicit opt-in"
#endif
#if THESIS_V003_F08_COMBINED_PROFILE == 1
#include <Arduino.h>
#endif
#include "weather_indicator_policy.h"

namespace V003F08CombinedBench {
static constexpr int kYellowPin = 39;
static constexpr int kRedPin = 44;
static constexpr int kBuzzerPin = 38;
static constexpr uint32_t kDemoLimitMs = 6000U;
static constexpr bool kOutputsEnabled = THESIS_V003_F08_COMBINED_PROFILE == 1 &&
    THESIS_V003_F08_COMBINED_EXPLICIT_OPT_IN == 1;

enum class Request : uint8_t { NotIndicator, Help, Status, Off, Front, Squall, SOS, Invalid };

inline Request parse(const char* line) {
  if (!line || strncmp(line, "F08 IND ", 8) != 0) return Request::NotIndicator;
  const char* arg = line + 8;
  if (strcmp(arg, "HELP") == 0) return Request::Help;
  if (strcmp(arg, "STATUS") == 0) return Request::Status;
  if (strcmp(arg, "OFF") == 0) return Request::Off;
  if (strcmp(arg, "FRONT") == 0) return Request::Front;
  if (strcmp(arg, "SQUALL") == 0) return Request::Squall;
  if (strcmp(arg, "SOS") == 0) return Request::SOS;
  return Request::Invalid;
}

inline const char* requestName(Request r) {
  switch (r) {
    case Request::Help: return "HELP";
    case Request::Status: return "STATUS";
    case Request::Off: return "OFF";
    case Request::Front: return "FRONT";
    case Request::Squall: return "SQUALL";
    case Request::SOS: return "SOS_SIMULATED";
    case Request::Invalid: return "INVALID";
    case Request::NotIndicator: return "NOT_INDICATOR";
  }
  return "INVALID";
}

class Controller {
 public:
  // Called after PMU setup and VesselIdentity::load(). Wrong ID / rail OFF
  // must perform NO pinMode or digitalWrite calls.
  void begin(bool vessel003, bool bldo2Enabled) {
    authorized_ = vessel003 && bldo2Enabled;
    demo_ = Request::Off;
    startedAtMs_ = 0;
    policy_ = WeatherIndicators::Policy{};
    current_ = WeatherIndicators::Outputs{};
    if (!authorized_) return;
#if THESIS_V003_F08_COMBINED_PROFILE == 1
    // Configure each pin before digitalWrite (Arduino-ESP32 3.x).
    // The externally wired GPIO38 12k base pulldown holds buzzer OFF at boot.
    pinMode(kYellowPin, OUTPUT);
    digitalWrite(kYellowPin, LOW);
    pinMode(kRedPin, OUTPUT);
    digitalWrite(kRedPin, LOW);
    pinMode(kBuzzerPin, OUTPUT);
    digitalWrite(kBuzzerPin, LOW);
#endif
  }

  bool authorized() const { return authorized_; }
  Request demo() const { return demo_; }
  const WeatherIndicators::Outputs& outputs() const { return current_; }

  bool command(Request request, uint32_t nowMs, bool liveSOS) {
    if (!authorized_) return false;
    if (request == Request::Off) {
      demo_ = Request::Off;
      tick(nowMs, liveSOS, false);
      return true;
    }
    if ((request != Request::Front && request != Request::Squall && request != Request::SOS) || liveSOS)
      return false;
    if (demo_ != Request::Off) return false; // cannot prolong an active demo
    demo_ = request;
    startedAtMs_ = nowMs;
    tick(nowMs, false, false);
    return true;
  }

  // Real SOS always outranks simulated conditions and cancels any demo.
  // Caller supplies authoritative local and received SOS states.
  void tick(uint32_t nowMs, bool localSOS, bool receivedSOS) {
    if (!authorized_) return;
    const bool liveSOS = localSOS || receivedSOS;
    if (liveSOS || (demo_ != Request::Off &&
        uint32_t(nowMs - startedAtMs_) >= kDemoLimitMs)) {
      demo_ = Request::Off;
    }
    const bool simulateFront = demo_ == Request::Front;
    const bool simulateSquall = demo_ == Request::Squall;
    const bool simulateSOS = demo_ == Request::SOS;
    // Bench sensor hardware is OFF. The selected demo is passed ONLY to the
    // indicator policy; it is not written into the real weatherState.
    const uint8_t state = simulateFront ? WEATHER_FRONT :
                          simulateSquall ? WEATHER_SQUALL : WEATHER_CALIBRATING;
    current_ = policy_.update(nowMs, state, simulateFront || simulateSquall,
                              localSOS || simulateSOS, receivedSOS);
#if THESIS_V003_F08_COMBINED_PROFILE == 1
    digitalWrite(kYellowPin, current_.yellow ? HIGH : LOW);
    digitalWrite(kRedPin, current_.red ? HIGH : LOW);
    digitalWrite(kBuzzerPin, current_.buzzer ? HIGH : LOW);
#endif
  }

 private:
  bool authorized_ = false;
  Request demo_ = Request::Off;
  uint32_t startedAtMs_ = 0;
  WeatherIndicators::Policy policy_{};
  WeatherIndicators::Outputs current_{};
};

inline Controller& instance() {
  static Controller controller;
  return controller;
}
} // namespace V003F08CombinedBench
