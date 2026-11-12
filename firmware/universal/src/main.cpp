#ifndef THESIS_LAB_RELAY_TX
#define THESIS_LAB_RELAY_TX 0
#endif
#include "../../shared/hardware/tbeam_pins.h"
#include "../../shared/hardware/vessel_identity.h"
#include "../../shared/hardware/oled_button_debounce.h"
#include "../../shared/hardware/tbeam_hardware_service.h"
#include "../../shared/hardware/gps_service.h"
#include <Arduino.h>
#include "sos_controls.h"
#include "physical_indicators.h"
// Isolated Vessel-003 preflight. Never changes normal firmware behavior.
#ifndef THESIS_V003_F08_INDICATOR_BENCH
#define THESIS_V003_F08_INDICATOR_BENCH 0
#endif
#if THESIS_V003_F08_INDICATOR_BENCH != 0 && THESIS_V003_F08_INDICATOR_BENCH != 1
#error "THESIS_V003_F08_INDICATOR_BENCH must be 0 or 1"
#endif
#if THESIS_V003_F08_INDICATOR_BENCH == 1
#if THESIS_LAB_RELAY_TX != 0
#error "V003 F08 preflight cannot enable laboratory relay TX"
#endif
static_assert(!PhysicalIndicators::kOutputsEnabled &&
              !PhysicalIndicators::kWiringAndDriverReviewed &&
              PhysicalIndicators::kYellowPin == -1 &&
              PhysicalIndicators::kRedPin == -1 &&
              PhysicalIndicators::kBuzzerDriverPin == -1,
              "V003 F08 preflight must keep all physical outputs unassigned/OFF");
#endif
// This LED bench uses virtual states only in this build-only environment.
#ifndef THESIS_V003_F08_LED_BENCH
#define THESIS_V003_F08_LED_BENCH 0
#endif
#if THESIS_V003_F08_LED_BENCH != 0 && THESIS_V003_F08_LED_BENCH != 1
#error "THESIS_V003_F08_LED_BENCH must be 0 or 1"
#endif
#if THESIS_V003_F08_LED_BENCH == 1
#if THESIS_V003_F08_INDICATOR_BENCH != 1
#error "V003 LED preflight requires the weather-disabled Vessel-003 bench"
#endif
#include "v003_f08_led_bench.h"
static_assert(!V003F08LedBench::kOutputsEnabled &&
              V003F08LedBench::kBuzzerPin == -1,
              "V003 LED preflight build must keep ALL physical outputs OFF");
#endif
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include <TinyGPSPlus.h>
#include "lora_manager.h"
#include "../../shared/communication/decentralized_mac.h"
#include "../../shared/communication/mac_contention_gate.h"
// 0=original airtime scheduler; 1=OBSERVE CAD (default); 2=ENFORCE CAD.
// Mode 2 is experimental: use only after Mode 1 has passed hardware tests.
#ifndef MAC_CAD_MODE
#define MAC_CAD_MODE 0
#endif
#if MAC_CAD_MODE < 0 || MAC_CAD_MODE > 2
#error "MAC_CAD_MODE must be 0, 1, or 2"
#endif
#include <XPowersLib.h>
#ifndef THESIS_SD_LOGGER
#define THESIS_SD_LOGGER 0
#endif
#ifndef THESIS_F07_WEATHER_LOG
#define THESIS_F07_WEATHER_LOG 0
#endif
#if THESIS_F07_WEATHER_LOG == 1 && THESIS_SD_LOGGER != 1
#error "F07 weather logging requires the SD logger"
#endif
#if THESIS_V003_F08_INDICATOR_BENCH == 1 && \
    (THESIS_SD_LOGGER != 0 || THESIS_F07_WEATHER_LOG != 0)
#error "V003 F08 preflight cannot enable SD logging or weather records"
#endif
// Display-only SD OLED view. Enabled only in its dedicated F06 test profile.
#ifndef THESIS_SD_OLED_UI
#define THESIS_SD_OLED_UI 0
#endif
#if THESIS_SD_OLED_UI != 0 && THESIS_SD_OLED_UI != 1
#error "THESIS_SD_OLED_UI must be 0 or 1"
#endif
#if THESIS_SD_OLED_UI == 1 && THESIS_SD_LOGGER != 1
#error "SD OLED UI requires SD logger"
#endif
#ifndef THESIS_F06_SD_HYBRID
#define THESIS_F06_SD_HYBRID 0
#endif
#if THESIS_F06_SD_HYBRID != 0 && THESIS_F06_SD_HYBRID != 1
#error "Invalid THESIS_F06_SD_HYBRID"
#endif
#if THESIS_F06_SD_HYBRID == 1 && (THESIS_SD_OLED_UI != 1 || THESIS_SD_LOGGER != 1 || THESIS_F06_SD_SANDBOX != 1)
#error "F06 hybrid requires OLED UI and isolated sandbox SD logger"
#endif
#if THESIS_SD_OLED_UI == 1
#define OLED_PAGE_TOTAL "8"
static constexpr uint8_t kOLEDPageCount = 8;
#else
#define OLED_PAGE_TOTAL "7"
static constexpr uint8_t kOLEDPageCount = 7;
#endif
#if THESIS_SD_LOGGER == 1
#include "../../shared/storage/thesis_sd_logger.h"
#endif
#include <U8g2lib.h>
#include "weather_manager.h"
#ifndef THESIS_SD_PROBE
#define THESIS_SD_PROBE 0
#endif
#if THESIS_SD_PROBE == 1
#include "../../shared/storage/sd_probe.h"
#endif
#include <esp_timer.h>  // 64-bit uptime counter

