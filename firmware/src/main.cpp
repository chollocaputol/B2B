#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include <TinyGPSPlus.h>
#include <RadioLib.h>
#include <XPowersLib.h>
#include <U8g2lib.h>
#include "weather_manager.h"
#include <esp_timer.h>  // 64-bit uptime counter

// ============================================================
// BOARD A - VESSEL-A
// T-Beam Supreme | L76K GPS | SX1262 | SH1106 OLED
// Dual I2C Environmental Sensors (BME680 & DHT20)
// ============================================================

#define LORA_SCK       12
#define LORA_MISO      13
#define LORA_MOSI      11
#define LORA_CS        10
#define LORA_DIO1       1
#define LORA_RST        5
#define LORA_BUSY       4

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

#define NODE_ID        0xA1
const char NODE_NAME[] = "Vessel-A";

// Must match Board B.
#define LORA_FREQUENCY  915.0
#define LORA_BANDWIDTH  125.0
#define LORA_SPREADING  9
#define LORA_CODING     7
#define LORA_SYNC_WORD  0x12
#define LORA_POWER      17
#define LORA_PREAMBLE   8
#define LORA_GAIN       1.8

#define PACKET_MAGIC    0x4D45
#define PACKET_VERSION  1
#define FLAG_GPS_FIX                0x01
#define FLAG_TEMP_HUMIDITY_VALID    0x02
#define FLAG_PRESSURE_VALID         0x04

const unsigned long TX_INTERVAL = 5000UL;
// DEVELOPMENT ONLY: set to 0 for real 10-minute sampling before field use.
#define WEATHER_FAST_TEST 1
const unsigned long WEATHER_INTERVAL = WEATHER_FAST_TEST ? 5000UL : 600000UL;
const unsigned long GPS_STATUS_INTERVAL = 5000UL;
const unsigned long OLED_UPDATE_INTERVAL = 500UL;
const unsigned long UPTIME_PRINT_INTERVAL = 60000UL; // Serial heartbeat every minute
unsigned long lastWeatherTime = 0;
#define RX_SEQUENCE_CACHE 256

enum PacketType : uint8_t {
  PACKET_TELEMETRY = 1,
  PACKET_SOS = 2
};

struct __attribute__((packed)) WeatherData {
  int16_t temperature_x10;
  uint16_t humidity_x10;
  uint16_t pressure_x10;
  int16_t pressureTrend_x10;
  uint8_t zambrettiState;
  uint8_t sensorFusionState;
  uint8_t weatherState;
};

struct __attribute__((packed)) VesselPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t packetType;
  uint32_t originNodeId;
  uint32_t sequenceNumber;
  uint32_t timestamp;
  uint8_t hopCount;
  uint8_t flags;
  char nodeName[12];
  int32_t latitude_e7;
  int32_t longitude_e7;
  uint32_t shoreDistance_m;
  uint8_t batteryLevel;
  WeatherData weather;
  char message[8];
};
static_assert(sizeof(VesselPacket) == 62, "Packet must remain 62 bytes");

// -------------------- Hardware --------------------------------
SPIClass loraSPI(FSPI);
Module radioModule(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);
SX1262 radio(&radioModule);
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

// -------------------- Board A transmission --------------------
uint32_t txSequence = 0;
uint32_t txCount = 0;
uint32_t txFailures = 0;
bool lastTXSuccess = false;

// -------------------- Last Board B received --------------------
uint32_t rxFromB = 0;
uint32_t uniqueFromB = 0;
uint32_t duplicateFromB = 0;
uint32_t missingFromB = 0;
uint32_t firstBSequence = 0;
uint32_t highestBSequence = 0;
bool haveBSequence = false;
uint32_t seenBSequences[RX_SEQUENCE_CACHE] = {};
bool seenBSequenceValid[RX_SEQUENCE_CACHE] = {};
uint16_t seenBCursor = 0;
uint32_t invalidPackets = 0;

bool bGPSValid = false;
double bLatitude = 0.0;
double bLongitude = 0.0;
double distanceToBkm = -1.0;
int16_t lastBRSSI = 0;
float lastBSNR = 0.0f;
unsigned long lastBSeen = 0;
uint8_t bWeatherState = WEATHER_CALIBRATING;

// -------------------- Declarations -----------------------------
void switchI2C(uint8_t sda, uint8_t scl);
void setupPMU();
void setupOLED();
bool detectOLED();
void setupGPS();
void updateGPS();
void printGPSStatus();
void setupLoRa();
void startReceive();
void createPacket(VesselPacket &p);
void transmitPacket();
void processReceivedPacket(const uint8_t *buffer, size_t length, int16_t rssi, float snr);
void trackBSequence(uint32_t sequence);
bool bSequenceWasSeen(uint32_t sequence);
void rememberBSequence(uint32_t sequence);
void resetBSequenceSession();
void calculateBSequencePDR();
float bSequencePDR();
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

  if (digitalRead(LORA_DIO1) == HIGH) {
    size_t len = radio.getPacketLength();
    if (len > 0 && len <= sizeof(VesselPacket)) {
      uint8_t buffer[sizeof(VesselPacket)];
      int state = radio.readData(buffer, len);
      if (state == RADIOLIB_ERR_NONE) {
        int16_t rssi = static_cast<int16_t>(radio.getRSSI());
        float snr = radio.getSNR();
        processReceivedPacket(buffer, len, rssi, snr);
      } else {
        Serial.print("[RX-A] readData error: ");
        Serial.println(state);
        invalidPackets++;
      }
    }
    startReceive();
  }

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

