// F23D SD begin
#include "../../shared/relay/f23d_sd_capture.h"
// F23D SD end
// F23D physical begin
#include "../../shared/relay/f23d_physical_contract.h"
// F23D physical end
// F23D readiness begin
#include "../../shared/relay/f23d_readiness_contract.h"
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
extern void f23dPollEvidence();
#endif
// F23D readiness end
// F23D begin
#include "../../shared/relay/f23d_bench_contract.h"
// F23D end
// F23C Phase 1 begin
#include "../../shared/relay/f23c_lab_contract.h"
// F23C Phase 1 end
#ifndef THESIS_LAB_RELAY_TX
#define THESIS_LAB_RELAY_TX 0
#endif
#include "../../shared/hardware/tbeam_pins.h"
#include "../../shared/hardware/vessel_identity.h"
#include "../../shared/hardware/fleet_home_title.h"
#include "../../shared/hardware/oled_button_debounce.h"
#include "../../shared/hardware/tbeam_hardware_service.h"
#include "../../shared/hardware/gps_service.h"
#include "../../shared/hardware/gnss_pht_clock.h"
#include <Arduino.h>
#include "sos_controls.h"
#ifndef THESIS_MULTIHOP_DEV_TEST
#define THESIS_MULTIHOP_DEV_TEST 0
#endif
#include "physical_indicators.h"
// USB-only fleet enrollment; excluded from every accepted V003 profile.
#ifndef THESIS_FLEET_USB_ENROLLMENT
#define THESIS_FLEET_USB_ENROLLMENT 0
#endif
#if THESIS_FLEET_USB_ENROLLMENT != 0 && THESIS_FLEET_USB_ENROLLMENT != 1
#error "Invalid fleet USB enrollment flag"
#endif
#ifndef THESIS_F22_FLEET_OPERATIONAL
#define THESIS_F22_FLEET_OPERATIONAL 0
#endif
#if THESIS_F22_FLEET_OPERATIONAL != 0 && THESIS_F22_FLEET_OPERATIONAL != 1
#error "Invalid F22 fleet operational flag"
#endif
#if THESIS_F22_FLEET_OPERATIONAL == 1 && (THESIS_V003_F09_SHARED_SENSOR_BUS != 1 ||     THESIS_V003_F15_SD_PILOT_ARMED != 1 || THESIS_SD_LOGGER != 1 ||     THESIS_V003_F13_LIVE_OUTPUTS != 0 || THESIS_LAB_RELAY_TX != 0)
#error "Fleet operational build needs V003 sensors, sandbox SD, radio relay OFF and GPIO outputs OFF"
#endif
#if THESIS_FLEET_USB_ENROLLMENT == 1
#if THESIS_LAB_RELAY_TX != 0 ||     (THESIS_F22_FLEET_OPERATIONAL != 1 && THESIS_F22R3A_STANDARD_FLEET_PREVIEW != 1)
#error "Fleet enrollment needs the standardized V003 firmware and relay OFF"
#endif
#include "../../shared/hardware/fleet_usb_enrollment.h"
#endif
// Isolated Vessel-003 preflight. Never changes normal firmware behavior.
#ifndef THESIS_V003_F08_INDICATOR_BENCH
#define THESIS_V003_F08_INDICATOR_BENCH 0
#endif
#if THESIS_V003_F08_INDICATOR_BENCH != 0 && THESIS_V003_F08_INDICATOR_BENCH != 1
#error "THESIS_V003_F08_INDICATOR_BENCH must be 0 or 1"
#endif
#if THESIS_V003_F08_INDICATOR_BENCH == 1
#if THESIS_LAB_RELAY_TX != 0
#error "V003 F08 preflight cannot enable laboratory relay TX"
#endif
static_assert(!PhysicalIndicators::kOutputsEnabled &&
              !PhysicalIndicators::kWiringAndDriverReviewed &&
              PhysicalIndicators::kYellowPin == -1 &&
              PhysicalIndicators::kRedPin == -1 &&
              PhysicalIndicators::kBuzzerDriverPin == -1,
              "V003 F08 preflight must keep all physical outputs unassigned/OFF");
#endif
// This LED bench uses virtual states only in this build-only environment.
#ifndef THESIS_V003_F08_LED_BENCH
#define THESIS_V003_F08_LED_BENCH 0
#endif
#if THESIS_V003_F08_LED_BENCH != 0 && THESIS_V003_F08_LED_BENCH != 1
#error "THESIS_V003_F08_LED_BENCH must be 0 or 1"
#endif
#if THESIS_V003_F08_LED_BENCH == 1
#if THESIS_V003_F08_INDICATOR_BENCH != 1
#error "V003 LED preflight requires the weather-disabled Vessel-003 bench"
#endif
#include "v003_f08_led_bench.h"
static_assert(V003F08LedBench::kBuzzerPin == -1 &&
              PhysicalIndicators::kBuzzerDriverPin == -1 &&
              !PhysicalIndicators::kOutputsEnabled,
              "V003 LED test must not enable buzzer or production indicators");
#if THESIS_V003_F08_LED_LIVE_PROFILE == 0
static_assert(!V003F08LedBench::kOutputsEnabled,
              "V003 LED preflight build must keep ALL physical outputs OFF");
#else
static_assert(V003F08LedBench::kOutputsEnabled,
              "Live LED profile must be explicitly opted in");
#endif
#endif
// Separate buzzer *software-only* profile. Never configures GPIO38.
#ifndef THESIS_V003_F08_BUZZER_PREFLIGHT
#define THESIS_V003_F08_BUZZER_PREFLIGHT 0
#endif
#if THESIS_V003_F08_BUZZER_PREFLIGHT == 1
#if THESIS_V003_F08_INDICATOR_BENCH != 1 || THESIS_V003_F08_LED_BENCH == 1
#error "F08 buzzer preflight requires the isolated weather-disabled, LED-off profile"
#endif
#include "v003_f08_buzzer_bench.h"
static_assert(!V003F08BuzzerBench::kOutputsEnabled &&
              V003F08BuzzerBench::kActualOutputPin == -1 &&
              !PhysicalIndicators::kOutputsEnabled &&
              PhysicalIndicators::kBuzzerDriverPin == -1,
              "F08 buzzer preflight must never drive the physical buzzer");
#endif
// Independent Vessel-003 live buzzer profile. Separate from software preflight.
#ifndef THESIS_V003_F08_BUZZER_LIVE_PROFILE
#define THESIS_V003_F08_BUZZER_LIVE_PROFILE 0
#endif
#if THESIS_V003_F08_BUZZER_LIVE_PROFILE == 1
#if THESIS_V003_F08_INDICATOR_BENCH != 1 || THESIS_V003_F08_LED_BENCH == 1 || \
    THESIS_V003_F08_BUZZER_PREFLIGHT == 1 || THESIS_LAB_RELAY_TX != 0 || \
    THESIS_SD_LOGGER != 0 || THESIS_F07_WEATHER_LOG != 0
#error "F08 buzzer live requires weather-disabled, SD-off, relay-off, LED-off profile"
#endif
#include "v003_f08_buzzer_live.h"
static_assert(V003F08BuzzerLive::kOutputsEnabled &&
              V003F08BuzzerLive::kBuzzerPin == 38 &&
              !PhysicalIndicators::kOutputsEnabled &&
              PhysicalIndicators::kBuzzerDriverPin == -1,
              "Live buzzer must be isolated from the production indicators");
#endif
// Isolated GPIO38/39/44 combined F08 bench profile, NOT field firmware.
#ifndef THESIS_V003_F08_COMBINED_PROFILE
#define THESIS_V003_F08_COMBINED_PROFILE 0
#endif
#if THESIS_V003_F08_COMBINED_PROFILE == 1
#if THESIS_V003_F08_INDICATOR_BENCH != 1 || THESIS_V003_F08_LED_BENCH == 1 || \
    THESIS_V003_F08_BUZZER_PREFLIGHT == 1 || THESIS_V003_F08_BUZZER_LIVE_PROFILE == 1 || \
    THESIS_LAB_RELAY_TX != 0 || THESIS_SD_LOGGER != 0 || THESIS_F07_WEATHER_LOG != 0
#error "F08 combined bench must isolate weather sensors, SD, relay and other GPIO owners"
#endif
#include "v003_f08_combined_bench.h"
static_assert(V003F08CombinedBench::kOutputsEnabled &&
              V003F08CombinedBench::kYellowPin == 39 &&
              V003F08CombinedBench::kRedPin == 44 &&
              V003F08CombinedBench::kBuzzerPin == 38 &&
              !PhysicalIndicators::kOutputsEnabled &&
              PhysicalIndicators::kBuzzerDriverPin == -1,
              "Combined test must keep the production adapter disabled");
#endif
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include <TinyGPSPlus.h>
#include "lora_manager.h"
#include "../../shared/communication/decentralized_mac.h"
#include "../../shared/communication/mac_contention_gate.h"
// 0=original airtime scheduler; 1=OBSERVE CAD (default); 2=ENFORCE CAD.
// Mode 2 is experimental: use only after Mode 1 has passed hardware tests.
#ifndef MAC_CAD_MODE
#define MAC_CAD_MODE 0
#endif
#if MAC_CAD_MODE < 0 || MAC_CAD_MODE > 2
#error "MAC_CAD_MODE must be 0, 1, or 2"
#endif
#include <XPowersLib.h>
#ifndef THESIS_SD_LOGGER
#define THESIS_SD_LOGGER 0
#endif
#ifndef THESIS_F07_WEATHER_LOG
#define THESIS_F07_WEATHER_LOG 0
#endif
#if THESIS_F07_WEATHER_LOG == 1 && THESIS_SD_LOGGER != 1
#error "F07 weather logging requires the SD logger"
#endif
#ifndef THESIS_V003_F15_MACRO_SD_SANDBOX
#define THESIS_V003_F15_MACRO_SD_SANDBOX 0
#endif
#if THESIS_V003_F15_MACRO_SD_SANDBOX != 0 && THESIS_V003_F15_MACRO_SD_SANDBOX != 1
#error "Invalid F15 macro SD sandbox flag"
#endif
// F17: the ONLY exception for combining F12/F13 policy with F15/F16 storage.
// This phase is deliberately OUTPUTS-DISABLED; no live-output profile exists.
#ifndef THESIS_V003_F17_INTEGRATED_PREFLIGHT
#define THESIS_V003_F17_INTEGRATED_PREFLIGHT 0
#endif
#ifndef THESIS_V003_F17_EXPLICIT_OPT_IN
#define THESIS_V003_F17_EXPLICIT_OPT_IN 0
#endif
#if (THESIS_V003_F17_INTEGRATED_PREFLIGHT != 0 && THESIS_V003_F17_INTEGRATED_PREFLIGHT != 1) || \
    (THESIS_V003_F17_EXPLICIT_OPT_IN != 0 && THESIS_V003_F17_EXPLICIT_OPT_IN != 1)
#error "Invalid F17 integrated preflight flags"
#endif
// F18: weather-first integration. Adds ONLY virtual UI + research SD trace.
// Never changes weather thresholds, LoRa frames, SOS logic or physical GPIO.
#ifndef THESIS_V003_F18_WEATHER_INTERFACE
#define THESIS_V003_F18_WEATHER_INTERFACE 0
#endif
#ifndef THESIS_V003_F18_EXPLICIT_OPT_IN
#define THESIS_V003_F18_EXPLICIT_OPT_IN 0
#endif
#if (THESIS_V003_F18_WEATHER_INTERFACE != 0 && THESIS_V003_F18_WEATHER_INTERFACE != 1) || \
    (THESIS_V003_F18_EXPLICIT_OPT_IN != 0 && THESIS_V003_F18_EXPLICIT_OPT_IN != 1)
#error "Invalid F18 weather-integration flags"
#endif
#if THESIS_V003_F18_WEATHER_INTERFACE == 1 && \
    (THESIS_V003_F18_EXPLICIT_OPT_IN != 1 || \
     THESIS_V003_F17_INTEGRATED_PREFLIGHT != 1 || \
     THESIS_V003_F10_MICRO_NOWCAST != 1 || \
     THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1 || \
     THESIS_V003_F12_INDICATOR_PREVIEW != 1 || \
     THESIS_V003_F13_SUPERVISED_DRIVER != 1 || \
     THESIS_V003_F15_MACRO_SD_SANDBOX != 1 || \
     THESIS_SD_OLED_UI != 1 || \
     THESIS_V003_F13_LIVE_OUTPUTS != 0 || \
     THESIS_LAB_RELAY_TX != 0 || \
     THESIS_V003_F14_INDICATOR_PROBE != 0)
#error "F18 requires F17 V003 outputs-OFF macro/micro SD OLED integration"
#endif

// F19: new Vessel-003 presentation only; F18 and older profiles stay untouched.
#ifndef THESIS_V003_F19_DUAL_DISPLAY
#define THESIS_V003_F19_DUAL_DISPLAY 0
#endif
#ifndef THESIS_V003_F19_EXPLICIT_OPT_IN
#define THESIS_V003_F19_EXPLICIT_OPT_IN 0
#endif
#if (THESIS_V003_F19_DUAL_DISPLAY != 0 && THESIS_V003_F19_DUAL_DISPLAY != 1) || \
    (THESIS_V003_F19_EXPLICIT_OPT_IN != 0 && THESIS_V003_F19_EXPLICIT_OPT_IN != 1)
#error "Invalid F19 display flags"
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1 && \
    (THESIS_V003_F19_EXPLICIT_OPT_IN != 1 || \
     THESIS_V003_F18_WEATHER_INTERFACE != 1 || \
     THESIS_V003_F18_EXPLICIT_OPT_IN != 1 || \
     THESIS_V003_F17_INTEGRATED_PREFLIGHT != 1 || \
     THESIS_V003_F15_MACRO_SD_SANDBOX != 1 || \
     THESIS_V003_F16_SD_OLED_HYBRID != 1 || \
     THESIS_F06_SD_HYBRID != 1 || THESIS_SD_OLED_UI != 1 || \
     THESIS_V003_F13_LIVE_OUTPUTS != 0 || \
     THESIS_LAB_RELAY_TX != 0 || THESIS_V003_F14_INDICATOR_PROBE != 0 || \
     WEATHER_FAST_TEST != 0)
#error "F19 requires F18 V003 outputs-OFF isolated dual-display profile"
#endif

// Separate F15 hardware opt-in: the existing build-only profile cannot mount
// the microSD card or queue weather data even if accidentally uploaded.
#ifndef THESIS_V003_F15_SD_PILOT_ARMED
#define THESIS_V003_F15_SD_PILOT_ARMED 0
#endif
#if THESIS_V003_F15_SD_PILOT_ARMED != 0 && THESIS_V003_F15_SD_PILOT_ARMED != 1
#error "Invalid F15 supervised SD pilot flag"
#endif
#if THESIS_V003_F15_SD_PILOT_ARMED == 1 && THESIS_V003_F15_MACRO_SD_SANDBOX != 1
#error "F15 supervised SD pilot requires the complete F15 sandbox profile"
#endif

// F22R2: strictly opt-in, passive readout of existing firmware state.
// No new GPIO, I2C probe, NVS, SD, LoRa, SOS or weather decision is permitted.
#ifndef THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER
#define THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER 0
#endif
#if THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER != 0 && THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER != 1
#error "F22R2 passive observer flag must be 0 or 1"
#endif
#if THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER == 1
#if THESIS_V003_F19_DUAL_DISPLAY != 1 || THESIS_V003_F15_SD_PILOT_ARMED != 0 || \
    THESIS_V003_F13_LIVE_OUTPUTS != 0 || THESIS_LAB_RELAY_TX != 0 || \
    THESIS_V003_F14_INDICATOR_PROBE != 0 || THESIS_V003_F08_INDICATOR_BENCH != 0
#error "F22R2 observer requires F19 build-only, SD pilot OFF, output OFF, relay OFF"
#endif
#include "../../shared/hardware/f22_universal_capability_policy.h"
#endif
// F22R3A: standardized V003 wiring, multi-ID PREVIEW. Never a flashing profile.
#ifndef THESIS_F22R3A_STANDARD_FLEET_PREVIEW
#define THESIS_F22R3A_STANDARD_FLEET_PREVIEW 0
#endif
#ifndef THESIS_F22R3A_V003_WIRING_EXPLICIT_OPT_IN
#define THESIS_F22R3A_V003_WIRING_EXPLICIT_OPT_IN 0
#endif
#if (THESIS_F22R3A_STANDARD_FLEET_PREVIEW != 0 && THESIS_F22R3A_STANDARD_FLEET_PREVIEW != 1) || \
    (THESIS_F22R3A_V003_WIRING_EXPLICIT_OPT_IN != 0 && THESIS_F22R3A_V003_WIRING_EXPLICIT_OPT_IN != 1)
#error "Invalid F22R3A fleet preview flags"
#endif
#if THESIS_F22R3A_STANDARD_FLEET_PREVIEW == 1
#if THESIS_F22R3A_V003_WIRING_EXPLICIT_OPT_IN != 1 || \
    THESIS_V003_F09_SHARED_SENSOR_BUS != 1 || THESIS_V003_F19_DUAL_DISPLAY != 1 || \
    THESIS_V003_F15_SD_PILOT_ARMED != 0 || THESIS_V003_F13_LIVE_OUTPUTS != 0 || \
    THESIS_LAB_RELAY_TX != 0 || THESIS_V003_F08_INDICATOR_BENCH != 0 || \
    THESIS_V003_F14_INDICATOR_PROBE != 0
#error "F22R3A requires standardized V003 shared I2C and SD/output/relay OFF"
#endif
#endif
// F22R3B: separate STORAGE POLICY preview ONLY. Never mounts media.
#ifndef THESIS_F22R3B_STORAGE_POLICY_PREVIEW
#define THESIS_F22R3B_STORAGE_POLICY_PREVIEW 0
#endif
#if THESIS_F22R3B_STORAGE_POLICY_PREVIEW != 0 && THESIS_F22R3B_STORAGE_POLICY_PREVIEW != 1
#error "Invalid F22R3B policy preview flag"
#endif
#if THESIS_F22R3B_STORAGE_POLICY_PREVIEW == 1
#if THESIS_F22R3A_STANDARD_FLEET_PREVIEW != 1 || \
    THESIS_F22R3A_V003_WIRING_EXPLICIT_OPT_IN != 1 || \
    THESIS_V003_F15_SD_PILOT_ARMED != 0 || THESIS_V003_F13_LIVE_OUTPUTS != 0 || \
    THESIS_LAB_RELAY_TX != 0 || THESIS_V003_F08_INDICATOR_BENCH != 0
#error "F22R3B requires standard V003 multi-ID, SD pilot OFF, outputs/relay OFF"
#endif
#include "../../shared/storage/f22_fleet_storage_policy.h"
#endif
#line 212
#if THESIS_V003_F15_MACRO_SD_SANDBOX == 1 && \
    (THESIS_V003_F09_SHARED_SENSOR_BUS != 1 || \
     THESIS_V003_F10_MICRO_NOWCAST != 1 || \
     THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1 || \
     THESIS_SD_LOGGER != 1 || THESIS_SD_FLEET != 1 || \
     THESIS_F05_RECOVERY != 1 || THESIS_F06_SD_SANDBOX != 1 || \
     THESIS_F07_WEATHER_LOG != 1 || WEATHER_FAST_TEST != 0 || \
     THESIS_LAB_RELAY_TX != 0 || \
     ((THESIS_V003_F12_INDICATOR_PREVIEW != 0 || THESIS_V003_F13_SUPERVISED_DRIVER != 0) && \
      THESIS_V003_F17_INTEGRATED_PREFLIGHT != 1) || \
     THESIS_V003_F14_INDICATOR_PROBE != 0)
#error "F15 requires Vessel-003 real F09-F11, F05/F06 sandbox SD, relay and physical indicators OFF"
#endif
#if THESIS_V003_F08_INDICATOR_BENCH == 1 && \
    (THESIS_SD_LOGGER != 0 || THESIS_F07_WEATHER_LOG != 0)
#error "V003 F08 preflight cannot enable SD logging or weather records"
#endif
// Display-only SD OLED view. Enabled only in its dedicated F06 test profile.
#ifndef THESIS_SD_OLED_UI
#define THESIS_SD_OLED_UI 0
#endif
#if THESIS_SD_OLED_UI != 0 && THESIS_SD_OLED_UI != 1
#error "THESIS_SD_OLED_UI must be 0 or 1"
#endif
#if THESIS_SD_OLED_UI == 1 && THESIS_SD_LOGGER != 1
#error "SD OLED UI requires SD logger"
#endif
#ifndef THESIS_F06_SD_HYBRID
#define THESIS_F06_SD_HYBRID 0
#endif
#if THESIS_F06_SD_HYBRID != 0 && THESIS_F06_SD_HYBRID != 1
#error "Invalid THESIS_F06_SD_HYBRID"
#endif
#if THESIS_F06_SD_HYBRID == 1 && (THESIS_SD_OLED_UI != 1 || THESIS_SD_LOGGER != 1 || THESIS_F06_SD_SANDBOX != 1)
#error "F06 hybrid requires OLED UI and isolated sandbox SD logger"
#endif
// F16: restore the tested F06 8th-page SD UI and hybrid routine logging only
// when paired with the F15 isolated V003 5-median SD profile. No GPIO outputs.
#ifndef THESIS_V003_F16_SD_OLED_HYBRID
#define THESIS_V003_F16_SD_OLED_HYBRID 0
#endif
#ifndef THESIS_V003_F16_EXPLICIT_OPT_IN
#define THESIS_V003_F16_EXPLICIT_OPT_IN 0
#endif
#if (THESIS_V003_F16_SD_OLED_HYBRID != 0 && THESIS_V003_F16_SD_OLED_HYBRID != 1) || \
    (THESIS_V003_F16_EXPLICIT_OPT_IN != 0 && THESIS_V003_F16_EXPLICIT_OPT_IN != 1)
#error "Invalid F16 SD OLED hybrid opt-in flags"
#endif
#if THESIS_V003_F16_SD_OLED_HYBRID == 1 && \
    (THESIS_V003_F16_EXPLICIT_OPT_IN != 1 || \
     THESIS_V003_F15_MACRO_SD_SANDBOX != 1 || \
     THESIS_V003_F09_SHARED_SENSOR_BUS != 1 || \
     THESIS_V003_F10_MICRO_NOWCAST != 1 || \
     THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1 || \
     THESIS_SD_LOGGER != 1 || THESIS_SD_FLEET != 1 || \
     THESIS_F05_RECOVERY != 1 || THESIS_F06_SD_SANDBOX != 1 || \
     THESIS_F07_WEATHER_LOG != 1 || THESIS_SD_OLED_UI != 1 || \
     THESIS_F06_SD_HYBRID != 1 || WEATHER_FAST_TEST != 0 || \
     THESIS_LAB_RELAY_TX != 0 || \
     ((THESIS_V003_F12_INDICATOR_PREVIEW != 0 || THESIS_V003_F13_SUPERVISED_DRIVER != 0) && \
      THESIS_V003_F17_INTEGRATED_PREFLIGHT != 1) || \
     THESIS_V003_F14_INDICATOR_PROBE != 0)
