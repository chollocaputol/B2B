#include "lora_manager.h"
#include "weather_manager.h"
#include <SPI.h>
#include <RadioLib.h>
#include <TinyGPSPlus.h>
#include <math.h>
#include <string.h>
#include <Preferences.h>

// Source firmware: Board A, 915 MHz, RadioLib SX1262.
#define LORA_SCK       12
#define LORA_MISO      13
#define LORA_MOSI      11
#define LORA_CS        10
#define LORA_DIO1       1
#define LORA_RST        5
#define LORA_BUSY       4

// Must match Board B.
#define LORA_FREQUENCY  915.0
#define LORA_BANDWIDTH  125.0
#define LORA_SPREADING  9
#define LORA_CODING     7
#define LORA_SYNC_WORD  0x12
#define LORA_POWER      17
#define LORA_PREAMBLE   8
#define LORA_GAIN       1.8
#define NODE_ID 0xA1
static const char NODE_NAME[] = "Vessel-A";
#define RX_SEQUENCE_CACHE 256

// GPS coordinates are owned by main.cpp; no GPS parsing is moved here.
extern bool gpsHasFix;
extern double latitude;
extern double longitude;

SPIClass loraSPI(FSPI);
Module radioModule(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);
SX1262 radio(&radioModule);

// -------------------- Board A transmission --------------------
uint32_t txSequence = 0;
uint32_t txCount = 0;
uint32_t txFailures = 0;
bool lastTXSuccess = false;

// Phase 3.4.1: single pending SOS, bounded retries, and direct-neighbor ACK.
static Preferences emergencyPrefs;
static bool nvsReady = false;
static uint32_t nextEmergencyId = 0;
static bool activeEmergency = false; // independent of ACK / retry state
static uint32_t activeEmergencyId = 0;
static bool sosPending = false;
static VesselPacket pendingSOS = {};
static unsigned long lastSOSAttempt = 0;
static uint8_t sosAttempts = 0;
static constexpr uint8_t SOS_MAX_ATTEMPTS = 4; // initial + up to 3 retries
static constexpr unsigned long SOS_RETRY_MS = 4500UL;

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
static const unsigned long NEIGHBOR_TIMEOUT_MS = 15000UL;

bool isVesselBActive() {
  return rxFromB > 0 && (millis() - lastBSeen <= NEIGHBOR_TIMEOUT_MS);
}

unsigned long vesselBLastSeenSeconds() {
  return rxFromB > 0 ? (millis() - lastBSeen) / 1000UL : 0UL;
}


// Helper forward declarations (kept private to this translation unit).
static void processReceivedPacket(const uint8_t*, size_t, int16_t, float);
static void trackBSequence(uint32_t);
static bool bSequenceWasSeen(uint32_t);
static void rememberBSequence(uint32_t);
static void resetBSequenceSession();
static void calculateBSequencePDR();

