#include <Arduino.h>
#include "sos_controls.h"
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include <TinyGPSPlus.h>
#include <RadioLib.h>
#include <XPowersLib.h>
#include <U8g2lib.h>
#include <Preferences.h>

// ============================================================
// BOARD B - VESSEL-B
// Scheduled TX/RX test. It synchronizes its first transmit slot
// to 2.5 s after the first valid new telemetry packet from A,
// then sends its own telemetry every 5 s. RX resumes after TX.
// PDR shown here is an observed sequence-span estimate, not the
// final controlled-test PDR measurement.
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

#define NODE_ID        0xB1
const char NODE_NAME[] = "Vessel-B";

// Must match Board A.
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
// Same validity flags as Board A packet_protocol.h (no packet-size change).
#define FLAG_TEMP_HUMIDITY_VALID    0x02
#define FLAG_PRESSURE_VALID         0x04

const unsigned long TX_INTERVAL = 5000UL;
const unsigned long TX_OFFSET_AFTER_FIRST_A_PACKET = 2500UL;
const unsigned long NEIGHBOR_TIMEOUT = 15000UL;
const unsigned long OLED_UPDATE_INTERVAL = 500UL;
// GPIO0 / BOOT button: short press advances one OLED page.
// Automatic page rotation has been removed.
#define USER_BUTTON_PIN 0
const unsigned long BUTTON_DEBOUNCE_MS = 40UL;
const unsigned long GPS_STATUS_INTERVAL = 5000UL;
const unsigned long STATUS_INTERVAL = 5000UL;
#define MAX_NEIGHBORS 10
#define SEQUENCE_CACHE_SIZE 256

// Weather code definitions match the shared packet.
enum PacketType : uint8_t {
  PACKET_TELEMETRY = 1,
  PACKET_SOS = 2,
  PACKET_SOS_ACK = 3,
  PACKET_SOS_CANCEL = 4,
  PACKET_SOS_CANCEL_ACK = 5
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
  uint32_t timestamp; // Uptime seconds in current development firmware, not UTC.
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
TwoWire PMUWire(1);
XPowersAXP2101 pmu;
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// -------------------- GPS -------------------------------------
bool myGPSFix = false;
double myLatitude = 0.0;
double myLongitude = 0.0;
unsigned long lastGPSStatus = 0;

// -------------------- OLED ------------------------------------
bool oledAvailable = false;
uint8_t oledAddress = 0x3C;
uint8_t oledPage = 0; // 0 GPS, 1 neighbors, 2 PDR, 3 remote weather, 4 SOS rescue
// Last received SOS is retained in RAM; freshness label changes after 60 s.
// No shore acknowledgment or durable persistence exists yet.
bool sosCancelled = false;
bool sosAlertActive = false;
unsigned long lastSOSMillis = 0;
uint32_t lastSOSSequence = 0;
uint32_t lastSOSOriginId = 0;
char lastSOSNodeName[13] = {0};
bool lastSOSGpsValid = false;
double lastSOSLatitude = 0.0, lastSOSLongitude = 0.0;
uint32_t sosUniqueEvents = 0;
uint32_t sosDuplicateEvents = 0;
const unsigned long SOS_ALERT_MS = 60000UL;
unsigned long lastOLEDUpdate = 0;
int buttonLastReading = HIGH;
int buttonStableState = HIGH;
unsigned long buttonLastDebounceTime = 0;

// -------------------- Neighbor/PDR ----------------------------
struct Neighbor {
  bool active;
  uint32_t nodeId;
  char nodeName[12];
  unsigned long lastSeen;
  uint32_t lastSequence;
  bool gpsValid;
  double latitude;
  double longitude;
  double distanceKm;
  int16_t rssi;
  float snr;
  uint8_t weatherState;
  // Weather values from the most recent packet (not from local sensors).
  bool remoteTempHumidityValid;
  bool remotePressureValid;
  int16_t remoteTemperature_x10;
  uint16_t remoteHumidity_x10;
  uint16_t remotePressure_x10;
  int16_t remotePressureTrend_x10;
  uint8_t remoteZIndex;
  uint8_t remoteFusionState;

  uint32_t packetsReceived;
  uint32_t uniquePackets;
  uint32_t duplicates;
  uint32_t firstSequence;
  uint32_t highestSequence;
  uint32_t missingPackets;
  bool haveSequence;
  float pdrPercent;

