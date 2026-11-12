#pragma once
#include <stdint.h>
#include <string.h>

// F08 Vessel-003 buzzer software preflight ONLY. No actual buzzer GPIO
// configuration or output API exists in this file. Keep BLDO2 disconnected.
#ifndef THESIS_V003_F08_BUZZER_PREFLIGHT
#define THESIS_V003_F08_BUZZER_PREFLIGHT 0
#endif
#ifndef THESIS_V003_F08_BUZZER_OUTPUTS_ENABLE
#define THESIS_V003_F08_BUZZER_OUTPUTS_ENABLE 0
#endif
#if THESIS_V003_F08_BUZZER_PREFLIGHT != 0 && THESIS_V003_F08_BUZZER_PREFLIGHT != 1
#error "Invalid Vessel-003 buzzer preflight selection"
#endif
#if THESIS_V003_F08_BUZZER_OUTPUTS_ENABLE != 0
#error "Vessel-003 buzzer preflight must never drive GPIO38"
#endif

namespace V003F08BuzzerBench {
static constexpr int kCandidateGpio = 38;  // Proposed transistor base drive, NOT assigned
static constexpr int kActualOutputPin = -1;
static constexpr bool kOutputsEnabled = false;
static constexpr uint32_t kMaximumVirtualPulseMs = 250U;

enum class Request : uint8_t { NotBuzzer, Help, Status, Off, Test, Invalid };

inline Request parse(const char *line) {
  if (!line || strncmp(line, "F08 BUZZER ", 11) != 0) return Request::NotBuzzer;
  const char *arg = line + 11;
  if (strcmp(arg, "HELP") == 0) return Request::Help;
  if (strcmp(arg, "STATUS") == 0) return Request::Status;
  if (strcmp(arg, "OFF") == 0) return Request::Off;
  if (strcmp(arg, "TEST") == 0) return Request::Test;
  return Request::Invalid;
}

inline const char *requestName(Request r) {
  switch (r) {
    case Request::NotBuzzer: return "NOT_BUZZER";
    case Request::Help: return "HELP";
    case Request::Status: return "STATUS";
    case Request::Off: return "OFF";
    case Request::Test: return "TEST";
    case Request::Invalid: return "INVALID";
  }
  return "INVALID";
}

class Controller {
 public:
  // VesselIdentity::load() must have completed before begin().
  void begin(bool vessel003) {
    authorized_ = vessel003;
    active_ = false;
    startedAtMs_ = 0;
  }

  bool authorized() const { return authorized_; }
  bool virtualActive() const { return active_; }

  // This changes software state ONLY; there is deliberately no apply()/GPIO.
  bool command(Request request, uint32_t nowMs, bool sosActive) {
    if (!authorized_) return false;
    if (request == Request::Off) {
      active_ = false;
      return true;
    }
    if (request != Request::Test) return false;
    if (sosActive) {
      active_ = false;
      return false;
    }
    startedAtMs_ = nowMs;
    active_ = true;
    return true;
  }

  void tick(uint32_t nowMs, bool sosActive) {
    if (!authorized_) return;
    if (sosActive ||
        (active_ && uint32_t(nowMs - startedAtMs_) >= kMaximumVirtualPulseMs)) {
      active_ = false;
    }
  }

 private:
  bool authorized_ = false;
  bool active_ = false;
  uint32_t startedAtMs_ = 0;
};

inline Controller &instance() {
  static Controller controller;
  return controller;
}
}  // namespace V003F08BuzzerBench