#error "F16 requires isolated V003 F15 macro SD, F06 hybrid OLED, relay/indicators OFF"
#endif
// F17 is authorized only when all proven F09-F16 features are deliberately
// combined, and F13 remains compiled with ZERO GPIO output capability.
#if THESIS_V003_F17_INTEGRATED_PREFLIGHT == 1 && \
    (THESIS_V003_F17_EXPLICIT_OPT_IN != 1 || \
     THESIS_V003_F15_MACRO_SD_SANDBOX != 1 || \
     THESIS_V003_F16_SD_OLED_HYBRID != 1 || THESIS_V003_F16_EXPLICIT_OPT_IN != 1 || \
     THESIS_V003_F09_SHARED_SENSOR_BUS != 1 || \
     THESIS_V003_F10_MICRO_NOWCAST != 1 || THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1 || \
     THESIS_V003_F12_INDICATOR_PREVIEW != 1 || THESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN != 1 || \
     THESIS_V003_F13_SUPERVISED_DRIVER != 1 || \
     THESIS_V003_F13_LIVE_OUTPUTS != 0 || \
     THESIS_V003_F13_EXPLICIT_OPT_IN != 0 || THESIS_V003_F13_HARDWARE_REVIEWED != 0 || \
     THESIS_SD_LOGGER != 1 || THESIS_SD_FLEET != 1 || THESIS_F05_RECOVERY != 1 || \
     THESIS_F06_SD_SANDBOX != 1 || THESIS_F07_WEATHER_LOG != 1 || \
     THESIS_SD_OLED_UI != 1 || THESIS_F06_SD_HYBRID != 1 || \
     WEATHER_FAST_TEST != 0 || THESIS_LAB_RELAY_TX != 0 || \
     THESIS_V003_F14_INDICATOR_PROBE != 0 || \
     THESIS_V003_F08_INDICATOR_BENCH != 0 || THESIS_V003_F08_LED_BENCH != 0 || \
     THESIS_V003_F08_BUZZER_PREFLIGHT != 0 || THESIS_V003_F08_BUZZER_LIVE_PROFILE != 0 || \
     THESIS_V003_F08_COMBINED_PROFILE != 0)
#error "F17 requires V003 F09-F16, F12 virtual, F13 GPIO OFF, isolated SD, relay OFF"
#endif
#if THESIS_V003_F17_EXPLICIT_OPT_IN == 1 && THESIS_V003_F17_INTEGRATED_PREFLIGHT != 1
#error "F17 opt-in requires integrated preflight mode"
#endif
#if THESIS_SD_OLED_UI == 1
#define OLED_PAGE_TOTAL "8"
static constexpr uint8_t kOLEDPageCount = 8;
#else
#define OLED_PAGE_TOTAL "7"
static constexpr uint8_t kOLEDPageCount = 7;
#endif
#if THESIS_SD_LOGGER == 1
#include "../../shared/storage/thesis_sd_logger.h"
#endif
#include <U8g2lib.h>
#if THESIS_V003_F19_DUAL_DISPLAY == 1
#include "v003_f19_display_modes.h"
#include "v003_f21r3_weather_display.h"
#include "v003_f21r4_ui_policy.h"
#endif
#include "weather_manager.h"
#if THESIS_V003_F10_MICRO_NOWCAST == 1
#include "v003_f10_micro_nowcast.h"
static_assert(!PhysicalIndicators::kOutputsEnabled &&
              DHT_SDA_PIN == 21 && DHT_SCL_PIN == 48,
              "F10 must not actuate hardware; use F09 shared weather sensor bus");
#endif
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
#include "v003_f11_macro_median.h"
static_assert(!PhysicalIndicators::kOutputsEnabled,
              "F11 macro profile must not drive physical indicators");
#endif
#if THESIS_V003_F12_INDICATOR_PREVIEW == 1
#include "v003_f12_indicator_preview.h"
#if THESIS_V003_F08_INDICATOR_BENCH != 0 || THESIS_V003_F08_LED_BENCH != 0 || \
    THESIS_V003_F08_BUZZER_PREFLIGHT != 0 || THESIS_V003_F08_BUZZER_LIVE_PROFILE != 0 || \
    THESIS_V003_F08_COMBINED_PROFILE != 0 || THESIS_LAB_RELAY_TX != 0 || \
    ((THESIS_SD_LOGGER != 0 || THESIS_F07_WEATHER_LOG != 0) && \
     THESIS_V003_F17_INTEGRATED_PREFLIGHT != 1)
#error "F12 preview must isolate GPIO bench, SD and relay profiles"
#endif
static_assert(!PhysicalIndicators::kOutputsEnabled &&
              PhysicalIndicators::kYellowPin == -1 &&
              PhysicalIndicators::kRedPin == -1 &&
              PhysicalIndicators::kBuzzerDriverPin == -1,
              "F12 preview makes no physical indicator GPIO assignments");
#endif
#if THESIS_V003_F13_SUPERVISED_DRIVER == 1
#include "v003_f13_supervised_indicators.h"
#if THESIS_V003_F17_INTEGRATED_PREFLIGHT == 1
static_assert(!V003F13Indicators::kPhysicalOutputsEnabled,
              "F17 integrated preflight must never enable F13 physical GPIO");
#endif
#if THESIS_V003_F14_INDICATOR_PROBE == 1
#include "v003_f14_indicator_probe.h"
#endif
#if THESIS_V003_F12_INDICATOR_PREVIEW != 1 || THESIS_V003_F08_INDICATOR_BENCH != 0 || \
    THESIS_V003_F08_LED_BENCH != 0 || THESIS_V003_F08_BUZZER_PREFLIGHT != 0 || \
    THESIS_V003_F08_BUZZER_LIVE_PROFILE != 0 || THESIS_V003_F08_COMBINED_PROFILE != 0 || \
    THESIS_LAB_RELAY_TX != 0 || \
    ((THESIS_SD_LOGGER != 0 || THESIS_F07_WEATHER_LOG != 0) && \
     THESIS_V003_F17_INTEGRATED_PREFLIGHT != 1)
#error "F13 must not share GPIO ownership, relay, or SD profiles"
#endif
static_assert(!PhysicalIndicators::kOutputsEnabled &&
              DHT_SDA_PIN == 21 && DHT_SCL_PIN == 48 &&
              V003F13Indicators::kYellowPin == 39 &&
              V003F13Indicators::kRedPin == 44 &&
              V003F13Indicators::kBuzzerDriverPin == 38,
              "F13 pins must be free of I2C and production indicator ownership");
#endif
#if THESIS_V003_F09_SHARED_SENSOR_BUS == 1
#if THESIS_V003_F08_INDICATOR_BENCH != 0 || THESIS_V003_F08_LED_BENCH != 0 || \
    THESIS_V003_F08_BUZZER_PREFLIGHT != 0 || THESIS_V003_F08_BUZZER_LIVE_PROFILE != 0 || \
    THESIS_V003_F08_COMBINED_PROFILE != 0 || THESIS_LAB_RELAY_TX != 0 || \
    ((THESIS_SD_LOGGER != 0 || THESIS_F07_WEATHER_LOG != 0) && \
     THESIS_V003_F15_MACRO_SD_SANDBOX != 1)
#error "F09 requires F08/relay OFF; SD permitted only by guarded F15 sandbox"
#endif
static_assert(!PhysicalIndicators::kOutputsEnabled &&
              PhysicalIndicators::kBuzzerDriverPin == -1 &&
              DHT_SDA_PIN == 21 && DHT_SCL_PIN == 48,
              "F09 sensors must share external bus without physical alert outputs");
#endif
#ifndef THESIS_SD_PROBE
#define THESIS_SD_PROBE 0
#endif
#if THESIS_SD_PROBE == 1
#include "../../shared/storage/sd_probe.h"
#endif
#include <esp_timer.h>  // 64-bit uptime counter

// ============================================================
// UNIVERSAL - NUMERIC VESSEL IDENTITY
// T-Beam Supreme | L76K GPS | SX1262 | SH1106 OLED
// Dual I2C Environmental Sensors (BME680 & DHT20)
// ============================================================


// T-Beam Supreme user/BOOT button (active LOW)
#define USER_BUTTON_PIN 0


const unsigned long TX_INTERVAL = 5000UL;
// PHASE 3G-2: randomized telemetry slots, SOS handling unchanged.
// Guardrail: 4.25s..5.75s telemetry intervals, initial 1..5 seconds.
static constexpr uint32_t TELEMETRY_JITTER_MS = 750UL;
static constexpr uint32_t TELEMETRY_INITIAL_MIN_MS = 1000UL;
static constexpr uint32_t TELEMETRY_INITIAL_MAX_MS = 5000UL;
// PHASE 5.2.2C-3G-4E1: decentralized airtime-based scheduling.
static DecentralizedMac::Scheduler telemetryMac;

static uint32_t nextTelemetryIntervalMs() {
  // Arduino random(max) has exclusive upper bound.
  const long swing = random(-(long)TELEMETRY_JITTER_MS, (long)TELEMETRY_JITTER_MS + 1L);
  return TX_INTERVAL + swing;
}

// DEVELOPMENT ONLY: set to 0 for real 10-minute sampling before field use.
#ifndef WEATHER_FAST_TEST
#define WEATHER_FAST_TEST 1
#endif
const unsigned long WEATHER_INTERVAL = WEATHER_FAST_TEST ? 5000UL : 600000UL;
const unsigned long GPS_STATUS_INTERVAL = 5000UL;
const unsigned long OLED_UPDATE_INTERVAL = 500UL;
const unsigned long UPTIME_PRINT_INTERVAL = 60000UL; // Serial heartbeat every minute
unsigned long lastWeatherTime = 0;
#if THESIS_V003_F09_SHARED_SENSOR_BUS == 1
// Runtime guard: don't touch GPIO21/48 sensor bus on misflashed vessels.
static bool f09SensorAuthorized = false;
#endif
#if THESIS_V003_F10_MICRO_NOWCAST == 1
static V003F10Micro::Forecaster f10Forecaster;
static uint32_t lastF10MicroTime = 0;
// Real first measurement is due on the first authorized loop.
static bool f10InitialSamplePending = true;
#endif
#if THESIS_V003_F12_INDICATOR_PREVIEW == 1
static V003F12Preview::Controller f12IndicatorPreview;
static uint32_t f12LastReportMs = 0;
#endif
#if THESIS_V003_F13_SUPERVISED_DRIVER == 1
static V003F13Indicators::Controller f13Indicators;
static uint32_t f13LastReportMs = 0;
static uint32_t f13LastRailPollMs = 0;
static bool f13RailEnableBitCached = false;
#if THESIS_V003_F14_INDICATOR_PROBE == 1
static V003F14Probe::Controller f14Probe;
static bool f14PreviousSynthetic = false;
#endif
#endif
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
static V003F11Macro::Batch f11MacroBatch;
static bool f11InitialBatchPending = true;
static bool f11MacroSampleForMicro = false;
#endif

// -------------------- Hardware --------------------------------
HardwareSerial GPSSerial(1);
TinyGPSPlus gps;
TwoWire PMUWire(1); // PMU stays isolated on hardware bus 1
XPowersAXP2101 pmu;
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// -------------------- OLED ------------------------------------
bool oledAvailable = false;
// Wire is reused across the OLED, BME680 and DHT20 pin pairs. Track the
// active pair so redraws do not repeatedly end/restart the same I2C bus.
int activeWireSDA = -1;
int activeWireSCL = -1;
uint8_t oledAddress = 0x3C;
uint8_t oledPage = 0;
unsigned long lastOLEDUpdate = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 40UL;
int buttonLastReading = HIGH;
int buttonStableState = HIGH;
unsigned long buttonLastDebounceTime = 0;
unsigned long buttonPressStartedAt = 0;
const unsigned long BUTTON_LONG_PRESS_MS = 900UL;
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
// Hold GPIO0 on the existing weather page to toggle MACRO / MICRO.
// Does not change SOS menu, storage mode, sensor clocks or OLED page count.
static bool f18WeatherMicroView = false;
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1
// Volatile operational menu / OLED sleep. Navigation does not own radio or SOS.
static V003F19Display::State f19Display;
static V003F19Display::SOSBrowse f21r42SOSBrowse;
static bool f19PhysicalScreenAsleep = false;
static bool f21PhysicalPortrait = false; // OLED-only cached rotation
// F21R4: PMU power bus is separate; renderers only read a cached estimate.
static bool f21r4PmuReady=false;
static int f21r4BatteryPercent=-1;
static uint32_t f21r4LastBatteryPoll=0;
void f21r4PollBattery();
#endif
// OLED-only alert demonstrations: 0 = LIVE, 1 = NORMAL, 2 = FRONT, 3 = SQUALL.
// These NEVER modify real weatherState, radio telemetry, or sensor history.
uint8_t alertDemoMode = 0;
#if THESIS_SD_OLED_UI == 1
// GPIO0 hold changes only the SD OLED subview; never changes logger state.
static uint8_t sdOLEDSubview = 0;
#if THESIS_F06_SD_HYBRID == 1
// GPIO0 mode menu is informational/selection only; GPIO2 owns physical SOS.
static bool sdHybridMenuOpen = false;
static uint8_t sdHybridMenuCursor = 0; // 0=SOS ARMED, 1=CONTINUOUS, 2=BACK
// Separate four-choice SD menu; both menus exist only on the hybrid profile.
static bool sdStorageMenuOpen = false;
static uint8_t sdStorageMenuCursor = 0; // 0=JOURNALS 1=USB 2=RECORD MODE 3=BACK
#endif
#endif
// 0 = AUTO (latest active peer). Nonzero = manually selected vessel.
uint32_t selectedOLEDPeerId = 0;
bool selectedOLEDPeer(PeerDisplaySnapshot &out) {
  if (!selectedOLEDPeerId) return getLivePeerSnapshot(out);
  if (getPeerSnapshotById(selectedOLEDPeerId,out) && out.active) return true;
  out=PeerDisplaySnapshot{};return false;
}
// Menu list is captured on opening; incoming telemetry cannot change its cursor.
static constexpr uint8_t kMenuPeersMax = 10;
bool oledPeerMenuOpen = false;
uint32_t oledMenuPeerIds[kMenuPeersMax] = {};
uint8_t oledMenuPeerCount = 0;
uint8_t oledMenuCursor = 0; // 0=Auto, 1..N=vessels, N+1=Back
void openOLEDPeerMenu() {
  oledMenuPeerCount = 0;
  oledMenuCursor = 0;
  uint32_t previous = 0;
  while (oledMenuPeerCount < kMenuPeersMax) {
    uint32_t id = 0;
    if (!nextActivePeerId(previous, id) || id <= previous) break;
    oledMenuPeerIds[oledMenuPeerCount++] = id;
    previous = id;
  }
  oledPeerMenuOpen = true;
  Serial.printf("[OLED-MENU] OPEN peers=%u\n", (unsigned)oledMenuPeerCount);
}
void advanceOLEDPeerMenu() {
  oledMenuCursor = (oledMenuCursor + 1) % (oledMenuPeerCount + 2);
  Serial.printf("[OLED-MENU] CURSOR %u\n", (unsigned)oledMenuCursor);
}
void confirmOLEDPeerMenu() {
  if (oledMenuCursor == 0) {
    selectedOLEDPeerId = 0;
    Serial.println("[OLED-MENU] CHOSEN AUTO");
  } else if (oledMenuCursor == oledMenuPeerCount + 1) {
    Serial.println("[OLED-MENU] BACK (selection unchanged)");
  } else {
    uint32_t id = oledMenuPeerIds[oledMenuCursor - 1];
    PeerDisplaySnapshot peer;
    if (!getPeerSnapshotById(id, peer) || !peer.active) {
      Serial.printf("[OLED-MENU] ID=%lu inactive; menu remains open\n", (unsigned long)id);
      return;
    }
    selectedOLEDPeerId = id;
    Serial.printf("[OLED-MENU] CHOSEN ID=%lu\n", (unsigned long)id);
  }
  oledPeerMenuOpen = false;
}
void drawOLEDPeerMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "SELECT VESSEL 3/" OLED_PAGE_TOTAL);
  const uint8_t choices = oledMenuPeerCount + 2;
  const uint8_t first = oledMenuCursor >= 3 ? oledMenuCursor - 3 : 0;
  for (uint8_t row = 0; row < 4 && first + row < choices; ++row) {
    uint8_t index = first + row;
    char label[24] = {};
    if (index == 0) strcpy(label, "Auto");
    else if (index == oledMenuPeerCount + 1) strcpy(label, "Back");
    else snprintf(label, sizeof(label), "Vessel-%03lu", (unsigned long)oledMenuPeerIds[index - 1]);
    char line[29];
    snprintf(line, sizeof(line), "%c %s", index == oledMenuCursor ? '>' : ' ', label);
    u8g2.drawStr(0, 21 + row * 12, line);
  }
  u8g2.sendBuffer();
}


// PHASE 3F-2 SOS MENU: display-only. Records remain managed by LoRa receiver.
// The menu freezes active origin+event identifiers on opening; no TX is triggered.
static constexpr uint8_t kSOSMenuCapacity = 10;
static uint32_t sosMenuOrigins[kSOSMenuCapacity] = {};
static uint32_t sosMenuEvents[kSOSMenuCapacity] = {};
static uint8_t sosMenuCount = 0, sosMenuCursor = 0;
static bool sosMenuOpen = false;
static uint32_t selectedSOSOrigin = 0, selectedSOSEvent = 0; // zero = AUTO

bool findSOSRecord(uint32_t origin, uint32_t event, SOSRecordSnapshot &out) {
  for (size_t i=0; i<kSOSMenuCapacity; ++i) {
    SOSRecordSnapshot current;
    if (getSOSRecordByIndex(i,current) && current.known &&
        current.originId==origin && current.eventId==event) {
      out=current;return true;
    }
  }
  out=SOSRecordSnapshot{};return false;
}
void openSOSMenu() {
  sosMenuCount=0;sosMenuCursor=0;
  for (size_t i=0;i<kSOSMenuCapacity;++i) {
    SOSRecordSnapshot rec;
    if (getSOSRecordByIndex(i,rec) && rec.known && !rec.cancelled) {
      sosMenuOrigins[sosMenuCount]=rec.originId;
      sosMenuEvents[sosMenuCount]=rec.eventId;
      if (++sosMenuCount>=kSOSMenuCapacity) break;
    }
  }
  sosMenuOpen=true;
  Serial.printf("[SOS-MENU] OPEN active=%u\n",(unsigned)sosMenuCount);
}
void advanceSOSMenu() {
  sosMenuCursor=(sosMenuCursor+1)%(sosMenuCount+2); // AUTO, active records, BACK
}
void confirmSOSMenu() {
  if (sosMenuCursor==0) {
    selectedSOSOrigin=0;selectedSOSEvent=0;
    Serial.println("[SOS-MENU] CHOSEN AUTO");
  } else if (sosMenuCursor==sosMenuCount+1) {
    Serial.println("[SOS-MENU] BACK");
  } else {
    const uint8_t i=sosMenuCursor-1;
    SOSRecordSnapshot rec;
    if (!findSOSRecord(sosMenuOrigins[i],sosMenuEvents[i],rec)||rec.cancelled) {
      Serial.println("[SOS-MENU] EVENT NO LONGER ACTIVE; CHOOSE AGAIN");
      openSOSMenu();return;
    }
    selectedSOSOrigin=rec.originId;selectedSOSEvent=rec.eventId;
    Serial.printf("[SOS-MENU] CHOSEN origin=%lu event=%lu\n",
      (unsigned long)selectedSOSOrigin,(unsigned long)selectedSOSEvent);
  }
  sosMenuOpen=false;
}
void drawSOSMenu() {
  u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"ACTIVE SOS MENU 7/" OLED_PAGE_TOTAL);
  const uint8_t choices=sosMenuCount+2;
  const uint8_t first=sosMenuCursor>=3?sosMenuCursor-3:0;
  for(uint8_t row=0;row<4 && first+row<choices;++row) {
    const uint8_t idx=first+row;
    char label[30]={};
    if(idx==0)strcpy(label,"Auto (newest)");
    else if(idx==sosMenuCount+1)strcpy(label,"Back");
    else snprintf(label,sizeof(label),"V%03lu SOS#%lu",
      (unsigned long)sosMenuOrigins[idx-1],(unsigned long)sosMenuEvents[idx-1]);
    char line[34];snprintf(line,sizeof(line),"%c %s",idx==sosMenuCursor?'>':' ',label);
    u8g2.drawStr(0,21+row*12,line);
  }
  u8g2.sendBuffer();
}
// In AUTO mode, show latest ACTIVE event, not the most recently cancelled one.
bool displaySOSRecord(SOSRecordSnapshot &out) {
  if (selectedSOSOrigin) return findSOSRecord(selectedSOSOrigin,selectedSOSEvent,out);
  bool have=false;
  for(size_t i=0;i<kSOSMenuCapacity;++i) {
    SOSRecordSnapshot rec;
    if (!getSOSRecordByIndex(i,rec)||!rec.known||rec.cancelled)continue;
    if(!have || (int32_t)(rec.receivedAtMs-out.receivedAtMs)>0) {out=rec;have=true;}
  }
  return have;
}

// -------------------- GPS -------------------------------------
bool gpsHasFix = false;
bool haveLastKnownPosition = false;
double latitude = 0.0;
double longitude = 0.0;
unsigned long lastGPSPrint = 0;


// -------------------- Declarations -----------------------------
void switchI2C(uint8_t sda, uint8_t scl);
void setupPMU();
void setupOLED();
bool detectOLED();
void setupGPS();
void updateGPS();
void printGPSStatus();
void handleOLEDButton();
void updateOLED();
void oledGPSPage();
void oledWeatherPage();
void oledLinkPage();
void oledPDRPage();
void oledAlertPage();
void oledUptimePage();
void oledSOSRescuePage();
#if THESIS_SD_OLED_UI == 1
void oledSDStoragePage();
#endif
void formatUptime(char *out, size_t outSize);

// ============================================================
// I2C MULTIPLEXER
// ============================================================
// Dynamically routes the primary hardware I2C bus to different pins
void switchI2C(uint8_t sda, uint8_t scl) {
  // Avoid repeatedly tearing down and recreating the same bus. The original
  // code called this on every OLED refresh (twice per second), which is
  // unnecessary and can make I2C state errors harder to diagnose.
  if (activeWireSDA == sda && activeWireSCL == scl) {
    return;
  }

  Wire.end();
  delay(2);

  if (!Wire.begin(sda, scl)) {
    activeWireSDA = -1;
    activeWireSCL = -1;
    Serial.printf(
      "[I2C] Failed to initialize SDA=%u SCL=%u\n",
      sda, scl
    );
    return;
  }

  activeWireSDA = sda;
  activeWireSCL = scl;
  Wire.setClock(100000);
  delay(2);
}

// ============================================================
// SETUP
// ============================================================
#if THESIS_SD_LOGGER == 1
// The single logger instance is owned by main.cpp; radio code only queues.
bool thesisLogEvent(bool urgent, const char* kind, uint32_t origin,
                    uint32_t seq, int32_t result, int16_t rssi,
                    int16_t snrX10, int32_t latE7, int32_t lonE7,
                    uint8_t flags, uint8_t hops, uint8_t attempt) {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(!strcmp(kind,"sos_ack_tx") || !strcmp(kind,"cancel_ack_tx") ||
       !strcmp(kind,"sos_ack_rx") || !strcmp(kind,"cancel_ack_rx")){
      F23DSD::Record row{};F23DSD::describe(row,strstr(kind,"_tx")?"ACK_TX":"ACK_RX");
      row.ms=millis();row.origin=origin;row.event=seq;row.rc=result;
      row.rssi=rssi;row.snr_x10=snrX10;row.type=strstr(kind,"cancel")?PACKET_SOS_CANCEL_ACK:PACKET_SOS_ACK;
      (void)ThesisSDLogger::f23dJournal.capture(row);
    }
#endif
// F23D SD end
    return ThesisSDLogger::queueEvent(urgent, kind, origin, seq, result,
                                     rssi, snrX10, latE7, lonE7,
                                     flags, hops, attempt);
}
#endif

// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
extern bool f23dSDRadioBusy();
bool thesisF23dSDCapture(const F23DSD::Record& row){return ThesisSDLogger::f23dJournal.capture(row);}
void thesisF23dSDFormatLoss(){ThesisSDLogger::f23dJournal.formatLost();}
bool thesisF23dSDHealthy(){return ThesisSDLogger::f23dJournal.permissionHealthy() && ThesisSDLogger::ready &&
  ThesisSDLogger::isAuthorizedVessel(THESIS_F23D_SD_ROLE) && !ThesisSDLogger::lostUrgent;}
bool thesisF23dSDCommand(const char* line) {
  if(!strcmp(line,"R23D SD START")){if(!f23dSDRadioBusy())(void)ThesisSDLogger::f23dJournal.start(millis());return true;}
  if(!strcmp(line,"R23D SD SEAL")){(void)ThesisSDLogger::f23dJournal.requestSeal();return true;}
  if(!strcmp(line,"R23D SD STATUS")){
    Serial.printf("[F23D-SD] local=%lu path=%s session=%s ready=%u active=%u sealed=%u pending=%u written=%lu lost=%lu health=%lu\n",
      (unsigned long)VesselIdentity::nodeId(),ThesisSDLogger::f23dWriter.path(),ThesisSDLogger::f23dJournal.session(),unsigned(ThesisSDLogger::f23dJournal.readyToStart()),
      unsigned(ThesisSDLogger::f23dJournal.captureActive()),unsigned(ThesisSDLogger::f23dJournal.sealed()),
      unsigned(ThesisSDLogger::f23dJournal.pending()),(unsigned long)ThesisSDLogger::f23dJournal.written(),
      (unsigned long)ThesisSDLogger::f23dJournal.lost(),(unsigned long)ThesisSDLogger::f23dJournal.health());
    return true;
  }
  return false;
}
#endif
// F23D SD end
#if THESIS_SD_LOGGER == 1
// Existing TEST commands remain handled by processSerialTestCommands().
bool pollSDExportCommand() {
  static char command[24];
  static uint8_t used = 0;
  if (!Serial.available() || (used == 0 && Serial.peek() != 'S')) return false;
  // Only runs after the first 'S' has arrived; await the newline across loops.
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\n' || c == '\r') {
      command[used] = '\0';
      if (strcmp(command, "SD EXPORT") == 0) {
#if THESIS_F06_SD_SANDBOX == 1
        if (ThesisSDLogger::historyReadActive())
          Serial.println("[SDH] ERROR HISTORY_BUSY");
        else
#endif
          ThesisSDLogger::requestExport();
      }
#if THESIS_F06_SD_SANDBOX == 1
      else if (strcmp(command, "SD LIST") == 0)
        ThesisSDLogger::listHistory();
      else if (strncmp(command, "SD READ ", 8) == 0)
        ThesisSDLogger::requestHistory(command + 8);
#endif
      else Serial.println("[SDX] ERROR UNKNOWN_COMMAND");
      used = 0;
      return true;
    }
    if (used >= sizeof(command) - 1) {
      used = 0;
      Serial.println("[SDX] ERROR COMMAND_TOO_LONG");
      return true;
    }
    command[used++] = (char)c;
  }
  return true;
}
#endif

#if THESIS_V003_F15_MACRO_SD_SANDBOX == 1
// A completed F11 median already represents five physical sensor readings.
// Stamp a fresh GNSS UTC value only when date AND time are valid and recent;
// otherwise the logger must serialize gps_utc as JSON null. No extra I2C read.
static void f15QueuePhysicalMacroMedian() {
  weatherMeasurement.gpsUtcValid = false;
  weatherMeasurement.gpsUtcIso8601[0] = '\0';
  if (gps.date.isValid() && gps.time.isValid() &&
      gps.date.age() <= 10000UL && gps.time.age() <= 10000UL) {
    const int n = snprintf(weatherMeasurement.gpsUtcIso8601,
        sizeof(weatherMeasurement.gpsUtcIso8601),
        "%04u-%02u-%02uT%02u:%02u:%02uZ",
        (unsigned)gps.date.year(), (unsigned)gps.date.month(),
        (unsigned)gps.date.day(), (unsigned)gps.time.hour(),
        (unsigned)gps.time.minute(), (unsigned)gps.time.second());
    weatherMeasurement.gpsUtcValid = (n == 20);
  }
#if THESIS_V003_F15_SD_PILOT_ARMED == 1
  const bool queued = ThesisSDLogger::queueWeatherSample(weatherMeasurement);
  Serial.printf("[F15-WX] median_queued=%u inputs=%u history=%u utc_valid=%u\n",
                unsigned(queued), unsigned(weatherMeasurement.inputsOK),
                unsigned(weatherMeasurement.historyCount),
                unsigned(weatherMeasurement.gpsUtcValid));
#else
  Serial.println("[F15-WX] BUILD_ONLY_NO_SD_QUEUE");
#endif
}
#endif

#if THESIS_V003_F19_DUAL_DISPLAY == 1 && THESIS_V003_F15_SD_PILOT_ARMED == 1
// Sampling/persistence are intentionally separate: this queues only RAM records.
// Never alters LoRa, radio scheduler, SOS control, NVS, or the sensor bus.
static void f19r3CaptureResearch30s() {
  static uint32_t lastCaptureMs=0;
  const uint32_t now=millis();
  if (!ThesisSDLogger::ready || !ThesisSDLogger::hybridIsContinuous()) {
    lastCaptureMs=now;return;
  }
  if (uint32_t(now-lastCaptureMs)<30000UL) return;
  lastCaptureMs=now;
  ThesisSDLogger::ResearchSystemSnapshot s{};
  s.uptimeMs=now;s.txOk=txCount;s.txFail=txFailures;
  s.rxTelemetry=getTotalPeerTelemetryReceived();
  s.invalidPackets=invalidPackets;s.peerCount=(uint32_t)getTrackedPeerCount();
  s.gpsFix=gpsHasFix;
  s.positionValid=gpsHasFix && gps.location.isValid() && gps.location.age()<=10000UL;
  if (s.positionValid) {s.latitude=latitude;s.longitude=longitude;}
  s.satellitesValid=gps.satellites.isValid() && gps.satellites.age()<=10000UL;
  if (s.satellitesValid) s.satellites=(uint32_t)gps.satellites.value();
  s.hdopValid=gps.hdop.isValid() && gps.hdop.age()<=10000UL;
  if (s.hdopValid) s.hdop=gps.hdop.hdop();
  s.speedValid=gps.speed.isValid() && gps.speed.age()<=10000UL;
  if (s.speedValid) s.speedKmph=gps.speed.kmph();
  if (gps.date.isValid() && gps.time.isValid() &&
      gps.date.age()<=10000UL && gps.time.age()<=10000UL) {
    const int n=snprintf(s.gpsUtc,sizeof(s.gpsUtc),
        "%04u-%02u-%02uT%02u:%02u:%02uZ",
        (unsigned)gps.date.year(),(unsigned)gps.date.month(),(unsigned)gps.date.day(),
        (unsigned)gps.time.hour(),(unsigned)gps.time.minute(),(unsigned)gps.time.second());
    s.utcValid=(n==20);
  }
  const bool systemQueued=ThesisSDLogger::queueResearchSystem(s);
  uint32_t peerQueued=0,peerRejected=0;
  const size_t peerCount=getTrackedPeerCount();
  // At most 10 bounded peer snapshots per 30 seconds (even inactive peers).
  for(size_t i=0;i<peerCount && i<10U;++i) {
    PeerDisplaySnapshot peer{};
    if(!getPeerSnapshotByIndex(i,peer)) {++peerRejected;continue;}
    if(ThesisSDLogger::queueResearchPeer(peer.id,peer.session,peer.received,
         peer.unique,peer.duplicates,peer.missing,peer.pdr,
         peer.rssi,peer.snr,peer.ageSeconds,peer.active)) ++peerQueued;
    else ++peerRejected;
  }
  Serial.printf("[F19R3-RESEARCH] system_queued=%u peer_queued=%lu peer_rejected=%lu "
                "peers=%u mode=CONTINUOUS pdr=SEQUENCE_SPAN_ESTIMATE\n",
      unsigned(systemQueued),(unsigned long)peerQueued,(unsigned long)peerRejected,
      (unsigned)peerCount);
}
#endif // F19R3 research capture

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("=== UNIVERSAL VESSEL: SCHEDULED TX + RX ===");
  Serial.println("Packet: 62 bytes | MAC adaptive telemetry (min base 5s)");
#if THESIS_V003_F08_INDICATOR_BENCH == 1
  Serial.println("[F08-BENCH] Weather operations DISABLED; physical outputs OFF/unassigned");
#else
  Serial.printf("[WEATHER] %s, sample interval %lu ms; history %u samples\n",
                WEATHER_FAST_TEST ? "ACCELERATED DEVELOPMENT (NOT REAL 100-MIN HISTORY)" : "FIELD",
                WEATHER_INTERVAL, WEATHER_HISTORY_SIZE);
#endif

  SOSControls::begin();
  PhysicalIndicators::begin();
#if THESIS_V003_F08_LED_BENCH == 1
  Serial.printf("[F08-LED] profile=%s buzzer=DISABLED node-gate=ID3\n",
                V003F08LedBench::kOutputsEnabled ? "LED_LIVE" : "BUILD_ONLY");
#endif
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  buttonLastReading = digitalRead(USER_BUTTON_PIN);
  buttonStableState = buttonLastReading;
  buttonLastDebounceTime = millis();
  Serial.println("[OLED] Tap GPIO0: next page; on ALERT page hold >=900ms: LIVE/NORMAL/FRONT/SQUALL demo");

  setupPMU();
#if THESIS_SD_PROBE == 1
  ThesisSD::probe(pmu);
#endif
  setupOLED();
#if THESIS_V003_F08_INDICATOR_BENCH == 1
  // GPIO38/39 are NOT configured for sensor I2C in this build-only profile.
  // Weather alerts are unavailable; authoritative SOS remains independent.
  weatherInputsValid = false;
  weatherState = WEATHER_CALIBRATING;
  Serial.println("[F08-BENCH] DHT20/BME680 initialization SKIPPED");
#elif THESIS_V003_F09_SHARED_SENSOR_BUS == 1
  // Move only this profile's identity check before external sensor access.
  VesselIdentity::load();
  f09SensorAuthorized = V003F09SharedSensorBus::authorized(
      VesselIdentity::ready(), VesselIdentity::nodeId());
#if THESIS_F22_FLEET_OPERATIONAL == 1
  // Override the legacy ID-3 gate only in the new common-fleet profile.
  f09SensorAuthorized = F22UniversalCapability::validIdentity(
      VesselIdentity::ready(), VesselIdentity::nodeId());
#endif
#if THESIS_F22R3A_STANDARD_FLEET_PREVIEW == 1
  // Override the legacy ID3-only rule in the separate BUILD-ONLY preview.
  // Operator MUST confirm physical SDA21/SCL48 standard wiring before flashing.
  f09SensorAuthorized = V003F09SharedSensorBus::authorizedStandardPreview(
      VesselIdentity::ready(), VesselIdentity::nodeId(),
      THESIS_F22R3A_V003_WIRING_EXPLICIT_OPT_IN == 1);
#endif
  Serial.printf("[F09-I2C] id=%lu authorized=%u SDA=21 SCL=48 OUTPUTS=OFF\n",
                (unsigned long)VesselIdentity::nodeId(), unsigned(f09SensorAuthorized));
  if (f09SensorAuthorized) {
    setupSensors();
  } else {
    weatherInputsValid = false;
    weatherState = WEATHER_CALIBRATING;
#if THESIS_F22R3A_STANDARD_FLEET_PREVIEW == 1
    Serial.println("[F22R3A-PREVIEW] SENSOR ACCESS BLOCKED: invalid/unprovisioned identity");
#else
    Serial.println("[F09-I2C] SENSOR ACCESS BLOCKED: requires Vessel-003");
#endif
  }
#else
  setupSensors();
#endif
  setupGPS();
#if THESIS_V003_F09_SHARED_SENSOR_BUS != 1
  VesselIdentity::load();
#endif
#if THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER == 1
  // Observe the existing sensor-init flags ONLY. No additional I2C transaction.
  // The active build's wiring is a compile-time claim, NOT reviewed hardware evidence.
  F22UniversalCapability::Evidence f22Observed{};
  f22Observed.vesselId=VesselIdentity::nodeId();
  f22Observed.provisionedIdentity=VesselIdentity::ready();
#if THESIS_V003_F09_SHARED_SENSOR_BUS == 1
  f22Observed.sensorWiring=F22UniversalCapability::SensorWiring::Shared2148;
#else
  f22Observed.sensorWiring=F22UniversalCapability::SensorWiring::LegacySplit;
#endif
  f22Observed.bme680Responded=bmeAvailable;
  f22Observed.dht20Responded=dhtAvailable;
  // Do not manufacture proof of board/pin approval or SD-card readiness.
  // Their Evidence defaults deliberately keep sensors, SD and GPIO unauthorized.
  const auto f22Decision=F22UniversalCapability::evaluate(f22Observed);
  Serial.printf("[F22R2-PASSIVE] id=%lu provisioned=%u bme_seen=%u dht_seen=%u "
                "board_review=UNKNOWN sensor_eligible=%u weather_complete=%u "
                "sd_eligible=%u gpio=%u auto_enroll=%u NO_NEW_IO\n",
                (unsigned long)f22Observed.vesselId,
                unsigned(f22Decision.identityAccepted),
                unsigned(f22Observed.bme680Responded),unsigned(f22Observed.dht20Responded),
                unsigned(f22Decision.sensorBusEligible),
                unsigned(f22Decision.weatherInputSetComplete),
                unsigned(f22Decision.sdMountEligible),
                unsigned(f22Decision.physicalOutputsEnabled),
                unsigned(f22Decision.autoEnrolled));
#endif
#if THESIS_F22R3B_STORAGE_POLICY_PREVIEW == 1
  // Deliberately supply ONLY validated identity. Every SD/media/review/consent
  // field is false, even if a physical card is inserted. This preview never
  // powers, mounts, reads or writes the card; it does not call ThesisSDLogger.
  F22FleetStoragePolicy::Evidence f22r3bEvidence{};
  f22r3bEvidence.vesselId = VesselIdentity::nodeId();
  f22r3bEvidence.provisionedIdentity = VesselIdentity::ready();
  const auto f22r3bDecision = F22FleetStoragePolicy::evaluate(f22r3bEvidence);
  Serial.printf("[F22R3B-STORAGE-PREVIEW] id=%lu identity=%u mount=%u "
                "sandbox_write=%u research=%u legacy_V002_read=%u "
                "format=%u auto_enroll=%u NO_SD_IO\n",
                (unsigned long)f22r3bEvidence.vesselId,
                unsigned(f22r3bDecision.identityAccepted),
                unsigned(f22r3bDecision.mountEligible),
                unsigned(f22r3bDecision.newSessionWriteEligible),
                unsigned(f22r3bDecision.researchJournalEligible),
                unsigned(f22r3bDecision.legacyV002HistoryReadEligible),
                unsigned(f22r3bDecision.formatOrEraseAllowed),
                unsigned(f22r3bDecision.autoEnroll));
#endif
#line 913
#if THESIS_V003_F08_LED_BENCH == 1
  const bool isVessel003 = VesselIdentity::nodeId() == 3U;
  // Runtime gate: even a misflashed live test cannot drive LED GPIOs on 001/002.
  V003F08LedBench::instance().begin(isVessel003);
  Serial.printf("[F08-LED] vessel_id=%lu authorized=%u GPIO39/44=%s\n",
                (unsigned long)VesselIdentity::nodeId(), unsigned(isVessel003),
                V003F08LedBench::kOutputsEnabled && isVessel003 ? "READY_OFF" : "DISABLED");
#endif
#if THESIS_V003_F08_BUZZER_PREFLIGHT == 1
  const bool isBuzzerVessel003 = VesselIdentity::nodeId() == 3U;
  V003F08BuzzerBench::instance().begin(isBuzzerVessel003);
  Serial.printf("[F08-BUZZER] profile=BUILD_ONLY id=%lu authorized=%u ACTUATION=DISABLED GPIO38_WRITES=0\n",
                (unsigned long)VesselIdentity::nodeId(), unsigned(isBuzzerVessel003));
#endif
#if THESIS_V003_F08_BUZZER_LIVE_PROFILE == 1
  // Firmware sees the PMU enable bit; this is NOT a voltage measurement.
  const bool isLiveBuzzerV003 = VesselIdentity::nodeId() == 3U;
  const bool buzzerRailEnabled = pmu.isEnableBLDO2();
  V003F08BuzzerLive::instance().begin(isLiveBuzzerV003, buzzerRailEnabled);
  Serial.printf("[F08-BUZZER-LIVE] id=%lu pmu_rail_enabled=%u authorized=%u GPIO38=%s\n",
                (unsigned long)VesselIdentity::nodeId(), unsigned(buzzerRailEnabled),
                unsigned(V003F08BuzzerLive::instance().authorized()),
                V003F08BuzzerLive::instance().authorized() ? "READY_OFF" : "DISABLED");
#endif
#if THESIS_V003_F08_COMBINED_PROFILE == 1
  const bool combinedIsV003 = VesselIdentity::nodeId() == 3U;
  const bool combinedRailEnabled = pmu.isEnableBLDO2();
  V003F08CombinedBench::instance().begin(combinedIsV003, combinedRailEnabled);
  Serial.printf("[F08-COMBINED] id=%lu rail=%u authorized=%u GPIO38/39/44=%s DEMO_ONLY\n",
                (unsigned long)VesselIdentity::nodeId(), unsigned(combinedRailEnabled),
                unsigned(V003F08CombinedBench::instance().authorized()),
                V003F08CombinedBench::instance().authorized() ? "READY_OFF" : "DISABLED");
#endif
  setupLoRa();
  setupEmergencyIdentity();
  startReceive();
#if THESIS_SD_LOGGER == 1
#if THESIS_V003_F15_MACRO_SD_SANDBOX == 1
#if THESIS_V003_F15_SD_PILOT_ARMED == 1
  if (f09SensorAuthorized && VesselIdentity::ready() &&
      VesselIdentity::nodeId() == 3U) {
    const bool f15SdReady = ThesisSDLogger::begin(pmu, VesselIdentity::nodeId());
    Serial.printf("[F15-SD] V003_AUTHORIZED; sandbox_mount_ok=%u\n",
                  unsigned(f15SdReady));
  }
#if THESIS_F22_FLEET_OPERATIONAL == 1
  else if (f09SensorAuthorized && VesselIdentity::ready() &&
           F22UniversalCapability::validIdentity(true,VesselIdentity::nodeId())) {
    const bool f15SdReady = ThesisSDLogger::begin(pmu, VesselIdentity::nodeId());
    Serial.printf("[F15-SD] FLEET_AUTHORIZED; sandbox_mount_ok=%u\n",
                  unsigned(f15SdReady));
  }
#endif
  else {
    Serial.println("[F15-SD] BLOCKED: Vessel-003 sensor/identity authorization required");
  }
#else
  Serial.println("[F15-SD] BUILD_ONLY_NO_SD_MOUNT");
#endif
#else
  ThesisSDLogger::begin(pmu, VesselIdentity::nodeId());
#endif
#endif

#if THESIS_F07_WEATHER_LOG == 1 && THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1
  // One REAL reading at boot for immediate OLED fault detection.
  // Log the same physical sample, then continue at 10-minute intervals.
  processWeather();
  if (!ThesisSDLogger::queueWeatherSample(weatherMeasurement))
    Serial.println("[WX-LOG] BOOT_SAMPLE_QUEUE_FAILED (not recorded)");
#endif

  // Each boot receives a different first telemetry slot. Subsequent intervals
  // vary independently; no change to SOS timing or packet serialization.
  randomSeed(esp_random());
  telemetryMac.begin(VesselIdentity::nodeId(), millis(), esp_random());
  Serial.printf("[MAC-INIT] v=3G4E3A id=%lu mode=AIRTIME_ONLY "
                "firstInMs=%lu maxKnownPeers=10 cadMode=%u\n",
    (unsigned long)VesselIdentity::nodeId(),
    (unsigned long)(uint32_t)(telemetryMac.nextDueMs() - millis()), (unsigned)MAC_CAD_MODE);

  lastWeatherTime = millis();
#if THESIS_V003_F12_INDICATOR_PREVIEW == 1
#if THESIS_F22_FLEET_OPERATIONAL == 1
  f12IndicatorPreview.begin(f09SensorAuthorized &&
      F22UniversalCapability::validIdentity(VesselIdentity::ready(),VesselIdentity::nodeId()));
#else
  f12IndicatorPreview.begin(f09SensorAuthorized &&
                            VesselIdentity::ready() && VesselIdentity::nodeId() == 3U);
#endif
  Serial.printf("[F12-PREVIEW] id=%lu authorized=%u GPIO_WRITES=0 "
                "micro=EXPERIMENTAL; weather/SOS routing VIRTUAL_ONLY\n",
                (unsigned long)VesselIdentity::nodeId(),
                unsigned(f12IndicatorPreview.authorized()));
#endif
#if THESIS_V003_F13_SUPERVISED_DRIVER == 1
  // F13 is a separate output owner. Normal F12 and PhysicalIndicators remain
  // software-only; physical outputs need a dedicated explicit LIVE profile.
  f13RailEnableBitCached = pmu.isEnableBLDO2();
  f13LastRailPollMs = millis();
#if THESIS_F22_FLEET_OPERATIONAL == 1
  f13Indicators.begin(f09SensorAuthorized &&
      F22UniversalCapability::validIdentity(VesselIdentity::ready(),VesselIdentity::nodeId()),
      f13RailEnableBitCached);
#else
  f13Indicators.begin(f09SensorAuthorized && VesselIdentity::ready() &&
                      VesselIdentity::nodeId() == 3U, f13RailEnableBitCached);
#endif
  Serial.printf("[F13-DRIVER] id=%lu authorized=%u BLDO2_ENABLE_BIT=%u "
                "GPIO_OUTPUTS=%s micro=VIRTUAL_ONLY; BENCH_ONLY\n",
                (unsigned long)VesselIdentity::nodeId(),
                unsigned(f13Indicators.authorized()), unsigned(f13RailEnableBitCached),
                V003F13Indicators::kPhysicalOutputsEnabled ? "LIVE" : "DISABLED");
#if THESIS_V003_F14_INDICATOR_PROBE == 1
  f14Probe.begin(f13Indicators.authorized() &&
                 V003F13Indicators::kPhysicalOutputsEnabled);
  Serial.printf("[F14-PROBE] authorized=%u limitMs=6000 LOCAL_ONLY "
                "NO_SENSOR_HISTORY_OR_RADIO_WRITES\n",
                unsigned(f14Probe.authorized()));
