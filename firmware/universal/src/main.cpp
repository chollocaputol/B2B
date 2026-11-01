#include "../../shared/tbeam_pins.h"
#include "../../shared/vessel_identity.h"
#include "../../shared/oled_button_debounce.h"
#include "../../shared/tbeam_hardware_service.h"
#include "../../shared/gps_service.h"
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
// UNIVERSAL - NUMERIC VESSEL IDENTITY
// T-Beam Supreme | L76K GPS | SX1262 | SH1106 OLED
// Dual I2C Environmental Sensors (BME680 & DHT20)
// ============================================================


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
  u8g2.drawStr(0, 9, "SELECT VESSEL 3/7");
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
  Serial.println("=== UNIVERSAL VESSEL: SCHEDULED TX + RX ===");
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
  VesselIdentity::load();
  setupLoRa();
  setupEmergencyIdentity();
  startReceive();

  lastWeatherTime = millis();
  lastOLEDUpdate = millis();
  Serial.printf("[UNIVERSAL] %s (ID %lu): firmware initialized; relay forwarding OFF; multi-vessel validation PENDING\n",
                VesselIdentity::label(), (unsigned long)VesselIdentity::nodeId());
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
  if (consumeNewReceivedSOS()) {
    oledPage = 6;  // Auto-open incoming SOS rescue page.
    lastOLEDUpdate = 0;
  }
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
    Serial.printf("[UPTIME-A] %s | Weather=%s | TX_OK=%lu | TX_FAIL=%lu | RX_TELEMETRY=%lu\n",
                  uptimeText, weatherName(weatherState),
                  static_cast<unsigned long>(txCount),
                  static_cast<unsigned long>(txFailures),
                  static_cast<unsigned long>(getTotalPeerTelemetryReceived()));
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
  if (!TBeamHardware::beginPower(PMUWire, pmu)) return;
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

// ============================================================
// GPS
// ============================================================
void setupGPS() {
  GPSService::begin(GPSSerial, "A");
}

void updateGPS() {
  gpsHasFix = GPSService::poll(GPSSerial, gps, latitude, longitude);
  if (gpsHasFix) haveLastKnownPosition = true;
}

void printGPSStatus() {
  GPSService::printStatus("A", gps, gpsHasFix, latitude, longitude);
}

// OLED BUTTON NAVIGATION
// ============================================================
// GPIO0 user button is active LOW. Debouncing prevents one physical
// press from skipping multiple pages. Page changes only on a new press.
void handleOLEDButton() {
  const int reading = digitalRead(USER_BUTTON_PIN);
  if (OLEDButton::stableEdge(reading, buttonLastReading, buttonStableState,
                             buttonLastDebounceTime, millis(), BUTTON_DEBOUNCE_MS)) {
    if (buttonStableState == LOW) {
      buttonPressStartedAt = millis();
    } else {
      unsigned long duration = millis() - buttonPressStartedAt;
      if (oledPage == 2 && duration >= BUTTON_LONG_PRESS_MS) {
        if (oledPeerMenuOpen) confirmOLEDPeerMenu();
        else openOLEDPeerMenu();
      } else if (oledPeerMenuOpen && oledPage == 2 && duration < BUTTON_LONG_PRESS_MS) {
        advanceOLEDPeerMenu();
      } else if (oledPage == 4 && duration >= BUTTON_LONG_PRESS_MS) {
        alertDemoMode = (alertDemoMode + 1) % 4;
        Serial.printf("[ALERT-DEMO] OLED-only mode: %u (0=LIVE,1=NORMAL,2=FRONT,3=SQUALL)\n", alertDemoMode);
      } else if (duration < BUTTON_LONG_PRESS_MS) {
        oledPage = (oledPage + 1) % 7;
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
    case 6: oledSOSRescuePage(); break;
  }
}

void oledGPSPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];
  snprintf(line, sizeof(line), "%s GPS 1/7", VesselIdentity::ready() ? VesselIdentity::label() : "NO ID");
  u8g2.drawStr(0, 9, line);
  u8g2.drawStr(0, 20, gpsHasFix ? "GPS: FIX" : "GPS: SEARCH");
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

  snprintf(line, sizeof(line), "WX 2/7: %s", shortWeatherName(weatherState));
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
  if (oledPeerMenuOpen) { drawOLEDPeerMenu(); return; }
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,selectedOLEDPeerId ? "SELECT PEER 3/7" : "LIVE PEER 3/7");
  char line[32];
  PeerDisplaySnapshot peer;
  if (!selectedOLEDPeer(peer)) {
    snprintf(line,sizeof(line),"KNOWN:%u ACTIVE:0",(unsigned)getTrackedPeerCount());
    u8g2.drawStr(0,22,line);
    u8g2.drawStr(0,39,selectedOLEDPeerId ? "SELECTED PEER LOST" : "NO LIVE TELEMETRY");
    u8g2.drawStr(0,56,"HOLD: CYCLE PEER");
  } else {
    snprintf(line,sizeof(line),"ID:%lu OF %u",(unsigned long)peer.id,(unsigned)getTrackedPeerCount());
    u8g2.drawStr(0,21,line);
    if (gpsHasFix && peer.gpsValid) {
      const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
      snprintf(line,sizeof(line),"DIST:%.2f km",km);
    } else snprintf(line,sizeof(line),"DIST: UNKNOWN");
    u8g2.drawStr(0,34,line);
    snprintf(line,sizeof(line),"RSSI:%d SNR:%.1f",(int)peer.rssi,peer.snr);
    u8g2.drawStr(0,47,line);
    snprintf(line,sizeof(line),"LAST:%lus",(unsigned long)peer.ageSeconds);
    u8g2.drawStr(0,60,line);
  }
  u8g2.sendBuffer();
}

void oledPDRPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"PEER PDR 4/7");
  char line[32];
  PeerDisplaySnapshot peer;
  if (!selectedOLEDPeer(peer)) {
    u8g2.drawStr(0,28,selectedOLEDPeerId ? "SELECTED PEER LOST" : "NO LIVE PEER");
    u8g2.drawStr(0,44,"PDR: UNKNOWN");
  } else {
    snprintf(line,sizeof(line),"ID:%lu UNIQUE:%lu",(unsigned long)peer.id,(unsigned long)peer.unique);
    u8g2.drawStr(0,21,line);
    snprintf(line,sizeof(line),"MISS:%lu DUP:%lu",(unsigned long)peer.missing,(unsigned long)peer.duplicates);
    u8g2.drawStr(0,34,line);
    snprintf(line,sizeof(line),"PDR EST:%.1f%%",peer.pdr);
    u8g2.drawStr(0,47,line);
  }
  snprintf(line,sizeof(line),"TX FAIL:%lu",(unsigned long)txFailures);
  u8g2.drawStr(0,60,line);
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
  u8g2.drawStr(0, 9, alertDemoMode ? "ALERT DEMO 5/7" : "ALERT 5/7");
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
  u8g2.drawStr(0, 9, "VESSEL UPTIME 6/7");
  char line[40];
  formatUptime(line, sizeof(line));
  u8g2.drawStr(0, 22, line);
  snprintf(line, sizeof(line), "WX 2/7: %s", shortWeatherName(weatherState));
  u8g2.drawStr(0, 34, line);
  snprintf(line, sizeof(line), "TX:%lu RX:%lu",
           static_cast<unsigned long>(txCount),
           static_cast<unsigned long>(getTotalPeerTelemetryReceived()));
  u8g2.drawStr(0, 46, line);
  u8g2.drawStr(0, 60, "Tap: NEXT PAGE");
  u8g2.sendBuffer();
}


// Phase 3.5.2: rescue navigation to last reported Vessel-B SOS position.
// Bearings are TRUE-north referenced; not bow-relative heading.
void oledSOSRescuePage() {
  const ReceivedSOSView sos = getReceivedSOSView();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "SOS / RESCUE 7/7");
  if (!sos.known) {
    u8g2.drawStr(0, 23, "NO SOS RECEIVED");
    u8g2.drawStr(0, 42, "LISTENING: ALL PEERS");
    u8g2.sendBuffer();
    return;
  }
  char line[38];
  snprintf(line, sizeof(line), "FROM: VESSEL-%03lu", (unsigned long)sos.originId);
  u8g2.drawStr(0, 20, line);
  const unsigned long ageSec = (millis() - sos.receivedAtMs) / 1000UL;
  if (sos.cancelled) u8g2.drawStr(0, 31, "CANCEL BY SENDER");
  else if (ageSec >= 60UL) u8g2.drawStr(0, 31, "OLD / UNRESOLVED");
  else u8g2.drawStr(0, 31, "SOS / UNRESOLVED");
  if (gpsHasFix && sos.positionValid) {
    const double meters = TinyGPSPlus::distanceBetween(latitude, longitude, sos.latitude, sos.longitude);
    const double brg = TinyGPSPlus::courseTo(latitude, longitude, sos.latitude, sos.longitude);
    snprintf(line, sizeof(line), "TO SOS: %.2f km", meters / 1000.0);
    u8g2.drawStr(0, 43, line);
    snprintf(line, sizeof(line), "BRG: %.0f deg TRUE", brg);
    u8g2.drawStr(0, 54, line);
  } else {
    u8g2.drawStr(0, 43, "DIST: UNKNOWN");
    u8g2.drawStr(0, 54, "BRG: UNKNOWN");
  }
  snprintf(line, sizeof(line), "SOS RX AGE: %lus", ageSec);
  u8g2.drawStr(0, 63, line);
  u8g2.sendBuffer();
}