  uint32_t seenSequences[SEQUENCE_CACHE_SIZE];
  bool seenSequenceValid[SEQUENCE_CACHE_SIZE];
  uint16_t seenCursor;
};
Neighbor neighbors[MAX_NEIGHBORS];
// The active table contains only live neighbors. Retain one last-known
// snapshot in RAM when an entry times out; no flash persistence yet.
Neighbor lastKnownNeighbor = {};
bool haveLastKnownNeighbor = false;


uint32_t totalValidPacketsReceived = 0;
uint32_t invalidPackets = 0;

// -------------------- Autonomous TX schedule -----------------
uint32_t txSequence = 0;
uint32_t txCount = 0;
uint32_t txFailures = 0;
bool lastTXSuccess = false;
bool txScheduleStarted = false;
unsigned long nextTransmitAt = 0;

// Phase 3.5.1: Vessel-B originates its own SOS using serial `SOS`.
// Keep the physical button disabled until wired/verified on Vessel-B.
static Preferences bEmergencyPrefs;
static bool bEmergencyNvsOK = false;
static uint32_t bNextEmergencyId = 0;
static bool bEmergencyActive = false;
static bool bSOSPending = false;
static VesselPacket bPendingSOS = {};
static uint8_t bSOSAttempts = 0;
static unsigned long bLastSOSAttempt = 0;
static constexpr uint8_t B_SOS_MAX_ATTEMPTS = 4;
static constexpr unsigned long B_SOS_RETRY_MS = 4500UL;
static String bSerialLine;
// Local cancellation transaction is independent of any newer SOS (RAM-only).
static bool bCancelPending = false;
static VesselPacket bPendingCancel = {};
static uint8_t bCancelAttempts = 0;
static unsigned long bLastCancelAttempt = 0;
static constexpr uint8_t B_CANCEL_MAX_ATTEMPTS = 4;
static constexpr unsigned long B_CANCEL_RETRY_MS = 4500UL;
void cancelVesselBSOS();
void pollVesselBCancel();
void sendVesselBCancel();

void startVesselBSOS();
void pollVesselBSOS();
void handleVesselBSerial();
void sendVesselBSOS();

// -------------------- Declarations -----------------------------
void setupPMU();
void setupOLED();
bool detectOLED();
void setupGPS();
void updateGPS();
void printGPSStatus();
void setupLoRa();
void startReceive();
void processReceivedPacket(const uint8_t *buffer, size_t length, int16_t rssi, float snr);
int findNeighbor(uint32_t id);
int createNeighbor(uint32_t id);
bool updateNeighbor(const VesselPacket &p, int16_t rssi, float snr);
bool sequenceAlreadySeen(const Neighbor &n, uint32_t seq);
void rememberSequence(Neighbor &n, uint32_t seq);
void resetSequenceStatistics(Neighbor &n);
void calculatePDR(Neighbor &n);
void calculateDistances();
void removeStaleNeighbors();
void printNeighborTable();
void createTXPacket(VesselPacket &p);
void transmitOwnPacket();
void updateOLED();
void oledGPSPage();
void oledNeighborPage();
void oledPDRPage();
void oledRemoteWeatherPage();
void printReceivedWeather(const VesselPacket &p);
void oledSOSPage();
const char *weatherName(uint8_t state);
void handleOLEDButton();

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("=== BOARD B: SCHEDULED AUTONOMOUS TX + RX ===");
  Serial.println("Packet: 62 bytes | TX interval: 5 seconds");
  Serial.println("First TX slot: about 2.5 s after first valid A packet");

  memset(neighbors, 0, sizeof(neighbors));
  for (int i = 0; i < MAX_NEIGHBORS; i++) {
    neighbors[i].distanceKm = -1.0;
    neighbors[i].weatherState = WEATHER_CALIBRATING;
  }

  SOSControls::begin();
  bEmergencyNvsOK = bEmergencyPrefs.begin("sos-b-identity", false);
  if (bEmergencyNvsOK) {
    bNextEmergencyId = bEmergencyPrefs.getUInt("nextId", 1);
    Serial.printf("[SOS-ID-B] Next emergency ID=%lu\n", (unsigned long)bNextEmergencyId);
  } else Serial.println("[SOS-ID-B] NVS unavailable; SOS disabled");
  Serial.println("[SOS-B] GPIO2: hold 1.5s SOS, separate 5s hold CANCEL. Serial: SOS / CANCEL SOS");
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  buttonLastReading = digitalRead(USER_BUTTON_PIN);
  buttonStableState = buttonLastReading;
  buttonLastDebounceTime = millis();
  Serial.println("[OLED-B] GPIO0 pages: GPS / Neighbors / PDR / Weather / SOS rescue");

  setupPMU();
  setupOLED();
  setupGPS();
  setupLoRa();
  startReceive();

  lastOLEDUpdate = millis();
  Serial.println("[B] Ready and listening for Vessel-A.");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  updateGPS();
  SOSControls::updateIndicators();
  handleVesselBSerial();
  const auto action = SOSControls::pollAction(bEmergencyActive);
  if (action == SOSControls::ButtonAction::StartSOS) {
    Serial.println("[SOS-BUTTON-B] Deliberate 1.5s SOS hold recognized");
    startVesselBSOS();
  } else if (action == SOSControls::ButtonAction::CancelSOS) {
    Serial.println("[SOS-BUTTON-B] Deliberate 5s CANCEL hold recognized");
    cancelVesselBSOS();
  }
  pollVesselBSOS();
  pollVesselBCancel();

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
        Serial.print("[RX-B] readData error: ");
        Serial.println(state);
        invalidPackets++;
      }
    }
    startReceive();
  }

  // B begins autonomous periodic TX after receiving its first new
  // valid telemetry from A. It does NOT send one immediate reply
  // for every A packet.
  if (txScheduleStarted &&
      static_cast<int32_t>(millis() - nextTransmitAt) >= 0) {
    // Preserve the intended 5-second phase instead of adding the
    // blocking transmit duration to every subsequent interval.
    unsigned long scheduledSlot = nextTransmitAt;
    transmitOwnPacket();
    nextTransmitAt = scheduledSlot + TX_INTERVAL;

    // If a long delay ever made the next slot already overdue,
    // skip the overdue slot rather than transmitting repeatedly.
    if (static_cast<int32_t>(millis() - nextTransmitAt) >= 0) {
      nextTransmitAt = millis() + TX_INTERVAL;
    }
  }

  removeStaleNeighbors();
  calculateDistances();

  if (millis() - lastGPSStatus >= GPS_STATUS_INTERVAL) {
    lastGPSStatus = millis();
    printGPSStatus();
  }

  static unsigned long lastStatus = 0;
  if (millis() - lastStatus >= STATUS_INTERVAL) {
    lastStatus = millis();
    printNeighborTable();
  }

  handleOLEDButton();

  if (millis() - lastOLEDUpdate >= OLED_UPDATE_INTERVAL) {
    lastOLEDUpdate = millis();
    updateOLED();
  }

  delay(5);
}

// ============================================================
// GPIO0 OLED PAGE NAVIGATION (active LOW, debounced)
// ============================================================
void handleOLEDButton() {
  const int reading = digitalRead(USER_BUTTON_PIN);
  if (reading != buttonLastReading) {
    buttonLastDebounceTime = millis();
    buttonLastReading = reading;
  }
  if (millis() - buttonLastDebounceTime >= BUTTON_DEBOUNCE_MS &&
      reading != buttonStableState) {
    buttonStableState = reading;
    if (buttonStableState == LOW) {
      oledPage = (oledPage + 1) % 5;
      lastOLEDUpdate = 0;  // Refresh new page at next loop.
      Serial.printf("[OLED-B] Page %u/5\n", static_cast<unsigned>(oledPage + 1));
    }
  }
}

