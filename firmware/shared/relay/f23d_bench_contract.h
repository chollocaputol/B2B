#pragma once
#include "f23c_lab_contract.h"
#ifndef THESIS_F23D_CANDIDATE
#define THESIS_F23D_CANDIDATE 0
#endif
#ifndef THESIS_F23D_OFFLINE_REVIEW
#define THESIS_F23D_OFFLINE_REVIEW 0
#endif
#ifndef THESIS_F23D_PHYSICAL_PERMISSION
#define THESIS_F23D_PHYSICAL_PERMISSION 0
#endif
#if THESIS_F23D_CANDIDATE != 0 && THESIS_F23D_CANDIDATE != 1
#error "F23D candidate must be 0 or 1"
#endif
#if THESIS_F23D_PHYSICAL_PERMISSION != 0
#error "F23D offline milestone cannot install physical RF permission"
#endif
#if THESIS_F23D_CANDIDATE == 1
#if THESIS_F23D_OFFLINE_REVIEW != 1 || THESIS_F23C_BUILD_ONLY != 0 || \
    THESIS_F22_FLEET_OPERATIONAL != 1 || THESIS_LAB_RELAY_TX != 0 || \
    THESIS_V003_F13_LIVE_OUTPUTS != 0 || THESIS_F05_RECOVERY != 1 || \
    THESIS_F06_SD_SANDBOX != 1 || THESIS_SD_LOGGER != 1 || \
    THESIS_V003_F19_DUAL_DISPLAY != 1 || THESIS_V003_F15_SD_PILOT_ARMED != 1
#error "F23D requires complete F22, explicit offline review, legacy/model relay and GPIO OFF"
#endif
#endif
namespace F23DBench {
// This provider must be separately reviewed in a future physical phase.
// IDs, labels, serial challenges and ACKs do NOT supply cryptographic proof.
static constexpr bool kPhysicalPermissionInstalled = false;
static_assert(!kPhysicalPermissionInstalled, "Offline qualification only");
}