void pollLoRaReceive() {
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
// PHASE 3.1: explicit manual distress test (no physical GPIO yet).
// Generates one SOS using the same wire format and sequence namespace.
// Requires a fresh GPS fix; when absent, FLAG_GPS_FIX stays clear and
// coordinates remain zero. A radio TX success is NOT a delivery ACK.
// ============================================================
static void sendPendingSOS() {
  if (!sosPending) return;
  const unsigned long now = millis();
  ++sosAttempts;
  lastSOSAttempt = now;
  Serial.printf("[SOS-A] ATTEMPT %u/%u id=(0x%lX,%lu) GPS=%s\n",
    sosAttempts, SOS_MAX_ATTEMPTS, (unsigned long)pendingSOS.originNodeId,
    (unsigned long)pendingSOS.sequenceNumber,
    (pendingSOS.flags & FLAG_GPS_FIX) ? "VALID" : "NO_FIX");
  const int state = radio.transmit(reinterpret_cast<uint8_t*>(&pendingSOS), sizeof(pendingSOS));
  if (state == RADIOLIB_ERR_NONE) { ++txCount; lastTXSuccess = true;
    Serial.println("[SOS-A] RADIO TX SUCCESS; AWAITING B ACK");
  } else { ++txFailures; lastTXSuccess = false;
    Serial.printf("[SOS-A] RADIO TX FAILED=%d\n", state);
  }
  startReceive();
}

void setupEmergencyIdentity() {
  nvsReady = emergencyPrefs.begin("sos-identity", false);
  if (!nvsReady) {
    Serial.println("[SOS-ID] ERROR NVS unavailable; SOS ID persistence unavailable");
    return;
  }
  nextEmergencyId = emergencyPrefs.getUInt("nextId", 1);
  Serial.printf("[SOS-ID] Next persistent ID=%lu\n", (unsigned long)nextEmergencyId);
}

void transmitSOS() {
  if (activeEmergency || sosPending) {
    Serial.println("[SOS-A] EXISTING UNRESOLVED SOS; cancel deliberately before starting a new one");
    return;
  }
  if (!nvsReady || nextEmergencyId == UINT32_MAX) {
    Serial.println("[SOS-ID] SOS blocked: persistent identity unavailable/exhausted");
    return;
  }
  // Reserve ID in NVS BEFORE transmission. Telemetry keeps its own sequence series.
  const uint32_t selectedId = nextEmergencyId;
  if (emergencyPrefs.putUInt("nextId", selectedId + 1) != sizeof(uint32_t)) {
    Serial.println("[SOS-ID] SOS blocked: NVS ID reservation failed");
    return;
  }
  nextEmergencyId = selectedId + 1;
  createPacket(pendingSOS);
  pendingSOS.sequenceNumber = selectedId;
  activeEmergencyId = selectedId;
  activeEmergency = true;
  pendingSOS.packetType = PACKET_SOS;
  strncpy(pendingSOS.message, "SOS", sizeof(pendingSOS.message) - 1);
  pendingSOS.message[sizeof(pendingSOS.message) - 1] = '\0';
  sosPending = true;
  sosAttempts = 0;
  sendPendingSOS();
}

void cancelActiveSOS() {
  if (!activeEmergency) {
    Serial.println("[SOS-A] No active emergency to cancel");
    return;
  }
  // DEVELOPMENT ONLY: one unauthenticated cancellation frame; not a rescue confirmation.
  VesselPacket cancel = {};
  cancel.magic = PACKET_MAGIC;
  cancel.version = PACKET_VERSION;
  cancel.packetType = PACKET_SOS_CANCEL;
  cancel.originNodeId = NODE_ID;
  cancel.sequenceNumber = activeEmergencyId;
  cancel.timestamp = millis() / 1000UL;
  strncpy(cancel.nodeName, NODE_NAME, sizeof(cancel.nodeName) - 1);
  memcpy(cancel.message, &activeEmergencyId, sizeof(activeEmergencyId));
  const uint32_t origin = NODE_ID;
  memcpy(cancel.message + 4, &origin, sizeof(origin));
  const int state = radio.transmit(reinterpret_cast<uint8_t*>(&cancel), sizeof(cancel));
  startReceive();
  Serial.printf("[SOS-A] CANCEL TX event=%lu state=%d (DELIVERY UNCONFIRMED)\n",
    (unsigned long)activeEmergencyId, state);
  // A deliberate operator cancellation releases the LOCAL activation lock.
  // IMPORTANT: local cancellation does NOT prove the other vessel received it.
  // In particular, a disconnected receiver may still show UNRESOLVED.
  // Reliable cancellation delivery/ACK and persistence are future work.
  sosPending = false;  // Stop retrying the cancelled SOS.
  activeEmergency = false;
  Serial.printf("[SOS-A] LOCAL CANCEL event=%lu; NEW SOS ENABLED; REMOTE DELIVERY NOT CONFIRMED\n",
                (unsigned long)activeEmergencyId);
}

void processSOSRetries() {
  if (!sosPending || (millis() - lastSOSAttempt < SOS_RETRY_MS)) return;
  if (sosAttempts >= SOS_MAX_ATTEMPTS) {
    Serial.printf("[SOS-A] UNCONFIRMED after %u attempts; emergency NOT resolved\n", sosAttempts);
    sosPending = false; // Transmission session ended; not emergency resolution.
    return;
  }
  sendPendingSOS();
}

// ============================================================
// RECEIVE BOARD B + TRACK REVERSE-LINK PDR
// ============================================================
static void processReceivedPacket(const uint8_t *buffer, size_t length,
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

  if (p.packetType == PACKET_SOS_ACK) {
    if (sosPending && matchesSOSAck(p, pendingSOS.sequenceNumber, pendingSOS.originNodeId)) {
      Serial.printf("[SOS-A] ACK RECEIVED from Vessel-B for SOS seq=%lu after %u attempt(s)\n",
        (unsigned long)pendingSOS.sequenceNumber, sosAttempts);
      sosPending = false;
    } else {
      Serial.println("[SOS-A] Ignored unrelated or expired SOS ACK");
    }
    return; // ACK is not ordinary telemetry; do not affect telemetry PDR.
  }
  if (p.packetType != PACKET_TELEMETRY && p.packetType != PACKET_SOS) {
    ++invalidPackets; return;
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

static bool bSequenceWasSeen(uint32_t sequence) {
  for (int i = 0; i < RX_SEQUENCE_CACHE; i++) {
    if (seenBSequenceValid[i] && seenBSequences[i] == sequence) return true;
  }
  return false;
}

static void rememberBSequence(uint32_t sequence) {
  seenBSequences[seenBCursor] = sequence;
  seenBSequenceValid[seenBCursor] = true;
  seenBCursor = (seenBCursor + 1) % RX_SEQUENCE_CACHE;
}

static void resetBSequenceSession() {
  uniqueFromB = duplicateFromB = missingFromB = 0;
  firstBSequence = highestBSequence = 0;
  haveBSequence = false;
  seenBCursor = 0;
  memset(seenBSequences, 0, sizeof(seenBSequences));
  memset(seenBSequenceValid, 0, sizeof(seenBSequenceValid));
}

static void trackBSequence(uint32_t sequence) {
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

static void calculateBSequencePDR() {
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