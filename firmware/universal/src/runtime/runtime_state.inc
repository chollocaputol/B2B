// F23D SD begin
#include "../../../shared/relay/f23d_sd_capture.h"
// F23D SD end
// F23D physical begin
#include "../../../shared/relay/f23d_physical_contract.h"
// F23D physical end
// F23D readiness begin
#include "../../../shared/relay/f23d_readiness_contract.h"
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
extern void f23dPollEvidence();
#endif
// F23D readiness end
// F23D begin
#include "../../../shared/relay/f23d_bench_contract.h"
// F23D end
// F23C Phase 1 begin
#include "../../../shared/relay/f23c_lab_contract.h"
// F23C Phase 1 end
#ifndef THESIS_LAB_RELAY_TX
#define THESIS_LAB_RELAY_TX 0
#endif
#include "../../../shared/hardware/tbeam_pins.h"
#include "../../../shared/hardware/vessel_identity.h"
#include "../../../shared/hardware/fleet_home_title.h"
#include "../../../shared/hardware/oled_button_debounce.h"
#include "../../../shared/hardware/tbeam_hardware_service.h"
#include "../../../shared/hardware/gps_service.h"
#include "../../../shared/hardware/gnss_pht_clock.h"
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
#include "../../../shared/hardware/fleet_usb_enrollment.h"
#endif
// Isolated Vessel-003 preflight. Never changes normal firmware behavior.
#ifndef THESIS_V003_F08_INDICATOR_BENCH
#define THESIS_V003_F08_INDICATOR_BENCH 0
#endif
#if THESIS_V003_F08_INDICATOR_BENCH != 0 && THESIS_V003_F08_INDICATOR_BENCH != 1
#error "THESIS_V003_F08_INDICATOR_BENCH must be 0 or 1"
#endif
// This LED bench uses virtual states only in this build-only environment.
#ifndef THESIS_V003_F08_LED_BENCH
#define THESIS_V003_F08_LED_BENCH 0
#endif
#if THESIS_V003_F08_LED_BENCH != 0 && THESIS_V003_F08_LED_BENCH != 1
#error "THESIS_V003_F08_LED_BENCH must be 0 or 1"
#endif
// Separate buzzer *software-only* profile. Never configures GPIO38.
#ifndef THESIS_V003_F08_BUZZER_PREFLIGHT
#define THESIS_V003_F08_BUZZER_PREFLIGHT 0
#endif
// Independent Vessel-003 live buzzer profile. Separate from software preflight.
#ifndef THESIS_V003_F08_BUZZER_LIVE_PROFILE
#define THESIS_V003_F08_BUZZER_LIVE_PROFILE 0
#endif
// Isolated GPIO38/39/44 combined F08 bench profile, NOT field firmware.
#ifndef THESIS_V003_F08_COMBINED_PROFILE
#define THESIS_V003_F08_COMBINED_PROFILE 0
#endif
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include <TinyGPSPlus.h>
#include "lora_manager.h"
#include "../../../shared/communication/decentralized_mac.h"
#include "../../../shared/communication/mac_contention_gate.h"
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
#include "../../../shared/hardware/vessel_capabilities.h"
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
#include "../../../shared/storage/fleet_storage_policy.h"
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
#if THESIS_STUDY_SF_CLOUD == 1
#define OLED_PAGE_TOTAL "9"
static constexpr uint8_t kOLEDPageCount = 9;
#elif THESIS_SD_OLED_UI == 1
#define OLED_PAGE_TOTAL "8"
static constexpr uint8_t kOLEDPageCount = 8;
#else
#define OLED_PAGE_TOTAL "7"
static constexpr uint8_t kOLEDPageCount = 7;
#endif
#if THESIS_SD_LOGGER == 1
#include "../../../shared/storage/thesis_sd_logger.h"
#endif
#include <U8g2lib.h>
#if THESIS_V003_F19_DUAL_DISPLAY == 1
#include "display_modes.h"
#include "weather_display.h"
#include "display_policy.h"
#endif
#include "weather_manager.h"
#include "dtn_worker.h"
#if THESIS_STUDY_SF_CLOUD == 1
#include "shore_sync.h"
#endif
#ifndef THESIS_STUDY_SF_UNIVERSAL_OFFLINE
#define THESIS_STUDY_SF_UNIVERSAL_OFFLINE 0
#endif
#if THESIS_STUDY_SF_UNIVERSAL_OFFLINE != 0 && THESIS_STUDY_SF_UNIVERSAL_OFFLINE != 1
#error "Invalid SF7 universal offline flag"
#endif
#if THESIS_STUDY_SF_CLOUD == 1 && THESIS_STUDY_SF_UNIVERSAL_OFFLINE != 1
#error "Cloud sync requires the universal study architecture"
#endif
#if THESIS_STUDY_SF_UNIVERSAL_OFFLINE == 1
#if THESIS_STUDY_SF_BUILD_ONLY != 1 || THESIS_DTN_F25A_BUILD_ONLY != 1 || THESIS_STUDY_SF_OFFLINE_V003 || THESIS_STUDY_SF_SYNC_V003 || THESIS_V003_F09_SHARED_SENSOR_BUS != 1 || THESIS_V003_F10_MICRO_NOWCAST != 1 || THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1 || WEATHER_FAST_TEST != 0
#error "SF7 needs the offline study architecture and field weather settings"
#endif
#endif
#if THESIS_V003_F10_MICRO_NOWCAST == 1
#include "micro_weather.h"
static_assert(!PhysicalIndicators::kOutputsEnabled &&
              DHT_SDA_PIN == 21 && DHT_SCL_PIN == 48,
              "F10 must not actuate hardware; use F09 shared weather sensor bus");