// ============================================================
// PMU / OLED
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
  Serial.println("[PMU] Initialized; OLED/GPS power rails enabled");
}

void setupOLED() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
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
  Serial.println("[GPS-B] L76K enabled");
}

void updateGPS() {
  while (GPSSerial.available()) gps.encode(GPSSerial.read());
  myGPSFix = gps.location.isValid() && gps.location.age() < 3000;
  if (myGPSFix) {
    myLatitude = gps.location.lat();
    myLongitude = gps.location.lng();
  }
}

void printGPSStatus() {
  Serial.println();
  Serial.println("[GPS-B]");
  Serial.print("Fix: "); Serial.println(myGPSFix ? "YES" : "NO");
  Serial.print("Satellites: ");
  if (gps.satellites.isValid()) Serial.println(gps.satellites.value());
  else Serial.println("?");
  Serial.print("HDOP: ");
  if (gps.hdop.isValid()) Serial.println(gps.hdop.hdop(), 1);
  else Serial.println("?");
  if (myGPSFix) {
    Serial.print("Lat: "); Serial.println(myLatitude, 7);
    Serial.print("Lon: "); Serial.println(myLongitude, 7);
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
    Serial.print("[LORA-B] Initialization failed: ");
    Serial.println(state);
    while (true) delay(1000);
  }
  radio.setDio2AsRfSwitch(true);
  Serial.println("[LORA-B] SX1262 initialized");
}

void startReceive() {
  int state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("[RX-B] startReceive error: ");
    Serial.println(state);
  }
}

// Phase 3.5.1: one local SOS event at a time, with four bounded attempts.
void sendVesselBSOS() {
  if (!bSOSPending) return;
  bLastSOSAttempt = millis();
  ++bSOSAttempts;
  const int rc = radio.transmit(reinterpret_cast<uint8_t*>(&bPendingSOS), sizeof(bPendingSOS));
  startReceive();
  Serial.printf("[SOS-B] ATTEMPT %u/%u id=(0x%lX,%lu) radio=%d; ACK PENDING\n",
    bSOSAttempts, B_SOS_MAX_ATTEMPTS,
    (unsigned long)bPendingSOS.originNodeId, (unsigned long)bPendingSOS.sequenceNumber, rc);
}
void startVesselBSOS() {
  if (bEmergencyActive || bSOSPending) {
    Serial.println("[SOS-B] EXISTING UNRESOLVED SOS; second event blocked");
    return;
  }
  if (!bEmergencyNvsOK || bNextEmergencyId == UINT32_MAX) {
    Serial.println("[SOS-B] NVS identity unavailable; SOS blocked");
    return;
  }
  const uint32_t id = bNextEmergencyId;
  if (bEmergencyPrefs.putUInt("nextId", id + 1) != sizeof(uint32_t)) {
    Serial.println("[SOS-B] NVS ID reservation failed");
    return;
  }
  bNextEmergencyId = id + 1;
  createTXPacket(bPendingSOS);
  bPendingSOS.packetType = PACKET_SOS;
  bPendingSOS.sequenceNumber = id;
  memset(bPendingSOS.message, 0, sizeof(bPendingSOS.message));
  memcpy(bPendingSOS.message, "SOS", 3);
  bEmergencyActive = true;
  bSOSPending = true;
  bSOSAttempts = 0;
  SOSControls::noteLocalSOS();
  sendVesselBSOS();
}
void pollVesselBSOS() {
  if (!bSOSPending || millis() - bLastSOSAttempt < B_SOS_RETRY_MS) return;
  if (bSOSAttempts >= B_SOS_MAX_ATTEMPTS) {
    Serial.println("[SOS-B] UNCONFIRMED after 4 attempts; emergency remains unresolved");
    bSOSPending = false;
    return;
  }
  sendVesselBSOS();
}
void sendVesselBCancel() {
  if (!bCancelPending) return;
  ++bCancelAttempts;
  bLastCancelAttempt = millis();
  const int rc = radio.transmit(reinterpret_cast<uint8_t*>(&bPendingCancel), sizeof(bPendingCancel));
  startReceive();
  Serial.printf("[CANCEL-B] ATTEMPT %u/%u event=%lu radio=%d; ACK PENDING\n",
                bCancelAttempts, B_CANCEL_MAX_ATTEMPTS,
                (unsigned long)bPendingCancel.sequenceNumber, rc);
}
void cancelVesselBSOS() {
  if (!bEmergencyActive) {
    Serial.println("[SOS-B] No active local emergency to cancel");
    return;
  }
  if (bCancelPending) {
    Serial.println("[CANCEL-B] Previous cancellation pending; try again after ACK/timeout");
    return;
  }
  memset(&bPendingCancel, 0, sizeof(bPendingCancel));
  bPendingCancel.magic = PACKET_MAGIC;
  bPendingCancel.version = PACKET_VERSION;
  bPendingCancel.packetType = PACKET_SOS_CANCEL;
  bPendingCancel.originNodeId = NODE_ID;
  bPendingCancel.sequenceNumber = bPendingSOS.sequenceNumber;
  bPendingCancel.timestamp = millis()/1000UL;
  strncpy(bPendingCancel.nodeName, NODE_NAME, sizeof(bPendingCancel.nodeName)-1);
  memcpy(bPendingCancel.message, &bPendingSOS.sequenceNumber, 4);
  const uint32_t origin = NODE_ID;
  memcpy(bPendingCancel.message+4, &origin, 4);
  bCancelPending = true;
  bCancelAttempts = 0;
  bEmergencyActive = false;
  bSOSPending = false;
  Serial.printf("[SOS-B] LOCAL CANCEL event=%lu; NEW SOS ENABLED; REMOTE UNCONFIRMED\n",
                (unsigned long)bPendingCancel.sequenceNumber);
  sendVesselBCancel();
}
void pollVesselBCancel() {
  if (!bCancelPending || millis()-bLastCancelAttempt < B_CANCEL_RETRY_MS) return;
  if (bCancelAttempts >= B_CANCEL_MAX_ATTEMPTS) {
    Serial.printf("[CANCEL-B] UNCONFIRMED after 4 attempts event=%lu; REMOTE STATE UNKNOWN\n",
                  (unsigned long)bPendingCancel.sequenceNumber);
    bCancelPending = false;
    return;
  }
  sendVesselBCancel();
}
void handleVesselBSerial() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      bSerialLine.trim();
      if (bSerialLine.equalsIgnoreCase("SOS")) startVesselBSOS();
      else if (bSerialLine.equalsIgnoreCase("CANCEL SOS")) cancelVesselBSOS();
      else if (bSerialLine.length()) Serial.printf("[SOS-B] Unknown command: %s\n", bSerialLine.c_str());
      bSerialLine = "";
    } else if (bSerialLine.length() < 32) bSerialLine += c;
  }
}