// ============================================================
// UNIVERSAL - NUMERIC VESSEL IDENTITY
// T-Beam Supreme | L76K GPS | SX1262 | SH1106 OLED
// Dual I2C Environmental Sensors (BME680 & DHT20)
// ============================================================


// T-Beam Supreme user/BOOT button (active LOW)
#define USER_BUTTON_PIN 0


const unsigned long TX_INTERVAL = 5000UL;
// PHASE 3G-2: randomized telemetry slots, SOS handling unchanged.
// Guardrail: 4.25s..5.75s telemetry intervals, initial 1..5 seconds.
static constexpr uint32_t TELEMETRY_JITTER_MS = 750UL;
static constexpr uint32_t TELEMETRY_INITIAL_MIN_MS = 1000UL;
static constexpr uint32_t TELEMETRY_INITIAL_MAX_MS = 5000UL;
// PHASE 5.2.2C-3G-4E1: decentralized airtime-based scheduling.
static DecentralizedMac::Scheduler telemetryMac;

static uint32_t nextTelemetryIntervalMs() {
  // Arduino random(max) has exclusive upper bound.
  const long swing = random(-(long)TELEMETRY_JITTER_MS, (long)TELEMETRY_JITTER_MS + 1L);
  return TX_INTERVAL + swing;
}

// DEVELOPMENT ONLY: set to 0 for real 10-minute sampling before field use.
#ifndef WEATHER_FAST_TEST
#define WEATHER_FAST_TEST 1
#endif
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
#if THESIS_SD_OLED_UI == 1
// GPIO0 hold changes only the SD OLED subview; never changes logger state.
static uint8_t sdOLEDSubview = 0;
#if THESIS_F06_SD_HYBRID == 1
// GPIO0 mode menu is informational/selection only; GPIO2 owns physical SOS.
static bool sdHybridMenuOpen = false;
static uint8_t sdHybridMenuCursor = 0; // 0=SOS ARMED, 1=CONTINUOUS, 2=BACK
// Separate four-choice SD menu; both menus exist only on the hybrid profile.
static bool sdStorageMenuOpen = false;
static uint8_t sdStorageMenuCursor = 0; // 0=JOURNALS 1=USB 2=RECORD MODE 3=BACK
#endif
#endif
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
  u8g2.drawStr(0, 9, "SELECT VESSEL 3/" OLED_PAGE_TOTAL);
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


// PHASE 3F-2 SOS MENU: display-only. Records remain managed by LoRa receiver.
// The menu freezes active origin+event identifiers on opening; no TX is triggered.
static constexpr uint8_t kSOSMenuCapacity = 10;
static uint32_t sosMenuOrigins[kSOSMenuCapacity] = {};
static uint32_t sosMenuEvents[kSOSMenuCapacity] = {};
static uint8_t sosMenuCount = 0, sosMenuCursor = 0;
static bool sosMenuOpen = false;
static uint32_t selectedSOSOrigin = 0, selectedSOSEvent = 0; // zero = AUTO

