#pragma once
#include <Arduino.h>

// PHASE 3.3: Set GPIOs ONLY after physically checking the board pins.
// -1 disables a control. GPIO0 is reserved for OLED navigation.
// T-Beam Supreme exposes GPIO43/GPIO44 but they may also be UART signals.
// Never connect a buzzer directly to a GPIO without checking current draw;
// use an appropriate driver transistor for non-trivial loads.
#ifndef SOS_BUTTON_GPIO
#define SOS_BUTTON_GPIO 2
#endif
#ifndef SOS_LED_GPIO
#define SOS_LED_GPIO -1
#endif
#ifndef SOS_BUZZER_GPIO
#define SOS_BUZZER_GPIO -1
#endif

#if (SOS_BUTTON_GPIO == 0) || (SOS_LED_GPIO == 0) || (SOS_BUZZER_GPIO == 0)
#error "GPIO0 is dedicated to OLED navigation"
#endif
#if (SOS_BUTTON_GPIO >= 0 && SOS_BUTTON_GPIO == SOS_LED_GPIO) ||    \
    (SOS_BUTTON_GPIO >= 0 && SOS_BUTTON_GPIO == SOS_BUZZER_GPIO) || \
    (SOS_LED_GPIO >= 0 && SOS_LED_GPIO == SOS_BUZZER_GPIO)
#error "SOS controls must have distinct pins"
#endif

namespace SOSControls
{
  static constexpr unsigned long HOLD_MS = 1500UL;
  static constexpr unsigned long DEBOUNCE_MS = 45UL;
  static constexpr unsigned long COOLDOWN_MS = 3000UL;
  static bool stablePressed = false, lastRawPressed = false, firedInHold = false;
  static unsigned long changedAt = 0, pressStartedAt = 0, lastFiredAt = 0;
  static bool everFired = false, localAlert = false, remoteAlert = false;
  static unsigned long localAlertAt = 0, remoteAlertAt = 0;

  inline void begin()
  {
    if (SOS_BUTTON_GPIO >= 0)
      pinMode(SOS_BUTTON_GPIO, INPUT_PULLUP);
    if (SOS_LED_GPIO >= 0)
    {
      pinMode(SOS_LED_GPIO, OUTPUT);
      digitalWrite(SOS_LED_GPIO, LOW);
    }
    if (SOS_BUZZER_GPIO >= 0)
    {
      pinMode(SOS_BUZZER_GPIO, OUTPUT);
      digitalWrite(SOS_BUZZER_GPIO, LOW);
    }
    Serial.printf("[SOS-CONTROLS] button=%d LED=%d buzzer=%d (negative=disabled)\n", SOS_BUTTON_GPIO, SOS_LED_GPIO, SOS_BUZZER_GPIO);
  }
  inline void noteLocalSOS()
  {
    localAlert = true;
    localAlertAt = millis();
  }
  inline void noteReceivedSOS()
  {
    remoteAlert = true;
    remoteAlertAt = millis();
  }
  // Returns true exactly once for a deliberate, continuously held press.
  inline bool pollButton()
  {
    if (SOS_BUTTON_GPIO < 0)
      return false;
    unsigned long now = millis();
    bool rawPressed = (digitalRead(SOS_BUTTON_GPIO) == LOW);
    if (rawPressed != lastRawPressed)
    {
      lastRawPressed = rawPressed;
      changedAt = now;
    }
    if (now - changedAt < DEBOUNCE_MS)
      return false;
    if (stablePressed != rawPressed)
    {
      stablePressed = rawPressed;
      if (stablePressed)
      {
        pressStartedAt = now;
        firedInHold = false;
      }
    }
    if (stablePressed && !firedInHold && now - pressStartedAt >= HOLD_MS &&
        (!everFired || now - lastFiredAt >= COOLDOWN_MS))
    {
      firedInHold = true;
      everFired = true;
      lastFiredAt = now;
      return true;
    }
    return false;
  }
  inline void updateIndicators()
  {
    unsigned long now = millis();
    bool local = localAlert && (now - localAlertAt < 10000UL);
    bool remote = remoteAlert && (now - remoteAlertAt < 60000UL);
    bool active = local || remote;
    if (!active)
    {
      localAlert = false;
      remoteAlert = false;
    }
    // LED blinks 500ms on / 500ms off; buzzer 150ms every 2 seconds.
    if (SOS_LED_GPIO >= 0)
      digitalWrite(SOS_LED_GPIO, active && (now % 1000UL < 500UL) ? HIGH : LOW);
    if (SOS_BUZZER_GPIO >= 0)
      digitalWrite(SOS_BUZZER_GPIO, active && (now % 2000UL < 150UL) ? HIGH : LOW);
  }
}