// Direct ACK for original SOS. Duplicate SOS also get ACK.
void acknowledgeSOS(const VesselPacket &sos) {
  VesselPacket ack = {};
  ack.magic = PACKET_MAGIC;
  ack.version = PACKET_VERSION;
  ack.packetType = PACKET_SOS_ACK;
  ack.originNodeId = NODE_ID;
  ack.sequenceNumber = 0; // ACK is outside telemetry sequence accounting.
  ack.timestamp = millis() / 1000UL;
  strncpy(ack.nodeName, NODE_NAME, sizeof(ack.nodeName) - 1);
  memcpy(ack.message, &sos.sequenceNumber, 4);
  memcpy(ack.message + 4, &sos.originNodeId, 4);
  int state = radio.transmit(reinterpret_cast<uint8_t*>(&ack), sizeof(ack));
  Serial.printf("[SOS-B] ACK TX origin=0x%lX seq=%lu state=%d\n",
    (unsigned long)sos.originNodeId, (unsigned long)sos.sequenceNumber, state);
  startReceive();
}

// A cancellation ACK confirms that this receiver applied the matching
// cancellation; it is NOT an SOS rescue-resolution confirmation.
void acknowledgeCancel(const VesselPacket &cancel) {
  VesselPacket ack = {};
  ack.magic = PACKET_MAGIC;
  ack.version = PACKET_VERSION;
  ack.packetType = PACKET_SOS_CANCEL_ACK;
  ack.originNodeId = NODE_ID;
  ack.timestamp = millis() / 1000UL;
  strncpy(ack.nodeName, NODE_NAME, sizeof(ack.nodeName) - 1);
  memcpy(ack.message, &cancel.sequenceNumber, 4);
  memcpy(ack.message + 4, &cancel.originNodeId, 4);
  const int state = radio.transmit(reinterpret_cast<uint8_t*>(&ack), sizeof(ack));
  Serial.printf("[CANCEL-B] ACK TX event=%lu state=%d\n",
    (unsigned long)cancel.sequenceNumber, state);
  startReceive();
}