#endif
#endif
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  f11InitialBatchPending = true;
  f11MacroSampleForMicro = false;
  Serial.println("[F11-MACRO] five physical readings per 10min; gap=2000ms; tendency=100min; OUTPUTS=OFF");
#endif
#if THESIS_V003_F10_MICRO_NOWCAST == 1
  lastF10MicroTime = lastWeatherTime;
  f10InitialSamplePending = true;
  Serial.println("[F10-MICRO] first read at boot; then 60s; 10min window; EXPERIMENTAL risk outlook; OUTPUTS=OFF");
#endif
  lastOLEDUpdate = millis();
  Serial.printf("[UNIVERSAL] %s (ID %lu): firmware initialized; relay forwarding controlled by build; multi-vessel validation PENDING\n",
                VesselIdentity::label(), (unsigned long)VesselIdentity::nodeId());
#if THESIS_LAB_RELAY_TX == 1
  Serial.println("[LAB-RELAY] BUILD=ENABLED; BENCH ONLY; VESSEL-002 GATE");
#else
  Serial.println("[LAB-RELAY] BUILD=DISABLED");
#endif
#if THESIS_MULTIHOP_BENCH_RF == 1
  Serial.printf("[MULTIHOP-BENCH-BOOT] PROFILE=V002_SUPERVISED_R1 identity=%lu session=OFF relay_armed=NO duration_limit_s=600 attempts_limit=128 DO_NOT_DEPLOY\n",
                (unsigned long)VesselIdentity::nodeId());
  Serial.println("[MULTIHOP-BENCH-BOOT] GPIO0=DEV_PDR_PAGE4_HOLD_OFF_PREP_LIVE SD_CONTINUOUS_REQUIRED_FOR_ROUTINE_RELAY_EVIDENCE");
#elif THESIS_MULTIHOP_DEV_TEST == 1
  Serial.println("[MULTIHOP-DEV-BOOT] PROFILE=POSITION_PREVIEW NO_RELAY_RF");
#endif
#if THESIS_V003_F17_INTEGRATED_PREFLIGHT == 1
  Serial.println("[F17-INTEGRATED] F10_FORECAST=EXPERIMENTAL_VIRTUAL F11_MACRO=REAL F12=VIRTUAL F13_GPIO=DISABLED F16_SD_OLED=ENABLED RELAY=OFF");
#endif
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
  Serial.println("[F18-WEATHER] MACRO=EMPIRICAL_100MIN MICRO=EXPERIMENTAL_10_30MIN OLED=LONG_HOLD_PAGE1 SD=V003_SANDBOX GPIO=OFF");
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  Serial.println("[F19-UI] MODE=OPERATIONAL OP_PAGES=4 DEV_PAGES=8 HOME_MENU=HOLD SOS=PRIORITY GPIO=OFF");
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  Serial.println("[F19R3] DEFAULT=SOS_ARMED DEV7=SOS_ONLY DEV8=SD_RECORDING RESEARCH=CONTINUOUS_ONLY HISTORY=READ_ONLY");
#endif
// F23C Phase 1 begin
#if THESIS_F23C_BUILD_ONLY == 1
  Serial.println("[F23C] BUILD_ONLY REAL_RELAY_TX=OFF MODEL=DISARMED AUTH=UNVERIFIED FAR_END=UNVERIFIED");
#endif
// F23C Phase 1 end
// F23D begin
#if THESIS_F23D_CANDIDATE == 1
  Serial.println("[F23D] CANDIDATE=OFFLINE RUN=DISARMED PHYSICAL_PERMISSION=UNAVAILABLE LEGACY_RELAY=OFF AUTH=UNVERIFIED FAR_END=UNVERIFIED");
#endif
// F23D end
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE != 1
// F23D physical end
  const uint32_t f23dExpectedRole=THESIS_F23D_EVENT_QUERY?1U:(THESIS_F23D_RX_OBSERVER?3U:2U);
  Serial.printf("[F23D-READINESS] local=%lu expected=%lu role_ok=%u permission=%u OFFLINE_ONLY=1\n",
      (unsigned long)VesselIdentity::nodeId(),(unsigned long)f23dExpectedRole,
      unsigned(VesselIdentity::ready() && VesselIdentity::nodeId()==f23dExpectedRole),
      unsigned(F23DBench::kPhysicalPermissionInstalled));
// F23D physical begin
#else
  Serial.println("[F23D-PHYSICAL] CANDIDATE_ONLY=1 PLATFORM_EVIDENCE=UNAVAILABLE STARTUP=DISARMED LEGACY_RELAY=OFF");
#endif
// F23D physical end
#endif
// F23D readiness end
  Serial.println("[TEST] Type TEST ALL, TEST NORMAL, TEST FRONT, TEST SQUALL, TEST BOTH, TEST CALIB, TEST Z, or TEST HELP");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
#if THESIS_SD_LOGGER == 1
#if THESIS_F06_SD_HYBRID == 1
  // Assess authoritative local and received SOS state before SD poll.
  // An unresolved SOS keeps recording regardless of manual mode.
  ThesisSDLogger::hybridObserveSOSPresence(
      hasActiveLocalSOS() || getActiveSOSCount() > 0);
#endif
// F23D SD begin
#if !THESIS_F23D_SD_EVIDENCE
// F23D SD end
  ThesisSDLogger::poll();
// F23D SD begin
#endif
// F23D SD end
#endif
#if THESIS_F06_SD_SANDBOX == 1
// F23D SD begin
#if !THESIS_F23D_SD_EVIDENCE
// F23D SD end
  ThesisSDLogger::pollHistory();
// F23D SD begin
#endif
// F23D SD end
#endif
  updateGPS();
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  f21r4PollBattery(); // 15-second cached PMU reading, no OLED I2C reads
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1 && THESIS_V003_F15_SD_PILOT_ARMED == 1
  f19r3CaptureResearch30s();
#endif
#if THESIS_FLEET_USB_ENROLLMENT == 1
  // USB-only: consumes FLEET lines without taking TEST/CANCEL SOS/SD commands.
  const bool fleetSerialConsumed = FleetUSBEnrollment::poll();
#endif
#if THESIS_SD_LOGGER == 1
  const bool sdCommandInProgress = pollSDExportCommand();
// F23D SD begin
#if !THESIS_F23D_SD_EVIDENCE
// F23D SD end
  ThesisSDLogger::pollExport();
// F23D SD begin
#endif
// F23D SD end
#if THESIS_FLEET_USB_ENROLLMENT == 1
  if (!sdCommandInProgress && !fleetSerialConsumed) processSerialTestCommands();
#else
  if (!sdCommandInProgress) processSerialTestCommands();
#endif
#else
#if THESIS_FLEET_USB_ENROLLMENT == 1
  if (!fleetSerialConsumed) processSerialTestCommands();
#else
  processSerialTestCommands();
#endif
#endif
  const auto action = SOSControls::pollAction(hasActiveLocalSOS());
  if (action == SOSControls::ButtonAction::StartSOS) {
    Serial.println("[SOS-BUTTON-A] Deliberate 1.5s SOS hold recognized");
    SOSControls::noteLocalSOS();
    transmitSOS();
  } else if (action == SOSControls::ButtonAction::CancelSOS) {
    Serial.println("[SOS-BUTTON-A] Deliberate 5s CANCEL hold recognized");
    cancelActiveSOS();
  }

  // CAD uses the same DIO1 as RX_DONE; consume its IRQ before RX polling.
  const TelemetryCadResult macCadCompleted = pollTelemetryCad();
  pollLoRaReceive();
  if (consumeNewReceivedSOS()) {
    oledPage = 6;  // New SOS overrides any page/selection.
#if THESIS_V003_F19_DUAL_DISPLAY == 1
    f21r42SOSBrowse.preempt(); // display attention only; radio record unchanged
    f19Display.dismissHomeMenuForSOS(); // a new alert replaces any old menu
    oledPeerMenuOpen=false;
#endif
    sosMenuOpen = false;
#if THESIS_F06_SD_HYBRID == 1
    sdStorageMenuOpen = false;
    sdHybridMenuOpen = false;
    sdOLEDSubview = 0;
#endif
    selectedSOSOrigin=0;selectedSOSEvent=0;
    lastOLEDUpdate = 0;
  }
  processSOSRetries();
  processCancelRetries();
#if THESIS_MULTIHOP_DEV_TEST == 1
  // Must run AFTER operational SOS and CANCEL retry handling.
  multihopDevPollPrepared();
#endif
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
  f23dPollEvidence(); // bounded nonblocking drain after operational controls
#endif
// F23D readiness end
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  ThesisSDLogger::pollF23DEvidence(f23dSDRadioBusy());
#endif
// F23D SD end

#if THESIS_V003_F08_INDICATOR_BENCH == 0
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  if (f09SensorAuthorized) {
    const uint32_t batchNow = millis();
    const bool startDue = f11InitialBatchPending ||
        (!f11MacroBatch.active() && uint32_t(batchNow - lastWeatherTime) >= WEATHER_INTERVAL);
    if (startDue) {
      f11InitialBatchPending = false;
      lastWeatherTime = batchNow;
      f11MacroBatch.start(batchNow);
      Serial.printf("[F11-MACRO] START ms=%lu count=5 gapMs=2000\n",
                    (unsigned long)batchNow);
    }
    if (f11MacroBatch.due(millis())) {
      MicroWeatherSample physical{};
      const bool readOK = readMicroWeatherSample(physical);
      V003F11Macro::Sample sample{};
      if (readOK) {
        sample.temperatureC = physical.temperatureC;
        sample.humidityPercent = physical.humidityPercent;
        sample.stationPressureHpa = physical.stationPressureHpa;
      }
      const bool finished = f11MacroBatch.push(millis(), readOK, sample);
      Serial.printf("[F11-MACRO-READ] n=%u/5 valid=%u\n",
                    unsigned(f11MacroBatch.count()), unsigned(readOK));
      if (finished) {
        const bool batchOK = f11MacroBatch.valid();
        const auto median = f11MacroBatch.result();
        processWeatherFromF11Median(batchOK, median.temperatureC,
                                     median.humidityPercent, median.stationPressureHpa);
        f11MacroSampleForMicro = true; // one fresh, accepted batch; no sixth read
        Serial.printf("[F11-MACRO] samples=5/5 valid=%u Tmed=%.2f RHmed=%.2f "
                      "Pmed=%.2f history=%u/%u dP100=%+.2f Z=%u state=%s\n",
                      unsigned(batchOK && weatherInputsValid),
                      median.temperatureC, median.humidityPercent,
                      median.stationPressureHpa,
                      unsigned(weatherMeasurement.historyCount), unsigned(WEATHER_HISTORY_SIZE),
                      pressureDelta, unsigned(zambrettiIndex), weatherName(weatherState));
#if THESIS_V003_F15_MACRO_SD_SANDBOX == 1
        // Precisely ONE queued record per completed 5-reading macro batch.
        // F10 may reuse this median; it MUST NOT trigger another SD record.
        f15QueuePhysicalMacroMedian();
#endif
      }
    }
  }
#else
  if (millis() - lastWeatherTime >= WEATHER_INTERVAL) {
    lastWeatherTime = millis();
#if THESIS_V003_F09_SHARED_SENSOR_BUS == 1
    if (f09SensorAuthorized) {
      processWeather();
      Serial.printf("[F09-SAMPLE] dht=%u bme_pressure=%u inputs=%u history=%u weather=%s\n",
                    unsigned(dhtReadingValid), unsigned(bmePressureReadingValid),
                    unsigned(weatherInputsValid), unsigned(weatherMeasurement.historyCount),
                    weatherName(weatherState));
    }
#else
    processWeather();
#endif
#if THESIS_SD_LOGGER == 1 && THESIS_F07_WEATHER_LOG == 1
    // GNSS time is usable without a position fix, but only if both the
    // date and time were received recently. Otherwise JSON gets null.
    // Do not change the device clock or synthesize a calendar time.
    if (gps.date.isValid() && gps.time.isValid() &&
        gps.date.age() <= 10000UL && gps.time.age() <= 10000UL) {
      const int n = snprintf(weatherMeasurement.gpsUtcIso8601,
          sizeof(weatherMeasurement.gpsUtcIso8601),
          "%04u-%02u-%02uT%02u:%02u:%02uZ",
          (unsigned)gps.date.year(), (unsigned)gps.date.month(),
          (unsigned)gps.date.day(), (unsigned)gps.time.hour(),
          (unsigned)gps.time.minute(), (unsigned)gps.time.second());
      weatherMeasurement.gpsUtcValid = (n == 20);
    }
    if (!ThesisSDLogger::queueWeatherSample(weatherMeasurement))
      Serial.println("[WX-LOG] QUEUE_FAILED (not recorded)");
#endif
  }
#endif  // THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
#endif  // THESIS_V003_F08_INDICATOR_BENCH == 0

#if THESIS_V003_F10_MICRO_NOWCAST == 1
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  // F11 owns the I2C bus during its nonblocking five-reading batch.
  // The completed median is the micro sample on each shared boundary.
  if (f09SensorAuthorized && !f11MacroBatch.active() &&
      (f11MacroSampleForMicro || f10InitialSamplePending ||
       uint32_t(millis() - lastF10MicroTime) >= V003F10Micro::kIntervalMs)) {
#else
  if (f09SensorAuthorized &&
      (f10InitialSamplePending ||
       uint32_t(millis() - lastF10MicroTime) >= V003F10Micro::kIntervalMs)) {
#endif
    f10InitialSamplePending = false;
    lastF10MicroTime = millis();
    MicroWeatherSample sample{};
    // The macro layer can complete a physical read at the same 10-minute
    // boundary. Reuse its accepted, fresh sample instead of issuing a second
    // back-to-back DHT20/BME680 transaction that can invalidate the micro FIFO.
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
    const bool reusedMacro = f11MacroSampleForMicro &&
        V003F10Micro::shouldReuseMacroSample(
            lastF10MicroTime, weatherSampleAttempted, weatherMeasurement.uptimeMs);
    f11MacroSampleForMicro = false;
#else
    const bool reusedMacro = V003F10Micro::shouldReuseMacroSample(
        lastF10MicroTime, weatherSampleAttempted, weatherMeasurement.uptimeMs);
#endif
    bool valid = false;
    if (reusedMacro) {
      valid = weatherMeasurement.inputsOK && weatherMeasurement.dhtOK &&
              weatherMeasurement.bmePressureOK && weatherMeasurement.tempSource == 1U;
      if (valid) {
        sample.temperatureC = weatherMeasurement.selectedTempC;
        sample.humidityPercent = weatherMeasurement.selectedRH;
        sample.stationPressureHpa = weatherMeasurement.stationPressureHpa;
      }
    } else {
      valid = readMicroWeatherSample(sample);
    }
    if (valid) {
      f10Forecaster.ingest(lastF10MicroTime, sample.temperatureC,
                           sample.humidityPercent, sample.stationPressureHpa);
    } else {
      f10Forecaster.invalidate();
    }
    Serial.printf("[F10-MICRO] valid=%u count=%u/11 state=%s pattern=%s "
                  "dT10=%+.2fC dRH10=%+.2fpp dP10=%+.2fhPa dewGap=%.2fC "
                  "horizon=EXPERIMENTAL_10_30_MIN OUTPUTS=OFF source=%s\n",
                  unsigned(valid), unsigned(f10Forecaster.count()),
                  V003F10Micro::stateName(f10Forecaster.state()),
                  V003F10Micro::patternName(f10Forecaster.pattern()),
                  f10Forecaster.deltaTemperature(), f10Forecaster.deltaHumidity(),
                  f10Forecaster.deltaPressure(), f10Forecaster.dewPointGap(),
                  reusedMacro ? "MACRO_REUSED" : "DIRECT");
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
#if THESIS_V003_F15_SD_PILOT_ARMED == 1
    // Micro data is an independent research product, NOT a physical alert.
    // Same bounded telemetry FIFO as F15: urgent SOS always dequeues first.
    const bool microQueued = ThesisSDLogger::queueMicroOutlook(
        lastF10MicroTime, valid, f10Forecaster.count(),
        V003F10Micro::stateName(f10Forecaster.state()),
        V003F10Micro::patternName(f10Forecaster.pattern()),
        f10Forecaster.deltaTemperature(), f10Forecaster.deltaHumidity(),
        f10Forecaster.deltaPressure(), f10Forecaster.dewPointGap(), reusedMacro);
    Serial.printf("[F18-MICRO-SD] queued=%u state=%s source=%s\n",
                  unsigned(microQueued), V003F10Micro::stateName(f10Forecaster.state()),
                  reusedMacro ? "MACRO_REUSED" : "DIRECT");
#else
    Serial.println("[F18-MICRO-SD] BUILD_ONLY_NO_SD_QUEUE");
#endif
#endif
  }
#endif

  // Live authoritative inputs only; OLED demo modes never reach physical outputs.
  PhysicalIndicators::update(millis(), weatherState, weatherInputsValid,
                             hasActiveLocalSOS(), getActiveSOSCount() > 0);
#if THESIS_V003_F12_INDICATOR_PREVIEW == 1
  // F12 is a separate, VIRTUAL policy instance. Production physical adapter
  // remains disabled. The OLED demo state never enters this decision path.
  const uint32_t f12Now = millis();
  const auto f12Result = f12IndicatorPreview.preview(
      f12Now, weatherState, weatherInputsValid, f10Forecaster.state(),
      hasActiveLocalSOS(), getActiveSOSCount() > 0);
  if (f12Result.authorized &&
      (f12Result.changed || uint32_t(f12Now - f12LastReportMs) >= 60000UL)) {
    f12LastReportMs = f12Now;
    Serial.printf("[F12-PREVIEW] source=%s mode=%u yellow=%u red=%u buzzer=%u "
                  "GPIO_WRITES=0 micro=EXPERIMENTAL\n",
                  V003F12Preview::name(f12Result.source), unsigned(f12Result.mode),
                  unsigned(f12Result.virtualOutputs.yellow),
                  unsigned(f12Result.virtualOutputs.red),
                  unsigned(f12Result.virtualOutputs.buzzer));
  }
#endif
#if THESIS_V003_F13_SUPERVISED_DRIVER == 1
  // Independent F13 production-weather policy; never uses OLED demos or
  // experimental F10 micro-watch as a physical warning source.
  const uint32_t f13Now = millis();
  // PMU enable-bit polling is limited to 1 Hz; sensor/radio loop stays free.
  // This bit is not a measurement of the external rail voltage/current.
  if (uint32_t(f13Now - f13LastRailPollMs) >= 1000U) {
    f13LastRailPollMs = f13Now;
    f13RailEnableBitCached = pmu.isEnableBLDO2();
  }
  uint8_t f13HardwareState = weatherState;
  bool f13HardwareValid = weatherInputsValid;
#if THESIS_V003_F14_INDICATOR_PROBE == 1
  const auto f14Selection = f14Probe.select(
      f13Now, hasActiveLocalSOS() || getActiveSOSCount() > 0,
      f13RailEnableBitCached && !f13Indicators.railLost(),
      weatherState, weatherInputsValid);
  f13HardwareState = f14Selection.state;
  f13HardwareValid = f14Selection.valid;
  if (f14PreviousSynthetic && !f14Selection.synthetic)
    Serial.println("[F14-PROBE] ENDED; REAL_MACRO_RESTORED");
  f14PreviousSynthetic = f14Selection.synthetic;
#endif
  f13Indicators.tick(f13Now, f13HardwareState, f13HardwareValid,
                     hasActiveLocalSOS(), getActiveSOSCount() > 0,
                     f13RailEnableBitCached);
  if (f13Indicators.authorized() &&
      uint32_t(f13Now - f13LastReportMs) >= 60000UL) {
    f13LastReportMs = f13Now;
    const auto physical = f13Indicators.outputs();
    Serial.printf("[F13-DRIVER] mode=%u yellow=%u red=%u buzzer=%u "
                  "GPIO_OUTPUTS=%s railLost=%u micro=VIRTUAL_ONLY\n",
                  unsigned(f13Indicators.mode()), unsigned(physical.yellow),
                  unsigned(physical.red), unsigned(physical.buzzer),
                  V003F13Indicators::kPhysicalOutputsEnabled ? "LIVE" : "DISABLED",
                  unsigned(f13Indicators.railLost()));
  }
#endif
#if THESIS_V003_F08_LED_BENCH == 1
  // SOS preempts any LED test; normal loop scheduling enforces the 500ms limit.
  V003F08LedBench::instance().tick(
      millis(), hasActiveLocalSOS() || getActiveSOSCount() > 0);
#endif
#if THESIS_V003_F08_BUZZER_PREFLIGHT == 1
  // Virtual pulse timeout / SOS preemption; NEVER writes to a GPIO pin.
  V003F08BuzzerBench::instance().tick(
      millis(), hasActiveLocalSOS() || getActiveSOSCount() > 0);
#endif
#if THESIS_V003_F08_BUZZER_LIVE_PROFILE == 1
  // Software timeout. Loop blocking can lengthen 250ms; not a hardware timer.
  V003F08BuzzerLive::instance().tick(
      millis(), hasActiveLocalSOS() || getActiveSOSCount() > 0);
#endif
#if THESIS_V003_F08_COMBINED_PROFILE == 1
  V003F08CombinedBench::instance().tick(
      millis(), hasActiveLocalSOS(), getActiveSOSCount() > 0);
#endif

  // PHASE 5.2.2C-3G-4E2: decentralized, nonblocking CAD experiment.
  // Mode 1 OBSERVE (default): busy CAD is logged but does not block telemetry.
  // Mode 2 ENFORCE (opt-in): finite random backoff, then fail-open TX.
#if MAC_CAD_MODE == 0
  if (telemetryMac.due(millis())) {
    const uint32_t macNowMs = millis();
    const uint32_t macPeers = getRecentMacPeerCount(
        macNowMs, DecentralizedMac::kRecentPeerHorizonMs);
    const uint32_t macIntervalMs = telemetryMac.scheduleNext(macNowMs, macPeers);
    Serial.printf("[MAC-TX] id=%lu mode=OFF peers=%lu baseMs=%lu nextInMs=%lu\n",
      (unsigned long)VesselIdentity::nodeId(), (unsigned long)macPeers,
      (unsigned long)telemetryMac.baseMs(), (unsigned long)macIntervalMs);
    transmitPacket();
  }
#else
  static MacContention::Gate macGate;
  if (!macGate.pending() && telemetryMac.due(millis())) {
    macGate.begin(millis());
  }
  if (macGate.pending()) {
    const uint32_t macNowMs = millis();
    TelemetryCadResult outcome = macCadCompleted;
    if (outcome == TelemetryCadResult::None &&
        !telemetryCadRunning() && macGate.ready(macNowMs)) {
      outcome = startTelemetryCad();
    }
    if (outcome != TelemetryCadResult::None &&
        outcome != TelemetryCadResult::Scanning) {
      MacContention::Sense sense = MacContention::Sense::Error;
      const char *outcomeName = "ERROR";
      switch (outcome) {
        case TelemetryCadResult::Free: sense=MacContention::Sense::Free; outcomeName="FREE"; break;
        case TelemetryCadResult::Busy: sense=MacContention::Sense::Busy; outcomeName="BUSY"; break;
        case TelemetryCadResult::RxActive: sense=MacContention::Sense::RxActive; outcomeName="RX_ACTIVE"; break;
        case TelemetryCadResult::Timeout: sense=MacContention::Sense::Timeout; outcomeName="TIMEOUT"; break;
        case TelemetryCadResult::Interrupted: sense=MacContention::Sense::Interrupted; outcomeName="CONTROL_TX"; break;
        default: break;
      }
      const uint32_t beforeMs = millis();
      const auto decision = macGate.report(beforeMs, sense,
                                           MAC_CAD_MODE, esp_random());
      Serial.printf("[MAC-CAD] id=%lu mode=%s result=%s action=%s "
                    "elapsedMs=%lu retries=%u\n",
        (unsigned long)VesselIdentity::nodeId(),
        MAC_CAD_MODE == 1 ? "OBSERVE" : "ENFORCE", outcomeName,
        decision == MacContention::Decision::Wait ? "BACKOFF" :
        decision == MacContention::Decision::ForcedSend ? "FORCE_SEND" : "SEND",
        (unsigned long)macGate.elapsed(beforeMs), (unsigned)macGate.retries());
      if (decision != MacContention::Decision::Wait) {
        const uint32_t nowTx = millis();
        const uint32_t peers = getRecentMacPeerCount(
            nowTx, DecentralizedMac::kRecentPeerHorizonMs);
        const uint32_t nextMs = telemetryMac.scheduleNext(nowTx, peers);
        macGate.finish(); // reset before the blocking radio transmission
        Serial.printf("[MAC-TX] id=%lu mode=%s peers=%lu nodes=%lu baseMs=%lu nextInMs=%lu\n",
          (unsigned long)VesselIdentity::nodeId(),
          MAC_CAD_MODE == 1 ? "OBSERVE" : "ENFORCE",
          (unsigned long)peers, (unsigned long)(peers + 1UL),
          (unsigned long)telemetryMac.baseMs(), (unsigned long)nextMs);
        transmitPacket(); // original routine TX and RX restoration unchanged
      }
    }
  }
#endif

  if (millis() - lastGPSPrint >= GPS_STATUS_INTERVAL) {
    lastGPSPrint = millis();
    printGPSStatus();
  }

  // Uptime begins at MCU boot and is not dependent on GPS or SD storage.
  static unsigned long lastUptimePrint = 0;
  if (millis() - lastUptimePrint >= UPTIME_PRINT_INTERVAL) {
    lastUptimePrint = millis();
    char uptimeText[40];
    formatUptime(uptimeText, sizeof(uptimeText));
    Serial.printf("[UPTIME-A] %s | Weather=%s | TX_OK=%lu | TX_FAIL=%lu | RX_TELEMETRY=%lu\n",
                  uptimeText, weatherName(weatherState),
                  static_cast<unsigned long>(txCount),
                  static_cast<unsigned long>(txFailures),
                  static_cast<unsigned long>(getTotalPeerTelemetryReceived()));
  }

  handleOLEDButton();

  if (millis() - lastOLEDUpdate >= OLED_UPDATE_INTERVAL) {
    lastOLEDUpdate = millis();
    updateOLED();
  }
  delay(5);
}

