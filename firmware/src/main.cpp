#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include <TinyGPSPlus.h>
#include <RadioLib.h>
#include <XPowersLib.h>
#include <U8g2lib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <DHT20.h>
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

// Sensor Pin Definitions
#define BME_SDA_PIN    21
#define BME_SCL_PIN    48
#define DHT_SDA_PIN    38
#define DHT_SCL_PIN    39

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
#define WEATHER_HISTORY_SIZE 10
const float NODE_HEIGHT_M = 0.0f;
#define RX_SEQUENCE_CACHE 256

enum PacketType : uint8_t {
  PACKET_TELEMETRY = 1,
  PACKET_SOS = 2
};

enum WeatherState : uint8_t {
  WEATHER_CALIBRATING = 0,
  WEATHER_NORMAL = 1,
  WEATHER_FRONT = 2,
  WEATHER_SQUALL = 3
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

Adafruit_BME680 bme;
DHT20 dht;
bool bmeAvailable = false;
bool dhtAvailable = false;

// Current-cycle measurement validity. A failed sensor must not hide readings
// from the other sensor or make stale values look current.
bool dhtReadingValid = false;
bool bmePressureReadingValid = false;
bool tempHumidityReadingValid = false;
bool tempHumidityFromDHT = false;
bool weatherInputsValid = false;

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

// -------------------- Weather State -----------------------
float temperatureC = 0.0f;
float humidityPercent = 0.0f;
float pressureStationHpa = 0.0f; // direct BME680 reading (station pressure)
float pressureMSL = 0.0f;        // altitude-normalized pressure used by weather logic
float pressureDelta = 0.0f;
uint8_t zambrettiIndex = 0;
uint8_t sensorFusionState = WEATHER_CALIBRATING;
uint8_t weatherState = WEATHER_CALIBRATING;
float pressureHistory[WEATHER_HISTORY_SIZE] = {};
uint8_t historyCount = 0;
uint8_t historyIndex = 0;
unsigned long lastWeatherTime = 0;
float previousTemperatureC = NAN;
float previousHumidityPercent = NAN;
bool havePreviousWeatherSample = false;
bool previousTempHumidityFromDHT = false;
float temperatureChangeC = NAN;
float humidityChangePoints = NAN;
uint32_t validWeatherSamples = 0;
// Sensor recovery safeguards. Values are development defaults for a near-sea-level
// installation. They are NOT storm thresholds and must be configured for deployment.
const float PRESSURE_MIN_HPA = 870.0f;
const float PRESSURE_MAX_HPA = 1085.0f;
const float MAX_SINGLE_PRESSURE_STEP_HPA = 8.0f;
float lastAcceptedStationPressure = NAN;
uint8_t bmeConsecutiveFailures = 0;
uint8_t dhtConsecutiveFailures = 0;
// Recovered sensors require a fresh full history; never reuse stale calibration.
// Simulated test values never enter production history or transmitted telemetry.
struct WeatherDecision {
  uint8_t state;
  uint8_t fusion;
  uint8_t z;
  float dP;
  float dT;
  float dRH;
};
char testLine[48];
uint8_t testLineLength = 0;


// -------------------- Declarations -----------------------------
void switchI2C(uint8_t sda, uint8_t scl);
void setupPMU();
void setupOLED();
bool detectOLED();
void setupSensors();
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
void processWeather();
WeatherDecision evaluateWeather(float pressure, const float *history, uint8_t count,
                                float previousT, float previousRH,
                                float currentT, float currentRH, bool comparable);
void processSerialTestCommands();
void runWeatherAlgorithmTests(const char *requested);
void resetWeatherCalibration(const char *reason);
void addPressure(float p);
float pressureAverage();
float normalizePressure(float p, float t);
uint8_t calculateZambretti(float p, float trend);
uint8_t calculateFusion(float deltaT, float deltaH);
const char *weatherName(uint8_t state);
const char *shortWeatherName(uint8_t state);
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

void setupSensors() {
  Serial.println("[SENSOR] Starting sensor diagnostics");

  // ----------------------------------------------------------
  // BME680: SDA 21, SCL 48
  // ----------------------------------------------------------
  switchI2C(BME_SDA_PIN, BME_SCL_PIN);

  Serial.println("[I2C-BME] Scanning addresses...");

  bool bmeAddress76 = false;
  bool bmeAddress77 = false;

  Wire.beginTransmission(0x76);
  bmeAddress76 = (Wire.endTransmission() == 0);

  Wire.beginTransmission(0x77);
  bmeAddress77 = (Wire.endTransmission() == 0);

  Serial.printf("[I2C-BME] 0x76: %s\n",
                bmeAddress76 ? "FOUND" : "not found");
  Serial.printf("[I2C-BME] 0x77: %s\n",
                bmeAddress77 ? "FOUND" : "not found");

  if (bme.begin(0x76)) {
    bmeAvailable = true;
  } else if (bme.begin(0x77)) {
    bmeAvailable = true;
  }

  if (bmeAvailable) {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);

    Serial.println("[SENSOR] BME680 initialized");
  } else {
    Serial.println("[SENSOR] BME680 initialization FAILED");
  }

