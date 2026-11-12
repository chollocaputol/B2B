#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <string.h>

// Vessel-003 LED-only *manual* test; not the production F08 indicator driver.
// Only the explicitly opted-in V003 live test build configures GPIO39/44.
// The buzzer and production F08 physical outputs remain permanently disabled.
// User-confirmed LED wiring is not an instrument-based electrical certification.
#ifndef THESIS_V003_F08_LED_BENCH
#define THESIS_V003_F08_LED_BENCH 0
#endif
#ifndef THESIS_V003_F08_LED_OUTPUTS_ENABLE
#define THESIS_V003_F08_LED_OUTPUTS_ENABLE 0
#endif
#ifndef THESIS_V003_F08_LED_HARDWARE_REVIEWED
#define THESIS_V003_F08_LED_HARDWARE_REVIEWED 0
#endif
#ifndef THESIS_V003_F08_LED_LIVE_PROFILE
#define THESIS_V003_F08_LED_LIVE_PROFILE 0
#endif
#if THESIS_V003_F08_LED_LIVE_PROFILE != 0 && THESIS_V003_F08_LED_LIVE_PROFILE != 1
#error "Invalid Vessel-003 live LED profile flag"
#endif
#if (THESIS_V003_F08_LED_OUTPUTS_ENABLE != 0 && THESIS_V003_F08_LED_OUTPUTS_ENABLE != 1) || \
    (THESIS_V003_F08_LED_HARDWARE_REVIEWED != 0 && THESIS_V003_F08_LED_HARDWARE_REVIEWED != 1)
#error "Invalid Vessel-003 LED test gate"
#endif
#if THESIS_V003_F08_LED_OUTPUTS_ENABLE == 1 && \
    (THESIS_V003_F08_LED_HARDWARE_REVIEWED != 1 || THESIS_V003_F08_LED_BENCH != 1 || \
     THESIS_V003_F08_INDICATOR_BENCH != 1 || THESIS_V003_F08_LED_LIVE_PROFILE != 1)
#error "LED output requires separately reviewed Vessel-003-only electrical authorization"
#endif

namespace V003F08LedBench {
static constexpr int kYellowPin = 39;
static constexpr int kRedPin = 44;
static constexpr int kBuzzerPin = -1;  // NEVER driven by this LED-only adapter.
static constexpr uint32_t kMaximumOnMs = 500U;
static constexpr bool kOutputsEnabled = THESIS_V003_F08_LED_OUTPUTS_ENABLE == 1;

enum class Request : uint8_t { NotLed, Help, Status, Off, Yellow, Red, Invalid };

inline Request parse(const char *line) {
  if (!line || strncmp(line, "F08 LED ", 8) != 0) return Request::NotLed;
  const char *arg = line + 8;
  if (strcmp(arg, "HELP") == 0) return Request::Help;
  if (strcmp(arg, "STATUS") == 0) return Request::Status;
  if (strcmp(arg, "OFF") == 0) return Request::Off;
  if (strcmp(arg, "YELLOW") == 0) return Request::Yellow;
  if (strcmp(arg, "RED") == 0) return Request::Red;
  return Request::Invalid;
}

inline const char *requestName(Request r) {
  switch (r) {
    case Request::NotLed: return "NOT_LED";
    case Request::Help: return "HELP";
    case Request::Status: return "STATUS";
    case Request::Off: return "OFF";
    case Request::Yellow: return "YELLOW";
    case Request::Red: return "RED";
    case Request::Invalid: return "INVALID";
  }
  return "INVALID";
}

class Controller {
 public:
  // Called once *after* VesselIdentity::load(). The caller must pass true
  // only for node ID 3; otherwise no GPIO configuration or writes occur.
  void begin(bool vessel003) {
    selected_ = Request::Off;
    authorized_ = vessel003;
#if THESIS_V003_F08_LED_OUTPUTS_ENABLE == 1
    if (!authorized_) return;
    // LOW before output mode; boot-ROM states are outside firmware control.
    digitalWrite(kYellowPin, LOW);
    digitalWrite(kRedPin, LOW);
    pinMode(kYellowPin, OUTPUT);
    pinMode(kRedPin, OUTPUT);
#endif
  }

  bool authorized() const { return authorized_; }

  // Only one LED may be selected. Live local/received SOS always preempts.
  bool command(Request r, uint32_t nowMs, bool sosActive) {
    if (!authorized_) return false;
    if (r == Request::Off) {
      selected_ = Request::Off;
      apply();
      return true;
    }
    if (r != Request::Yellow && r != Request::Red) return false;
    if (sosActive) {
      selected_ = Request::Off;
      apply();
      return false;
    }
    selected_ = r;
    startedAtMs_ = nowMs;
    apply();
    return true;
  }

  void tick(uint32_t nowMs, bool sosActive) {
    if (!authorized_) return;
    if (sosActive ||
        (selected_ != Request::Off && uint32_t(nowMs - startedAtMs_) >= kMaximumOnMs)) {
      selected_ = Request::Off;
    }
    apply();
  }

  Request selected() const { return selected_; }
  bool yellow() const { return selected_ == Request::Yellow; }
  bool red() const { return selected_ == Request::Red; }

 private:
  void apply() {
#if THESIS_V003_F08_LED_OUTPUTS_ENABLE == 1
    if (!authorized_) return;
    digitalWrite(kYellowPin, yellow() ? HIGH : LOW);
    digitalWrite(kRedPin, red() ? HIGH : LOW);
#endif
  }

  Request selected_ = Request::Off;
  bool authorized_ = false;
  uint32_t startedAtMs_ = 0;
};

// Inline function's local static is one shared instance across translation units.
inline Controller &instance() {
  static Controller controller;
  return controller;
}
}  // namespace V003F08LedBench