// ============================================================
// RX / SCHEDULE TX
// ============================================================
void processReceivedPacket(const uint8_t *buffer, size_t length,
                           int16_t rssi, float snr) {
  if (length != sizeof(VesselPacket)) {
    invalidPackets++;
    Serial.println("[RX-B] Invalid packet length");
    return;
  }
  VesselPacket p;
  memcpy(&p, buffer, sizeof(p));
  if (p.magic != PACKET_MAGIC || p.version != PACKET_VERSION) {
    invalidPackets++;
    Serial.println("[RX-B] Invalid packet header");
    return;
  }
  if (p.originNodeId == NODE_ID) return;

  if (p.originNodeId != 0xA1) {
    Serial.print("[RX-B] Unknown node ID 0x");
    Serial.println(p.originNodeId, HEX);
    return;
  }

  if (p.packetType == PACKET_SOS_CANCEL_ACK) {
    uint32_t referredId = 0, referredOrigin = 0;
    memcpy(&referredId, p.message, 4);
    memcpy(&referredOrigin, p.message+4, 4);
    if (bCancelPending && p.originNodeId == 0xA1 &&
        referredOrigin == NODE_ID && referredId == bPendingCancel.sequenceNumber) {
      Serial.printf("[CANCEL-B] ACK RECEIVED from Vessel-A for event=%lu after %u attempt(s)\n",
                    (unsigned long)referredId, bCancelAttempts);
      bCancelPending = false;
    } else Serial.println("[CANCEL-B] Ignored unrelated/late cancel ACK");
    return;
  }
  if (p.packetType == PACKET_SOS_ACK) {
    uint32_t referredId = 0, referredOrigin = 0;
    memcpy(&referredId, p.message, 4);
    memcpy(&referredOrigin, p.message + 4, 4);
    if (bSOSPending && p.originNodeId == 0xA1 && referredOrigin == NODE_ID &&
        referredId == bPendingSOS.sequenceNumber) {
      Serial.printf("[SOS-B] ACK RECEIVED from Vessel-A for SOS id=%lu after %u attempt(s)\n",
        (unsigned long)referredId, bSOSAttempts);
      bSOSPending = false;
    } else Serial.println("[SOS-B] Ignored unmatched/late SOS ACK");
    return;
  }
  if (p.packetType == PACKET_SOS_CANCEL) {
    uint32_t referredId = 0, referredOrigin = 0;
    memcpy(&referredId, p.message, 4);
    memcpy(&referredOrigin, p.message + 4, 4);
    if (sosAlertActive && p.originNodeId == lastSOSOriginId &&
        referredOrigin == lastSOSOriginId && referredId == lastSOSSequence &&
        p.sequenceNumber == lastSOSSequence) {
      const bool alreadyCancelled = sosCancelled;
      sosCancelled = true;
      oledPage = 4;
      lastOLEDUpdate = 0;
      Serial.printf("[SYNC-B] %s cancellation from A event=(0x%lX,%lu); ACK follows; NOT rescue resolved\n",
        alreadyCancelled ? "DUPLICATE" : "MATCHED",
        (unsigned long)referredOrigin, (unsigned long)referredId);
      acknowledgeCancel(p); // Duplicate valid cancel also gets ACK.
    } else {
      Serial.println("[SOS-B] Unmatched CANCEL rejected");
    }
    return;
  }
  if (p.packetType == PACKET_SOS) {
    // Emergency IDs are independent of ordinary telemetry sequence tracking.
    const bool duplicate = sosAlertActive &&
      lastSOSOriginId == p.originNodeId && lastSOSSequence == p.sequenceNumber;
    if (!duplicate) {
      // Replays of older emergencies cannot supersede the last observed ID.
      if (sosAlertActive && p.originNodeId == lastSOSOriginId &&
          p.sequenceNumber < lastSOSSequence) {
        Serial.printf("[SYNC-B] STALE SOS rejected from A event=%lu; latest=%lu; NO ACK\n",
                      (unsigned long)p.sequenceNumber, (unsigned long)lastSOSSequence);
        return;
      }
      sosUniqueEvents++;
      SOSControls::noteReceivedSOS();
      sosAlertActive = true;
      sosCancelled = false;
      lastSOSMillis = millis();
      lastSOSSequence = p.sequenceNumber;
      lastSOSOriginId = p.originNodeId;
      memcpy(lastSOSNodeName, p.nodeName, sizeof(p.nodeName));
      lastSOSNodeName[sizeof(p.nodeName)] = '\0';
      lastSOSGpsValid = (p.flags & FLAG_GPS_FIX) != 0;
      lastSOSLatitude = p.latitude_e7 / 10000000.0;
      lastSOSLongitude = p.longitude_e7 / 10000000.0;
      oledPage = 4;
      lastOLEDUpdate = 0;
      Serial.printf("[SOS-B] NEW EMERGENCY origin=0x%lX id=%lu GPS=%s\n",
        (unsigned long)p.originNodeId, (unsigned long)p.sequenceNumber,
        lastSOSGpsValid ? "VALID" : "NO_FIX");
    } else {
      sosDuplicateEvents++;
      Serial.printf("[SOS-B] DUPLICATE emergency id=%lu ignored\n", (unsigned long)p.sequenceNumber);
    }
    acknowledgeSOS(p);
    return;
  }
  if (p.packetType != PACKET_TELEMETRY) {
    invalidPackets++;
    Serial.println("[RX-B] Unsupported packet type");
    return;
  }
  bool isNew = updateNeighbor(p, rssi, snr);
  totalValidPacketsReceived++;

  int index = findNeighbor(p.originNodeId);
  Serial.println();
  Serial.println("========== PACKET RECEIVED BY B ==========");
  Serial.print("Node: "); Serial.println(p.nodeName);
  Serial.print("Sequence: "); Serial.println(p.sequenceNumber);
  Serial.print("RSSI: "); Serial.print(rssi); Serial.println(" dBm");
  Serial.print("SNR: "); Serial.print(snr, 2); Serial.println(" dB");
  printReceivedWeather(p);
  if (index >= 0) {
    const Neighbor &n = neighbors[index];
    Serial.print("Unique: "); Serial.println(n.uniquePackets);
    Serial.print("Missing: "); Serial.println(n.missingPackets);
    Serial.print("Duplicates: "); Serial.println(n.duplicates);
    Serial.print("Observed A->B PDR: "); Serial.print(n.pdrPercent, 2); Serial.println("%");
  }

  if (!txScheduleStarted && isNew && p.packetType == PACKET_TELEMETRY) {
    txScheduleStarted = true;
    nextTransmitAt = millis() + TX_OFFSET_AFTER_FIRST_A_PACKET;
    Serial.println("[SCHEDULE-B] Autonomous TX started; first slot in ~2.5 s");
  }
  Serial.println("==========================================");
}

// ============================================================
// NEIGHBOR / SEQUENCE STATISTICS
// updateNeighbor returns true for a previously unseen sequence.
// ============================================================
int findNeighbor(uint32_t id) {
  for (int i = 0; i < MAX_NEIGHBORS; i++) {
    if (neighbors[i].active && neighbors[i].nodeId == id) return i;
  }
  return -1;
}

int createNeighbor(uint32_t id) {
  for (int i = 0; i < MAX_NEIGHBORS; i++) {
    if (!neighbors[i].active) {
      memset(&neighbors[i], 0, sizeof(Neighbor));
      neighbors[i].active = true;
      neighbors[i].nodeId = id;
      neighbors[i].lastSeen = millis();
      neighbors[i].distanceKm = -1.0;
      neighbors[i].weatherState = WEATHER_CALIBRATING;
      return i;
    }
  }
  Serial.println("[NEIGHBOR] Table full");
  return -1;
}

bool sequenceAlreadySeen(const Neighbor &n, uint32_t seq) {
  for (int i = 0; i < SEQUENCE_CACHE_SIZE; i++) {
    if (n.seenSequenceValid[i] && n.seenSequences[i] == seq) return true;
  }
  return false;
}

void rememberSequence(Neighbor &n, uint32_t seq) {
  n.seenSequences[n.seenCursor] = seq;
  n.seenSequenceValid[n.seenCursor] = true;
  n.seenCursor = (n.seenCursor + 1) % SEQUENCE_CACHE_SIZE;
}

void resetSequenceStatistics(Neighbor &n) {
  n.packetsReceived = 0;
  n.uniquePackets = 0;
  n.duplicates = 0;
  n.firstSequence = 0;
  n.highestSequence = 0;
  n.missingPackets = 0;
  n.haveSequence = false;
  n.pdrPercent = 0.0f;
  n.seenCursor = 0;
  memset(n.seenSequences, 0, sizeof(n.seenSequences));
  memset(n.seenSequenceValid, 0, sizeof(n.seenSequenceValid));
}

void calculatePDR(Neighbor &n) {
  if (!n.haveSequence) {
    n.missingPackets = 0;
    n.pdrPercent = 0.0f;
    return;
  }
  uint64_t expected = static_cast<uint64_t>(n.highestSequence) -
                      static_cast<uint64_t>(n.firstSequence) + 1ULL;
  n.missingPackets = expected > n.uniquePackets
    ? static_cast<uint32_t>(expected - n.uniquePackets) : 0;
  n.pdrPercent = expected > 0
    ? 100.0f * static_cast<float>(n.uniquePackets) / static_cast<float>(expected)
    : 0.0f;
}

