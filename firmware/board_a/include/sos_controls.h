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
  // Use the emergency state AT PRESS START. One continuous hold can never
  // generate both a new SOS (1.5 s) and its cancellation (5 s).
  static constexpr unsigned long SOS_HOLD_MS = 1500UL;
  static constexpr unsigned long CANCEL_HOLD_MS = 5000UL;
  static constexpr unsigned long DEBOUNCE_MS = 45UL;
  static bool stablePressed = false, lastRawPressed = false, firedInHold = false;
  static bool cancelModeAtPress = false;
  static unsigned long changedAt = 0, pressStartedAt = 0;
  static bool localAlert = false, remoteAlert = false;
  static unsigned long localAlertAt = 0, remoteAlertAt = 0;

  enum class ButtonAction : uint8_t
  {
    None,
    StartSOS,
    CancelSOS
  };
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
    stablePressed = false;
    lastRawPressed = false;
    firedInHold = false;
    changedAt = millis();
    Serial.printf("[SOS-CONTROLS] button=%d LED=%d buzzer=%d\n", SOS_BUTTON_GPIO, SOS_LED_GPIO, SOS_BUZZER_GPIO);
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
  // Caller supplies its own active-emergency flag; captured only on debounced
  // press edge. Release is mandatory before any further action.
  inline ButtonAction pollAction(bool emergencyActive)
  {
    if (SOS_BUTTON_GPIO < 0)
      return ButtonAction::None;
    const unsigned long now = millis();
    const bool rawPressed = (digitalRead(SOS_BUTTON_GPIO) == LOW);
    if (rawPressed != lastRawPressed)
    {
      lastRawPressed = rawPressed;
      changedAt = now;
    }
    if (now - changedAt < DEBOUNCE_MS)
      return ButtonAction::None;
    if (stablePressed != rawPressed)
    {
      stablePressed = rawPressed;
      if (stablePressed)
      {
        pressStartedAt = now;
        cancelModeAtPress = emergencyActive;
        firedInHold = false;
        Serial.println(cancelModeAtPress ? "[SOS-BUTTON] Hold 5s to CANCEL local SOS" : "[SOS-BUTTON] Hold 1.5s to SEND SOS");
      }
    }
    if (stablePressed && !firedInHold &&
        now - pressStartedAt >= (cancelModeAtPress ? CANCEL_HOLD_MS : SOS_HOLD_MS))
    {
      firedInHold = true;
      return cancelModeAtPress ? ButtonAction::CancelSOS : ButtonAction::StartSOS;
    }
    return ButtonAction::None;
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