bool findSOSRecord(uint32_t origin, uint32_t event, SOSRecordSnapshot &out) {
  for (size_t i=0; i<kSOSMenuCapacity; ++i) {
    SOSRecordSnapshot current;
    if (getSOSRecordByIndex(i,current) && current.known &&
        current.originId==origin && current.eventId==event) {
      out=current;return true;
    }
  }
  out=SOSRecordSnapshot{};return false;
}
void openSOSMenu() {
  sosMenuCount=0;sosMenuCursor=0;
  for (size_t i=0;i<kSOSMenuCapacity;++i) {
    SOSRecordSnapshot rec;
    if (getSOSRecordByIndex(i,rec) && rec.known && !rec.cancelled) {
      sosMenuOrigins[sosMenuCount]=rec.originId;
      sosMenuEvents[sosMenuCount]=rec.eventId;
      if (++sosMenuCount>=kSOSMenuCapacity) break;
    }
  }
  sosMenuOpen=true;
  Serial.printf("[SOS-MENU] OPEN active=%u\n",(unsigned)sosMenuCount);
}
void advanceSOSMenu() {
  sosMenuCursor=(sosMenuCursor+1)%(sosMenuCount+2); // AUTO, active records, BACK
}
void confirmSOSMenu() {
  if (sosMenuCursor==0) {
    selectedSOSOrigin=0;selectedSOSEvent=0;
    Serial.println("[SOS-MENU] CHOSEN AUTO");
  } else if (sosMenuCursor==sosMenuCount+1) {
    Serial.println("[SOS-MENU] BACK");
  } else {
    const uint8_t i=sosMenuCursor-1;
    SOSRecordSnapshot rec;
    if (!findSOSRecord(sosMenuOrigins[i],sosMenuEvents[i],rec)||rec.cancelled) {
      Serial.println("[SOS-MENU] EVENT NO LONGER ACTIVE; CHOOSE AGAIN");
      openSOSMenu();return;
    }
    selectedSOSOrigin=rec.originId;selectedSOSEvent=rec.eventId;
    Serial.printf("[SOS-MENU] CHOSEN origin=%lu event=%lu\n",
      (unsigned long)selectedSOSOrigin,(unsigned long)selectedSOSEvent);
  }
  sosMenuOpen=false;
}
void drawSOSMenu() {
  u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"ACTIVE SOS MENU 7/" OLED_PAGE_TOTAL);
  const uint8_t choices=sosMenuCount+2;
  const uint8_t first=sosMenuCursor>=3?sosMenuCursor-3:0;
  for(uint8_t row=0;row<4 && first+row<choices;++row) {
    const uint8_t idx=first+row;
    char label[30]={};
    if(idx==0)strcpy(label,"Auto (newest)");
    else if(idx==sosMenuCount+1)strcpy(label,"Back");
    else snprintf(label,sizeof(label),"V%03lu SOS#%lu",
      (unsigned long)sosMenuOrigins[idx-1],(unsigned long)sosMenuEvents[idx-1]);
    char line[34];snprintf(line,sizeof(line),"%c %s",idx==sosMenuCursor?'>':' ',label);
    u8g2.drawStr(0,21+row*12,line);
  }
  u8g2.sendBuffer();
}
// In AUTO mode, show latest ACTIVE event, not the most recently cancelled one.
bool displaySOSRecord(SOSRecordSnapshot &out) {
  if (selectedSOSOrigin) return findSOSRecord(selectedSOSOrigin,selectedSOSEvent,out);
  bool have=false;
  for(size_t i=0;i<kSOSMenuCapacity;++i) {
    SOSRecordSnapshot rec;
    if (!getSOSRecordByIndex(i,rec)||!rec.known||rec.cancelled)continue;
    if(!have || (int32_t)(rec.receivedAtMs-out.receivedAtMs)>0) {out=rec;have=true;}
  }
  return have;
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
#if THESIS_SD_OLED_UI == 1
void oledSDStoragePage();
#endif
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
#if THESIS_SD_LOGGER == 1
// The single logger instance is owned by main.cpp; radio code only queues.
bool thesisLogEvent(bool urgent, const char* kind, uint32_t origin,
                    uint32_t seq, int32_t result, int16_t rssi,
                    int16_t snrX10, int32_t latE7, int32_t lonE7,
                    uint8_t flags, uint8_t hops, uint8_t attempt) {
    return ThesisSDLogger::queueEvent(urgent, kind, origin, seq, result,
                                     rssi, snrX10, latE7, lonE7,
                                     flags, hops, attempt);
}
#endif

#if THESIS_SD_LOGGER == 1
// Existing TEST commands remain handled by processSerialTestCommands().
bool pollSDExportCommand() {
  static char command[24];
  static uint8_t used = 0;
  if (!Serial.available() || (used == 0 && Serial.peek() != 'S')) return false;
  // Only runs after the first 'S' has arrived; await the newline across loops.
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\n' || c == '\r') {
      command[used] = '\0';
      if (strcmp(command, "SD EXPORT") == 0) {
#if THESIS_F06_SD_SANDBOX == 1
        if (ThesisSDLogger::historyReadActive())
          Serial.println("[SDH] ERROR HISTORY_BUSY");
        else
#endif
          ThesisSDLogger::requestExport();
      }
#if THESIS_F06_SD_SANDBOX == 1
      else if (strcmp(command, "SD LIST") == 0)
        ThesisSDLogger::listHistory();
      else if (strncmp(command, "SD READ ", 8) == 0)
        ThesisSDLogger::requestHistory(command + 8);
#endif
      else Serial.println("[SDX] ERROR UNKNOWN_COMMAND");
      used = 0;
      return true;
    }
    if (used >= sizeof(command) - 1) {
      used = 0;
      Serial.println("[SDX] ERROR COMMAND_TOO_LONG");
      return true;
    }
    command[used++] = (char)c;
  }
  return true;
}
#endif

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("=== UNIVERSAL VESSEL: SCHEDULED TX + RX ===");
  Serial.println("Packet: 62 bytes | MAC adaptive telemetry (min base 5s)");
#if THESIS_V003_F08_INDICATOR_BENCH == 1
  Serial.println("[F08-BENCH] Weather operations DISABLED; physical outputs OFF/unassigned");
#else
  Serial.printf("[WEATHER] %s, sample interval %lu ms; history %u samples\n",
                WEATHER_FAST_TEST ? "ACCELERATED DEVELOPMENT (NOT REAL 100-MIN HISTORY)" : "FIELD",
                WEATHER_INTERVAL, WEATHER_HISTORY_SIZE);
#endif

  SOSControls::begin();
  PhysicalIndicators::begin();
#if THESIS_V003_F08_LED_BENCH == 1
  V003F08LedBench::instance().begin();
  Serial.println("[F08-LED] preflight=BUILD_ONLY GPIO39/44=DISABLED buzzer=DISABLED");
#endif
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  buttonLastReading = digitalRead(USER_BUTTON_PIN);
  buttonStableState = buttonLastReading;
  buttonLastDebounceTime = millis();
  Serial.println("[OLED] Tap GPIO0: next page; on ALERT page hold >=900ms: LIVE/NORMAL/FRONT/SQUALL demo");

  setupPMU();
