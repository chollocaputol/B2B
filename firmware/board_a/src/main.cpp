#include <Arduino.h>
#include "sos_controls.h"
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include <TinyGPSPlus.h>
#include "lora_manager.h"
#include <XPowersLib.h>
#include <U8g2lib.h>
#include "weather_manager.h"
#include <esp_timer.h>  // 64-bit uptime counter

// ============================================================
// BOARD A - VESSEL-A
// T-Beam Supreme | L76K GPS | SX1262 | SH1106 OLED
// Dual I2C Environmental Sensors (BME680 & DHT20)
// ============================================================


#define GPS_EN_PIN      7
#define GPS_RX_PIN      9
#define GPS_TX_PIN      8
#define GPS_PPS_PIN     6

#define OLED_SDA_PIN   17
#define OLED_SCL_PIN   18
#define PMU_SDA_PIN    42
#define PMU_SCL_PIN    41

// T-Beam Supreme user/BOOT button (active LOW)
#define USER_BUTTON_PIN 0




const unsigned long TX_INTERVAL = 5000UL;
// DEVELOPMENT ONLY: set to 0 for real 10-minute sampling before field use.
#define WEATHER_FAST_TEST 1
const unsigned long WEATHER_INTERVAL = WEATHER_FAST_TEST ? 5000UL : 600000UL;
const unsigned long GPS_STATUS_INTERVAL = 5000UL;
const unsigned long OLED_UPDATE_INTERVAL = 500UL;
const unsigned long UPTIME_PRINT_INTERVAL = 60000UL; // Serial heartbeat every minute
unsigned long lastWeatherTime = 0;


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
// OLED-only alert demonstrations: 0 = LIVE, 1 = NORMAL, 2 = FRONT, 3 = SQUALL.
// These NEVER modify real weatherState, radio telemetry, or sensor history.
uint8_t alertDemoMode = 0;


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
void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("=== BOARD A: SCHEDULED TX + RX ===");
  Serial.println("Packet: 62 bytes | TX interval: 5 seconds");
  Serial.printf("[WEATHER] %s, sample interval %lu ms; history %u samples\n",
                WEATHER_FAST_TEST ? "ACCELERATED DEVELOPMENT (NOT REAL 100-MIN HISTORY)" : "FIELD",
                WEATHER_INTERVAL, WEATHER_HISTORY_SIZE);

  SOSControls::begin();
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  buttonLastReading = digitalRead(USER_BUTTON_PIN);
  buttonStableState = buttonLastReading;
  buttonLastDebounceTime = millis();
  Serial.println("[OLED] Tap GPIO0: next page; on ALERT page hold >=900ms: LIVE/NORMAL/FRONT/SQUALL demo");

  setupPMU();
  setupOLED();
  setupSensors();
  setupGPS();
  setupLoRa();
  setupEmergencyIdentity();
  startReceive();

  lastWeatherTime = millis();
  lastOLEDUpdate = millis();
  Serial.println("[A] Ready: sends own telemetry and listens for Board B.");
  Serial.println("[TEST] Type TEST ALL, TEST NORMAL, TEST FRONT, TEST SQUALL, TEST BOTH, TEST CALIB, TEST Z, or TEST HELP");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  updateGPS();
  processSerialTestCommands();
  const auto action = SOSControls::pollAction(hasActiveLocalSOS());
  if (action == SOSControls::ButtonAction::StartSOS) {
    Serial.println("[SOS-BUTTON-A] Deliberate 1.5s SOS hold recognized");
    SOSControls::noteLocalSOS();
    transmitSOS();
  } else if (action == SOSControls::ButtonAction::CancelSOS) {
    Serial.println("[SOS-BUTTON-A] Deliberate 5s CANCEL hold recognized");
    cancelActiveSOS();
  }
  SOSControls::updateIndicators();

  pollLoRaReceive();
  processSOSRetries();
  processCancelRetries();

  if (millis() - lastWeatherTime >= WEATHER_INTERVAL) {
    lastWeatherTime = millis();
    processWeather();
  }

  static unsigned long lastTransmit = 0;
  if (millis() - lastTransmit >= TX_INTERVAL) {
    lastTransmit = millis();
    transmitPacket();
  }

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
    Serial.printf("[UPTIME-A] %s | Weather=%s | TX_OK=%lu | TX_FAIL=%lu | RX_B=%lu\n",
                  uptimeText, weatherName(weatherState),
                  static_cast<unsigned long>(txCount),
                  static_cast<unsigned long>(txFailures),
                  static_cast<unsigned long>(rxFromB));
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
void setupPMU() {
  PMUWire.begin(PMU_SDA_PIN, PMU_SCL_PIN);
  if (!pmu.init(PMUWire, PMU_SDA_PIN, PMU_SCL_PIN)) {
    Serial.println("[PMU] Initialization failed");
    return;
  }
  pmu.setALDO1Voltage(3300);
  pmu.enableALDO1();
  pmu.setALDO4Voltage(3300);
  pmu.enableALDO4();
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
  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() == 0) { oledAddress = 0x3C; return true; }
  Wire.beginTransmission(0x3D);
  if (Wire.endTransmission() == 0) { oledAddress = 0x3D; return true; }
  return false;
}