#endif
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
#include "macro_weather.h"
static_assert(!PhysicalIndicators::kOutputsEnabled,
              "F11 macro profile must not drive physical indicators");
#endif
#if THESIS_V003_F12_INDICATOR_PREVIEW == 1
#include "indicator_policy.h"
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
#include "supervised_indicators.h"
#if THESIS_V003_F17_INTEGRATED_PREFLIGHT == 1
static_assert(!V003F13Indicators::kPhysicalOutputsEnabled,
              "F17 integrated preflight must never enable F13 physical GPIO");
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
#ifndef THESIS_DTN_F25A_BUILD_ONLY
#define THESIS_DTN_F25A_BUILD_ONLY 0
#endif
#if THESIS_DTN_F25A_BUILD_ONLY != 0 && THESIS_DTN_F25A_BUILD_ONLY != 1
#error "Invalid F25A flag"
#endif
#if THESIS_DTN_F25A_BUILD_ONLY == 1
#if THESIS_DTN_F24Z_BUILD_ONLY != 1 || THESIS_DTN_F24M_BUILD_ONLY != 1 || THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1 || THESIS_SD_LOGGER != 1 || THESIS_SD_PROBE != 0 || THESIS_F23D_SD_EVIDENCE || THESIS_F23D_PHYSICAL_CANDIDATE || THESIS_F23D_CANDIDATE || THESIS_MULTIHOP_DEV_TEST || THESIS_MULTIHOP_BENCH_RF || THESIS_LAB_RELAY_TX || THESIS_FLEET_USB_ENROLLMENT || THESIS_F22_FLEET_OPERATIONAL || THESIS_V003_F15_SD_PILOT_ARMED || THESIS_V003_F13_LIVE_OUTPUTS
#error "F25A requires F24Z/M/K and all competing physical paths OFF"
#endif
static_assert(!PhysicalIndicators::kOutputsEnabled,"F25A outputs must stay OFF");
extern bool thesisF25ABindLoop();
extern DTNF24M::CaptureResult thesisF25ACapture(DTNF24M::PendingQueue<8>&,const VesselPacket&,uint8_t);
extern void thesisF25APoll(DTNF24M::PendingQueue<8>&,bool);
extern bool thesisF25ARadioBusy();
#endif
#if THESIS_SD_PROBE == 1
#include "../../../shared/storage/sd_probe.h"
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
#if THESIS_STUDY_SF_UNIVERSAL_OFFLINE == 1 && WEATHER_FAST_TEST != 0
#error "SF7 requires actual ten-minute weather history"
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
#if THESIS_STUDY_SF_CLOUD == 1
static StudyShore::Policy studyShore;
#endif
static V003F19Display::SOSBrowse f21r42SOSBrowse;
static bool f19PhysicalScreenAsleep = false;
static bool f21PhysicalPortrait = false; // OLED-only cached rotation
// F21R4: PMU power bus is separate; renderers only read a cached estimate.
static bool displayBatteryPmuReady=false;
static int cachedDisplayBatteryPercent=-1;
static uint32_t lastDisplayBatteryPoll=0;
void pollDisplayBatteryStatus();
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

