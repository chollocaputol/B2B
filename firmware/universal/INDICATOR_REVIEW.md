# Physical indicator review ? outputs disabled

Baseline: 961d2a2, F07 checkpoint. No weather/radio/storage changes are required.

## Requirements provenance

Manuscript references and approved behavior were supplied and confirmed by the
user on 2026-11-10. The original manuscript file was not independently inspected.

- SR2, Chapter 3: manual SOS and critical emergencies require LED strobe and active
  piezo buzzer feedback, independent of routine telemetry.
- Section 4.8.10, pp. 79-80: Immediate Squall activates red LED and buzzer;
  Approaching Front activates yellow LED; Normal Weather is the default classification.
- Section 4.14.2: bench tests verify buzzer and lights against simulated emergencies.

The approved Task 3 behavior list defines SOS priority, return to current valid
weather after SOS clears, silent calibration/normal/invalid states, and OLED demo
isolation. Exact cadences below are implementation choices, not manuscript thresholds.
The existing 100-minute calibration gating and 10-minute sampling remain unchanged.

CRITICAL WIRING CONFLICT: Section 4.8.3 uses older GPIO assignments that differ
from the tested firmware. Do not apply that diagram to this implementation. Preserve
DHT20 SDA38/SCL39, BME680 SDA21/SCL48, SOS button GPIO2, OLED button GPIO0,
and disabled legacy SOS LED/buzzer (-1). The board is a LilyGO T-Beam Supreme
ESP32-S3; its exact PCB revision remains unconfirmed. No hardware-specific pin
assignment or output activation is authorized by this software-only patch.

| Live condition | Output policy |
| --- | --- |
| Calibrating, normal, invalid sensors, unknown state | Both LEDs and buzzer OFF |
| Approaching front with valid inputs | Yellow double blink; red/buzzer OFF |
| Immediate squall with valid inputs | Red flashes; warning buzzer |
| Any active local or received SOS | Red rapid strobe; distinct buzzer, highest priority |
| All SOS cleared | Re-evaluate current live valid weather; no alert timeout |

Timing implementation choices: front = 150 ms ON / 150 ms OFF / 150 ms ON,
repeated every 2000 ms; squall = 500 ms ON / 500 ms OFF for red and buzzer;
SOS red = 100 ms ON / 100 ms OFF, buzzer = three 100 ms chirps separated by
100 ms gaps, repeated every 2000 ms. Patterns start on mode entry. No delays,
loops waiting for time, tone generator, green GPIO, demo input, or new weather
classification logic. Buzzer values describe an ON/OFF driver envelope only.

Local SOS comes from hasActiveLocalSOS(), including F05 recovered cancellation
requiring deliberate hold. Received SOS comes from getActiveSOSCount(), whose
existing table retains uncancelled events without timeout. A mismatched CANCEL
or clearing only one of several events cannot silence the remaining SOS.
No transmission, acknowledgement, retry, cancellation, or table semantics change.
The old time-limited SOSControls::updateIndicators() call is replaced, and its
legacy LED/buzzer pins must stay disabled. Button handling stays intact.

## GPIO ownership audit

| Owner | Current GPIOs |
| --- | --- |
| OLED button / SOS button | 0 / 2 |
| SX1262 | 1, 4, 5, 10, 11, 12, 13 |
| GNSS | 6, 7, 8, 9 |
| OLED I2C | 17, 18 |
| BME680 / DHT20 external I2C | 21, 48 / 38, 39 |
| SD SPI / SD CS / IMU CS held HIGH | 35, 36, 37 / 47 / 34 |
| PMU I2C | 42, 41 |
| Onboard RTC INT / IMU INT / PMU IRQ (vendor map) | 14 / 33 / 40 |
| USB, kept reserved | 19, 20 |
| Flash bus reserved / nonexistent ESP32-S3 GPIO numbers | 26-32 / 22-25 |
| Explicitly prohibited without approval | 3, 45, 46 |
| Vendor expansion candidates, not assigned or physically verified | 43, 44 (UART/QWIIC) |

Source: shared/tbeam_pins.h, include/weather_manager.h, include/sos_controls.h,
shared/thesis_sd_logger.h and the official LILYGO pin map:
https://wiki.lilygo.cc/products/t-beam-series/t-beam-supreme/#pin-mapping
Vendor documentation is not proof of the actual board revision or connector wiring.
The generic PlatformIO board definition does not establish physical availability.
The adapter's reservation check is a minimum exclusion list, not an approved pin map.

## Minimum safe wiring plan (not activated)

1. Confirm Vessel-003 board revision, exact schematic, connector pinout, current
   wiring and continuity. Do not change Vessel-001/002 wiring or configuration.
2. Identify three independent output channels with no peripheral/boot conflict.
   GPIO43/44 are candidates only; a third direct GPIO is NOT verified. If three
   suitable pins are unavailable, review a separate expander proposal before wiring;
   this patch adds no expander or sensor-pin reuse.
3. Give each LED its own current-limiting resistor, selected from actual supply,
   LED forward voltage and desired current. Use a driver for loads exceeding the
   reviewed GPIO budget. No green physical LED.
4. Review buzzer type, supply and current. Use a 3.3 V-compatible transistor/MOSFET
   driver with OFF bias and common ground; add suppression if its load requires it.
   This envelope suits an active buzzer/driver; a passive transducer needs a
   separately reviewed oscillator/driver. Never connect an unverified buzzer directly.
5. Verify active-HIGH polarity, supply budget and boot/reset OFF behavior. Only then
   review explicit enabling and pin assignments in physical_indicators.h. Both
   enable/review constants are false and every output pin is -1 in this checkpoint.

Host tests cannot verify brightness, sound, electrical drive, or physical loop
latency. Existing blocking sensor/radio/SD calls can delay a pattern update; this
policy adds no blocking work. Physical acceptance remains pending confirmed wiring.
