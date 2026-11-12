#pragma once
// Phase 5.2.2B-3: common T-Beam Supreme PMU rails and OLED address probe.
// The caller owns I2C initialization: Board A must retain its sensor-aware
// switchI2C() implementation, Board B continues to use Wire.begin().
#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>
#include "tbeam_pins.h"

namespace TBeamHardware {
inline bool beginPower(TwoWire& bus, XPowersAXP2101& pmu) {
  bus.begin(PMU_SDA_PIN, PMU_SCL_PIN);
  if (!pmu.init(bus, PMU_SDA_PIN, PMU_SCL_PIN)) {
    Serial.println("[PMU] Initialization failed");
    return false;
  }
  pmu.setALDO1Voltage(3300);
  pmu.enableALDO1();
  pmu.setALDO4Voltage(3300);
  pmu.enableALDO4();
  return true;
}
inline bool detectOLEDAddress(TwoWire& bus, uint8_t& address) {
  bus.beginTransmission(0x3C);
  if (bus.endTransmission() == 0) { address = 0x3C; return true; }
  bus.beginTransmission(0x3D);
  if (bus.endTransmission() == 0) { address = 0x3D; return true; }
  return false;
}
}  // namespace TBeamHardware
