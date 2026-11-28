// ============================================================
// HARDWARE INITIALIZATION
// ============================================================
// No NVS, radio, SD, sensor, or GPIO interaction: PMU read only.
#if THESIS_V003_F19_DUAL_DISPLAY == 1
void pollDisplayBatteryStatus() {
  if(!displayBatteryPmuReady)return;
  const uint32_t now=millis();
  if(lastDisplayBatteryPoll!=0U && uint32_t(now-lastDisplayBatteryPoll)<15000U)return;
  lastDisplayBatteryPoll=now;
  if(!pmu.isBatteryConnect()){cachedDisplayBatteryPercent=-1;return;}
  const int pct=pmu.getBatteryPercent();
  cachedDisplayBatteryPercent=V003F21R4UI::validBatteryPercent(pct)?pct:-1;
}
#endif
void setupPMU() {
  if (!TBeamHardware::beginPower(PMUWire, pmu)) return;
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  displayBatteryPmuReady=true;
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

