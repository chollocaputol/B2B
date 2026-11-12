#pragma once
#include <Arduino.h>
#include "weather_indicator_policy.h"

// One output owner. Legacy timed SOS indicators must remain unassigned.
#if (defined(SOS_LED_GPIO) && SOS_LED_GPIO >= 0) || \
    (defined(SOS_BUZZER_GPIO) && SOS_BUZZER_GPIO >= 0)
#error "Leave legacy SOS output pins disabled; unified indicators own the outputs"
#endif

namespace PhysicalIndicators {
// STOP: board revision, wiring, polarity and buzzer driver are not verified.
// Change these only after hardware review. No pins are selected by this patch.
static constexpr bool kOutputsEnabled = false;
static constexpr bool kWiringAndDriverReviewed = false;
static constexpr int kYellowPin = -1;
static constexpr int kRedPin = -1;
static constexpr int kBuzzerDriverPin = -1;

// Reserve current firmware/onboard functions, USB, and prohibited strap pins.
constexpr bool unreservedPin(int pin) {
  return pin >= 0 && pin <= 48 &&
      !(pin <= 14 || pin == 17 || pin == 18 || pin == 19 || pin == 20 ||
        pin == 21 || (pin >= 22 && pin <= 42) || pin == 45 || pin == 46 ||
        pin == 47 || pin == 48);
}
static_assert(!kOutputsEnabled || kWiringAndDriverReviewed,
              "Review actual board wiring, polarity and buzzer driver first");
static_assert(!kOutputsEnabled ||
              (unreservedPin(kYellowPin) && unreservedPin(kRedPin) &&
               unreservedPin(kBuzzerDriverPin) && kYellowPin != kRedPin &&
               kYellowPin != kBuzzerDriverPin && kRedPin != kBuzzerDriverPin),
              "Indicators require three distinct reviewed, unreserved outputs");

static WeatherIndicators::Policy policy;
inline void begin() {
  if constexpr (kOutputsEnabled) {
    // Active-HIGH interface; reviewed external pull-downs keep drivers OFF at boot.
    digitalWrite(kYellowPin, LOW);
    digitalWrite(kRedPin, LOW);
    digitalWrite(kBuzzerDriverPin, LOW);
    pinMode(kYellowPin, OUTPUT);
    pinMode(kRedPin, OUTPUT);
    pinMode(kBuzzerDriverPin, OUTPUT);
  }
}
inline void update(uint32_t now, uint8_t weather, bool inputsValid,
                   bool localSOS, bool receivedSOS) {
  const auto out = policy.update(now, weather, inputsValid, localSOS, receivedSOS);
  if constexpr (kOutputsEnabled) {
    digitalWrite(kYellowPin, out.yellow ? HIGH : LOW);
    digitalWrite(kRedPin, out.red ? HIGH : LOW);
    digitalWrite(kBuzzerDriverPin, out.buzzer ? HIGH : LOW);
  }
}
} // namespace PhysicalIndicators
