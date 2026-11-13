#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "weather_indicator_policy.h"

// F13, Vessel-003-only supervised physical weather/SOS adapter.
// Existing production PhysicalIndicators remains DISABLED. F12 micro watch
// remains VIRTUAL, never automatically drives the physical buzzer or LEDs.
// A separate LIVE build profile is required to make any pinMode/GPIO writes.
#ifndef THESIS_V003_F13_SUPERVISED_DRIVER
#define THESIS_V003_F13_SUPERVISED_DRIVER 0
#endif
#ifndef THESIS_V003_F13_LIVE_OUTPUTS
#define THESIS_V003_F13_LIVE_OUTPUTS 0
#endif
#ifndef THESIS_V003_F13_EXPLICIT_OPT_IN
#define THESIS_V003_F13_EXPLICIT_OPT_IN 0
#endif
#ifndef THESIS_V003_F13_HARDWARE_REVIEWED
#define THESIS_V003_F13_HARDWARE_REVIEWED 0
#endif
#if (THESIS_V003_F13_SUPERVISED_DRIVER != 0 && THESIS_V003_F13_SUPERVISED_DRIVER != 1) || \
    (THESIS_V003_F13_LIVE_OUTPUTS != 0 && THESIS_V003_F13_LIVE_OUTPUTS != 1) || \
    (THESIS_V003_F13_EXPLICIT_OPT_IN != 0 && THESIS_V003_F13_EXPLICIT_OPT_IN != 1) || \
    (THESIS_V003_F13_HARDWARE_REVIEWED != 0 && THESIS_V003_F13_HARDWARE_REVIEWED != 1)
#error "Invalid F13 indicator driver flag"
#endif
#if THESIS_V003_F13_SUPERVISED_DRIVER == 1 && \
    (THESIS_V003_F12_INDICATOR_PREVIEW != 1 || THESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN != 1 || \
     THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1 || THESIS_V003_F10_MICRO_NOWCAST != 1 || \
     THESIS_V003_F09_SHARED_SENSOR_BUS != 1 || WEATHER_FAST_TEST != 0)
#error "F13 requires real-time F09/F10/F11/F12 preview profile"
#endif
#if THESIS_V003_F13_LIVE_OUTPUTS == 1 && \
    (THESIS_V003_F13_SUPERVISED_DRIVER != 1 || THESIS_V003_F13_EXPLICIT_OPT_IN != 1 || \
     THESIS_V003_F13_HARDWARE_REVIEWED != 1)
#error "F13 physical GPIO needs explicit opt-in and reviewed wiring"
#endif

namespace V003F13Indicators {
static constexpr int kYellowPin = 39;
static constexpr int kRedPin = 44;
static constexpr int kBuzzerDriverPin = 38;
static constexpr bool kPhysicalOutputsEnabled = THESIS_V003_F13_SUPERVISED_DRIVER == 1 &&
    THESIS_V003_F13_LIVE_OUTPUTS == 1 && THESIS_V003_F13_EXPLICIT_OPT_IN == 1 &&
    THESIS_V003_F13_HARDWARE_REVIEWED == 1;
static_assert(kYellowPin == 39 && kRedPin == 44 && kBuzzerDriverPin == 38,
              "Vessel-003 F08-reviewed pins must remain unchanged");

class Controller {
 public:
  // PMU's isEnableBLDO2() is the rail *enable bit*, NOT measured voltage.
  // Caller must check persistent ID 3 and F09 sensor authorization first.
  void begin(bool vessel003Authorized, bool pmuBldo2EnableBit) {
    authorized_ = vessel003Authorized && pmuBldo2EnableBit;
    railLost_ = false;
    previous_ = WeatherIndicators::Outputs{};
    applied_ = WeatherIndicators::Outputs{};
    policy_ = WeatherIndicators::Policy{};
    if (!authorized_) return; // wrong ID / disabled rail -> zero GPIO calls
    if constexpr (kPhysicalOutputsEnabled) {
      // Dedicated transistor pulldown keeps buzzer OFF before GPIO setup.
      // Configure outputs before writing LOW (Arduino-ESP32 3.x).
      pinMode(kYellowPin, OUTPUT); digitalWrite(kYellowPin, LOW);
      pinMode(kRedPin, OUTPUT); digitalWrite(kRedPin, LOW);
      pinMode(kBuzzerDriverPin, OUTPUT); digitalWrite(kBuzzerDriverPin, LOW);
    }
  }

  void tick(uint32_t now, uint8_t macroState, bool macroInputsValid,
            bool localSOS, bool receivedSOS, bool pmuBldo2EnableBit) {
    if (!authorized_ || railLost_) return;
    if (!pmuBldo2EnableBit) {
      // Fail closed, latched until reboot: a dropped PMU enable state
      // must not automatically rearm outputs when the rail returns.
      railLost_ = true;
      previous_ = WeatherIndicators::Outputs{};
      policy_ = WeatherIndicators::Policy{};
      if constexpr (kPhysicalOutputsEnabled) {
        digitalWrite(kYellowPin, LOW);
        digitalWrite(kRedPin, LOW);
        digitalWrite(kBuzzerDriverPin, LOW);
      }
      return;
    }
    // Production weather policy: macro valid FRONT/SQUALL and *real* SOS only.
    // F10 experimental risk never reaches this hardware-owning function.
    previous_ = policy_.update(now, macroState, macroInputsValid,
                               localSOS, receivedSOS);
    if constexpr (kPhysicalOutputsEnabled) {
      // Write only transitions: no continuous GPIO bus churn in the fast loop.
      if (previous_.yellow != applied_.yellow)
        digitalWrite(kYellowPin, previous_.yellow ? HIGH : LOW);
      if (previous_.red != applied_.red)
        digitalWrite(kRedPin, previous_.red ? HIGH : LOW);
      if (previous_.buzzer != applied_.buzzer)
        digitalWrite(kBuzzerDriverPin, previous_.buzzer ? HIGH : LOW);
      applied_ = previous_;
    }
  }

  bool authorized() const { return authorized_; }
  bool railLost() const { return railLost_; }
  WeatherIndicators::Mode mode() const { return policy_.mode(); }
  WeatherIndicators::Outputs outputs() const { return previous_; }
 private:
  bool authorized_ = false;
  bool railLost_ = false;
  WeatherIndicators::Policy policy_{};
  WeatherIndicators::Outputs previous_{};
  WeatherIndicators::Outputs applied_{};
};
} // namespace V003F13Indicators