  // ----------------------------------------------------------
  // DHT20: SDA 38, SCL 39
  // ----------------------------------------------------------
  switchI2C(DHT_SDA_PIN, DHT_SCL_PIN);

  Serial.println("[I2C-DHT] Scanning address 0x38...");

  Wire.beginTransmission(0x38);
  bool dhtAddressFound = (Wire.endTransmission() == 0);

  Serial.printf("[I2C-DHT] 0x38: %s\n",
                dhtAddressFound ? "FOUND" : "not found");

  dht.begin();

  if (dhtAddressFound && dht.isConnected()) {
    dhtAvailable = true;
    Serial.println("[SENSOR] DHT20 initialized");
  } else {
    dhtAvailable = false;
    Serial.println("[SENSOR] DHT20 initialization FAILED");
  }

  // Restore OLED bus
  switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.setClock(400000);

  Serial.printf("[SENSOR] BME680 available: %s\n",
                bmeAvailable ? "YES" : "NO");
  Serial.printf("[SENSOR] DHT20 available: %s\n",
                dhtAvailable ? "YES" : "NO");
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

// ============================================================
// HARDWARE WEATHER READINGS
// ============================================================
void processWeather() {
  float t = NAN;
  float h = NAN;
  float pStation = NAN;
  float bmeT = NAN;
  float bmeH = NAN;

  bool gotDHT = false;
  bool gotBMEPressure = false;
  bool gotBMETempHumidity = false;

  // ----------------------------------------------------------
  // Read DHT20 temperature and humidity independently
  // ----------------------------------------------------------
  if (dhtAvailable) {
    switchI2C(DHT_SDA_PIN, DHT_SCL_PIN);
    // Verify the actual DHT20 responds before trusting library readback.
    if (!dht.isConnected()) {
      Serial.println("[DHT20] No I2C response; backup BME680 T/RH if available");
    } else {
    int dhtStatus = dht.read();

    if (dhtStatus == DHT20_OK) {
      float dhtT = dht.getTemperature();
      float dhtH = dht.getHumidity();

      if (isfinite(dhtT) && isfinite(dhtH) &&
          dhtT >= -40.0f && dhtT <= 85.0f &&
          dhtH >= 0.0f && dhtH <= 100.0f) {
        t = dhtT;
        h = dhtH;
        gotDHT = true;
      } else {
        Serial.println("[DHT20] Invalid measurement");
      }
    } else {
      Serial.printf("[DHT20] Read failed, status=%d\n", dhtStatus);
    }
    }
  }

  // ----------------------------------------------------------
  // Read BME680 pressure and keep its T/RH available as backup
  // ----------------------------------------------------------
  if (bmeAvailable) {
    switchI2C(BME_SDA_PIN, BME_SCL_PIN);

    if (bme.performReading()) {
      bmeT = bme.temperature;
      bmeH = bme.humidity;
      float bmeP = bme.pressure / 100.0f;

      bool plausible = isfinite(bmeP) && bmeP >= PRESSURE_MIN_HPA &&
                       bmeP <= PRESSURE_MAX_HPA;
      bool jump = plausible && isfinite(lastAcceptedStationPressure) &&
                  fabsf(bmeP - lastAcceptedStationPressure) > MAX_SINGLE_PRESSURE_STEP_HPA;
      if (plausible && !jump) {
        pStation = bmeP;
        gotBMEPressure = true;
      } else {
        Serial.printf("[BME680] PRESSURE REJECTED: %.2f hPa (%s). Not used for history/forecast.\n",
                      bmeP, jump ? "abrupt jump" : "outside configured range");
      }

      if (isfinite(bmeT) && isfinite(bmeH) &&
          bmeT >= -40.0f && bmeT <= 85.0f &&
          bmeH >= 0.0f && bmeH <= 100.0f) {
        gotBMETempHumidity = true;
      }
    } else {
      Serial.println("[BME680] Reading failed");
    }
  }

  // Sensor recovery is explicit and separate from the OLED reading flags.
  if (gotDHT) {
    if (dhtConsecutiveFailures) Serial.println("[DHT20] Recovered; T/RH source may switch");
    dhtConsecutiveFailures = 0;
  } else if (dhtConsecutiveFailures < 255) {
    ++dhtConsecutiveFailures;
  }
  if (gotBMEPressure) {
    if (bmeConsecutiveFailures) Serial.println("[BME680] Recovered; fresh calibration required");
    bmeConsecutiveFailures = 0;
    lastAcceptedStationPressure = pStation;
  } else if (bmeConsecutiveFailures < 255) {
    ++bmeConsecutiveFailures;
  }

  // Prefer DHT20 for T/RH; fall back to BME680 if DHT20 failed.
  if (!gotDHT && gotBMETempHumidity) {
    t = bmeT;
    h = bmeH;
  }

  // Store current-cycle status independently of the overall weather result.
  dhtReadingValid = gotDHT;
  bmePressureReadingValid = gotBMEPressure;
  tempHumidityReadingValid = gotDHT || gotBMETempHumidity;
  tempHumidityFromDHT = gotDHT;
  weatherInputsValid = tempHumidityReadingValid && bmePressureReadingValid;

  // Publish each valid measurement even if the other sensor failed.
  if (tempHumidityReadingValid) {
    temperatureC = t;
    humidityPercent = h;
  }
  if (bmePressureReadingValid) {
    pressureStationHpa = pStation;
    // If T/RH is unavailable, keep the raw station pressure for display.
    // Forecast calculations remain disabled until all inputs are valid.
    pressureMSL = tempHumidityReadingValid
                    ? normalizePressure(pStation, temperatureC)
                    : pStation;
  }

  // Restore OLED I2C configuration before any display update.
  switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.setClock(400000);

  // ----------------------------------------------------------
  // A missing sensor disables weather calculations only. It must not clear
  // otherwise valid readings or make the working sensor look broken.
  // ----------------------------------------------------------
  if (!weatherInputsValid) {
    pressureDelta = 0.0f;
    zambrettiIndex = 0;
    temperatureChangeC = NAN;
    humidityChangePoints = NAN;
    resetWeatherCalibration("missing/rejected weather input");
    sensorFusionState = WEATHER_CALIBRATING;
    weatherState = WEATHER_CALIBRATING;

    Serial.print("[WEATHER-A] T=");
    if (tempHumidityReadingValid) {
      Serial.printf("%.1fC RH=%.1f%%", temperatureC, humidityPercent);
    } else {
      Serial.print("-- RH=--");
    }
    Serial.print(" P=");
    if (bmePressureReadingValid) {
      Serial.printf("%.1fhPa", pressureStationHpa);
    } else {
      Serial.print("-- (BME680 FAIL)");
    }
    Serial.printf(" DHT20=%s BME680=%s State=%s; weather calculations skipped\n",
                  dhtReadingValid ? "OK" : "FAIL",
                  bmePressureReadingValid ? "OK" : "FAIL",
                  weatherName(weatherState));
    return;
  }

  // ----------------------------------------------------------
  // Both T/RH and pressure are valid: run manuscript decision hierarchy.
  // Compare only consecutive valid samples from the SAME T/RH source;
  // switching DHT20 to BME680 can cause an artificial temperature jump.
  // ----------------------------------------------------------
  pressureMSL = normalizePressure(pStation, temperatureC);
  bool comparable = havePreviousWeatherSample &&
                    previousTempHumidityFromDHT == tempHumidityFromDHT;
  // Evaluate the EXACT same function used by the synthetic test harness.
  WeatherDecision decision = evaluateWeather(
    pressureMSL, pressureHistory, historyCount,
    previousTemperatureC, previousHumidityPercent,
    temperatureC, humidityPercent, comparable);
  pressureDelta = decision.dP;
  temperatureChangeC = decision.dT;
  humidityChangePoints = decision.dRH;
  zambrettiIndex = decision.z;
  sensorFusionState = decision.fusion;
  weatherState = decision.state;

  addPressure(pressureMSL);
  validWeatherSamples++;
  previousTemperatureC = temperatureC;
  previousHumidityPercent = humidityPercent;
  previousTempHumidityFromDHT = tempHumidityFromDHT;
  havePreviousWeatherSample = true;

  Serial.printf(
    "[WEATHER-A] T=%.1fC RH=%.1f%% Psta=%.2f Pmsl=%.2f "
    "dP=%+.2f Z=%u hist=%u/%u DHT=%s BME=%s State=%s\n",
    temperatureC, humidityPercent, pressureStationHpa, pressureMSL,
    pressureDelta, zambrettiIndex, historyCount, WEATHER_HISTORY_SIZE,
    gotDHT ? "OK" : "FAIL (BME T/RH fallback)",
    gotBMEPressure ? "OK" : "FAIL", weatherName(weatherState)
  );
  if (comparable) {
    Serial.printf("[FUSION-A] dT=%+.2fC dRH=%+.2f percentage points; squall=%s\n",
                  temperatureChangeC, humidityChangePoints,
                  weatherState == WEATHER_SQUALL ? "YES" : "NO");
  } else {
    Serial.println("[FUSION-A] Baseline unavailable or source changed; awaiting next comparable sample");
  }
}

void resetWeatherCalibration(const char *reason) {
  if (historyCount || havePreviousWeatherSample)
    Serial.printf("[WEATHER] Calibration reset: %s\n", reason);
  historyCount = 0;
  historyIndex = 0;
  havePreviousWeatherSample = false;
  previousTemperatureC = NAN;
  previousHumidityPercent = NAN;
}

WeatherDecision evaluateWeather(float pressure, const float *history, uint8_t count,
                                float previousT, float previousRH,
                                float currentT, float currentRH, bool comparable) {
  WeatherDecision out = {WEATHER_CALIBRATING, WEATHER_CALIBRATING, 0,
                         0.0f, NAN, NAN};
  float sum = 0.0f;
  for (uint8_t i = 0; i < count; ++i) sum += history[i];
  if (count) out.dP = pressure - sum / count;
  if (comparable) {
    out.dT = currentT - previousT;
    out.dRH = currentRH - previousRH;
  }
  if (count < WEATHER_HISTORY_SIZE) return out;
  out.fusion = comparable ? calculateFusion(out.dT, out.dRH) : WEATHER_NORMAL;
  out.z = calculateZambretti(pressure, out.dP);
  if (out.fusion == WEATHER_SQUALL) out.state = WEATHER_SQUALL;
  else if (out.dP < -1.6f) out.state = WEATHER_FRONT;
  else out.state = WEATHER_NORMAL;
  return out;
}

struct WeatherTestCase {
  const char *name;
  float prevT, prevRH, curT, curRH, curP, historicalP;
  uint8_t historyLength;
  uint8_t expected;
  int expectedZ; // -1 means not asserted
};

void runWeatherAlgorithmTests(const char *requested) {
  // Development-only synthetic samples; no writes to real telemetry state.
  const WeatherTestCase cases[] = {
    {"NORMAL", 32.1f, 70.2f, 32.0f, 70.5f, 1010.3f, 1010.4f, 10, WEATHER_NORMAL, 13},
    {"FRONT", 32.1f, 70.2f, 31.8f, 71.0f, 1008.0f, 1010.3f, 10, WEATHER_FRONT, 6},
    {"SQUALL", 32.1f, 70.2f, 29.6f, 76.2f, 1010.0f, 1010.3f, 10, WEATHER_SQUALL, 13},
    {"BOTH", 32.1f, 70.2f, 29.6f, 76.2f, 1008.0f, 1010.3f, 10, WEATHER_SQUALL, 6},
    {"CALIB", 32.1f, 70.2f, 29.6f, 76.2f, 1008.0f, 1010.3f, 9, WEATHER_CALIBRATING, 0},
    {"Z", 32.1f, 70.2f, 32.0f, 70.5f, 1010.3f, 1010.4f, 10, WEATHER_NORMAL, 13}
  };
  int total = 0, passed = 0;
  Serial.println("\n========== SYNTHETIC WEATHER ALGORITHM TEST ==========");
  for (const auto &c : cases) {
    if (strcmp(requested, "ALL") != 0 && strcmp(requested, c.name) != 0) continue;
    float syntheticHistory[WEATHER_HISTORY_SIZE] = {};
    for (uint8_t i = 0; i < c.historyLength; ++i) syntheticHistory[i] = c.historicalP;
    WeatherDecision d = evaluateWeather(c.curP, syntheticHistory, c.historyLength,
                                        c.prevT, c.prevRH, c.curT, c.curRH, true);
    bool ok = (d.state == c.expected) && (c.expectedZ < 0 || d.z == c.expectedZ);
    ++total;
    if (ok) ++passed;
    Serial.printf("[TEST %s] T %.1f->%.1f dT=%+.2f | RH %.1f->%.1f dRH=%+.2f | P=%.2f dP=%+.2f hist=%u/10 Z=%u\n",
                  c.name, c.prevT, c.curT, d.dT, c.prevRH, c.curRH, d.dRH,
                  c.curP, d.dP, c.historyLength, d.z);
    Serial.printf("[TEST %s] expected=%s actual=%s Z_expected=%d result=%s\n",
                  c.name, weatherName(c.expected), weatherName(d.state), c.expectedZ,
                  ok ? "PASS" : "FAIL");
  }
  if (total) Serial.printf("[TEST] TOTAL: %d/%d PASS (synthetic only; REAL sensors/LoRa unchanged)\n", passed, total);
  else Serial.println("[TEST] Unknown test. Type TEST HELP");
  Serial.println("======================================================\n");
}

void processSerialTestCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (!testLineLength) continue;
      testLine[testLineLength] = '\0';
      for (uint8_t i = 0; i < testLineLength; ++i)
        if (testLine[i] >= 'a' && testLine[i] <= 'z') testLine[i] -= ('a' - 'A');
      if (strcmp(testLine, "TEST HELP") == 0) {
        Serial.println("[TEST] Commands: TEST ALL | NORMAL | FRONT | SQUALL | BOTH | CALIB | Z");
        Serial.println("[TEST] Synthetic tests do not overwrite live weather or LoRa payloads");
      } else if (strncmp(testLine, "TEST ", 5) == 0) {
        runWeatherAlgorithmTests(testLine + 5);
      } else {
        Serial.println("[TEST] Unknown command; type TEST HELP");
      }
      testLineLength = 0;
    } else if (testLineLength < sizeof(testLine) - 1) {
      testLine[testLineLength++] = c;
    } else {
      testLineLength = 0;
      Serial.println("[TEST] Command too long; discarded");
    }
  }
}