// ============================================================
// HARDWARE INITIALIZATION
// ============================================================
// No NVS, radio, SD, sensor, or GPIO interaction: PMU read only.
#if THESIS_V003_F19_DUAL_DISPLAY == 1
void f21r4PollBattery() {
  if(!f21r4PmuReady)return;
  const uint32_t now=millis();
  if(f21r4LastBatteryPoll!=0U && uint32_t(now-f21r4LastBatteryPoll)<15000U)return;
  f21r4LastBatteryPoll=now;
  if(!pmu.isBatteryConnect()){f21r4BatteryPercent=-1;return;}
  const int pct=pmu.getBatteryPercent();
  f21r4BatteryPercent=V003F21R4UI::validBatteryPercent(pct)?pct:-1;
}
#endif
void setupPMU() {
  if (!TBeamHardware::beginPower(PMUWire, pmu)) return;
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  f21r4PmuReady=true;
#endif
  // External BME680 and DHT20 power: independent of microSD logging.
  const bool bldo2Configured = pmu.setBLDO2Voltage(3300);
  if (!bldo2Configured) {
    Serial.println("[PMU] ERROR: BLDO2 sensor rail voltage configuration failed");
  } else {
    pmu.enableBLDO2();
    if (!pmu.isEnableBLDO2()) {
      Serial.println("[PMU] ERROR: BLDO2 sensor rail not enabled");
    } else {
      Serial.println("[PMU] BLDO2 sensor rail enabled (3.3V configured)");
    }
  }
  Serial.println("[PMU] Initialized; OLED and GPS rails enabled");
}

void setupOLED() {
  switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.setClock(400000);
  if (!detectOLED()) {
    Serial.println("[OLED] Not found; continuing without display");
    return;
  }
  oledAvailable = true;
  u8g2.setI2CAddress(oledAddress << 1);
  u8g2.begin();
  u8g2.setContrast(255);
  Serial.println("[OLED] Ready");
}

bool detectOLED() {
  return TBeamHardware::detectOLEDAddress(Wire, oledAddress);
}

// ============================================================
// GPS
// ============================================================
#if THESIS_V003_F19_DUAL_DISPLAY == 1
static GnssPhtClock::Tracker f21PhtClock;
#endif
void setupGPS() {
  GPSService::begin(GPSSerial, "A");
}

void updateGPS() {
  gpsHasFix = GPSService::poll(GPSSerial, gps, latitude, longitude);
  if (gpsHasFix) haveLastKnownPosition = true;
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  // A stationary startup UTC 00:00 is not reliable synchronized GNSS time.
  GnssPhtClock::Utc observed{};
  observed.year=static_cast<uint16_t>(gps.date.year());
  observed.month=static_cast<uint8_t>(gps.date.month());
  observed.day=static_cast<uint8_t>(gps.date.day());
  observed.hour=static_cast<uint8_t>(gps.time.hour());
  observed.minute=static_cast<uint8_t>(gps.time.minute());
  observed.second=static_cast<uint8_t>(gps.time.second());
  observed.fresh=gps.date.isValid() && gps.time.isValid() &&
    gps.date.age()<=GnssPhtClock::kMaxFreshMs &&
    gps.time.age()<=GnssPhtClock::kMaxFreshMs;
  f21PhtClock.observe(observed,millis());
#endif
}

void printGPSStatus() {
  GPSService::printStatus("A", gps, gpsHasFix, latitude, longitude);
}

// OLED BUTTON NAVIGATION
// ============================================================
// GPIO0 user button is active LOW. Debouncing prevents one physical
// press from skipping multiple pages. Page changes only on a new press.
void handleOLEDButton() {
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  // Observe transitions and restore SOS after 20 seconds without a GPIO0 tap.
  // Neither this check nor the OLED button can initiate/cancel real distress.
  if(f21r42SOSBrowse.observe(V003F19Display::emergencyActive(
       hasActiveLocalSOS(),getActiveSOSCount()),millis())) {
    if(!f21r42SOSBrowse.emergency() || f21r42SOSBrowse.forceSOS()) sosMenuOpen=false;
    if(f21r42SOSBrowse.forceSOS()) {
      f19Display.dismissHomeMenuForSOS();
      oledPeerMenuOpen=false;
    }
    lastOLEDUpdate=0;
  }
  // Selection must not obscure an SOS indefinitely when left unattended.
  if(sosMenuOpen && f21r42SOSBrowse.idleForReturn(millis())) {
    sosMenuOpen=false;
    f21r42SOSBrowse.returnToSOS(millis());
    lastOLEDUpdate=0;
  }
#endif
  const int reading = digitalRead(USER_BUTTON_PIN);
  if (OLEDButton::stableEdge(reading, buttonLastReading, buttonStableState,
                             buttonLastDebounceTime, millis(), BUTTON_DEBOUNCE_MS)) {
    if (buttonStableState == LOW) {
      buttonPressStartedAt = millis();
    } else {
      unsigned long duration = millis() - buttonPressStartedAt;
#if THESIS_V003_F19_DUAL_DISPLAY == 1
      // SOS still preempts the display initially and after idle. GPIO0 can
      // VIEW other pages or choose a received SOS without altering distress.
      // GPIO2 remains the only physical SOS start/cancel input.
      const bool f19ActiveSOS=f21r42SOSBrowse.emergency();
      if (f19ActiveSOS) {
#if THESIS_MULTIHOP_BENCH_RF == 1
        // The SOS display path bypasses the DEV PDR page. A long GPIO0 hold
        // must therefore be an emergency relay STOP while PREP/LIVE is active.
        // This changes only V002 lab relay state, never the SOS/CANCEL state.
        if(duration>=BUTTON_LONG_PRESS_MS && multihopDevRequested()) {
          const bool disarmed=multihopDevToggle();
          if(disarmed) {
            Serial.println("[MULTIHOP-BENCH] EMERGENCY_UI_DISARM relay=OFF; SOS_UNCHANGED=1");
            lastOLEDUpdate=0;
            return;
          }
        }
#endif
        // F21R4.3: SOS is always visible first, but during deliberate
        // browsing GPIO0 retains each operational page's own hold menu.
        // Menus are view-only; GPIO2 and the LoRa SOS state are untouched.
        if(sosMenuOpen) {
          if(duration>=BUTTON_LONG_PRESS_MS) confirmSOSMenu();
          else advanceSOSMenu();
          f21r42SOSBrowse.noteInput(millis());
        } else if(f19Display.homeMenuOpen()) {
          if(duration<BUTTON_LONG_PRESS_MS)
            f19Display.moveHomeCursorForSOS(duration);
          else {
            const auto choice=f19Display.confirmHomeMenuForSOS(duration);
            if(choice==V003F19Display::HomeChoice::Portrait ||
               choice==V003F19Display::HomeChoice::Landscape)
              Serial.printf("[F21R43-SOS-UI] ORIENTATION=%s DISPLAY_ONLY\n",
                  f19Display.portrait()?"PORTRAIT":"LANDSCAPE");
            else if(choice==V003F19Display::HomeChoice::None)
              Serial.println("[F21R43-SOS-UI] HOME_CHOICE_LOCKED_WHILE_SOS");
          }
          f21r42SOSBrowse.noteInput(millis());
        } else if(oledPeerMenuOpen) {
          if(duration>=BUTTON_LONG_PRESS_MS) confirmOLEDPeerMenu();
          else advanceOLEDPeerMenu();
          f21r42SOSBrowse.noteInput(millis());
        } else if(duration>=BUTTON_LONG_PRESS_MS) {
          if(f21r42SOSBrowse.forceSOS() || f21r42SOSBrowse.page()==3U) {
            openSOSMenu();
            f21r42SOSBrowse.noteInput(millis());
          } else if(f21r42SOSBrowse.page()==0U) {
            f19Display.openHomeMenuForSOS(duration);
            f21r42SOSBrowse.noteInput(millis());
          } else if(f21r42SOSBrowse.page()==2U) {
            openOLEDPeerMenu();
            f21r42SOSBrowse.noteInput(millis());
          } else {
            f21r42SOSBrowse.returnToSOS(millis()); // WX: hold = SOS shortcut
          }
        } else {
          f21r42SOSBrowse.tap(millis()); // forced SOS -> Home -> WX -> Peers -> SOS
        }
        lastOLEDUpdate=0;return;
      }
      if (f19Display.wake()) {
        Serial.println("[F19-UI] OLED_WAKE DISPLAY_ONLY");
        lastOLEDUpdate=0; return;
      }
      if (f19Display.operational()) {
        if (f19Display.homeMenuOpen()) {
          if (f19Display.moveHomeCursor(duration,false)) {
            Serial.printf("[F19-UI] HOME_MENU_CURSOR=%u\n",
                unsigned(f19Display.homeMenuCursor()));
          } else {
            const auto choice=f19Display.confirmHomeMenu(duration,false);
            if (choice==V003F19Display::HomeChoice::ScreenOff)
              Serial.println("[F19-UI] OLED_SCREEN_OFF ONLY; CORE_RUNNING=1");
            else if (choice==V003F19Display::HomeChoice::Development) {
              oledPage=0; selectedOLEDPeerId=0; oledPeerMenuOpen=false;
              sosMenuOpen=false; alertDemoMode=0; selectedSOSOrigin=0; selectedSOSEvent=0;
#if THESIS_SD_OLED_UI == 1
              sdOLEDSubview=0;
#if THESIS_F06_SD_HYBRID == 1
              sdStorageMenuOpen=false; sdHybridMenuOpen=false;
#endif
#endif
              Serial.println("[F19-UI] MODE=DEVELOPMENT DISPLAY_ONLY OUTPUTS=OFF");
            } else if (choice==V003F19Display::HomeChoice::Back)
              Serial.println("[F19-UI] HOME_MENU_BACK");
            else if (choice==V003F19Display::HomeChoice::Portrait ||
                     choice==V003F19Display::HomeChoice::Landscape)
              Serial.printf("[F21-UI] ORIENTATION_SELECTED=%s VOLATILE=1 DISPLAY_ONLY\n",
                  f19Display.portrait()?"PORTRAIT":"LANDSCAPE");
          }
        } else if (f19Display.operationalPage()==2U && oledPeerMenuOpen) {
          if (duration>=BUTTON_LONG_PRESS_MS) confirmOLEDPeerMenu();
          else advanceOLEDPeerMenu();
        } else if (f19Display.openHomeMenu(duration,false)) {
          Serial.println("[F19-UI] HOME_MENU_OPEN");
        } else if (f19Display.operationalPage()==2U &&
                   duration>=BUTTON_LONG_PRESS_MS) {
          openOLEDPeerMenu();
          Serial.println("[F19-UI] OP_PEER_MENU_OPEN");
        } else if (f19Display.tapOperational(duration,false)) {
          if (f19Display.operationalPage()!=2U) oledPeerMenuOpen=false;
          Serial.printf("[F19-UI] OP_PAGE=%u/4 DISPLAY_ONLY\n",
                        unsigned(f19Display.operationalPage()+1U));
        }
        lastOLEDUpdate=0; return;
      }
      // Development page 1 long hold returns to Operational; all other eight
      // existing F18 development page controls remain unchanged below.
      if (f19Display.returnFromDevelopment(oledPage==0U,duration,false)) {
        oledPage=0; oledPeerMenuOpen=false; sosMenuOpen=false; alertDemoMode=0;
#if THESIS_SD_OLED_UI == 1
        sdOLEDSubview=0;
#if THESIS_F06_SD_HYBRID == 1
        sdHybridMenuOpen=false; sdStorageMenuOpen=false;
#endif
#endif
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
        f18WeatherMicroView=false;
#endif
        Serial.println("[F19-UI] MODE=OPERATIONAL DISPLAY_ONLY OUTPUTS=OFF");
        lastOLEDUpdate=0; return;
      }
#endif
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
      if (oledPage == 1 && duration >= BUTTON_LONG_PRESS_MS) {
        f18WeatherMicroView = !f18WeatherMicroView;
        Serial.printf("[F18-WX-UI] VIEW=%s OUTPUTS=OFF\n",
                      f18WeatherMicroView ? "MICRO_EXPERIMENTAL" : "MACRO_EMPIRICAL");
      } else
#endif
      if (oledPage == 2 && duration >= BUTTON_LONG_PRESS_MS) {
        if (oledPeerMenuOpen) confirmOLEDPeerMenu();
        else openOLEDPeerMenu();
      } else if (oledPeerMenuOpen && oledPage == 2 && duration < BUTTON_LONG_PRESS_MS) {
        advanceOLEDPeerMenu();
      } else if (oledPage == 6 && duration >= BUTTON_LONG_PRESS_MS) {
        if (sosMenuOpen) confirmSOSMenu(); else openSOSMenu();
      } else if (oledPage == 6 && sosMenuOpen && duration < BUTTON_LONG_PRESS_MS) {
        advanceSOSMenu();
#if THESIS_SD_OLED_UI == 1
#if THESIS_F06_SD_HYBRID == 1
      } else if (oledPage == 7 && sdHybridMenuOpen) {
        // Mode submenu: TAP moves, HOLD confirms. BACK returns to SD menu.
        if (duration >= BUTTON_LONG_PRESS_MS) {
          if (sdHybridMenuCursor < 2) {
            ThesisSDLogger::hybridSetContinuous(sdHybridMenuCursor == 1);
            Serial.printf("[OLED-SD] MODE_CONFIRMED=%s\n",
                sdHybridMenuCursor == 1 ? "CONTINUOUS" : "SOS_ARMED");
            sdOLEDSubview = 0;
          } else {
            sdStorageMenuOpen = true;
            sdStorageMenuCursor = 2;
            Serial.println("[OLED-SD] MODE_BACK_TO_SD_MENU");
          }
          sdHybridMenuOpen = false;
        } else {
          sdHybridMenuCursor = (sdHybridMenuCursor + 1U) % 3U;
          Serial.printf("[OLED-SD] MODE_CURSOR=%u\n", (unsigned)sdHybridMenuCursor);
        }
      } else if (oledPage == 7 && sdStorageMenuOpen) {
        // SD menu: Journals, USB, Recording Mode, Back. No direct file writes.
        if (duration >= BUTTON_LONG_PRESS_MS) {
          switch (sdStorageMenuCursor) {
            case 0:
              sdOLEDSubview = 1; // Journals: cached counters only
              sdStorageMenuOpen = false;
              Serial.println("[OLED-SD] OPEN_JOURNALS");
              break;
            case 1:
              sdOLEDSubview = 2; // USB: instructional text only
              sdStorageMenuOpen = false;
              Serial.println("[OLED-SD] OPEN_USB_READBACK");
              break;
            case 2:
              sdStorageMenuOpen = false;
              sdHybridMenuOpen = true;
              sdHybridMenuCursor = ThesisSDLogger::hybridIsContinuous() ? 1 : 0;
              Serial.println("[OLED-SD] MODE_MENU_OPEN");
              break;
            default:
              sdStorageMenuOpen = false;
              sdOLEDSubview = 0;
              Serial.println("[OLED-SD] STORAGE_MENU_BACK");
              break;
          }
        } else {
          sdStorageMenuCursor = (sdStorageMenuCursor + 1U) % 4U;
          Serial.printf("[OLED-SD] STORAGE_CURSOR=%u\n", (unsigned)sdStorageMenuCursor);
        }
      } else if (oledPage == 7 && duration >= BUTTON_LONG_PRESS_MS) {
        // Hold on status, journals or USB always opens the same options menu.
        sdStorageMenuOpen = true;
        sdStorageMenuCursor = 0;
        Serial.println("[OLED-SD] STORAGE_MENU_OPEN");
#else
      } else if (oledPage == 7 && duration >= BUTTON_LONG_PRESS_MS) {
        // Keep original three-view hold navigation for the non-hybrid UI.
        sdOLEDSubview = (sdOLEDSubview + 1U) % 3U;
        Serial.printf("[OLED-SD] VIEW=%u DISPLAY_ONLY\n", (unsigned)sdOLEDSubview);
#endif
#endif
#if THESIS_MULTIHOP_DEV_TEST == 1
      } else if (oledPage == 3 && duration >= BUTTON_LONG_PRESS_MS) {
        // DEV PDR page 4: lab observation toggle only; never grants relay RF.
        // Existing GPIO2 SOS handling and SD recording are unaffected.
        const bool changed=multihopDevToggle();
        Serial.printf("[MULTIHOP-DEV-UI] changed=%u status=%s bench_profile=%u\n",
                      unsigned(changed),multihopDevStatus(),
#if THESIS_MULTIHOP_BENCH_RF == 1
                      1U
#else
                      0U
#endif
                      );
#endif
      } else if (oledPage == 4 && duration >= BUTTON_LONG_PRESS_MS) {
        alertDemoMode = (alertDemoMode + 1) % 4;
        Serial.printf("[ALERT-DEMO] OLED-only mode: %u (0=LIVE,1=NORMAL,2=FRONT,3=SQUALL)\n", alertDemoMode);
      } else if (duration < BUTTON_LONG_PRESS_MS) {
        oledPage = (oledPage + 1) % kOLEDPageCount;
#if THESIS_SD_OLED_UI == 1
        if (oledPage != 7) sdOLEDSubview = 0;
#endif
        // Do not leave a simulated alert active after departing test page.
        if (oledPage != 4) alertDemoMode = 0;
        Serial.printf("[OLED] Page changed to %u\n", oledPage);
      }
      lastOLEDUpdate = 0;
    }
  }
}

// ============================================================
// OLED PAGES
// ============================================================
#if THESIS_V003_F19_DUAL_DISPLAY == 1
// 128x64 landscape, six rows of 6x10 font. Optional portrait hardware
// orientation is NOT enabled until physically inspected and separately tested.
// These renderers use cached sensor/peer/SOS state only; never perform I/O.
void oledF19HomeMenu() {
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"HOME MENU OP 1/4");
  const char *choices[]={"SCREEN OFF","DEVELOPMENT","PORTRAIT","LANDSCAPE","BACK"};
  const uint8_t selected=f19Display.homeMenuCursor();
  const uint8_t first=selected>=3U?uint8_t(selected-2U):0U;
  for (uint8_t row=0;row<3U && first+row<V003F19Display::kHomeChoiceCount;++row) {
    const uint8_t idx=uint8_t(first+row);
    const bool active=(idx==2U && f19Display.portrait()) ||
                      (idx==3U && !f19Display.portrait());
    char line[24];
    snprintf(line,sizeof(line),"%c%c%s",idx==selected?'>':' ',
             active?'*':' ',choices[idx]);
    u8g2.drawStr(0,21+12*row,line);
  }
  u8g2.drawStr(0,62,"TAP:MOVE HOLD:SET");
  u8g2.sendBuffer();
}

void oledF19GPSPage() {
  if (f19Display.homeMenuOpen()) { oledF19HomeMenu(); return; }
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  char line[40];
  if (VesselIdentity::ready())
    snprintf(line,sizeof(line),"V%03lu BAT:-- OP1/4",
             (unsigned long)VesselIdentity::nodeId());
  else strcpy(line,"V--- BAT:-- OP1/4");
  u8g2.drawStr(0,9,line);
  // Apply the same GNSS trust gate to the F19 fallback screen.
  char pht[8];f21PhtClock.phtText(millis(),pht,sizeof(pht));
  if(strcmp(pht,"--:--")==0)strcpy(line,"TIME: --:-- NO SYNC");
  else snprintf(line,sizeof(line),"TIME:%s PHT",pht);
  u8g2.drawStr(0,19,line);
  char uptime[40]; formatUptime(uptime,sizeof(uptime));
  snprintf(line,sizeof(line),"UP:%.17s",uptime);
  u8g2.drawStr(0,29,line);
  // No home anchor or battery fuel-gauge calibration accepted yet.
  u8g2.drawStr(0,39,"H DST: -- (NOT SET)");
  unsigned livePeers=0; uint32_t previous=0,next=0;
  while (livePeers<32U && nextActivePeerId(previous,next) && next>previous) {
    ++livePeers; previous=next;
  }
  snprintf(line,sizeof(line),"GPS:%s PEERS:%u",gpsHasFix?"FIX":"NO FIX",livePeers);
  u8g2.drawStr(0,49,line);
  u8g2.drawStr(0,59,"HOLD:MENU TAP:NEXT");
  u8g2.sendBuffer();
}

