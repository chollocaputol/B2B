#pragma once
#include <stdint.h>
#include "weather_manager.h"

// No GPIO or radio/storage operations. Inputs are live state, never OLED demos.
namespace WeatherIndicators {
enum class Mode : uint8_t { Off, Front, Squall, SOS };
struct Outputs {
  bool yellow = false;
  bool red = false;
  bool buzzer = false; // envelope for a reviewed driver, not a passive-buzzer tone
};

class Policy {
 public:
  Outputs update(uint32_t now, uint8_t weather, bool inputsValid,
                 bool localSOS, bool receivedSOS) {
    Mode next = Mode::Off;
    if (localSOS || receivedSOS) next = Mode::SOS;
    else if (inputsValid && weather == WEATHER_FRONT) next = Mode::Front;
    else if (inputsValid && weather == WEATHER_SQUALL) next = Mode::Squall;
    if (next != mode_) {
      mode_ = next;
      startedAt_ = now;
    }
    const uint32_t elapsed = now - startedAt_;
    Outputs out;
    // Cadences are implementation choices, not manuscript weather thresholds.
    switch (mode_) {
      case Mode::Front: {
        const uint32_t phase = elapsed % 2000U;
        out.yellow = phase < 150U || (phase >= 300U && phase < 450U);
        break;
      }
      case Mode::Squall:
        out.red = elapsed % 1000U < 500U;
        out.buzzer = out.red;
        break;
      case Mode::SOS: {
        out.red = elapsed % 200U < 100U;
        const uint32_t phase = elapsed % 2000U;
        out.buzzer = phase < 600U && phase % 200U < 100U;
        break;
      }
      case Mode::Off: break;
    }
    return out;
  }
  Mode mode() const { return mode_; }
 private:
  Mode mode_ = Mode::Off;
  uint32_t startedAt_ = 0;
};
} // namespace WeatherIndicators
