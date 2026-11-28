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