bool updateNeighbor(const VesselPacket &p, int16_t rssi, float snr) {
  int index = findNeighbor(p.originNodeId);
  if (index < 0) index = createNeighbor(p.originNodeId);
  if (index < 0) return false;

  Neighbor &n = neighbors[index];

  if (n.haveSequence && p.sequenceNumber == 0 && n.highestSequence > 10) {
    Serial.println("[PDR-B] Board A sequence reset; beginning new observation session");
    resetSequenceStatistics(n);
  }

  n.active = true;
  n.nodeId = p.originNodeId;
  n.lastSeen = millis();
  n.lastSequence = p.sequenceNumber;
  n.rssi = rssi;
  n.snr = snr;
  n.weatherState = p.weather.weatherState;
  n.remoteTempHumidityValid = (p.flags & FLAG_TEMP_HUMIDITY_VALID) != 0;
  n.remotePressureValid = (p.flags & FLAG_PRESSURE_VALID) != 0;
  n.remoteTemperature_x10 = p.weather.temperature_x10;
  n.remoteHumidity_x10 = p.weather.humidity_x10;
  n.remotePressure_x10 = p.weather.pressure_x10;
  n.remotePressureTrend_x10 = p.weather.pressureTrend_x10;
  n.remoteZIndex = p.weather.zambrettiState;
  n.remoteFusionState = p.weather.sensorFusionState;
  strncpy(n.nodeName, p.nodeName, sizeof(n.nodeName) - 1);
  n.nodeName[sizeof(n.nodeName) - 1] = '\0';

  if (p.flags & FLAG_GPS_FIX) {
    double lat = p.latitude_e7 / 10000000.0;
    double lon = p.longitude_e7 / 10000000.0;
    if (lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0)) {
      n.gpsValid = true;
      n.latitude = lat;
      n.longitude = lon;
    } else {
      n.gpsValid = false;
      n.distanceKm = -1.0;
    }
  } else {
    n.gpsValid = false;
    n.latitude = 0.0;
    n.longitude = 0.0;
    n.distanceKm = -1.0;
  }

  n.packetsReceived++;
  if (sequenceAlreadySeen(n, p.sequenceNumber)) {
    n.duplicates++;
    calculatePDR(n);
    return false;
  }

  rememberSequence(n, p.sequenceNumber);
  n.uniquePackets++;

  if (!n.haveSequence) {
    n.haveSequence = true;
    n.firstSequence = p.sequenceNumber;
    n.highestSequence = p.sequenceNumber;
  } else {
    if (p.sequenceNumber < n.firstSequence) n.firstSequence = p.sequenceNumber;
    if (p.sequenceNumber > n.highestSequence) n.highestSequence = p.sequenceNumber;
  }

  calculatePDR(n);
  return true;
}

void calculateDistances() {
  for (int i = 0; i < MAX_NEIGHBORS; i++) {
    Neighbor &n = neighbors[i];
    if (!n.active) continue;
    if (myGPSFix && n.gpsValid) {
      n.distanceKm = TinyGPSPlus::distanceBetween(
        myLatitude, myLongitude, n.latitude, n.longitude
      ) / 1000.0;
    } else {
      n.distanceKm = -1.0;
    }
  }
}

void removeStaleNeighbors() {
  for (int i = 0; i < MAX_NEIGHBORS; i++) {
    Neighbor &n = neighbors[i];
    if (n.active && millis() - n.lastSeen > NEIGHBOR_TIMEOUT) {
      Serial.print("[NEIGHBOR] Lost: "); Serial.println(n.nodeName);
      lastKnownNeighbor = n;
      lastKnownNeighbor.distanceKm = -1.0; // historical data, never a live distance
      haveLastKnownNeighbor = true;
      n.active = false;
      n.distanceKm = -1.0;
    }
  }
}

void printNeighborTable() {
  calculateDistances();
  Serial.println();
  Serial.println("========== B COMMUNICATION SUMMARY ==========");
  Serial.print("My GPS: "); Serial.println(myGPSFix ? "VALID" : "NO FIX");
  for (int i = 0; i < MAX_NEIGHBORS; i++) {
    const Neighbor &n = neighbors[i];
    if (!n.active) continue;
    Serial.print("Node: "); Serial.println(n.nodeName);
    Serial.print("Distance: ");
    if (n.distanceKm >= 0.0) { Serial.print(n.distanceKm, 3); Serial.println(" km"); }
    else Serial.println("?");
    Serial.print("RX: "); Serial.print(n.packetsReceived);
    Serial.print(" Unique: "); Serial.print(n.uniquePackets);
    Serial.print(" Missing: "); Serial.print(n.missingPackets);
    Serial.print(" Duplicates: "); Serial.println(n.duplicates);
    Serial.print("Observed A->B PDR: "); Serial.print(n.pdrPercent, 2); Serial.println("%");
    Serial.print("RSSI: "); Serial.print(n.rssi); Serial.print(" dBm, SNR: "); Serial.println(n.snr, 2);
  }
  Serial.print("B transmissions: "); Serial.println(txCount);
  Serial.print("B TX failures: "); Serial.println(txFailures);
  Serial.print("Invalid packets: "); Serial.println(invalidPackets);
  Serial.println("=============================================");
}