// ============================================================
// LORA
// ============================================================
void setupLoRa() {
  loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREADING,
                          LORA_CODING, LORA_SYNC_WORD, LORA_POWER,
                          LORA_PREAMBLE, LORA_GAIN, false);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("[LORA-A] Initialization failed: ");
    Serial.println(state);
    while (true) delay(1000);
  }
  radio.setDio2AsRfSwitch(true);
  Serial.println("[LORA-A] SX1262 initialized");
}

void startReceive() {
  int state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("[RX-A] startReceive error: ");
    Serial.println(state);
  }
}

// ============================================================
// TRANSMISSION
// ============================================================
void createPacket(VesselPacket &p) {
  memset(&p, 0, sizeof(p));
  p.magic = PACKET_MAGIC;
  p.version = PACKET_VERSION;
  p.packetType = PACKET_TELEMETRY;
  p.originNodeId = NODE_ID;
  p.sequenceNumber = txSequence++;
  p.timestamp = millis() / 1000UL;
  p.hopCount = 0;
  if (gpsHasFix) {
    p.flags |= FLAG_GPS_FIX;
    p.latitude_e7 = static_cast<int32_t>(round(latitude * 10000000.0));
    p.longitude_e7 = static_cast<int32_t>(round(longitude * 10000000.0));
  }
  strncpy(p.nodeName, NODE_NAME, sizeof(p.nodeName) - 1);
  p.shoreDistance_m = 0;
  p.batteryLevel = 100;
  // The packet remains 62 bytes. The existing flags byte now also tells a
  // receiver which weather fields are valid for this sample.
  if (tempHumidityReadingValid) {
    p.flags |= FLAG_TEMP_HUMIDITY_VALID;
    p.weather.temperature_x10 = static_cast<int16_t>(round(temperatureC * 10.0f));
    p.weather.humidity_x10 = static_cast<uint16_t>(round(humidityPercent * 10.0f));
  }

  // pressure_x10 is the normalized value used by the weather algorithm.
  // Do not advertise it as valid unless the full weather input set is valid.
  if (weatherInputsValid) {
    p.flags |= FLAG_PRESSURE_VALID;
    p.weather.pressure_x10 = static_cast<uint16_t>(round(pressureMSL * 10.0f));
    p.weather.pressureTrend_x10 = static_cast<int16_t>(round(pressureDelta * 10.0f));
    p.weather.zambrettiState = zambrettiIndex;
    p.weather.sensorFusionState = sensorFusionState;
    p.weather.weatherState = weatherState;
  } else {
    p.weather.pressure_x10 = 0;
    p.weather.pressureTrend_x10 = 0;
    p.weather.zambrettiState = 0;
    p.weather.sensorFusionState = WEATHER_CALIBRATING;
    p.weather.weatherState = WEATHER_CALIBRATING;
  }
  strncpy(p.message, shortWeatherName(p.weather.weatherState), sizeof(p.message) - 1);
}

void transmitPacket() {
  VesselPacket p;
  createPacket(p);
  Serial.print("[TX-A] Seq="); Serial.print(p.sequenceNumber);
  Serial.print(" GPS="); Serial.print((p.flags & FLAG_GPS_FIX) ? "VALID" : "NO_FIX");
  Serial.print(" Weather="); Serial.println(weatherName(p.weather.weatherState));

  int state = radio.transmit(reinterpret_cast<uint8_t*>(&p), sizeof(p));
  if (state == RADIOLIB_ERR_NONE) {
    lastTXSuccess = true;
    txCount++;
    Serial.println("[TX-A] SUCCESS");
  } else {
    lastTXSuccess = false;
    txFailures++;
    Serial.print("[TX-A] FAILED: "); Serial.println(state);
  }
  startReceive();
}

