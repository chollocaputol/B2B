#pragma once
#include "f23d_readiness_contract.h"
#ifndef THESIS_F23D_PHYSICAL_CANDIDATE
#define THESIS_F23D_PHYSICAL_CANDIDATE 0
#endif
#if THESIS_F23D_PHYSICAL_CANDIDATE != 0 && THESIS_F23D_PHYSICAL_CANDIDATE != 1
#error "F23D physical candidate must be 0 or 1"
#endif
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
#if THESIS_F23D_CANDIDATE != 0 || THESIS_F23D_OFFLINE_REVIEW != 0 || THESIS_F23D_READINESS_ACTIVE || THESIS_F23C_BUILD_ONLY != 0
#error "Physical phase is separate from every accepted offline role"
#endif
#if THESIS_F22_FLEET_OPERATIONAL != 1 || THESIS_F05_RECOVERY != 1 || THESIS_F06_SD_SANDBOX != 1 || THESIS_SD_LOGGER != 1 || THESIS_V003_F19_DUAL_DISPLAY != 1 || THESIS_V003_F15_SD_PILOT_ARMED != 1 || THESIS_V003_F13_LIVE_OUTPUTS != 0 || THESIS_LAB_RELAY_TX != 0
#error "Physical candidate requires all F22 guards and legacy relay/GPIO OFF"
#endif
#endif
namespace F23DPhysical {
// This artifact supplies NO board/operator/timing evidence and cannot relay.
static constexpr bool kPlatformEvidenceInstalled=false;
static_assert(!F23DBench::kPhysicalPermissionInstalled,"Offline contract must remain false");
}