// ============================================================
// BOARD B TRANSMISSION
// ============================================================
void createTXPacket(VesselPacket &p) {
  memset(&p, 0, sizeof(p));
  p.magic = PACKET_MAGIC;
  p.version = PACKET_VERSION;
  p.packetType = PACKET_TELEMETRY;
  p.originNodeId = NODE_ID;
  p.sequenceNumber = txSequence++;
  p.timestamp = millis() / 1000UL; // Uptime seconds, not UTC.
  p.hopCount = 0;

  if (myGPSFix) {
    p.flags |= FLAG_GPS_FIX;
    p.latitude_e7 = static_cast<int32_t>(round(myLatitude * 10000000.0));
    p.longitude_e7 = static_cast<int32_t>(round(myLongitude * 10000000.0));
  }

  strncpy(p.nodeName, NODE_NAME, sizeof(p.nodeName) - 1);
  p.shoreDistance_m = 0; // Not implemented yet.
  p.batteryLevel = 100;  // Placeholder until battery measurement is added.

  // Board B has no real weather sensors connected yet.
  p.weather.temperature_x10 = 0;
  p.weather.humidity_x10 = 0;
  p.weather.pressure_x10 = 0;
  p.weather.pressureTrend_x10 = 0;
  p.weather.zambrettiState = 0;
  p.weather.sensorFusionState = WEATHER_CALIBRATING;
  p.weather.weatherState = WEATHER_CALIBRATING;
  strncpy(p.message, "CALIB", sizeof(p.message) - 1);
}

void transmitOwnPacket() {
  VesselPacket p;
  createTXPacket(p);

  Serial.println();
  Serial.print("[TX-B] Seq="); Serial.print(p.sequenceNumber);
  Serial.print(" GPS="); Serial.println((p.flags & FLAG_GPS_FIX) ? "VALID" : "NO_FIX");

  int state = radio.transmit(reinterpret_cast<uint8_t*>(&p), sizeof(p));
  if (state == RADIOLIB_ERR_NONE) {
    lastTXSuccess = true;
    txCount++;
    Serial.println("[TX-B] SUCCESS");
  } else {
    lastTXSuccess = false;
    txFailures++;
    Serial.print("[TX-B] FAILED: "); Serial.println(state);
  }

  startReceive();
}

// ============================================================
// OLED
// ============================================================
void updateOLED() {
  if (!oledAvailable) return;
  if (oledPage == 0) oledGPSPage();
  else if (oledPage == 1) oledNeighborPage();
  else if (oledPage == 2) oledPDRPage();
  else if (oledPage == 3) oledRemoteWeatherPage();
  else oledSOSPage();
}

void oledGPSPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "VESSEL-B GPS 1/5");
  u8g2.drawStr(0, 20, myGPSFix ? "GPS: FIX" : "GPS: SEARCH");
  char line[32];
  if (gps.satellites.isValid()) snprintf(line, sizeof(line), "SAT: %lu", gps.satellites.value());
  else strcpy(line, "SAT: ?");
  u8g2.drawStr(0, 32, line);
  if (myGPSFix) {
    snprintf(line, sizeof(line), "%.6f", myLatitude); u8g2.drawStr(0, 44, line);
    snprintf(line, sizeof(line), "%.6f", myLongitude); u8g2.drawStr(0, 56, line);
  } else {
    u8g2.drawStr(0, 44, "NO POSITION FIX");
    u8g2.drawStr(0, 56, "MOVE TO OPEN SKY");
  }
  u8g2.sendBuffer();
}

void oledNeighborPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "NEIGHBORS 2/5");
  int count = 0, first = -1;
  for (int i = 0; i < MAX_NEIGHBORS; i++) {
    if (neighbors[i].active) { count++; if (first < 0) first = i; }
  }
  char line[32];
  snprintf(line, sizeof(line), "NEAR: %d", count); u8g2.drawStr(0, 20, line);
  if (first < 0) {
    u8g2.drawStr(0, 32, "ACTIVE: 0 / STALE");
    if (haveLastKnownNeighbor) {
      char ago[30];
      snprintf(ago, sizeof(ago), "LAST %s", lastKnownNeighbor.nodeName);
      u8g2.drawStr(0, 44, ago);
      snprintf(ago, sizeof(ago), "SEEN %lus AGO",
               (millis() - lastKnownNeighbor.lastSeen) / 1000UL);
      u8g2.drawStr(0, 56, ago);
    } else {
      u8g2.drawStr(0, 44, "NEVER SEEN");
      u8g2.drawStr(0, 56, "LISTENING...");
    }
    u8g2.sendBuffer(); return;
  }
  const Neighbor &n = neighbors[first];
  u8g2.drawStr(0, 32, n.nodeName);
  if (n.gpsValid && myGPSFix && n.distanceKm >= 0.0) snprintf(line, sizeof(line), "DIST: %.2f km", n.distanceKm);
  else if (n.gpsValid) strcpy(line, "DIST: MY GPS?");
  else strcpy(line, "REMOTE GPS: NO FIX");
  u8g2.drawStr(0, 45, line);
  snprintf(line, sizeof(line), "RSSI:%d SNR:%.1f", n.rssi, n.snr);
  u8g2.drawStr(0, 57, line);
  u8g2.sendBuffer();
}

void oledPDRPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "A->B LINK/PDR 3/5");
  int first = -1;
  for (int i = 0; i < MAX_NEIGHBORS; i++) if (neighbors[i].active) { first = i; break; }
  if (first < 0) {
    u8g2.drawStr(0, 24, "WAITING FOR NODE");
    u8g2.drawStr(0, 40, "PDR: --");
    u8g2.drawStr(0, 56, txScheduleStarted ? "TX SCHEDULED" : "TX: WAIT FOR A");
    u8g2.sendBuffer(); return;
  }
  const Neighbor &n = neighbors[first];
  char line[32];
  snprintf(line, sizeof(line), "RX:%lu TX:%lu", static_cast<unsigned long>(n.packetsReceived), static_cast<unsigned long>(txCount));
  u8g2.drawStr(0, 20, line);
  snprintf(line, sizeof(line), "MISS:%lu DUP:%lu", static_cast<unsigned long>(n.missingPackets), static_cast<unsigned long>(n.duplicates));
  u8g2.drawStr(0, 32, line);
  snprintf(line, sizeof(line), "PDR: %.1f%%", n.pdrPercent);
  u8g2.drawStr(0, 44, line);
  u8g2.drawStr(0, 56, txScheduleStarted ? (lastTXSuccess ? "TX: OK" : "TX SCHEDULED") : "TX: WAIT FOR A");
  u8g2.sendBuffer();
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

