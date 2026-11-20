#pragma once
#include <stdint.h>

// Phase 1 is a host model, never a new permission to transmit relay RF.
#ifndef THESIS_F23C_BUILD_ONLY
#define THESIS_F23C_BUILD_ONLY 0
#endif
#ifndef THESIS_F23C_RELAY_RF_TX
#define THESIS_F23C_RELAY_RF_TX 0
#endif
#if THESIS_F23C_BUILD_ONLY != 0 && THESIS_F23C_BUILD_ONLY != 1
#error "F23C build-only flag must be 0 or 1"
#endif
#if THESIS_F23C_RELAY_RF_TX != 0
#error "F23C Phase 1 forbids actual relay RF TX"
#endif
#if THESIS_F23C_BUILD_ONLY == 1
#if THESIS_F22_FLEET_OPERATIONAL != 1 || THESIS_LAB_RELAY_TX != 0 || \
    THESIS_V003_F13_LIVE_OUTPUTS != 0 || THESIS_SD_LOGGER != 1 || \
    THESIS_F05_RECOVERY != 1 || THESIS_F06_SD_SANDBOX != 1 || \
    THESIS_V003_F19_DUAL_DISPLAY != 1 || THESIS_V003_F15_SD_PILOT_ARMED != 1
#error "F23C needs intact F22 operational integration, legacy relay and GPIO OFF"
#endif
#endif

namespace F23CLab {
static constexpr bool kRealRelayTxEnabled = false;
static constexpr bool kOriginAuthenticated = false;
static constexpr bool kEndToEndReceiptVerified = false;
static constexpr uint8_t kMaxModelAttempts = 2; // one SOS plus one CANCEL
static constexpr uint32_t kPreparationDelayMs = 250;
static constexpr uint32_t kModelSeparationMs = 1000; // simulation, not RF evidence
static constexpr uint32_t kRunLifetimeMs = 15000;
static_assert(!kRealRelayTxEnabled, "Phase 1 cannot grant radio access");
static_assert(kRunLifetimeMs < 0x80000000UL, "bounded rollover-safe interval");
}