#if THESIS_SD_PROBE == 1
  ThesisSD::probe(pmu);
#endif
  setupOLED();
#if THESIS_V003_F08_INDICATOR_BENCH == 1
  // GPIO38/39 are NOT configured for sensor I2C in this build-only profile.
  // Weather alerts are unavailable; authoritative SOS remains independent.
  weatherInputsValid = false;
  weatherState = WEATHER_CALIBRATING;
  Serial.println("[F08-BENCH] DHT20/BME680 initialization SKIPPED");
#else
  setupSensors();
#endif
  setupGPS();
  VesselIdentity::load();
  setupLoRa();
  setupEmergencyIdentity();
  startReceive();
#if THESIS_SD_LOGGER == 1
  ThesisSDLogger::begin(pmu, VesselIdentity::nodeId());
#endif

#if THESIS_F07_WEATHER_LOG == 1
  // One REAL reading at boot for immediate OLED fault detection.
  // Log the same physical sample, then continue at 10-minute intervals.
  processWeather();
  if (!ThesisSDLogger::queueWeatherSample(weatherMeasurement))
    Serial.println("[WX-LOG] BOOT_SAMPLE_QUEUE_FAILED (not recorded)");
#endif

  // Each boot receives a different first telemetry slot. Subsequent intervals
  // vary independently; no change to SOS timing or packet serialization.
  randomSeed(esp_random());
  telemetryMac.begin(VesselIdentity::nodeId(), millis(), esp_random());
  Serial.printf("[MAC-INIT] v=3G4E3A id=%lu mode=AIRTIME_ONLY "
                "firstInMs=%lu maxKnownPeers=10 cadMode=%u\n",
    (unsigned long)VesselIdentity::nodeId(),
    (unsigned long)(uint32_t)(telemetryMac.nextDueMs() - millis()), (unsigned)MAC_CAD_MODE);

  lastWeatherTime = millis();
  lastOLEDUpdate = millis();
  Serial.printf("[UNIVERSAL] %s (ID %lu): firmware initialized; relay forwarding controlled by build; multi-vessel validation PENDING\n",
                VesselIdentity::label(), (unsigned long)VesselIdentity::nodeId());
#if THESIS_LAB_RELAY_TX == 1
  Serial.println("[LAB-RELAY] BUILD=ENABLED; BENCH ONLY; VESSEL-002 GATE");
#else
  Serial.println("[LAB-RELAY] BUILD=DISABLED");
#endif
  Serial.println("[TEST] Type TEST ALL, TEST NORMAL, TEST FRONT, TEST SQUALL, TEST BOTH, TEST CALIB, TEST Z, or TEST HELP");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
#if THESIS_SD_LOGGER == 1
#if THESIS_F06_SD_HYBRID == 1
  // Assess authoritative local and received SOS state before SD poll.
  // An unresolved SOS keeps recording regardless of manual mode.
  ThesisSDLogger::hybridObserveSOSPresence(
      hasActiveLocalSOS() || getActiveSOSCount() > 0);
#endif
  ThesisSDLogger::poll();
#endif
#if THESIS_F06_SD_SANDBOX == 1
  ThesisSDLogger::pollHistory();
#endif
  updateGPS();
#if THESIS_SD_LOGGER == 1
  const bool sdCommandInProgress = pollSDExportCommand();
  ThesisSDLogger::pollExport();
  if (!sdCommandInProgress) processSerialTestCommands();
#else
  processSerialTestCommands();
#endif
  const auto action = SOSControls::pollAction(hasActiveLocalSOS());
  if (action == SOSControls::ButtonAction::StartSOS) {
    Serial.println("[SOS-BUTTON-A] Deliberate 1.5s SOS hold recognized");
    SOSControls::noteLocalSOS();
    transmitSOS();
  } else if (action == SOSControls::ButtonAction::CancelSOS) {
    Serial.println("[SOS-BUTTON-A] Deliberate 5s CANCEL hold recognized");
    cancelActiveSOS();
  }

  // CAD uses the same DIO1 as RX_DONE; consume its IRQ before RX polling.
  const TelemetryCadResult macCadCompleted = pollTelemetryCad();
  pollLoRaReceive();
  if (consumeNewReceivedSOS()) {
    oledPage = 6;  // New SOS overrides any page/selection.
    sosMenuOpen = false;
#if THESIS_F06_SD_HYBRID == 1
    sdStorageMenuOpen = false;
    sdHybridMenuOpen = false;
    sdOLEDSubview = 0;
#endif
    selectedSOSOrigin=0;selectedSOSEvent=0;
    lastOLEDUpdate = 0;
  }
  processSOSRetries();
  processCancelRetries();