// ============================================================
// REMOTE WEATHER (VESSEL-A): VALIDITY FLAGS PROTECT INVALID DATA
// ============================================================
void printReceivedWeather(const VesselPacket &p) {
  const bool thValid = (p.flags & FLAG_TEMP_HUMIDITY_VALID) != 0;
  const bool pressureValid = (p.flags & FLAG_PRESSURE_VALID) != 0;

  Serial.println("----- WEATHER FROM VESSEL-A -----");
  if (thValid) {
    Serial.printf("Temperature: %.1f C\n", p.weather.temperature_x10 / 10.0f);
    Serial.printf("Humidity: %.1f %%\n", p.weather.humidity_x10 / 10.0f);
  } else {
    Serial.println("Temperature: N/A (invalid/unavailable)");
    Serial.println("Humidity: N/A (invalid/unavailable)");
  }

  if (pressureValid) {
    Serial.printf("MSL pressure: %.1f hPa\n", p.weather.pressure_x10 / 10.0f);
    Serial.printf("Pressure delta: %+.1f hPa\n", p.weather.pressureTrend_x10 / 10.0f);
    // Z=0 is the calibrated-history-unavailable sentinel in Board A.
    if (p.weather.zambrettiState == 0)
      Serial.println("Zambretti index: -- (calibrating)");
    else
      Serial.printf("Zambretti index: %u\n", p.weather.zambrettiState);
    Serial.printf("Weather state: %s\n", weatherName(p.weather.weatherState));
  } else {
    Serial.println("MSL pressure: N/A (not validated)");
    Serial.println("Pressure delta: N/A");
    Serial.println("Zambretti index: N/A");
    Serial.println("Weather state: CALIBRATING/UNAVAILABLE");
  }
  Serial.println("---------------------------------");
}

void oledRemoteWeatherPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "VESSEL-A WEATHER 4/5");

  // There is currently only one expected remote node: Vessel-A.
  const int idx = findNeighbor(0xA1);
  if (idx < 0) {
    u8g2.drawStr(0, 28, "NO LIVE A WEATHER");
    u8g2.drawStr(0, 43, "WAITING FOR PACKETS");
    u8g2.sendBuffer();
    return;
  }

  const Neighbor &n = neighbors[idx];
  char line[32];
  if (n.remoteTempHumidityValid)
    snprintf(line, sizeof(line), "T:%.1fC RH:%.1f%%",
             n.remoteTemperature_x10 / 10.0f, n.remoteHumidity_x10 / 10.0f);
  else
    snprintf(line, sizeof(line), "T:--.-C RH:--.-%%");
  u8g2.drawStr(0, 21, line);

  if (n.remotePressureValid)
    snprintf(line, sizeof(line), "MSLP:%.1f hPa", n.remotePressure_x10 / 10.0f);
  else
    snprintf(line, sizeof(line), "MSLP:--.- hPa");
  u8g2.drawStr(0, 32, line);

  if (n.remotePressureValid && n.remoteZIndex != 0)
    snprintf(line, sizeof(line), "Z:%u DP:%+.1f", n.remoteZIndex,
             n.remotePressureTrend_x10 / 10.0f);
  else
    snprintf(line, sizeof(line), "Z:-- DP:--");
  u8g2.drawStr(0, 43, line);

  snprintf(line, sizeof(line), "WX:%s",
           n.remotePressureValid ? weatherName(n.weatherState) : "UNAVAILABLE");
  u8g2.drawStr(0, 54, line);
  u8g2.sendBuffer();
}

// ============================================================
// Phase 3.3: rescue-first SOS navigation, retained in RAM.
// The target is the LAST REPORTED SOS LOCATION, never proof that
// the distressed vessel is still there. Receiver-to-target range
// and true-north bearing are recalculated each OLED refresh using
// receiver GPS only while its position is fresh.
// ============================================================
void oledSOSPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "SOS / RESCUE  5/5");
  if (!sosAlertActive) {
    u8g2.drawStr(0, 27, "NO SOS RECEIVED");
    u8g2.drawStr(0, 47, "BTN: NEXT PAGE");
    u8g2.sendBuffer();
    return;
  }

  const unsigned long elapsed = millis() - lastSOSMillis;
  const bool recent = elapsed <= SOS_ALERT_MS;
  char line[40];
  if (sosCancelled) {
    u8g2.drawStr(0, 20, "CANCEL BY SENDER");
  } else if (recent) {
    u8g2.drawStr(0, 20, "SOS NEW / UNRES");
  } else {
    // Age is time since SOS packet reception, not GPS fix age.
    snprintf(line, sizeof(line), "SOS OLD %lum / UNRES", elapsed / 60000UL);
    u8g2.drawStr(0, 20, line);
  }
  snprintf(line, sizeof(line), "FROM: %.12s", lastSOSNodeName[0] ? lastSOSNodeName : "UNKNOWN");
  u8g2.drawStr(0, 31, line);

  if (!lastSOSGpsValid) {
    u8g2.drawStr(0, 43, "SOS GPS: NO FIX");
    u8g2.drawStr(0, 55, "DIST/BRG: UNKNOWN");
  } else if (!myGPSFix) {
    // Do not substitute a stale receiver location for an actual fix.
    u8g2.drawStr(0, 43, "OWN GPS: NO FIX");
    u8g2.drawStr(0, 55, "DIST/BRG: UNKNOWN");
  } else {
    // This always targets the stored SOS coordinates. If the receiver
    // moves, both values update; source coordinates do not change.
    const double km = TinyGPSPlus::distanceBetween(
      myLatitude, myLongitude, lastSOSLatitude, lastSOSLongitude) / 1000.0;
    const double bearing = TinyGPSPlus::courseTo(
      myLatitude, myLongitude, lastSOSLatitude, lastSOSLongitude);
    snprintf(line, sizeof(line), "TO SOS: %.2f km", km);
    u8g2.drawStr(0, 43, line);
    snprintf(line, sizeof(line), "BRG: %.0f deg N", bearing);
    u8g2.drawStr(0, 55, line);
  }
  u8g2.sendBuffer();
}