// ============================================================
// RECEIVE BOARD B + TRACK REVERSE-LINK PDR
// ============================================================
void processReceivedPacket(const uint8_t *buffer, size_t length,
                           int16_t rssi, float snr) {
  if (length != sizeof(VesselPacket)) {
    invalidPackets++;
    Serial.println("[RX-A] Invalid packet length");
    return;
  }
  VesselPacket p;
  memcpy(&p, buffer, sizeof(p));
  if (p.magic != PACKET_MAGIC || p.version != PACKET_VERSION) {
    invalidPackets++;
    Serial.println("[RX-A] Invalid packet header");
    return;
  }
  if (p.originNodeId == NODE_ID) return;

  if (p.originNodeId != 0xB1) {
    Serial.print("[RX-A] Unknown node ID 0x");
    Serial.println(p.originNodeId, HEX);
    return;
  }

  rxFromB++;
  lastBSeen = millis();
  lastBRSSI = rssi;
  lastBSNR = snr;
  bWeatherState = p.weather.weatherState;

  if (p.flags & FLAG_GPS_FIX) {
    double lat = p.latitude_e7 / 10000000.0;
    double lon = p.longitude_e7 / 10000000.0;
    if (lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0)) {
      bGPSValid = true;
      bLatitude = lat;
      bLongitude = lon;
    } else {
      bGPSValid = false;
      distanceToBkm = -1.0;
    }
  } else {
    bGPSValid = false;
    distanceToBkm = -1.0;
  }

  trackBSequence(p.sequenceNumber);

  if (gpsHasFix && bGPSValid) {
    distanceToBkm = TinyGPSPlus::distanceBetween(latitude, longitude,
                                                  bLatitude, bLongitude) / 1000.0;
  } else {
    distanceToBkm = -1.0;
  }

  Serial.println();
  Serial.println("========== RX-A: BOARD B PACKET ==========");
  Serial.print("Node: "); Serial.println(p.nodeName);
  Serial.print("Sequence: "); Serial.println(p.sequenceNumber);
  Serial.print("Remote GPS: ");
  if (bGPSValid) {
    Serial.print(bLatitude, 7); Serial.print(", "); Serial.println(bLongitude, 7);
  } else Serial.println("NO FIX");
  Serial.print("Distance to B: ");
  if (distanceToBkm >= 0.0) { Serial.print(distanceToBkm, 3); Serial.println(" km"); }
  else Serial.println("?");
  Serial.print("RSSI: "); Serial.print(rssi); Serial.println(" dBm");
  Serial.print("SNR: "); Serial.print(snr, 2); Serial.println(" dB");
  Serial.print("Observed B->A PDR: "); Serial.print(bSequencePDR()); Serial.println("%");
  Serial.println("==========================================");
}

bool bSequenceWasSeen(uint32_t sequence) {
  for (int i = 0; i < RX_SEQUENCE_CACHE; i++) {
    if (seenBSequenceValid[i] && seenBSequences[i] == sequence) return true;
  }
  return false;
}

void rememberBSequence(uint32_t sequence) {
  seenBSequences[seenBCursor] = sequence;
  seenBSequenceValid[seenBCursor] = true;
  seenBCursor = (seenBCursor + 1) % RX_SEQUENCE_CACHE;
}

void resetBSequenceSession() {
  uniqueFromB = duplicateFromB = missingFromB = 0;
  firstBSequence = highestBSequence = 0;
  haveBSequence = false;
  seenBCursor = 0;
  memset(seenBSequences, 0, sizeof(seenBSequences));
  memset(seenBSequenceValid, 0, sizeof(seenBSequenceValid));
}

void trackBSequence(uint32_t sequence) {
  if (haveBSequence && sequence == 0 && highestBSequence > 10) {
    resetBSequenceSession();
  }
  if (bSequenceWasSeen(sequence)) {
    duplicateFromB++;
    calculateBSequencePDR();
    return;
  }
  rememberBSequence(sequence);
  uniqueFromB++;

  if (!haveBSequence) {
    haveBSequence = true;
    firstBSequence = highestBSequence = sequence;
  } else {
    if (sequence < firstBSequence) firstBSequence = sequence;
    if (sequence > highestBSequence) highestBSequence = sequence;
  }
  calculateBSequencePDR();
}

void calculateBSequencePDR() {
  if (!haveBSequence) { missingFromB = 0; return; }
  uint64_t expected = static_cast<uint64_t>(highestBSequence) - firstBSequence + 1ULL;
  missingFromB = expected > uniqueFromB ? static_cast<uint32_t>(expected - uniqueFromB) : 0;
}

float bSequencePDR() {
  if (!haveBSequence) return 0.0f;
  uint64_t expected = static_cast<uint64_t>(highestBSequence) - firstBSequence + 1ULL;
  if (expected == 0) return 0.0f;
  return 100.0f * static_cast<float>(uniqueFromB) / static_cast<float>(expected);
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
  snprintf(line, sizeof(line), "TX:%lu RX:%lu", static_cast<unsigned long>(txCount), static_cast<unsigned long>(rxFromB));
  u8g2.drawStr(0, 20, line);
  u8g2.drawStr(0, 32, bGPSValid ? "B GPS: VALID" : (rxFromB ? "B GPS: NO FIX" : "B: WAITING"));
  if (distanceToBkm >= 0.0) snprintf(line, sizeof(line), "DIST: %.2f km", distanceToBkm);
  else strcpy(line, "DIST: ?");
  u8g2.drawStr(0, 44, line);
  if (rxFromB > 0) snprintf(line, sizeof(line), "RSSI:%d SNR:%.1f", lastBRSSI, lastBSNR);
  else strcpy(line, "RSSI: -- SNR: --");
  u8g2.drawStr(0, 56, line);
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