#if THESIS_V003_F08_INDICATOR_BENCH == 0
  if (millis() - lastWeatherTime >= WEATHER_INTERVAL) {
    lastWeatherTime = millis();
    processWeather();
#if THESIS_SD_LOGGER == 1 && THESIS_F07_WEATHER_LOG == 1
    // GNSS time is usable without a position fix, but only if both the
    // date and time were received recently. Otherwise JSON gets null.
    // Do not change the device clock or synthesize a calendar time.
    if (gps.date.isValid() && gps.time.isValid() &&
        gps.date.age() <= 10000UL && gps.time.age() <= 10000UL) {
      const int n = snprintf(weatherMeasurement.gpsUtcIso8601,
          sizeof(weatherMeasurement.gpsUtcIso8601),
          "%04u-%02u-%02uT%02u:%02u:%02uZ",
          (unsigned)gps.date.year(), (unsigned)gps.date.month(),
          (unsigned)gps.date.day(), (unsigned)gps.time.hour(),
          (unsigned)gps.time.minute(), (unsigned)gps.time.second());
      weatherMeasurement.gpsUtcValid = (n == 20);
    }
    if (!ThesisSDLogger::queueWeatherSample(weatherMeasurement))
      Serial.println("[WX-LOG] QUEUE_FAILED (not recorded)");
#endif
  }
#endif  // THESIS_V003_F08_INDICATOR_BENCH == 0

  // Live authoritative inputs only; OLED demo modes never reach physical outputs.
  PhysicalIndicators::update(millis(), weatherState, weatherInputsValid,
                             hasActiveLocalSOS(), getActiveSOSCount() > 0);
#if THESIS_V003_F08_LED_BENCH == 1
  // SOS ALWAYS clears any virtual LED test, with a rollover-safe 500ms limit.
  V003F08LedBench::instance().tick(
      millis(), hasActiveLocalSOS() || getActiveSOSCount() > 0);
#endif

  // PHASE 5.2.2C-3G-4E2: decentralized, nonblocking CAD experiment.
  // Mode 1 OBSERVE (default): busy CAD is logged but does not block telemetry.
  // Mode 2 ENFORCE (opt-in): finite random backoff, then fail-open TX.
#if MAC_CAD_MODE == 0
  if (telemetryMac.due(millis())) {
    const uint32_t macNowMs = millis();
    const uint32_t macPeers = getRecentMacPeerCount(
        macNowMs, DecentralizedMac::kRecentPeerHorizonMs);
    const uint32_t macIntervalMs = telemetryMac.scheduleNext(macNowMs, macPeers);
    Serial.printf("[MAC-TX] id=%lu mode=OFF peers=%lu baseMs=%lu nextInMs=%lu\n",
      (unsigned long)VesselIdentity::nodeId(), (unsigned long)macPeers,
      (unsigned long)telemetryMac.baseMs(), (unsigned long)macIntervalMs);
    transmitPacket();
  }
#else
  static MacContention::Gate macGate;
  if (!macGate.pending() && telemetryMac.due(millis())) {
    macGate.begin(millis());
  }
  if (macGate.pending()) {
    const uint32_t macNowMs = millis();
    TelemetryCadResult outcome = macCadCompleted;
    if (outcome == TelemetryCadResult::None &&
        !telemetryCadRunning() && macGate.ready(macNowMs)) {
      outcome = startTelemetryCad();
    }
    if (outcome != TelemetryCadResult::None &&
        outcome != TelemetryCadResult::Scanning) {
      MacContention::Sense sense = MacContention::Sense::Error;
      const char *outcomeName = "ERROR";
      switch (outcome) {
        case TelemetryCadResult::Free: sense=MacContention::Sense::Free; outcomeName="FREE"; break;
        case TelemetryCadResult::Busy: sense=MacContention::Sense::Busy; outcomeName="BUSY"; break;
        case TelemetryCadResult::RxActive: sense=MacContention::Sense::RxActive; outcomeName="RX_ACTIVE"; break;
        case TelemetryCadResult::Timeout: sense=MacContention::Sense::Timeout; outcomeName="TIMEOUT"; break;
        case TelemetryCadResult::Interrupted: sense=MacContention::Sense::Interrupted; outcomeName="CONTROL_TX"; break;
        default: break;
      }
      const uint32_t beforeMs = millis();
      const auto decision = macGate.report(beforeMs, sense,
                                           MAC_CAD_MODE, esp_random());
      Serial.printf("[MAC-CAD] id=%lu mode=%s result=%s action=%s "
                    "elapsedMs=%lu retries=%u\n",
        (unsigned long)VesselIdentity::nodeId(),
        MAC_CAD_MODE == 1 ? "OBSERVE" : "ENFORCE", outcomeName,
        decision == MacContention::Decision::Wait ? "BACKOFF" :
        decision == MacContention::Decision::ForcedSend ? "FORCE_SEND" : "SEND",
        (unsigned long)macGate.elapsed(beforeMs), (unsigned)macGate.retries());
      if (decision != MacContention::Decision::Wait) {
        const uint32_t nowTx = millis();
        const uint32_t peers = getRecentMacPeerCount(
            nowTx, DecentralizedMac::kRecentPeerHorizonMs);
        const uint32_t nextMs = telemetryMac.scheduleNext(nowTx, peers);
        macGate.finish(); // reset before the blocking radio transmission
        Serial.printf("[MAC-TX] id=%lu mode=%s peers=%lu nodes=%lu baseMs=%lu nextInMs=%lu\n",
          (unsigned long)VesselIdentity::nodeId(),
          MAC_CAD_MODE == 1 ? "OBSERVE" : "ENFORCE",
          (unsigned long)peers, (unsigned long)(peers + 1UL),
          (unsigned long)telemetryMac.baseMs(), (unsigned long)nextMs);
        transmitPacket(); // original routine TX and RX restoration unchanged
      }
    }
  }
