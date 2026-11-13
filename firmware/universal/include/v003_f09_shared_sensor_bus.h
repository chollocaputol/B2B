#pragma once
#include <stdint.h>

// Vessel-003 isolated F09 external weather-sensor bus review profile.
// Does not enable any physical indicator or change the weather algorithm.
#ifndef THESIS_V003_F09_SHARED_SENSOR_BUS
#define THESIS_V003_F09_SHARED_SENSOR_BUS 0
#endif
#ifndef THESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN
#define THESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN 0
#endif
#if (THESIS_V003_F09_SHARED_SENSOR_BUS != 0 && THESIS_V003_F09_SHARED_SENSOR_BUS != 1) || \
    (THESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN != 0 && THESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN != 1)
#error "Invalid Vessel-003 F09 shared-I2C build flag"
#endif
#if THESIS_V003_F09_SHARED_SENSOR_BUS == 1 && THESIS_V003_F09_SHARED_SENSOR_BUS_OPT_IN != 1
#error "F09 shared-I2C profile requires explicit opt-in"
#endif

namespace V003F09SharedSensorBus {
static constexpr uint8_t kBmeAddress = 0x77; // confirmed on earlier Vessel-003 tests
static constexpr uint8_t kDhtAddress = 0x38;
static constexpr uint8_t kSdaPin = 21;
static constexpr uint8_t kSclPin = 48;
constexpr bool authorized(bool identityReady, uint32_t nodeId) {
  return identityReady && nodeId == 3U;
}
} // namespace V003F09SharedSensorBus