// ============================================================
// GPS
// ============================================================
void setupGPS() {
  pinMode(GPS_EN_PIN, OUTPUT);
  digitalWrite(GPS_EN_PIN, HIGH);
  pinMode(GPS_PPS_PIN, INPUT);
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("[GPS-A] L76K enabled");
}

void updateGPS() {
  while (GPSSerial.available()) gps.encode(GPSSerial.read());
  gpsHasFix = gps.location.isValid() && gps.location.age() < 3000;
  if (gpsHasFix) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    haveLastKnownPosition = true;
  }
}

void printGPSStatus() {
  Serial.println();
  Serial.println("[GPS-A]");
  Serial.print("Fix: "); Serial.println(gpsHasFix ? "YES" : "NO");
  Serial.print("Satellites: ");
  if (gps.satellites.isValid()) Serial.println(gps.satellites.value());
  else Serial.println("?");
  Serial.print("HDOP: ");
  if (gps.hdop.isValid()) Serial.println(gps.hdop.hdop(), 1);
  else Serial.println("?");
  if (gpsHasFix) {
    Serial.print("Lat: "); Serial.println(latitude, 7);
    Serial.print("Lon: "); Serial.println(longitude, 7);
  }
}

// OLED BUTTON NAVIGATION
// ============================================================
// GPIO0 user button is active LOW. Debouncing prevents one physical
// press from skipping multiple pages. Page changes only on a new press.
void handleOLEDButton() {
  int reading = digitalRead(USER_BUTTON_PIN);
  if (reading != buttonLastReading) {
    buttonLastDebounceTime = millis();
    buttonLastReading = reading;
  }
  if (millis() - buttonLastDebounceTime >= BUTTON_DEBOUNCE_MS && reading != buttonStableState) {
    buttonStableState = reading;
    if (buttonStableState == LOW) {
      buttonPressStartedAt = millis();
    } else {
      unsigned long duration = millis() - buttonPressStartedAt;
      if (oledPage == 4 && duration >= BUTTON_LONG_PRESS_MS) {
        alertDemoMode = (alertDemoMode + 1) % 4;
        Serial.printf("[ALERT-DEMO] OLED-only mode: %u (0=LIVE,1=NORMAL,2=FRONT,3=SQUALL)\n", alertDemoMode);
      } else if (duration < BUTTON_LONG_PRESS_MS) {
        oledPage = (oledPage + 1) % 6;
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
void updateOLED() {
  if (!oledAvailable) return;
  // Ensure OLED is targeted before drawing
  switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);
  switch (oledPage) {
    case 0: oledGPSPage(); break;
    case 1: oledWeatherPage(); break;
    case 2: oledLinkPage(); break;
    case 3: oledPDRPage(); break;
    case 4: oledAlertPage(); break;
    case 5: oledUptimePage(); break;
  }
}

void oledGPSPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "VESSEL-A GPS");
  u8g2.drawStr(0, 20, gpsHasFix ? "GPS: FIX" : "GPS: SEARCH");
  char line[32];
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

void oledWeatherPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];

  snprintf(line, sizeof(line), "WX: %s", shortWeatherName(weatherState));
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

  snprintf(line, sizeof(line), "DHT:%s BME-P:%s",
           dhtReadingValid ? "OK" : "FAIL",
           bmePressureReadingValid ? "OK" : "FAIL");
  u8g2.drawStr(0, 39, line);

  if (tempHumidityReadingValid) {
    snprintf(line, sizeof(line), "T/RH SRC:%s", tempHumidityFromDHT ? "DHT20" : "BME680");
  } else {
    strcpy(line, "T/RH SRC: NONE");
  }
  u8g2.drawStr(0, 49, line);

  if (weatherInputsValid) {
    snprintf(line, sizeof(line), "Z:%u DP:%+.1f", zambrettiIndex, pressureDelta);
  } else {
    strcpy(line, "CALC: WAIT SENSORS");
  }
  u8g2.drawStr(0, 59, line);
  u8g2.sendBuffer();
}

void oledLinkPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "LINK TO VESSEL-B");
  char line[32];
  snprintf(line, sizeof(line), "TX:%lu RX:%lu",
           static_cast<unsigned long>(txCount), static_cast<unsigned long>(rxFromB));
  u8g2.drawStr(0, 20, line);
  if (rxFromB == 0) {
    u8g2.drawStr(0, 33, "B: NEVER SEEN");
    u8g2.drawStr(0, 46, "DIST: UNKNOWN");
    u8g2.drawStr(0, 58, "LISTENING...");
  } else if (!isVesselBActive()) {
    u8g2.drawStr(0, 32, "B: STALE / LOST");
    snprintf(line, sizeof(line), "LAST SEEN: %lus", vesselBLastSeenSeconds());
    u8g2.drawStr(0, 44, line);
    u8g2.drawStr(0, 56, "DIST: UNKNOWN");
  } else {
    u8g2.drawStr(0, 32, bGPSValid ? "B: ACTIVE GPS OK" : "B: ACTIVE NO FIX");
    // Recalculate with the CURRENT local fix; remote position comes from
    // a packet received within the timeout, not from the last-known cache.
    if (gpsHasFix && bGPSValid) {
      const double km = TinyGPSPlus::distanceBetween(latitude, longitude,
                                                    bLatitude, bLongitude) / 1000.0;
      snprintf(line, sizeof(line), "DIST: %.2f km", km);
    } else {
      strcpy(line, "DIST: UNKNOWN");
    }
    u8g2.drawStr(0, 44, line);
    snprintf(line, sizeof(line), "RSSI:%d SNR:%.1f", lastBRSSI, lastBSNR);
    u8g2.drawStr(0, 56, line);
  }
  u8g2.sendBuffer();
}

void oledPDRPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "REVERSE LINK B->A");
  char line[32];
  snprintf(line, sizeof(line), "UNIQUE:%lu", static_cast<unsigned long>(uniqueFromB)); u8g2.drawStr(0, 20, line);
  snprintf(line, sizeof(line), "MISS:%lu DUP:%lu", static_cast<unsigned long>(missingFromB), static_cast<unsigned long>(duplicateFromB)); u8g2.drawStr(0, 32, line);
  snprintf(line, sizeof(line), "PDR: %.1f%%", bSequencePDR()); u8g2.drawStr(0, 44, line);
  snprintf(line, sizeof(line), "TX FAIL:%lu", static_cast<unsigned long>(txFailures)); u8g2.drawStr(0, 56, line);
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
  u8g2.drawStr(0, 9, alertDemoMode ? "ALERT DEMO (FAKE)" : "ALERT INDICATORS");
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
  u8g2.drawStr(0, 9, "VESSEL-A UPTIME");
  char line[40];
  formatUptime(line, sizeof(line));
  u8g2.drawStr(0, 22, line);
  snprintf(line, sizeof(line), "WX: %s", shortWeatherName(weatherState));
  u8g2.drawStr(0, 34, line);
  snprintf(line, sizeof(line), "TX:%lu RX:%lu",
           static_cast<unsigned long>(txCount),
           static_cast<unsigned long>(rxFromB));
  u8g2.drawStr(0, 46, line);
  u8g2.drawStr(0, 60, "Tap: NEXT PAGE");
  u8g2.sendBuffer();
}