#endif

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
      } else if (oledPage == 6 && duration >= BUTTON_LONG_PRESS_MS) {
        if (sosMenuOpen) confirmSOSMenu(); else openSOSMenu();
      } else if (oledPage == 6 && sosMenuOpen && duration < BUTTON_LONG_PRESS_MS) {
        advanceSOSMenu();
#if THESIS_SD_OLED_UI == 1
#if THESIS_F06_SD_HYBRID == 1
      } else if (oledPage == 7 && sdHybridMenuOpen) {
        // Mode submenu: TAP moves, HOLD confirms. BACK returns to SD menu.
        if (duration >= BUTTON_LONG_PRESS_MS) {
          if (sdHybridMenuCursor < 2) {
            ThesisSDLogger::hybridSetContinuous(sdHybridMenuCursor == 1);
            Serial.printf("[OLED-SD] MODE_CONFIRMED=%s\n",
                sdHybridMenuCursor == 1 ? "CONTINUOUS" : "SOS_ARMED");
            sdOLEDSubview = 0;
          } else {
            sdStorageMenuOpen = true;
            sdStorageMenuCursor = 2;
            Serial.println("[OLED-SD] MODE_BACK_TO_SD_MENU");
          }
          sdHybridMenuOpen = false;
        } else {
          sdHybridMenuCursor = (sdHybridMenuCursor + 1U) % 3U;
          Serial.printf("[OLED-SD] MODE_CURSOR=%u\n", (unsigned)sdHybridMenuCursor);
        }
      } else if (oledPage == 7 && sdStorageMenuOpen) {
        // SD menu: Journals, USB, Recording Mode, Back. No direct file writes.
        if (duration >= BUTTON_LONG_PRESS_MS) {
          switch (sdStorageMenuCursor) {
            case 0:
              sdOLEDSubview = 1; // Journals: cached counters only
              sdStorageMenuOpen = false;
              Serial.println("[OLED-SD] OPEN_JOURNALS");
              break;
            case 1:
              sdOLEDSubview = 2; // USB: instructional text only
              sdStorageMenuOpen = false;
              Serial.println("[OLED-SD] OPEN_USB_READBACK");
              break;
            case 2:
              sdStorageMenuOpen = false;
              sdHybridMenuOpen = true;
              sdHybridMenuCursor = ThesisSDLogger::hybridIsContinuous() ? 1 : 0;
              Serial.println("[OLED-SD] MODE_MENU_OPEN");
              break;
            default:
              sdStorageMenuOpen = false;
              sdOLEDSubview = 0;
              Serial.println("[OLED-SD] STORAGE_MENU_BACK");
              break;
          }
        } else {
          sdStorageMenuCursor = (sdStorageMenuCursor + 1U) % 4U;
          Serial.printf("[OLED-SD] STORAGE_CURSOR=%u\n", (unsigned)sdStorageMenuCursor);
        }
      } else if (oledPage == 7 && duration >= BUTTON_LONG_PRESS_MS) {
        // Hold on status, journals or USB always opens the same options menu.
        sdStorageMenuOpen = true;
        sdStorageMenuCursor = 0;
        Serial.println("[OLED-SD] STORAGE_MENU_OPEN");
#else
      } else if (oledPage == 7 && duration >= BUTTON_LONG_PRESS_MS) {
        // Keep original three-view hold navigation for the non-hybrid UI.
        sdOLEDSubview = (sdOLEDSubview + 1U) % 3U;
        Serial.printf("[OLED-SD] VIEW=%u DISPLAY_ONLY\n", (unsigned)sdOLEDSubview);
#endif
#endif
      } else if (oledPage == 4 && duration >= BUTTON_LONG_PRESS_MS) {
        alertDemoMode = (alertDemoMode + 1) % 4;
        Serial.printf("[ALERT-DEMO] OLED-only mode: %u (0=LIVE,1=NORMAL,2=FRONT,3=SQUALL)\n", alertDemoMode);
      } else if (duration < BUTTON_LONG_PRESS_MS) {
        oledPage = (oledPage + 1) % kOLEDPageCount;
#if THESIS_SD_OLED_UI == 1
        if (oledPage != 7) sdOLEDSubview = 0;
#endif
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
#if THESIS_SD_OLED_UI == 1
    case 7: oledSDStoragePage(); break;
#endif
  }
}

void oledGPSPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];
  snprintf(line, sizeof(line), "%s GPS 1/" OLED_PAGE_TOTAL, VesselIdentity::ready() ? VesselIdentity::label() : "NO ID");
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

  const char* wxHeader = !weatherSampleAttempted ? "CHECKING"
      : !weatherInputsValid ? "SENSOR FAIL" : shortWeatherName(weatherState);
  snprintf(line, sizeof(line), "WX 2/" OLED_PAGE_TOTAL ": %s", wxHeader);
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

  if (!weatherSampleAttempted) {
    snprintf(line, sizeof(line), "DHT:%s BME:%s",
             dhtAvailable ? "FOUND" : "MISS",
             bmeAvailable ? "FOUND" : "MISS");
  } else {
    snprintf(line, sizeof(line), "DHT:%s BME-P:%s",
             dhtReadingValid ? "OK" : "FAIL",
             bmePressureReadingValid ? "OK" : "FAIL");
  }
  u8g2.drawStr(0, 39, line);

  if (!weatherSampleAttempted) strcpy(line, "WAIT: FIRST READ");
  else if (!dhtReadingValid && !bmePressureReadingValid) strcpy(line, "DHT+BME: READ FAIL");
  else if (!dhtReadingValid) strcpy(line, "DHT20: READ FAIL");
  else if (!bmePressureReadingValid) strcpy(line, "BME680: READ FAIL");
  else strcpy(line, "T/RH: DHT20 ONLY");
  u8g2.drawStr(0, 49, line);

  if (!weatherSampleAttempted) strcpy(line, "WAIT: FIRST SAMPLE");
  else if (!weatherInputsValid) strcpy(line, "FORECAST: DISABLED");
  else if (weatherState == WEATHER_CALIBRATING ||
           weatherMeasurement.historyCount < WEATHER_HISTORY_SIZE) {
    snprintf(line, sizeof(line), "CALIBRATING %u/%u",
             (unsigned)weatherMeasurement.historyCount, (unsigned)WEATHER_HISTORY_SIZE);
  } else {
    snprintf(line, sizeof(line), "Z:%u DP:%+.1f", zambrettiIndex, pressureDelta);
  }
  u8g2.drawStr(0, 59, line);
  u8g2.sendBuffer();
}

void oledLinkPage() {
  if (oledPeerMenuOpen) { drawOLEDPeerMenu(); return; }
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,selectedOLEDPeerId ? "SELECT PEER 3/" OLED_PAGE_TOTAL : "LIVE PEER 3/" OLED_PAGE_TOTAL);
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
  u8g2.drawStr(0,9,"PEER PDR 4/" OLED_PAGE_TOTAL);
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
  u8g2.drawStr(0, 9, alertDemoMode ? "ALERT DEMO 5/" OLED_PAGE_TOTAL : "ALERT 5/" OLED_PAGE_TOTAL);
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
  u8g2.drawStr(0, 9, "VESSEL UPTIME 6/" OLED_PAGE_TOTAL);
  char line[40];
  formatUptime(line, sizeof(line));
  u8g2.drawStr(0, 22, line);
  snprintf(line, sizeof(line), "WX 2/" OLED_PAGE_TOTAL ": %s", shortWeatherName(weatherState));
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
  if(sosMenuOpen){drawSOSMenu();return;}
  SOSRecordSnapshot sos;
  const bool present=displaySOSRecord(sos);
  u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"SOS / RESCUE 7/" OLED_PAGE_TOTAL);
  if(!present) {
    u8g2.drawStr(0,23,selectedSOSOrigin?"EVENT REPLACED":"NO ACTIVE SOS");
    u8g2.drawStr(0,42,"Hold: SELECT SOS");
    u8g2.sendBuffer();return;
  }
  char line[40];
  snprintf(line,sizeof(line),"FROM: VESSEL-%03lu",(unsigned long)sos.originId);
  u8g2.drawStr(0,20,line);
  snprintf(line,sizeof(line),"EVENT:%lu %s",(unsigned long)sos.eventId,
           selectedSOSOrigin?"SELECT":"AUTO");
  u8g2.drawStr(0,30,line);
  if(sos.cancelled)u8g2.drawStr(0,40,"CANCEL BY SENDER");
  else if(millis()-sos.receivedAtMs>=60000UL)u8g2.drawStr(0,40,"OLD / UNRESOLVED");
  else u8g2.drawStr(0,40,"SOS / UNRESOLVED");
  if(gpsHasFix && sos.positionValid) {
    const double meters=TinyGPSPlus::distanceBetween(latitude,longitude,sos.latitude,sos.longitude);
    const double bearing=TinyGPSPlus::courseTo(latitude,longitude,sos.latitude,sos.longitude);
    snprintf(line,sizeof(line),"DIST: %.2f km",meters/1000.0);
    u8g2.drawStr(0,51,line);
    snprintf(line,sizeof(line),"BRG: %.0f deg TRUE",bearing);
    u8g2.drawStr(0,62,line);
  } else {
    u8g2.drawStr(0,51,"DIST: UNKNOWN");
    u8g2.drawStr(0,62,"BRG: UNKNOWN");
  }
  u8g2.sendBuffer();
}

#if THESIS_SD_OLED_UI == 1
// 128x64 SH1106 display. Read cached logger counters ONLY: no SPI, file
// opens, directory mutations, radio TX, or emergency state writes here.
void oledSDStoragePage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[38];
#if THESIS_F06_SD_HYBRID == 1
  if (sdStorageMenuOpen) {
    u8g2.drawStr(0, 9, "SD STORAGE MENU 8/8");
    const char* menuItems[4] = {
        "SD JOURNALS", "SD USB READBACK", "RECORDING MODE", "BACK"};
    for (uint8_t index = 0; index < 4; ++index) {
      snprintf(line, sizeof(line), "%c %s",
          index == sdStorageMenuCursor ? '>' : ' ', menuItems[index]);
      u8g2.drawStr(0, 21 + 11 * index, line);
    }
    u8g2.drawStr(0, 64, "HOLD:OPEN TAP:MOVE");
    u8g2.sendBuffer();
    return;
  }
  if (sdHybridMenuOpen) {
    u8g2.drawStr(0, 9, "RECORDING MODE 8/8");
    const char* menuItems[3] = {"SOS ARMED", "CONTINUOUS", "BACK"};
    for (uint8_t index = 0; index < 3; ++index) {
      snprintf(line, sizeof(line), "%c %s",
          index == sdHybridMenuCursor ? '>' : ' ', menuItems[index]);
      u8g2.drawStr(0, 23 + 13 * index, line);
    }
    u8g2.drawStr(0, 62, "HOLD:SET TAP:MOVE");
    u8g2.sendBuffer();
    return;
  }