void oledF19WeatherPage() {
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];
  u8g2.drawStr(0,9,"WEATHER OP 2/4");
  const bool fresh=weatherSampleAttempted && weatherInputsValid &&
      uint32_t(millis()-lastWeatherTime)<=720000UL;
  // Current observational availability, NOT a direct sea-state diagnosis.
  if (!weatherSampleAttempted) strcpy(line,"CURRENT: WAITING");
  else if (!weatherInputsValid) strcpy(line,"CURRENT: SENSOR FAIL");
  else if (!fresh) strcpy(line,"CURRENT: DATA STALE");
  else strcpy(line,"CURRENT: DATA VALID");
  u8g2.drawStr(0,19,line);
  const bool ready=fresh && weatherState!=WEATHER_CALIBRATING &&
      weatherMeasurement.historyCount>=WEATHER_HISTORY_SIZE;
  if (!fresh) strcpy(line,"OUTLOOK: UNAVAILABLE");
  else if (!ready) strcpy(line,"OUTLOOK: CALIBRATING");
  else snprintf(line,sizeof(line),"OUTLOOK: %.12s",shortWeatherName(weatherState));
  u8g2.drawStr(0,29,line);
  if (ready) snprintf(line,sizeof(line),"Z:%u HIST:%u/%u",
      unsigned(zambrettiIndex),unsigned(weatherMeasurement.historyCount),
      unsigned(WEATHER_HISTORY_SIZE));
  else snprintf(line,sizeof(line),"Z:-- HIST:%u/%u",
      unsigned(weatherMeasurement.historyCount),unsigned(WEATHER_HISTORY_SIZE));
  u8g2.drawStr(0,39,line);
  if (!weatherSampleAttempted) strcpy(line,"UPDATE: NOT YET");
  else snprintf(line,sizeof(line),"UPDATE: %lum AGO",
      (unsigned long)(uint32_t(millis()-lastWeatherTime)/60000UL));
  u8g2.drawStr(0,49,line);
  u8g2.drawStr(0,59,"WEATHER GUIDANCE ONLY");
  u8g2.sendBuffer();
}

void oledF19PeerMenu() {
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"SELECT PEER OP 3/4");
  const uint8_t total=oledMenuPeerCount+2U;
  const uint8_t first=oledMenuCursor>=3U?oledMenuCursor-3U:0U;
  for (uint8_t row=0;row<4U && first+row<total;++row) {
    const uint8_t idx=first+row;
    char label[28];
    if (idx==0) strcpy(label,"AUTO");
    else if (idx==oledMenuPeerCount+1U) strcpy(label,"BACK");
    else snprintf(label,sizeof(label),"VESSEL-%03lu",
        (unsigned long)oledMenuPeerIds[idx-1U]);
    char line[32]; snprintf(line,sizeof(line),"%c %s",
        idx==oledMenuCursor?'>':' ',label);
    u8g2.drawStr(0,20+10*row,line);
  }
  u8g2.drawStr(0,60,"TAP:MOVE HOLD:SET");
  u8g2.sendBuffer();
}

void oledF19NearbyPage() {
  if (oledPeerMenuOpen) { oledF19PeerMenu(); return; }
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  char line[36];
  unsigned activeCount=0; uint32_t previous=0,next=0;
  while (activeCount<32U && nextActivePeerId(previous,next) && next>previous) {
    ++activeCount; previous=next;
  }
  snprintf(line,sizeof(line),"PEERS:%u OP 3/4",activeCount);
  u8g2.drawStr(0,9,line);
  if (selectedOLEDPeerId!=0U) {
    PeerDisplaySnapshot peer{};
    if (!getPeerSnapshotById(selectedOLEDPeerId,peer) || !peer.active) {
      u8g2.drawStr(0,20,"SELECTED PEER LOST");
      u8g2.drawStr(0,30,"DIST: UNKNOWN");
      u8g2.drawStr(0,40,"DIR: UNKNOWN");
    } else {
      snprintf(line,sizeof(line),"SELECTED: V%03lu",(unsigned long)peer.id);
      u8g2.drawStr(0,20,line);
      if (gpsHasFix && peer.gpsValid && isfinite(latitude) &&
          isfinite(longitude) && isfinite(peer.latitude) && isfinite(peer.longitude)) {
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
                            peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,
                            peer.latitude,peer.longitude);
        if (isfinite(km)) snprintf(line,sizeof(line),"DIST:%.2fkm",km);
        else strcpy(line,"DIST: UNKNOWN");
        u8g2.drawStr(0,30,line);
        snprintf(line,sizeof(line),"DIR:%s TRUE",V003F19Display::cardinal8(brg));
      } else {
        u8g2.drawStr(0,30,"DIST: UNKNOWN");
        strcpy(line,"DIR: UNKNOWN");
      }
      u8g2.drawStr(0,40,line);
      snprintf(line,sizeof(line),"LAST:%lus",(unsigned long)peer.ageSeconds);
      u8g2.drawStr(0,50,line);
    }
  } else {
    previous=0;next=0;
    for (uint8_t row=0;row<3U && nextActivePeerId(previous,next) && next>previous;++row) {
      previous=next;
      PeerDisplaySnapshot peer{};
      if (!getPeerSnapshotById(next,peer) || !peer.active) continue;
      if (gpsHasFix && peer.gpsValid && isfinite(latitude) && isfinite(longitude) &&
          isfinite(peer.latitude) && isfinite(peer.longitude)) {
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
                           peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,
                           peer.latitude,peer.longitude);
        if (isfinite(km)) snprintf(line,sizeof(line),"V%03lu %.1fkm %s",
           (unsigned long)peer.id,km,V003F19Display::cardinal8(brg));
        else snprintf(line,sizeof(line),"V%03lu DIST:--",(unsigned long)peer.id);
      } else snprintf(line,sizeof(line),"V%03lu DIST:--",(unsigned long)peer.id);
      u8g2.drawStr(0,20+10*row,line);
    }
    if (!activeCount) u8g2.drawStr(0,30,"NO LIVE PEERS");
  }
  u8g2.drawStr(0,60,"HOLD:SELECT TAP:NEXT");
  u8g2.sendBuffer();
}

void oledF19SOSPage() {
  const bool local=hasActiveLocalSOS();
  const unsigned incoming=getActiveSOSCount();
  const bool distress=local || incoming>0U;
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  char line[40];
  u8g2.drawStr(0,9,distress?"!! SOS ACTIVE !!":"SOS STATUS OP 4/4");
  u8g2.drawStr(0,19,local?"LOCAL: ACTIVE":"LOCAL: INACTIVE");
  snprintf(line,sizeof(line),"INCOMING:%u",incoming);
  u8g2.drawStr(0,29,line);
  if (local) {
    u8g2.drawStr(0,39,"BOAT REPLY: UNKNOWN");
    u8g2.drawStr(0,49,gpsHasFix?"GPS: FIX":"GPS: NO FIX");
    u8g2.drawStr(0,59,"GPIO2: SOS CONTROL");
    u8g2.sendBuffer(); return;
  }
  if (!incoming) {
    u8g2.drawStr(0,39,"NO ACTIVE DISTRESS");
    u8g2.drawStr(0,49,"GPIO2: SOS CONTROL");
    u8g2.drawStr(0,59,"TAP: NEXT");
    u8g2.sendBuffer(); return;
  }
  SOSRecordSnapshot chosen{}; bool found=false;
  for (size_t i=0;i<kSOSMenuCapacity;++i) {
    SOSRecordSnapshot rec{};
    if (!getSOSRecordByIndex(i,rec) || !rec.known || rec.cancelled) continue;
    if (!found || int32_t(rec.receivedAtMs-chosen.receivedAtMs)>0) {
      chosen=rec;found=true;
    }
  }
  if (!found) {
    u8g2.drawStr(0,39,"RX SOS: DATA PENDING");
    u8g2.drawStr(0,49,"DIST: UNKNOWN");
    u8g2.drawStr(0,59,"DIR: UNKNOWN");
    u8g2.sendBuffer();return;
  }
  snprintf(line,sizeof(line),"FROM:V%03lu EVENT:%lu",
      (unsigned long)chosen.originId,(unsigned long)chosen.eventId);
  u8g2.drawStr(0,39,line);
  if (gpsHasFix && chosen.positionValid && isfinite(latitude) && isfinite(longitude) &&
      isfinite(chosen.latitude) && isfinite(chosen.longitude)) {
    const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
                     chosen.latitude,chosen.longitude)/1000.0;
    const double bearing=TinyGPSPlus::courseTo(latitude,longitude,
                     chosen.latitude,chosen.longitude);
    if (isfinite(km)) snprintf(line,sizeof(line),"DIST:%.2fkm (LAST)",km);
    else strcpy(line,"DIST: UNKNOWN");
    u8g2.drawStr(0,49,line);
    snprintf(line,sizeof(line),"DIR:%s TRUE (LAST)",
             V003F19Display::cardinal8(bearing));
    u8g2.drawStr(0,59,line);
  } else {
    u8g2.drawStr(0,49,"DIST: UNKNOWN");
    u8g2.drawStr(0,59,"DIR: UNKNOWN");
  }
  u8g2.sendBuffer();
}
#endif // F19 renderer