float normalizePressure(float p, float t) {
  float term = 1.0f - ((0.0065f * NODE_HEIGHT_M) / (t + 273.15f));
  return p * pow(term, -5.257f);
}

void addPressure(float p) {
  pressureHistory[historyIndex] = p;
  historyIndex = (historyIndex + 1) % WEATHER_HISTORY_SIZE;
  if (historyCount < WEATHER_HISTORY_SIZE) historyCount++;
}

float pressureAverage() {
  if (historyCount == 0) return 0.0f;
  float sum = 0.0f;
  for (uint8_t i = 0; i < historyCount; i++) sum += pressureHistory[i];
  return sum / historyCount;
}

uint8_t calculateFusion(float deltaT, float deltaH) {
  // Manuscript thresholds compare consecutive sample changes, not absolute T/RH.
  return (deltaT <= -2.0f && deltaH >= 5.0f)
           ? WEATHER_SQUALL : WEATHER_NORMAL;
}

uint8_t calculateZambretti(float p, float trend) {
  float z;
  if (trend < -0.5f) z = 127.0f - (0.12f * p);
  else if (trend > 0.5f) z = 185.0f - (0.16f * p);
  else z = 144.0f - (0.13f * p);
  if (z < 0) z = 0;
  if (z > 255) z = 255;
  uint8_t result = static_cast<uint8_t>(round(z));
  if (p > 900.0f && p < 1100.0f && (result < 1 || result > 32)) {
    Serial.printf("[Z-CHECK] Unexpected Z=%u Pmsl=%.2f trend=%+.2f\n", result, p, trend);
  }
  return result;
}

const char *weatherName(uint8_t state) {
  switch (state) {
    case WEATHER_CALIBRATING: return "CALIBRATING";
    case WEATHER_NORMAL: return "NORMAL";
    case WEATHER_FRONT: return "APPROACHING FRONT";
    case WEATHER_SQUALL: return "IMMEDIATE SQUALL";
    default: return "UNKNOWN";
  }
}

const char *shortWeatherName(uint8_t state) {
  switch (state) {
    case WEATHER_CALIBRATING: return "CALIB";
    case WEATHER_NORMAL: return "NORMAL";
    case WEATHER_FRONT: return "FRONT";
    case WEATHER_SQUALL: return "SQUALL";
    default: return "UNKNOWN";
  }
}

// ============================================================
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