#endif
  if (sdOLEDSubview == 0) {
    u8g2.drawStr(0, 9, "SD STORAGE 8/8");
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 20, ThesisSDLogger::ready ? "CARD: MOUNTED" : "CARD: NOT READY");
    u8g2.drawStr(0, 30, ThesisSDLogger::hybridIsContinuous()
        ? "MODE: CONTINUOUS" : "MODE: SOS ARMED");
    const char* recStatus = !ThesisSDLogger::ready ? "REC: UNAVAILABLE" :
        ThesisSDLogger::hybridSOSOverride() ? "REC: SOS OVERRIDE" :
        ThesisSDLogger::hybridIsContinuous() ? "REC: CONTINUOUS" : "REC: STANDBY";
    u8g2.drawStr(0, 41, recStatus);
    const char* slash = strrchr(ThesisSDLogger::DIRECTORY, '/');
    snprintf(line, sizeof(line), "FOLDER: %.12s", slash ? slash + 1 : "UNKNOWN");
    u8g2.drawStr(0, 52, line);
#else
    u8g2.drawStr(0, 20, ThesisSDLogger::ready ? "CARD: LOGGING" : "CARD: NOT READY");
#if THESIS_F06_SD_SANDBOX == 1
    u8g2.drawStr(0, 30, "MODE: F06 SANDBOX");
#else
    u8g2.drawStr(0, 30, "MODE: VESSEL JOURNAL");
#endif
    const char* slash = strrchr(ThesisSDLogger::DIRECTORY, '/');
    const char* folder = slash ? slash + 1 : "UNKNOWN";
    snprintf(line, sizeof(line), "FOLDER: %.12s", folder);
    u8g2.drawStr(0, 41, line);
    snprintf(line, sizeof(line), "Q SOS:%u TLM:%u",
      (unsigned)ThesisSDLogger::urgentQueue.count,
      (unsigned)ThesisSDLogger::telemetryQueue.count);
    u8g2.drawStr(0, 52, line);
#endif
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 63, "HOLD:MENU TAP:NEXT");
#else
    u8g2.drawStr(0, 63, "HOLD:FILES TAP:NEXT");
#endif
  } else if (sdOLEDSubview == 1) {
    u8g2.drawStr(0, 9, "SD JOURNALS 8/8");
    if (ThesisSDLogger::oledAuditValid) {
      snprintf(line, sizeof(line), "TLM: %llu B",
        (unsigned long long)ThesisSDLogger::oledTelemetryBytes);
      u8g2.drawStr(0, 21, line);
      snprintf(line, sizeof(line), "SOS: %llu B",
        (unsigned long long)ThesisSDLogger::oledSOSBytes);
      u8g2.drawStr(0, 33, line);
    } else {
      u8g2.drawStr(0, 21, "FILE SIZES: WAIT");
      u8g2.drawStr(0, 33, "SD AUDIT (30 SEC)");
    }
#if THESIS_F06_SD_HYBRID == 1
    snprintf(line, sizeof(line), "EV:%lu SKIP:%lu",
      (unsigned long)ThesisSDLogger::eventCounter,
      (unsigned long)ThesisSDLogger::hybridSkippedTelemetry());
#else
    snprintf(line, sizeof(line), "EVENTS: %lu",
      (unsigned long)ThesisSDLogger::eventCounter);
#endif
    u8g2.drawStr(0, 44, line);
    snprintf(line, sizeof(line), "DROP S:%lu T:%lu",
      (unsigned long)ThesisSDLogger::lostUrgent,
      (unsigned long)ThesisSDLogger::lostTelemetry);
    u8g2.drawStr(0, 55, line);
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 64, "HOLD:MENU TAP:NEXT");
#else
    u8g2.drawStr(0, 64, "HOLD:USB TAP:NEXT");
#endif
  } else {
    u8g2.drawStr(0, 9, "SD USB READBACK 8/8");
#if THESIS_F06_SD_SANDBOX == 1
    u8g2.drawStr(0, 21, "PC COMMAND: SD LIST");
    u8g2.drawStr(0, 33, "SD READ Sxxxxxxx");
    u8g2.drawStr(0, 45, "OR: SD READ LEGACY");
#else
    u8g2.drawStr(0, 21, "PC: SD EXPORT");
    u8g2.drawStr(0, 33, "USB SERIAL 115200");
    u8g2.drawStr(0, 45, "READBACK TO PC");
#endif
    u8g2.drawStr(0, 55, "NO ACTION FROM OLED");
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 64, "HOLD:MENU TAP:NEXT");
#else
    u8g2.drawStr(0, 64, "HOLD:HOME TAP:NEXT");
#endif
  }
  u8g2.sendBuffer();
}
#endif // THESIS_SD_OLED_UI