#if THESIS_V003_F19_DUAL_DISPLAY == 1
// F21R3: OLED-only icon/phone-style OP presentation. Do not draw page numbers.
// Physical OLED 128x64 (R0), logical 64x128 portrait (R1).
// NO weather, SOS, GPIO, LoRa, or SD processing is performed here.
namespace F21Icon {enum Kind : uint8_t { Boat, Battery, Pin, Clock, Home, Peers, Cloud, Radio, Alert, Compass };}
// Minimal vector icons; monochrome, 8-10px, readable on SH1106.
void oledF21Icon(F21Icon::Kind kind, int x, int y) {
  switch(kind) {
    case F21Icon::Boat:
      u8g2.drawHLine(x+1,y+6,8);u8g2.drawLine(x+2,y+4,x+8,y+4);
      u8g2.drawLine(x+1,y+5,x+3,y+8);u8g2.drawLine(x+3,y+8,x+7,y+8);
      u8g2.drawLine(x+7,y+8,x+9,y+5);u8g2.drawBox(x+4,y+1,3,3);break;
    case F21Icon::Battery:
      u8g2.drawFrame(x,y+1,9,6);u8g2.drawBox(x+9,y+3,1,2);break;
    case F21Icon::Pin:
      u8g2.drawCircle(x+5,y+3,3);u8g2.drawDisc(x+5,y+3,1);
      u8g2.drawLine(x+3,y+6,x+5,y+9);u8g2.drawLine(x+7,y+6,x+5,y+9);break;
    case F21Icon::Clock:
      u8g2.drawCircle(x+5,y+5,4);u8g2.drawLine(x+5,y+5,x+5,y+2);
      u8g2.drawLine(x+5,y+5,x+7,y+6);break;
    case F21Icon::Home:
      u8g2.drawLine(x,y+4,x+5,y);u8g2.drawLine(x+5,y,x+10,y+4);
      u8g2.drawFrame(x+2,y+4,7,5);break;
    case F21Icon::Peers:
      u8g2.drawCircle(x+3,y+3,2);u8g2.drawCircle(x+8,y+3,2);
      u8g2.drawHLine(x,y+8,5);u8g2.drawHLine(x+6,y+8,5);break;
    case F21Icon::Cloud:
      u8g2.drawCircle(x+4,y+4,3);u8g2.drawCircle(x+7,y+3,2);
      u8g2.drawHLine(x+1,y+7,10);break;
    case F21Icon::Radio:
      u8g2.drawLine(x+5,y+9,x+5,y+2);u8g2.drawLine(x+2,y+9,x+8,y+9);
      u8g2.drawCircle(x+5,y+2,1);u8g2.drawLine(x+2,y+1,x,y+3);
      u8g2.drawLine(x+8,y+1,x+10,y+3);break;
    case F21Icon::Alert:
      u8g2.drawLine(x+5,y,x,y+9);u8g2.drawLine(x,y+9,x+10,y+9);
      u8g2.drawLine(x+10,y+9,x+5,y);u8g2.drawLine(x+5,y+3,x+5,y+6);
      u8g2.drawDisc(x+5,y+8,1);break;
    case F21Icon::Compass:
      u8g2.drawCircle(x+5,y+5,4);u8g2.drawLine(x+5,y+1,x+5,y+8);
      u8g2.drawLine(x+5,y+1,x+3,y+4);u8g2.drawLine(x+5,y+1,x+7,y+4);break;
  }
}
void oledF21P(int x,int baseline,const char* msg) {
  if(!msg||x<0||x>=64) return;
  char b[13];const int cap=(64-x)/5;
  snprintf(b,sizeof(b),"%.*s",cap<12?cap:12,msg);u8g2.drawStr(x,baseline,b);
}
void oledF21L(int x,int baseline,const char* msg) {
  if(!msg||x<0||x>=128) return;
  char b[22];const int cap=(128-x)/6;
  snprintf(b,sizeof(b),"%.*s",cap<21?cap:21,msg);u8g2.drawStr(x,baseline,b);
}
F21Icon::Kind oledF21HeaderIcon(const char* title) {
  if(strncmp(title,"SOS",3)==0||strstr(title,"SOS"))return F21Icon::Alert;
  if(strstr(title,"WEATHER"))return F21Icon::Cloud;
  if(strstr(title,"PEER")||strstr(title,"NEARBY"))return F21Icon::Radio;
  return F21Icon::Boat;
}
void oledF21PortraitTop(const char* title,bool alert=false,bool vesselHome=false) {
  u8g2.clearBuffer();u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawBox(0,0,64,13);u8g2.setDrawColor(0);
  // Keep the full-width portrait HOME title (no boat icon) for EVERY
  // provisioned vessel, not just the original hardcoded V003.
  if(vesselHome)oledF21P(3,10,title);
  else{oledF21Icon(oledF21HeaderIcon(title),2,1);oledF21P(13,10,title);}
  u8g2.setDrawColor(1);
  if(alert)u8g2.drawFrame(0,14,64,102);
}
void oledF21LandscapeTop(const char* title,bool alert=false) {
  u8g2.clearBuffer();u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawBox(0,0,128,13);u8g2.setDrawColor(0);
  oledF21Icon(oledF21HeaderIcon(title),3,1);oledF21L(17,10,title);
  u8g2.setDrawColor(1);
  if(alert)u8g2.drawFrame(0,14,128,40);
}
void oledF21Dots(bool portrait,uint8_t page) {
  const int y=portrait?124:60;const int center=portrait?32:107;
  // Persistent distress cue whenever the operator browses another OP page.
  // Page 3 already has the unmistakable SOS header and SELECT hint.
  if(page!=3U && V003F19Display::emergencyActive(
       hasActiveLocalSOS(),getActiveSOSCount())) {
    u8g2.setFont(u8g2_font_5x8_tf);
    if(portrait)u8g2.drawStr(0,126,"SOS"); // x0..14; dots start at x17
    else u8g2.drawStr(3,62,"SOS ACTIVE"); // ends before dots at x92
  }
  for(uint8_t i=0;i<4U;++i){const int x=center-15+10*i;
    if(i==page)u8g2.drawDisc(x,y,2);else u8g2.drawCircle(x,y,2);
  }
  u8g2.sendBuffer();
}
void oledF21MenuFooter(bool portrait) {
  u8g2.setFont(u8g2_font_5x8_tf);
  if(portrait){u8g2.drawHLine(0,113,64);oledF21P(2,124,"TAP/HOLD:SET");}
  else{u8g2.drawHLine(0,54,128);oledF21P(3,63,"TAP MOVE");oledF21L(68,63,"HOLD SET");}
  u8g2.sendBuffer();
}
void oledF21PortraitHomeMenu(){
  const bool distress=V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount());
  oledF21PortraitTop(distress?"SOS MENU":"DISPLAY");
  const char*const names[]={"SCREEN OFF","DEV MODE","PORTRAIT","LANDSCAPE","BACK"};
  for(uint8_t i=0;i<V003F19Display::kHomeChoiceCount;++i){
    const bool active=(i==2U&&f19Display.portrait())||(i==3U&&!f19Display.portrait());
    const bool locked=distress && i<2U;
    char b[22];snprintf(b,sizeof(b),"%c%c%s",i==f19Display.homeMenuCursor()?'>':' ',
      locked?'X':active?'*':' ',names[i]);oledF21P(2,26+17*i,b);
  }
  oledF21MenuFooter(true);
}
void oledF21LandscapeHomeMenu(){
  const bool distress=V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount());
  oledF21LandscapeTop(distress?"SOS HOME MENU":"DISPLAY MENU");
  const char*const names[]={"SCREEN OFF","DEVELOPMENT","PORTRAIT","LANDSCAPE","BACK"};
  const uint8_t chosen=f19Display.homeMenuCursor();
  const uint8_t first=chosen>=3U?uint8_t(chosen-2U):0U;
  for(uint8_t row=0;row<3U&&first+row<V003F19Display::kHomeChoiceCount;++row){
    const uint8_t i=uint8_t(first+row);
    const bool active=(i==2U&&f19Display.portrait())||(i==3U&&!f19Display.portrait());
    const bool locked=distress && i<2U;
    char b[26];snprintf(b,sizeof(b),"%c%c %s",i==chosen?'>':' ',locked?'X':active?'*':' ',names[i]);
    oledF21L(5,25+12*row,b);
  }
  oledF21MenuFooter(false);
}
// F21R4: cached PMU gauge estimate, never read I2C from a renderer.
void oledF21BatteryLabel(int x,int y,bool portrait) {
  char label[8];V003F21R4UI::batteryText(f21r4BatteryPercent,label,sizeof(label));
  // Battery percentage: 6x10 in portrait, 5x8 in landscape (unchanged).
  const uint8_t *font=portrait?u8g2_font_6x10_tf:u8g2_font_5x8_tf;
  u8g2.setFont(font);u8g2.drawStr(x,y,label);u8g2.setFont(u8g2_font_5x8_tf);
}
void oledF21HomeLabel(char* dst,size_t n) {
  const bool good=V003F21R4UI::freshPosition(gpsHasFix,gps.location.isValid(),
                   gps.location.age(),latitude,longitude);
  if(!good){V003F21R4UI::distanceText(NAN,dst,n);return;}
  const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
      V003F21R4UI::kHomeLatitude,V003F21R4UI::kHomeLongitude)/1000.0;
  V003F21R4UI::distanceText(km,dst,n);
}
void oledF21PortraitGPSPage(){
  if(f19Display.homeMenuOpen()){oledF21PortraitHomeMenu();return;}
  const uint32_t vesselId=VesselIdentity::nodeId();
  const bool vesselReady=VesselIdentity::ready();
  char vesselTitle[8];FleetHomeTitle::shortTitle(vesselId,vesselReady,
                                                vesselTitle,sizeof(vesselTitle));
  char b[42];oledF21PortraitTop(vesselTitle,false,true);
  // For V001..V999, preserve the V003 portrait layout exactly:
  // title x3, battery icon x26, percentage x38. V1000+ or NO ID gets
  // the full title and the percentage without an overlapping icon.
  u8g2.setDrawColor(0);
  if(FleetHomeTitle::portraitBatteryIconFits(vesselId,vesselReady))
    oledF21Icon(F21Icon::Battery,26,2);
  oledF21BatteryLabel(38,11,true);u8g2.setDrawColor(1);
  u8g2.drawFrame(1,16,62,35);
  u8g2.setFont(u8g2_font_5x8_tf);oledF21P(5,26,"TIME / PHT");
  // Display valid advancing GNSS time only; never invent a startup clock.
  f21PhtClock.phtText(millis(),b,sizeof(b));
  u8g2.setFont(u8g2_font_7x14B_tf);u8g2.drawStr(10,45,b);
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawHLine(2,55,60);
  oledF21P(3,67,gpsHasFix?"GPS FIX":"GPS NO FIX");
  u8g2.drawHLine(2,71,60);
  V003F21R4UI::uptimeText(millis(),b,sizeof(b));oledF21P(3,82,b);
  u8g2.drawHLine(2,86,60);
  oledF21HomeLabel(b,sizeof(b));oledF21P(3,97,b);
  u8g2.drawHLine(2,101,60);
  unsigned peers=0;uint32_t prev=0,nxt=0;
  while(peers<32U&&nextActivePeerId(prev,nxt)&&nxt>prev){++peers;prev=nxt;}
  snprintf(b,sizeof(b),"PEERS %u",peers);oledF21P(3,112,b);
  // No portrait footer instructions; the navigation dots are sufficient.
  oledF21Dots(true,0);
}
void oledF21LandscapeGPSPage(){
  if(f19Display.homeMenuOpen()){oledF21LandscapeHomeMenu();return;}
  char vesselTitle[16];FleetHomeTitle::landscapeTitle(
      VesselIdentity::nodeId(),VesselIdentity::ready(),
      vesselTitle,sizeof(vesselTitle));
  char b[42];oledF21LandscapeTop(vesselTitle);
  u8g2.setDrawColor(0);oledF21Icon(F21Icon::Battery,91,2);
  oledF21BatteryLabel(104,10,false);u8g2.setDrawColor(1);
  u8g2.drawFrame(1,15,62,39);u8g2.drawFrame(66,15,61,39);
  u8g2.setFont(u8g2_font_5x8_tf);oledF21P(6,23,"TIME PHT");
  // Display valid advancing GNSS time only; never invent a startup clock.
  f21PhtClock.phtText(millis(),b,sizeof(b));
  u8g2.setFont(u8g2_font_7x14B_tf);u8g2.drawStr(6,41,b);
  u8g2.setFont(u8g2_font_5x8_tf);
  oledF21HomeLabel(b,sizeof(b));
  if(strlen(b)>11U)strcpy(b,"HOME >99km");
  oledF21P(6,50,b);
  oledF21Icon(F21Icon::Pin,68,18);oledF21L(81,25,gpsHasFix?"GPS FIX":"GPS --");
  unsigned peers=0;uint32_t prev=0,nxt=0;
  while(peers<32U&&nextActivePeerId(prev,nxt)&&nxt>prev){++peers;prev=nxt;}
  oledF21Icon(F21Icon::Peers,68,30);
  snprintf(b,sizeof(b),"PEERS %u",peers);oledF21L(81,38,b);
  oledF21Icon(F21Icon::Clock,68,42);
  V003F21R4UI::shortUptime(millis(),b,sizeof(b));oledF21L(81,50,b);
  // Keep the landscape help text, as requested.
  if(!V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount()))
    u8g2.drawStr(3,62,"HOLD MENU");
  oledF21Dots(false,0);
}
V003F21WeatherDisplay::View oledF21WeatherView(){
  V003F21WeatherDisplay::Input x{};
  x.nowMs=millis();x.macroAttempted=weatherSampleAttempted;
  x.macroInputsValid=weatherInputsValid;x.macroLastStartMs=lastWeatherTime;
  x.macroHistoryCount=weatherMeasurement.historyCount;x.macroState=weatherState;
  x.macroPressureFinite=isfinite(pressureDelta);
  x.microLastMs=lastF10MicroTime;x.microHistoryCount=f10Forecaster.count();
  x.microState=static_cast<uint8_t>(f10Forecaster.state());
  return V003F21WeatherDisplay::select(x);
}
void oledF21PortraitWeatherPage(){
  oledF21PortraitTop("WEATHER");
  const auto v=oledF21WeatherView();
  u8g2.drawFrame(1,16,62,38);
  u8g2.setFont(u8g2_font_5x8_tf);oledF21P(5,26,"CURRENT");
  const char*label=V003F21WeatherDisplay::currentText(v.current);
  const int w=int(strlen(label))*7;
  u8g2.setFont(u8g2_font_7x14B_tf);u8g2.drawStr(w<=60?(64-w)/2:2,46,label);
  u8g2.drawFrame(1,56,62,49);
  u8g2.setFont(u8g2_font_5x8_tf);oledF21P(5,66,"OUTLOOK");
  const char*state=V003F21WeatherDisplay::outlookText(v.outlook);
  if(v.outlook==V003F21WeatherDisplay::Outlook::FrontSignal||
     v.outlook==V003F21WeatherDisplay::Outlook::SquallSignal){
    oledF21P(5,81,v.outlook==V003F21WeatherDisplay::Outlook::FrontSignal?"FRONT":"SQUALL");
    oledF21P(5,90,"SIGNAL");
  }else oledF21P(5,84,state);
  oledF21P(5,100,V003F21WeatherDisplay::horizonText(v.horizon));
  // F21R4: remove cosmetic GAP/GUIDE footer; status labels remain authoritative.
  oledF21Dots(true,1);
}
void oledF21LandscapeWeatherPage(){
  oledF21LandscapeTop("WEATHER");
  const auto v=oledF21WeatherView();
  u8g2.drawFrame(1,15,61,39);u8g2.drawFrame(65,15,62,39);
  u8g2.setFont(u8g2_font_5x8_tf);u8g2.drawStr(4,24,"CURRENT");
  const char*label=V003F21WeatherDisplay::currentText(v.current);
  u8g2.setFont(u8g2_font_7x14B_tf);u8g2.drawStr(4,43,label);
  u8g2.setFont(u8g2_font_5x8_tf);u8g2.drawStr(68,24,"OUTLOOK");
  const char*state=V003F21WeatherDisplay::outlookText(v.outlook);
  if(v.outlook==V003F21WeatherDisplay::Outlook::FrontSignal||
     v.outlook==V003F21WeatherDisplay::Outlook::SquallSignal){
    u8g2.drawStr(68,34,v.outlook==V003F21WeatherDisplay::Outlook::FrontSignal?"FRONT":"SQUALL");
    u8g2.drawStr(68,42,"SIGNAL");
  }else{
    // Bound the 62px card rather than silently run into the right edge.
    char shortState[12];snprintf(shortState,sizeof(shortState),"%.11s",state);
    u8g2.drawStr(68,38,shortState);
  }
  u8g2.drawStr(68,51,V003F21WeatherDisplay::horizonText(v.horizon));
  // F21R4: no cosmetic weather guidance footer.
  oledF21Dots(false,1);
}
void oledF21PortraitPeerMenu(){
  oledF21PortraitTop(V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount())?
                     "SOS PEERS":"PEER MENU");
  const uint8_t total=uint8_t(oledMenuPeerCount+2U);
  const uint8_t first=oledMenuCursor>=7U?uint8_t(oledMenuCursor-6U):0U;
  for(uint8_t row=0;row<8U && uint8_t(first+row)<total;++row){
    const uint8_t idx=uint8_t(first+row);
    char label[24],b[28];
    if(idx==0U)strcpy(label,"AUTO");
    else if(idx==oledMenuPeerCount+1U)strcpy(label,"BACK");
    else snprintf(label,sizeof(label),"V%03lu",(unsigned long)oledMenuPeerIds[idx-1U]);
    snprintf(b,sizeof(b),"%c %s",idx==oledMenuCursor?'>':' ',label);
    oledF21P(3,23+11*row,b);
  }
  oledF21MenuFooter(true);
}
void oledF21LandscapePeerMenu(){
  oledF21LandscapeTop(V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount())?
                      "SOS SELECT PEER":"SELECT PEER");
  const uint8_t total=uint8_t(oledMenuPeerCount+2U);
  const uint8_t first=oledMenuCursor>=3U?uint8_t(oledMenuCursor-2U):0U;
  for(uint8_t row=0;row<3U && uint8_t(first+row)<total;++row){
    const uint8_t idx=uint8_t(first+row);
    char label[24],b[28];
    if(idx==0U)strcpy(label,"AUTO");
    else if(idx==oledMenuPeerCount+1U)strcpy(label,"BACK");
    else snprintf(label,sizeof(label),"V%03lu",(unsigned long)oledMenuPeerIds[idx-1U]);
    snprintf(b,sizeof(b),"%c %s",idx==oledMenuCursor?'>':' ',label);
    oledF21L(5,24+12*row,b);
  }
  oledF21MenuFooter(false);
}
void oledF21PortraitNearbyPage(){
  if(oledPeerMenuOpen){oledF21PortraitPeerMenu();return;}
  oledF21PortraitTop("PEERS");
  unsigned active=0;uint32_t prev=0,nxt=0;
  while(active<32U && nextActivePeerId(prev,nxt) && nxt>prev){++active;prev=nxt;}
  u8g2.drawFrame(1,16,62,23);
  char b[40];snprintf(b,sizeof(b),"LIVE %u",active);oledF21P(5,25,b);
  oledF21P(5,35,"HOLD SELECT");
  if(selectedOLEDPeerId){
    PeerDisplaySnapshot peer{};
    if(!getPeerSnapshotById(selectedOLEDPeerId,peer)||!peer.active){
      oledF21P(5,58,"PEER LOST");oledF21P(5,80,"DIST --");oledF21P(5,99,"DIR --");
    }else{
      oledF21Icon(F21Icon::Boat,3,47);
      snprintf(b,sizeof(b),"V%03lu",(unsigned long)peer.id);oledF21P(17,57,b);
      if(gpsHasFix && peer.gpsValid && isfinite(latitude) && isfinite(longitude) &&
         isfinite(peer.latitude) && isfinite(peer.longitude)){
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,peer.latitude,peer.longitude);
        if(isfinite(km))snprintf(b,sizeof(b),"%.1f km",km);
        else strcpy(b,"DIST --");
        oledF21P(5,79,b);
        snprintf(b,sizeof(b),"DIR %s TRUE",V003F19Display::cardinal8(brg));oledF21P(5,97,b);
      }else{oledF21P(5,79,"DIST --");oledF21P(5,97,"DIR --");}
      snprintf(b,sizeof(b),"AGE %lus",(unsigned long)peer.ageSeconds);oledF21P(5,113,b);
    }
  }else if(!active){
    oledF21P(5,65,"NO PEERS");
  }else{
    prev=0;nxt=0;
    for(uint8_t row=0;row<3U && nextActivePeerId(prev,nxt) && nxt>prev;++row){
      prev=nxt;PeerDisplaySnapshot peer{};
      if(!getPeerSnapshotById(nxt,peer)||!peer.active)continue;
      const int y=49+22*row;
      oledF21Icon(F21Icon::Boat,2,y);
      if(gpsHasFix&&peer.gpsValid&&isfinite(latitude)&&isfinite(longitude)&&
         isfinite(peer.latitude)&&isfinite(peer.longitude)){
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,peer.latitude,peer.longitude);
        if(isfinite(km)&&km<100.0)snprintf(b,sizeof(b),"%03lu %.1fkm",(unsigned long)peer.id,km);
        else snprintf(b,sizeof(b),"%03lu DIST --",(unsigned long)peer.id);
        oledF21P(15,y+7,b);
        snprintf(b,sizeof(b),"DIR %s",V003F19Display::cardinal8(brg));
        oledF21P(15,y+16,b);
      }else{
        snprintf(b,sizeof(b),"%03lu DIST--",(unsigned long)peer.id);oledF21P(15,y+7,b);
        oledF21P(15,y+16,"DIR --");
      }
      if(row<2U)u8g2.drawHLine(2,y+19,60);
    }
  }
  oledF21Dots(true,2);
}
void oledF21LandscapeNearbyPage(){
  if(oledPeerMenuOpen){oledF21LandscapePeerMenu();return;}
  oledF21LandscapeTop("PEERS");
  unsigned active=0;uint32_t prev=0,nxt=0;
  while(active<32U && nextActivePeerId(prev,nxt)&&nxt>prev){++active;prev=nxt;}
  u8g2.drawFrame(1,15,40,39);u8g2.drawFrame(44,15,83,39);
  char b[40];snprintf(b,sizeof(b),"LIVE %u",active);oledF21L(4,27,b);
  oledF21L(4,41,"HOLD");oledF21L(4,51,"SELECT");
  if(selectedOLEDPeerId){
    PeerDisplaySnapshot peer{};
    if(!getPeerSnapshotById(selectedOLEDPeerId,peer)||!peer.active){
      oledF21L(48,29,"PEER LOST");oledF21L(48,42,"DIST --");
      oledF21L(48,51,"AGE --");
    }else{
      oledF21Icon(F21Icon::Boat,47,18);
      snprintf(b,sizeof(b),"V%03lu",(unsigned long)peer.id);oledF21L(62,27,b);
      if(gpsHasFix&&peer.gpsValid&&isfinite(latitude)&&isfinite(longitude)&&
         isfinite(peer.latitude)&&isfinite(peer.longitude)){
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,peer.latitude,peer.longitude);
        if(isfinite(km))snprintf(b,sizeof(b),"%.1fkm %s",km,V003F19Display::cardinal8(brg));
        else strcpy(b,"DIST --");
        oledF21L(48,42,b);
      }else oledF21L(48,42,"DIST -- DIR --");
      snprintf(b,sizeof(b),"AGE %lus",(unsigned long)peer.ageSeconds);
      oledF21L(48,51,b);
    }
  }else if(!active)oledF21L(48,34,"NO LIVE PEERS");
  else{
    prev=0;nxt=0;
    for(uint8_t row=0;row<3U&&nextActivePeerId(prev,nxt)&&nxt>prev;++row){
      prev=nxt;PeerDisplaySnapshot peer{};
      if(!getPeerSnapshotById(nxt,peer)||!peer.active)continue;
      const int y=17+12*row;
      oledF21Icon(F21Icon::Boat,46,y);
      if(gpsHasFix&&peer.gpsValid&&isfinite(latitude)&&isfinite(longitude)&&
         isfinite(peer.latitude)&&isfinite(peer.longitude)){
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,peer.latitude,peer.longitude);
        if(isfinite(km)&&km<100.0)snprintf(b,sizeof(b),"%03lu %.1f %s",(unsigned long)peer.id,km,V003F19Display::cardinal8(brg));
        else snprintf(b,sizeof(b),"%03lu DIST --",(unsigned long)peer.id);
      }else snprintf(b,sizeof(b),"%03lu DIST --",(unsigned long)peer.id);
      u8g2.setFont(u8g2_font_5x8_tf);oledF21L(61,y+8,b);
    }
  }
  if(!V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount())) {
    u8g2.setFont(u8g2_font_5x8_tf);u8g2.drawStr(3,62,"HOLD SELECT");
  }
  oledF21Dots(false,2);
}
// F21R4: one read-only event selector for both OP orientations.
// AUTO shows most recent ACTIVE received SOS; manual selection is origin+event.
// Restore AUTO if a formerly selected incoming event was cancelled/evicted.
// A still-active MY SOS must never incorrectly show "NO ACTIVE SOS".
void f21r42ValidateSOSSelection(){
  if(selectedSOSOrigin==0U)return;
  SOSRecordSnapshot selected{};
  if(!findSOSRecord(selectedSOSOrigin,selectedSOSEvent,selected) ||
     !selected.known || selected.cancelled){
    selectedSOSOrigin=0;selectedSOSEvent=0;
  }
}
bool f21r4ChooseSOS(SOSRecordSnapshot &chosen){
  if(selectedSOSOrigin!=0U){
    if(findSOSRecord(selectedSOSOrigin,selectedSOSEvent,chosen) &&
       chosen.known && !chosen.cancelled)return true;
    // A cancelled/evicted selected event must never remain an active SOS screen.
    selectedSOSOrigin=0;selectedSOSEvent=0;
  }
  return displaySOSRecord(chosen);
}
void oledF21PortraitSOSMenu(){
  oledF21PortraitTop("SOS SELECT",true);
  const uint8_t total=uint8_t(sosMenuCount+2U);
  const uint8_t first=sosMenuCursor>=8U?uint8_t(sosMenuCursor-7U):0U;
  for(uint8_t row=0;row<8U && uint8_t(first+row)<total;++row){
    const uint8_t idx=uint8_t(first+row);
    char b[32];
    if(idx==0U)strcpy(b,hasActiveLocalSOS()?"MY SOS":"AUTO");
    else if(idx==sosMenuCount+1U)strcpy(b,"BACK");
    else snprintf(b,sizeof(b),"V%03lu #%lu",
        (unsigned long)sosMenuOrigins[idx-1U],(unsigned long)sosMenuEvents[idx-1U]);
    char line[36];snprintf(line,sizeof(line),"%c%s",idx==sosMenuCursor?'>':' ',b);
    oledF21P(3,25+11*row,line);
  }
  oledF21MenuFooter(true);
}
void oledF21LandscapeSOSMenu(){
  oledF21LandscapeTop("SOS SELECT",true);
  const uint8_t total=uint8_t(sosMenuCount+2U);
  const uint8_t first=sosMenuCursor>=3U?uint8_t(sosMenuCursor-2U):0U;
  for(uint8_t row=0;row<3U && uint8_t(first+row)<total;++row){
    const uint8_t idx=uint8_t(first+row);
    char b[32];
    if(idx==0U)strcpy(b,hasActiveLocalSOS()?"MY SOS (AUTO)":"AUTO LATEST");
    else if(idx==sosMenuCount+1U)strcpy(b,"BACK");
    else snprintf(b,sizeof(b),"V%03lu EVENT%lu",
        (unsigned long)sosMenuOrigins[idx-1U],(unsigned long)sosMenuEvents[idx-1U]);
    char line[36];snprintf(line,sizeof(line),"%c %s",idx==sosMenuCursor?'>':' ',b);
    oledF21L(5,24+12*row,line);
  }
  oledF21MenuFooter(false);
}
void oledF21PortraitSOSPage(){
  const bool local=hasActiveLocalSOS();const unsigned incoming=getActiveSOSCount();
  const bool distress=local || incoming>0U;
  if(sosMenuOpen){oledF21PortraitSOSMenu();return;}
  f21r42ValidateSOSSelection();
  oledF21PortraitTop(distress?"SOS ACTIVE":"SOS STATUS",distress);
  u8g2.drawFrame(2,17,60,34);
  char b[40];
  const bool showLocal=local && selectedSOSOrigin==0U;
  oledF21P(6,29,showLocal?"MY SOS":distress?"INCOMING":"NO DISTRESS");
  snprintf(b,sizeof(b),"ACTIVE %u",incoming+(local?1U:0U));oledF21P(6,43,b);
  if(showLocal){
    oledF21P(5,65,"BOAT REPLY");
    oledF21P(5,79,"UNKNOWN");
    oledF21P(5,94,gpsHasFix?"GPS FIX":"GPS --");
    oledF21P(5,108,"HOLD SELECT");oledF21Dots(true,3);return;
  }
  if(!incoming){oledF21P(5,77,"NO ACTIVE SOS");oledF21Dots(true,3);return;}
  SOSRecordSnapshot chosen{};
  if(!f21r4ChooseSOS(chosen)){
    oledF21P(5,65,"SOS DATA WAIT");oledF21P(5,82,"DIST --");
    oledF21P(5,99,"DIR --");u8g2.sendBuffer();return;
  }
  snprintf(b,sizeof(b),"FROM V%03lu",(unsigned long)chosen.originId);oledF21P(5,61,b);
  snprintf(b,sizeof(b),"EVENT %lu",(unsigned long)chosen.eventId);oledF21P(5,73,b);
  if(gpsHasFix && chosen.positionValid && isfinite(latitude) && isfinite(longitude) &&
     isfinite(chosen.latitude) && isfinite(chosen.longitude)){
    const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
                                  chosen.latitude,chosen.longitude)/1000.0;
    if(isfinite(km))snprintf(b,sizeof(b),"DIST %.1fkm",km);
    else strcpy(b,"DIST --");
    oledF21P(5,85,b);
    const double brg=TinyGPSPlus::courseTo(latitude,longitude,
                                  chosen.latitude,chosen.longitude);
    snprintf(b,sizeof(b),"DIR %s",V003F19Display::cardinal8(brg));
    oledF21P(5,97,b);
  }else{oledF21P(5,85,"DIST --");oledF21P(5,97,"DIR --");}
  snprintf(b,sizeof(b),"AGE %lus",(unsigned long)(uint32_t(millis()-chosen.receivedAtMs)/1000UL));
  oledF21P(5,111,b);
  oledF21Dots(true,3);
}
void oledF21LandscapeSOSPage(){
  const bool local=hasActiveLocalSOS();const unsigned incoming=getActiveSOSCount();
  const bool distress=local || incoming>0U;
  if(sosMenuOpen){oledF21LandscapeSOSMenu();return;}
  f21r42ValidateSOSSelection();
  oledF21LandscapeTop(distress?"SOS ACTIVE":"SOS STATUS",distress);
  char b[48];u8g2.drawFrame(1,15,126,39);
  if(local && selectedSOSOrigin==0U){
    oledF21L(5,26,"MY SOS ACTIVE");
    oledF21L(5,39,"BOAT REPLY: UNKNOWN");
    oledF21L(5,49,gpsHasFix?"GPS FIX":"GPS --");
    u8g2.drawStr(3,62,"HOLD SELECT");oledF21Dots(false,3);return;
  }
  if(!incoming){oledF21L(5,35,"NO ACTIVE DISTRESS");oledF21Dots(false,3);return;}
  SOSRecordSnapshot chosen{};
  if(!f21r4ChooseSOS(chosen)){
    oledF21L(5,34,"SOS DATA WAIT");u8g2.sendBuffer();return;
  }
  snprintf(b,sizeof(b),"FROM V%03lu EVENT %lu",
      (unsigned long)chosen.originId,(unsigned long)chosen.eventId);
  oledF21L(5,25,b);
  if(gpsHasFix && chosen.positionValid && isfinite(latitude) && isfinite(longitude) &&
     isfinite(chosen.latitude) && isfinite(chosen.longitude)){
    const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
                            chosen.latitude,chosen.longitude)/1000.0;
    if(isfinite(km))snprintf(b,sizeof(b),"DIST %.1fkm",km);
    else strcpy(b,"DIST --");
    oledF21L(5,37,b);
    const double brg=TinyGPSPlus::courseTo(latitude,longitude,
                            chosen.latitude,chosen.longitude);
    snprintf(b,sizeof(b),"DIR %s TRUE",V003F19Display::cardinal8(brg));
    oledF21L(75,37,b);
  }else{oledF21L(5,37,"DIST --");oledF21L(75,37,"DIR --");}
  snprintf(b,sizeof(b),"AGE %lus",(unsigned long)(uint32_t(millis()-chosen.receivedAtMs)/1000UL));
  oledF21L(5,49,b);
  u8g2.drawStr(3,62,"HOLD SELECT");oledF21Dots(false,3);
}
#endif // F21R3 icon renderers

void updateOLED() {
  if (!oledAvailable) return;
  // Ensure OLED is targeted before drawing
  switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  // Screen sleep affects OLED ONLY. Distress wakes it and takes precedence.
  const bool f19SOS=V003F19Display::emergencyActive(
      hasActiveLocalSOS(),getActiveSOSCount());
  const bool f19WantOff=f19Display.screenAsleep() && !f19SOS;
  if (f19PhysicalScreenAsleep!=f19WantOff) {
    u8g2.setPowerSave(f19WantOff?1:0);
    f19PhysicalScreenAsleep=f19WantOff;
  }
  if (f19WantOff) return;
  // F21 rotation is display-only and never runs on development pages.
  const bool f21WantPortrait=(f19Display.operational() || f19SOS) && f19Display.portrait();
  if (f21WantPortrait!=f21PhysicalPortrait) {
    u8g2.setDisplayRotation(f21WantPortrait?U8G2_R1:U8G2_R0);
    f21PhysicalPortrait=f21WantPortrait;
  }
  if (f19SOS && f21r42SOSBrowse.forceSOS()) {
    if(f21WantPortrait)oledF21PortraitSOSPage();
    else oledF21LandscapeSOSPage();
    return;
  }
  if (f19Display.operational() || f19SOS) {
    const uint8_t opPage=f19SOS ? f21r42SOSBrowse.page() : f19Display.operationalPage();
    if(f21WantPortrait) {
      switch(opPage) {
        case 0: oledF21PortraitGPSPage(); break;
        case 1: oledF21PortraitWeatherPage(); break;
        case 2: oledF21PortraitNearbyPage(); break;
        case 3: oledF21PortraitSOSPage(); break;
        default: oledF21PortraitGPSPage(); break;
      }
      return;
    }
    switch (opPage) {
      case 0: oledF21LandscapeGPSPage(); break;
      case 1: oledF21LandscapeWeatherPage(); break;
      case 2: oledF21LandscapeNearbyPage(); break;
      case 3: oledF21LandscapeSOSPage(); break;
      default: oledF21LandscapeGPSPage(); break;
    }
    return;
  }
#endif
  switch (oledPage) {
    case 0: oledGPSPage(); break;
    case 1: oledWeatherPage(); break;
    case 2: oledLinkPage(); break;
    case 3: oledPDRPage(); break;
    case 4: oledAlertPage(); break;
    case 5: oledUptimePage(); break;
    case 6: oledSOSRescuePage(); break;
#if THESIS_SD_OLED_UI == 1
    case 7: oledSDStoragePage(); break;
#endif
  }
}

void oledGPSPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];
  snprintf(line, sizeof(line), "%s GPS 1/" OLED_PAGE_TOTAL, VesselIdentity::ready() ? VesselIdentity::label() : "NO ID");
  u8g2.drawStr(0, 9, line);
  u8g2.drawStr(0, 20, gpsHasFix ? "GPS: FIX" : "GPS: SEARCH");
  if (gps.satellites.isValid()) snprintf(line, sizeof(line), "SAT: %lu", gps.satellites.value());
  else strcpy(line, "SAT: ?");
  u8g2.drawStr(0, 32, line);
  if (gpsHasFix) {
    snprintf(line, sizeof(line), "%.6f", latitude); u8g2.drawStr(0, 44, line);
    snprintf(line, sizeof(line), "%.6f", longitude); u8g2.drawStr(0, 56, line);
  } else {
    u8g2.drawStr(0, 44, "NO POSITION FIX");
    u8g2.drawStr(0, 56, "MOVE TO OPEN SKY");
  }
  u8g2.sendBuffer();
}

