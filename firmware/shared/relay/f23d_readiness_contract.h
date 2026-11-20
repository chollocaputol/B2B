#pragma once
#include "f23d_bench_contract.h"
#ifndef THESIS_F23D_EVENT_QUERY
#define THESIS_F23D_EVENT_QUERY 0
#endif
#ifndef THESIS_F23D_RX_OBSERVER
#define THESIS_F23D_RX_OBSERVER 0
#endif
#ifndef THESIS_F23D_RELAY_EVIDENCE
#define THESIS_F23D_RELAY_EVIDENCE 0
#endif
#if (THESIS_F23D_EVENT_QUERY != 0 && THESIS_F23D_EVENT_QUERY != 1) || \
    (THESIS_F23D_RX_OBSERVER != 0 && THESIS_F23D_RX_OBSERVER != 1) || \
    (THESIS_F23D_RELAY_EVIDENCE != 0 && THESIS_F23D_RELAY_EVIDENCE != 1)
#error "F23D readiness flags must be 0 or 1"
#endif
#define THESIS_F23D_READINESS_ACTIVE (THESIS_F23D_EVENT_QUERY || THESIS_F23D_RX_OBSERVER || THESIS_F23D_RELAY_EVIDENCE)
#if (THESIS_F23D_EVENT_QUERY + THESIS_F23D_RX_OBSERVER + THESIS_F23D_RELAY_EVIDENCE) > 1
#error "F23D readiness profiles have exclusive vessel roles"
#endif
#if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F22_FLEET_OPERATIONAL != 1 || THESIS_F23C_BUILD_ONLY != 0 || \
    THESIS_LAB_RELAY_TX != 0 || THESIS_V003_F13_LIVE_OUTPUTS != 0 || \
    THESIS_F05_RECOVERY != 1 || THESIS_F06_SD_SANDBOX != 1 || THESIS_SD_LOGGER != 1 || \
    THESIS_V003_F19_DUAL_DISPLAY != 1 || THESIS_V003_F15_SD_PILOT_ARMED != 1
#error "F23D readiness requires complete F22 integration and relay/GPIO OFF"
#endif
#if THESIS_F23D_RELAY_EVIDENCE == 1 && THESIS_F23D_CANDIDATE != 1
#error "F23D relay evidence needs the immutable offline candidate"
#endif
#if (THESIS_F23D_EVENT_QUERY || THESIS_F23D_RX_OBSERVER) && THESIS_F23D_CANDIDATE != 0
#error "F23D endpoint observation cannot include the relay candidate"
#endif
#endif