#if THESIS_V003_F18_WEATHER_INTERFACE == 1
// F18 OLED CONSISTENCY: shared six-row 6x10 layout with macro view.
// Separate experimental scientific state; no weather or output side effects.
void oledMicroWeatherPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];
  u8g2.drawStr(0, 9, "MICRO WX 2/8 EXP");
  const auto state = f10Forecaster.state();
  const char* label = "INVALID";
  switch (state) {
    case V003F10Micro::State::WARMUP: label = "BASELINE"; break;
    case V003F10Micro::State::NO_PATTERN: label = "NO_PATTERN"; break;
    case V003F10Micro::State::CANDIDATE: label = "CANDIDATE"; break;
    case V003F10Micro::State::RISK_WATCH: label = "RISK_WATCH"; break;
    default: break;
  }
  snprintf(line, sizeof(line), "STATE:%.15s", label);
  u8g2.drawStr(0, 19, line);
  const char* pattern = "--";
  if (state != V003F10Micro::State::INVALID &&
      state != V003F10Micro::State::WARMUP) {
    switch (f10Forecaster.pattern()) {
      case V003F10Micro::Pattern::PRESSURE_RISE: pattern = "RISE"; break;
      case V003F10Micro::Pattern::PRESSURE_FALL: pattern = "FALL"; break;
      default: pattern = "NONE"; break;
    }
  }
  snprintf(line, sizeof(line), "H:%u/11 PAT:%s",
           unsigned(f10Forecaster.count()), pattern);
  u8g2.drawStr(0, 29, line);
  const bool trendReady = f10Forecaster.count() == V003F10Micro::kWindowCount &&
      state != V003F10Micro::State::INVALID &&
      state != V003F10Micro::State::WARMUP &&
      isfinite(f10Forecaster.deltaTemperature()) &&
      isfinite(f10Forecaster.deltaHumidity()) &&
      isfinite(f10Forecaster.deltaPressure());
  if (trendReady) {
    snprintf(line, sizeof(line), "dT:%+.1fC dRH:%+.0fpp",
             (double)f10Forecaster.deltaTemperature(),
             (double)f10Forecaster.deltaHumidity());
    u8g2.drawStr(0, 39, line);
    snprintf(line, sizeof(line), "dP:%+.2fhPa",
             (double)f10Forecaster.deltaPressure());
    u8g2.drawStr(0, 49, line);
  } else {
    u8g2.drawStr(0, 39, "TRENDS: NOT READY");
    u8g2.drawStr(0, 49, "NO SAFETY ALL-CLEAR");
  }
  u8g2.drawStr(0, 59, "EXPERIMENTAL 10-30m");
  u8g2.sendBuffer();
}
#endif

void oledWeatherPage() {
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
  if (f18WeatherMicroView) { oledMicroWeatherPage(); return; }
  {  // Isolate F18 renderer locals from legacy renderer.
  // F18 OLED CONSISTENCY: no changes to legacy F10/F11 calculations or to
  // earlier F10-F17 profiles, which retain their original macro renderer.
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];
  u8g2.drawStr(0, 9, "MACRO WX 2/8 EMP");
  const char* macroState = !weatherSampleAttempted ? "CHECKING"
      : !weatherInputsValid ? "SENSOR_FAIL" : shortWeatherName(weatherState);
  snprintf(line, sizeof(line), "STATE:%.15s", macroState);
  u8g2.drawStr(0, 19, line);
  snprintf(line, sizeof(line), "H:%u/%u D:%s B:%s",
           unsigned(weatherMeasurement.historyCount), unsigned(WEATHER_HISTORY_SIZE),
           dhtReadingValid ? "OK" : "--",
           bmePressureReadingValid ? "OK" : "--");
  u8g2.drawStr(0, 29, line);
  if (tempHumidityReadingValid) {
    snprintf(line, sizeof(line), "T:%.1fC RH:%.1f%%",
             (double)temperatureC, (double)humidityPercent);
  } else {
    strcpy(line, "T:--.-C RH:--.-%");
  }
  u8g2.drawStr(0, 39, line);
  if (bmePressureReadingValid && isfinite(pressureStationHpa)) {
    snprintf(line, sizeof(line), "P:%.1fhPa", (double)pressureStationHpa);
  } else {
    strcpy(line, "P:-- hPa (BME FAIL)");
  }
  u8g2.drawStr(0, 49, line);
  if (!weatherSampleAttempted) {
    strcpy(line, "WAIT: FIRST READ");
  } else if (!weatherInputsValid) {
    strcpy(line, "FORECAST: DISABLED");
  } else if (weatherState == WEATHER_CALIBRATING ||
             weatherMeasurement.historyCount < WEATHER_HISTORY_SIZE) {
    strcpy(line, "WAIT: MACRO HISTORY");
  } else if (isfinite(pressureDelta)) {
    snprintf(line, sizeof(line), "Z:%u dP:%+.1fhPa",
             unsigned(zambrettiIndex), (double)pressureDelta);
  } else {
    strcpy(line, "Z:-- dP:--");
  }
  u8g2.drawStr(0, 59, line);
  u8g2.sendBuffer();
  }
  return;
#endif
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];

  const char* wxHeader = !weatherSampleAttempted ? "CHECKING"
      : !weatherInputsValid ? "SENSOR FAIL" : shortWeatherName(weatherState);
  snprintf(line, sizeof(line), "WX 2/" OLED_PAGE_TOTAL ": %s", wxHeader);
  u8g2.drawStr(0, 9, line);

  if (tempHumidityReadingValid) {
    snprintf(line, sizeof(line), "T:%.1fC RH:%.1f%%", temperatureC, humidityPercent);
  } else {
    strcpy(line, "T:--.-C RH:--.-%");
  }
  u8g2.drawStr(0, 19, line);

  if (bmePressureReadingValid) {
    // Show the live pressure sensor result, even when T/RH is unavailable.
    snprintf(line, sizeof(line), "P:%.1f hPa", pressureStationHpa);
  } else {
    strcpy(line, "P:-- hPa (BME FAIL)");
  }
  u8g2.drawStr(0, 29, line);

  if (!weatherSampleAttempted) {
    snprintf(line, sizeof(line), "DHT:%s BME:%s",
             dhtAvailable ? "FOUND" : "MISS",
             bmeAvailable ? "FOUND" : "MISS");
  } else {
    snprintf(line, sizeof(line), "DHT:%s BME-P:%s",
             dhtReadingValid ? "OK" : "FAIL",
             bmePressureReadingValid ? "OK" : "FAIL");
  }
  u8g2.drawStr(0, 39, line);

  if (!weatherSampleAttempted) strcpy(line, "WAIT: FIRST READ");
  else if (!dhtReadingValid && !bmePressureReadingValid) strcpy(line, "DHT+BME: READ FAIL");
  else if (!dhtReadingValid) strcpy(line, "DHT20: READ FAIL");
  else if (!bmePressureReadingValid) strcpy(line, "BME680: READ FAIL");
  else strcpy(line, "T/RH: DHT20 ONLY");
  u8g2.drawStr(0, 49, line);

  if (!weatherSampleAttempted) strcpy(line, "WAIT: FIRST SAMPLE");
  else if (!weatherInputsValid) strcpy(line, "FORECAST: DISABLED");
  else if (weatherState == WEATHER_CALIBRATING ||
           weatherMeasurement.historyCount < WEATHER_HISTORY_SIZE) {
    snprintf(line, sizeof(line), "CALIBRATING %u/%u",
             (unsigned)weatherMeasurement.historyCount, (unsigned)WEATHER_HISTORY_SIZE);
  } else {
    snprintf(line, sizeof(line), "Z:%u DP:%+.1f", zambrettiIndex, pressureDelta);
  }
  u8g2.drawStr(0, 59, line);
  u8g2.sendBuffer();
}

void oledLinkPage() {
  if (oledPeerMenuOpen) { drawOLEDPeerMenu(); return; }
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,selectedOLEDPeerId ? "SELECT PEER 3/" OLED_PAGE_TOTAL : "LIVE PEER 3/" OLED_PAGE_TOTAL);
  char line[32];
  PeerDisplaySnapshot peer;
  if (!selectedOLEDPeer(peer)) {
    snprintf(line,sizeof(line),"KNOWN:%u ACTIVE:0",(unsigned)getTrackedPeerCount());
    u8g2.drawStr(0,22,line);
    u8g2.drawStr(0,39,selectedOLEDPeerId ? "SELECTED PEER LOST" : "NO LIVE TELEMETRY");
    u8g2.drawStr(0,56,"HOLD: CYCLE PEER");
  } else {
    snprintf(line,sizeof(line),"ID:%lu OF %u",(unsigned long)peer.id,(unsigned)getTrackedPeerCount());
    u8g2.drawStr(0,21,line);
    if (gpsHasFix && peer.gpsValid) {
      const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
      snprintf(line,sizeof(line),"DIST:%.2f km",km);
    } else snprintf(line,sizeof(line),"DIST: UNKNOWN");
    u8g2.drawStr(0,34,line);
    snprintf(line,sizeof(line),"RSSI:%d SNR:%.1f",(int)peer.rssi,peer.snr);
    u8g2.drawStr(0,47,line);
    snprintf(line,sizeof(line),"LAST:%lus",(unsigned long)peer.ageSeconds);
    u8g2.drawStr(0,60,line);
  }
  u8g2.sendBuffer();
}

void oledPDRPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"PEER PDR 4/" OLED_PAGE_TOTAL);
  char line[32];
  PeerDisplaySnapshot peer;
  if (!selectedOLEDPeer(peer)) {
    u8g2.drawStr(0,28,selectedOLEDPeerId ? "SELECTED PEER LOST" : "NO LIVE PEER");
    u8g2.drawStr(0,44,"PDR: UNKNOWN");
  } else {
    snprintf(line,sizeof(line),"ID:%lu UNIQUE:%lu",(unsigned long)peer.id,(unsigned long)peer.unique);
    u8g2.drawStr(0,21,line);
    snprintf(line,sizeof(line),"MISS:%lu DUP:%lu",(unsigned long)peer.missing,(unsigned long)peer.duplicates);
    u8g2.drawStr(0,34,line);
    snprintf(line,sizeof(line),"PDR EST:%.1f%%",peer.pdr);
    u8g2.drawStr(0,47,line);
  }
#if THESIS_MULTIHOP_DEV_TEST == 1
  if (VesselIdentity::ready() && VesselIdentity::nodeId()==2U)
    snprintf(line,sizeof(line),"TXF:%lu RLY:%s",(unsigned long)txFailures,
             multihopDevStatus());
  else
    snprintf(line,sizeof(line),"TXF:%lu RLY:N/A",(unsigned long)txFailures);
#else
  snprintf(line,sizeof(line),"TX FAIL:%lu",(unsigned long)txFailures);
#endif
  u8g2.drawStr(0,60,line);
  u8g2.sendBuffer();
}
// ============================================================
// ALERT INDICATOR DISPLAY (SIMULATED LED/BUZZER ONLY)
// ============================================================
void oledAlertPage() {
  uint8_t indicatorState = weatherState;
  if (alertDemoMode == 1) indicatorState = WEATHER_NORMAL;
  if (alertDemoMode == 2) indicatorState = WEATHER_FRONT;
  if (alertDemoMode == 3) indicatorState = WEATHER_SQUALL;

  const bool greenLed = (indicatorState == WEATHER_NORMAL);
  const bool redLed = (indicatorState == WEATHER_SQUALL);
  const bool yellowLed = (indicatorState == WEATHER_FRONT);
  const bool buzzer = (indicatorState == WEATHER_SQUALL);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, alertDemoMode ? "ALERT DEMO 5/" OLED_PAGE_TOTAL : "ALERT 5/" OLED_PAGE_TOTAL);
  char line[30];
  snprintf(line, sizeof(line), "STATE: %s", shortWeatherName(indicatorState));
  u8g2.drawStr(0, 20, line);
  snprintf(line, sizeof(line), "G:%s Y:%s R:%s", greenLed ? "ON" : "OFF",
           yellowLed ? "ON" : "OFF", redLed ? "ON" : "OFF");
  u8g2.drawStr(0, 34, line);
  snprintf(line, sizeof(line), "BUZZER: %s", buzzer ? "ON" : "OFF");
  u8g2.drawStr(0, 47, line);
  u8g2.drawStr(0, 61, "Hold:TEST  Tap:NEXT");
  u8g2.sendBuffer();
}

// ============================================================
// 64-BIT UPTIME (does not wrap after 49.7 days like millis())
// ============================================================
void formatUptime(char *out, size_t outSize) {
  const uint64_t totalSeconds = static_cast<uint64_t>(esp_timer_get_time()) / 1000000ULL;
  const unsigned long days = static_cast<unsigned long>(totalSeconds / 86400ULL);
  const unsigned hours = static_cast<unsigned>((totalSeconds / 3600ULL) % 24ULL);
  const unsigned minutes = static_cast<unsigned>((totalSeconds / 60ULL) % 60ULL);
  const unsigned seconds = static_cast<unsigned>(totalSeconds % 60ULL);
  snprintf(out, outSize, "%lud %02u:%02u:%02u", days, hours, minutes, seconds);
}

void oledUptimePage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "VESSEL UPTIME 6/" OLED_PAGE_TOTAL);
  char line[40];
  formatUptime(line, sizeof(line));
  u8g2.drawStr(0, 22, line);
  snprintf(line, sizeof(line), "WX 2/" OLED_PAGE_TOTAL ": %s", shortWeatherName(weatherState));
  u8g2.drawStr(0, 34, line);
  snprintf(line, sizeof(line), "TX:%lu RX:%lu",
           static_cast<unsigned long>(txCount),
           static_cast<unsigned long>(getTotalPeerTelemetryReceived()));
  u8g2.drawStr(0, 46, line);
  u8g2.drawStr(0, 60, "Tap: NEXT PAGE");
  u8g2.sendBuffer();
}


// Phase 3.5.2: rescue navigation to last reported Vessel-B SOS position.
// Bearings are TRUE-north referenced; not bow-relative heading.
void oledSOSRescuePage() {
  if(sosMenuOpen){drawSOSMenu();return;}
  SOSRecordSnapshot sos;
  const bool present=displaySOSRecord(sos);
  u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"SOS / RESCUE 7/" OLED_PAGE_TOTAL);
  if(!present) {
    u8g2.drawStr(0,23,selectedSOSOrigin?"EVENT REPLACED":"NO ACTIVE SOS");
    u8g2.drawStr(0,42,"Hold: SELECT SOS");
    u8g2.sendBuffer();return;
  }
  char line[40];
  snprintf(line,sizeof(line),"FROM: VESSEL-%03lu",(unsigned long)sos.originId);
  u8g2.drawStr(0,20,line);
  snprintf(line,sizeof(line),"EVENT:%lu %s",(unsigned long)sos.eventId,
           selectedSOSOrigin?"SELECT":"AUTO");
  u8g2.drawStr(0,30,line);
  if(sos.cancelled)u8g2.drawStr(0,40,"CANCEL BY SENDER");
  else if(millis()-sos.receivedAtMs>=60000UL)u8g2.drawStr(0,40,"OLD / UNRESOLVED");
  else u8g2.drawStr(0,40,"SOS / UNRESOLVED");
  if(gpsHasFix && sos.positionValid) {
    const double meters=TinyGPSPlus::distanceBetween(latitude,longitude,sos.latitude,sos.longitude);
    const double bearing=TinyGPSPlus::courseTo(latitude,longitude,sos.latitude,sos.longitude);
    snprintf(line,sizeof(line),"DIST: %.2f km",meters/1000.0);
    u8g2.drawStr(0,51,line);
    snprintf(line,sizeof(line),"BRG: %.0f deg TRUE",bearing);
    u8g2.drawStr(0,62,line);
  } else {
    u8g2.drawStr(0,51,"DIST: UNKNOWN");
    u8g2.drawStr(0,62,"BRG: UNKNOWN");
  }
  u8g2.sendBuffer();
}

#if THESIS_SD_OLED_UI == 1
// 128x64 SH1106 display. Read cached logger counters ONLY: no SPI, file
// opens, directory mutations, radio TX, or emergency state writes here.
void oledSDStoragePage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[38];
#if THESIS_F06_SD_HYBRID == 1
  if (sdStorageMenuOpen) {
    u8g2.drawStr(0, 9, "SD STORAGE MENU 8/8");
    const char* menuItems[4] = {
        "SD JOURNALS", "SD USB READBACK", "RECORDING MODE", "BACK"};
    for (uint8_t index = 0; index < 4; ++index) {
      snprintf(line, sizeof(line), "%c %s",
          index == sdStorageMenuCursor ? '>' : ' ', menuItems[index]);
      u8g2.drawStr(0, 21 + 11 * index, line);
    }
    u8g2.drawStr(0, 64, "HOLD:OPEN TAP:MOVE");
    u8g2.sendBuffer();
    return;
  }
  if (sdHybridMenuOpen) {
    u8g2.drawStr(0, 9, "RECORDING MODE 8/8");
    const char* menuItems[3] = {"SOS ARMED", "CONTINUOUS", "BACK"};
    for (uint8_t index = 0; index < 3; ++index) {
      snprintf(line, sizeof(line), "%c %s",
          index == sdHybridMenuCursor ? '>' : ' ', menuItems[index]);
      u8g2.drawStr(0, 23 + 13 * index, line);
    }
    u8g2.drawStr(0, 62, "HOLD:SET TAP:MOVE");
    u8g2.sendBuffer();
    return;
  }
#endif
  if (sdOLEDSubview == 0) {
    u8g2.drawStr(0, 9, "SD STORAGE 8/8");
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 20, ThesisSDLogger::ready ? "CARD: MOUNTED" : "CARD: NOT READY");
    u8g2.drawStr(0, 30, ThesisSDLogger::hybridIsContinuous()
        ? "MODE: CONTINUOUS" : "MODE: SOS ARMED");
    const char* recStatus = !ThesisSDLogger::ready ? "REC: UNAVAILABLE" :
        ThesisSDLogger::hybridSOSOverride() ? "REC: SOS OVERRIDE" :
        ThesisSDLogger::hybridIsContinuous() ? "REC: CONTINUOUS" : "REC: STANDBY";
    u8g2.drawStr(0, 41, recStatus);
    const char* slash = strrchr(ThesisSDLogger::DIRECTORY, '/');
    snprintf(line, sizeof(line), "FOLDER: %.12s", slash ? slash + 1 : "UNKNOWN");
    u8g2.drawStr(0, 52, line);
#else
    u8g2.drawStr(0, 20, ThesisSDLogger::ready ? "CARD: LOGGING" : "CARD: NOT READY");
#if THESIS_F06_SD_SANDBOX == 1
    u8g2.drawStr(0, 30, "MODE: F06 SANDBOX");
#else
    u8g2.drawStr(0, 30, "MODE: VESSEL JOURNAL");
#endif
    const char* slash = strrchr(ThesisSDLogger::DIRECTORY, '/');
    const char* folder = slash ? slash + 1 : "UNKNOWN";
    snprintf(line, sizeof(line), "FOLDER: %.12s", folder);
    u8g2.drawStr(0, 41, line);
    snprintf(line, sizeof(line), "Q SOS:%u TLM:%u",
      (unsigned)ThesisSDLogger::urgentQueue.count,
      (unsigned)ThesisSDLogger::telemetryQueue.count);
    u8g2.drawStr(0, 52, line);
#endif
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 63, "HOLD:MENU TAP:NEXT");
#else
    u8g2.drawStr(0, 63, "HOLD:FILES TAP:NEXT");
#endif
  } else if (sdOLEDSubview == 1) {
    u8g2.drawStr(0, 9, "SD JOURNALS 8/8");
    if (ThesisSDLogger::oledAuditValid) {
      snprintf(line, sizeof(line), "TLM: %llu B",
        (unsigned long long)ThesisSDLogger::oledTelemetryBytes);
      u8g2.drawStr(0, 21, line);
      snprintf(line, sizeof(line), "SOS: %llu B",
        (unsigned long long)ThesisSDLogger::oledSOSBytes);
      u8g2.drawStr(0, 33, line);
    } else {
      u8g2.drawStr(0, 21, "FILE SIZES: WAIT");
      u8g2.drawStr(0, 33, "SD AUDIT (30 SEC)");
    }
#if THESIS_F06_SD_HYBRID == 1
    snprintf(line, sizeof(line), "EV:%lu SKIP:%lu",
      (unsigned long)ThesisSDLogger::eventCounter,
      (unsigned long)ThesisSDLogger::hybridSkippedTelemetry());
#else
    snprintf(line, sizeof(line), "EVENTS: %lu",
      (unsigned long)ThesisSDLogger::eventCounter);
#endif
    u8g2.drawStr(0, 44, line);
    snprintf(line, sizeof(line), "DROP S:%lu T:%lu",
      (unsigned long)ThesisSDLogger::lostUrgent,
      (unsigned long)ThesisSDLogger::lostTelemetry);
    u8g2.drawStr(0, 55, line);
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 64, "HOLD:MENU TAP:NEXT");
#else
    u8g2.drawStr(0, 64, "HOLD:USB TAP:NEXT");
#endif
  } else {
    u8g2.drawStr(0, 9, "SD USB READBACK 8/8");
#if THESIS_V003_F16_SD_OLED_HYBRID == 1
    // F19R2 adds bounded V003 history, while F16 retains the old
    // active-session-only export view.
#if THESIS_V003_F19_DUAL_DISPLAY == 1
    u8g2.drawStr(0, 21, "PC: SD LIST/READ");
    u8g2.drawStr(0, 33, "USB SERIAL 115200");
    u8g2.drawStr(0, 45, "V003 HISTORY / EXPORT");
#else
    u8g2.drawStr(0, 21, "PC: SD EXPORT");
    u8g2.drawStr(0, 33, "USB SERIAL 115200");
    u8g2.drawStr(0, 45, "ACTIVE SESSION ONLY");
#endif
#elif THESIS_F06_SD_SANDBOX == 1
    u8g2.drawStr(0, 21, "PC COMMAND: SD LIST");
    u8g2.drawStr(0, 33, "SD READ Sxxxxxxx");
    u8g2.drawStr(0, 45, "OR: SD READ LEGACY");
#else
    u8g2.drawStr(0, 21, "PC: SD EXPORT");
    u8g2.drawStr(0, 33, "USB SERIAL 115200");
    u8g2.drawStr(0, 45, "READBACK TO PC");
#endif
    u8g2.drawStr(0, 55, "NO ACTION FROM OLED");
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 64, "HOLD:MENU TAP:NEXT");
#else
    u8g2.drawStr(0, 64, "HOLD:HOME TAP:NEXT");
#endif
  }
  u8g2.sendBuffer();
}
#endif // THESIS_SD_OLED_UI

#if THESIS_V003_F14_INDICATOR_PROBE == 1
// Local *indicator-driver-only* probe: no mutation of weatherState, SOS,
// radio packets, micro/macro sensor history, or OLED demonstration state.
void v003F14HandleSerialCommand(const char* line) {
  const auto request = V003F14Probe::parse(line);
  if (request == V003F14Probe::Command::Help) {
    Serial.println("[F14-PROBE] F14 IND FRONT | SQUALL | OFF | STATUS | HELP");
    Serial.println("[F14-PROBE] LOCAL_ONLY; 6s MAX; no real SOS or radio changes");
    return;
  }
  if (request == V003F14Probe::Command::Invalid) {
    Serial.println("[F14-PROBE] INVALID; type F14 IND HELP");
    return;
  }
  if (request == V003F14Probe::Command::Status) {
    Serial.printf("[F14-PROBE] authorized=%u active=%u mode=%s\n",
                  unsigned(f14Probe.authorized()), unsigned(f14Probe.active()),
                  V003F14Probe::name(f14Probe.mode()));
    return;
  }
  const bool activeSOS = hasActiveLocalSOS() || getActiveSOSCount() > 0;
  const bool railGood = f13RailEnableBitCached && !f13Indicators.railLost();
  const bool realAlert = V003F14Probe::actualWarning(weatherState, weatherInputsValid);
  const bool accepted = f14Probe.request(request, millis(), activeSOS,
                                          railGood, realAlert);
  Serial.printf("[F14-PROBE] request=%s accepted=%u active=%u "
                "REAL_WEATHER_UNCHANGED=1 NO_RADIO_SOS=1\n",
                V003F14Probe::name(request), unsigned(accepted),
                unsigned(f14Probe.active()));
}
#endif
