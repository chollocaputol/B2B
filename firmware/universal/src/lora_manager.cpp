#include "../../shared/tbeam_pins.h"
#include "../../shared/vessel_identity.h"
#include "../../shared/relay_policy.h"
#include "../../shared/relay_queue.h"
#include "../../shared/relay_dryrun_scheduler.h"
#include "../../shared/relay_preflight.h"
#include "../../shared/relay_receipt_state.h"
#include "../../shared/relay_tx_readiness.h"
#include "../../shared/relay_authorization_contract.h"
#include "../../shared/lab_relay_gate.h"
#include "lora_manager.h"
#include "../../shared/control_retry_schedule.h"
#include "weather_manager.h"
#include "../../shared/compact_position_v2.h"
#include "../../shared/phase5_1_6_rx_guard.h"
#include "../../shared/phase5_1_8_device_backend.h"
#include "../../shared/phase5_1_9D_shadow_rx.h"
// Set to 0 to perform the original 62-byte baseline comparison.
// In either mode, SOS/ACK/CANCEL remain unchanged legacy v1 packets.
// A manually enabled, bench-only forwarding experiment. OFF in normal builds.
#ifndef THESIS_LAB_RELAY_TX
#define THESIS_LAB_RELAY_TX 0
#endif
#if THESIS_LAB_RELAY_TX != 0 && THESIS_LAB_RELAY_TX != 1
#error "THESIS_LAB_RELAY_TX must be 0 or 1"
#endif

#ifndef POSITION_V2_TX_ENABLED
#define POSITION_V2_TX_ENABLED 1
#endif
#if POSITION_V2_TX_ENABLED != 0 && POSITION_V2_TX_ENABLED != 1
#error "POSITION_V2_TX_ENABLED must be 0 or 1"
#endif
#include "../../shared/multi_peer_telemetry.h"
#include "../../shared/multi_peer_sos.h"
#include <SPI.h>
#include <RadioLib.h>
#include <TinyGPSPlus.h>
#include <math.h>
#include <string.h>
#include <Preferences.h>
#ifndef THESIS_F05_RECOVERY
#define THESIS_F05_RECOVERY 0
#endif
#if THESIS_F05_RECOVERY != 0 && THESIS_F05_RECOVERY != 1
#error "THESIS_F05_RECOVERY must be 0 or 1"
#endif
#if THESIS_F05_RECOVERY == 1
#include "../../shared/f05_sos_recovery.h"
#endif
#ifndef THESIS_SD_LOGGER
#define THESIS_SD_LOGGER 0
#endif
#if THESIS_SD_LOGGER == 1
// Bridge is defined in main.cpp so the SD queue has one owning instance.
extern bool thesisLogEvent(bool urgent, const char* kind, uint32_t origin,
                           uint32_t seq, int32_t result, int16_t rssi = 0,
                           int16_t snrX10 = 0, int32_t latE7 = 0, int32_t lonE7 = 0,
                           uint8_t flags = 0, uint8_t hops = 0,
                           uint8_t attempt = 0);
#define SD_EVENT(...) thesisLogEvent(__VA_ARGS__)
#else
#define SD_EVENT(...) ((void)0)
#endif

// Source firmware: Board A, 915 MHz, RadioLib SX1262.


// Must match Board B.
#define LORA_FREQUENCY  915.0
#define LORA_BANDWIDTH  125.0
#define LORA_SPREADING  9
#define LORA_CODING     7
#define LORA_SYNC_WORD  0x12
#define LORA_POWER      17
#define LORA_PREAMBLE   8
#define LORA_GAIN       1.8
#define NODE_ID (VesselIdentity::nodeId())
#define NODE_NAME (VesselIdentity::label())
#define RX_SEQUENCE_CACHE 256

// GPS coordinates are owned by main.cpp; no GPS parsing is moved here.
extern bool gpsHasFix;
extern double latitude;
extern double longitude;

SPIClass loraSPI(FSPI);
Module radioModule(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);
SX1262 radio(&radioModule);

// PHASE 5.2.2C-3G-4E2: CAD owns the DIO1 interrupt ONLY while scanActive.
// RadioLib 6.6 SX1262 supports startChannelScan(), getChannelScanResult().
// This is a brief RX interruption; it is NOT passive RF monitoring.
static bool macCadScanActive = false;
static uint32_t macCadScanStartedAt = 0;
static TelemetryCadResult macCadInterrupted = TelemetryCadResult::None;
static constexpr uint32_t kMacCadTimeoutMs = 110UL;

// Phase 3G-4E2B1: controlled CAD profiles. Observation only!
// 0: unmodified RadioLib 6.6.x CAD defaults (preferred control).
// 1: four symbols, SF9 baseline thresholds 24 / 10 (experimental).
// 2: four symbols, higher peak threshold 28 / 10 (experimental).
// Values 24/10 and 28/10 are research trial values, NOT calibrated recommendations.
#ifndef CAD_PROFILE_ID
#define CAD_PROFILE_ID 0
#endif
#if CAD_PROFILE_ID < 0 || CAD_PROFILE_ID > 2
#error "CAD_PROFILE_ID must be 0, 1, or 2"
#endif
struct CadTrialProfile { uint8_t symbols; uint8_t detPeak; uint8_t detMin; };
static constexpr CadTrialProfile kCadTrialProfiles[] = {{2,24,10},{4,24,10},{4,28,10}};
static constexpr uint8_t kCadProfileId = CAD_PROFILE_ID;
static uint32_t macCadScanEndedAt=0;
static float macCadRssiBefore=0.0f;
static uint32_t macCadRssiSampleMs=0;
static bool cadProfileAnnounced=false;
static void announceCadTrialProfile() {
  if (cadProfileAnnounced) return;
  cadProfileAnnounced=true;
  const auto& x=kCadTrialProfiles[kCadProfileId];
  Serial.printf("[CAD-PROFILE] id=%lu profile=%u symbols=%u peak=%u min=%u libDefault=%u mode=OBSERVE_ONLY\n",
     (unsigned long)VesselIdentity::nodeId(),(unsigned)kCadProfileId,
     (unsigned)x.symbols,(unsigned)x.detPeak,(unsigned)x.detMin,(unsigned)(kCadProfileId==0));
}


// 3G-4E2A: passive diagnostics of EXISTING CAD scans. Never gate TX.
// Values describe sampled CAD results, NOT continuous channel occupancy.
struct MacCadDiagnostics {
  uint32_t started=0, free=0, busy=0, inconsistent=0;
  uint32_t error=0, timeouts=0, startErrors=0, rxGuards=0, interrupted=0;
};
static MacCadDiagnostics macCadDiag;
static uint32_t macCadDiagPrintedAtMs=0;

static void printMacCadDiagnostics() {
  const uint32_t now=millis();
  if (uint32_t(now-macCadDiagPrintedAtMs)<60000UL) return;
  macCadDiagPrintedAtMs=now;
  const uint32_t interpreted=macCadDiag.free+macCadDiag.busy;
  const uint32_t sampledBusyPct=interpreted ?
      (uint32_t)((100ULL*macCadDiag.busy)/interpreted) : 0;
  Serial.printf("[CAD-SUMMARY] id=%lu started=%lu free=%lu busy=%lu "
                "busyPctOfDecisions=%lu inconsistent=%lu errors=%lu "
                "timeouts=%lu startErr=%lu rxGuard=%lu interrupted=%lu\n",
      (unsigned long)VesselIdentity::nodeId(),
      (unsigned long)macCadDiag.started,
      (unsigned long)macCadDiag.free, (unsigned long)macCadDiag.busy,
      (unsigned long)sampledBusyPct,
      (unsigned long)macCadDiag.inconsistent,
      (unsigned long)macCadDiag.error, (unsigned long)macCadDiag.timeouts,
      (unsigned long)macCadDiag.startErrors,
      (unsigned long)macCadDiag.rxGuards,
      (unsigned long)macCadDiag.interrupted);
}


static void interruptMacCadForControlTx() {
  if (macCadScanActive) {
    macCadScanActive = false;
    // A real SOS / ACK / cancellation takes precedence over routine CAD.
    macCadInterrupted = TelemetryCadResult::Interrupted;
    ++macCadDiag.interrupted;
  }
}

bool telemetryCadRunning() { return macCadScanActive; }
TelemetryCadResult startTelemetryCad() {
  if (macCadScanActive) return TelemetryCadResult::Scanning;
  // Preserve an already received frame, and avoid replacing an ongoing
  // demodulation with CAD when the SX1262 RX IRQ flags reveal it.
  if (digitalRead(LORA_DIO1) == HIGH) {
    ++macCadDiag.rxGuards;
    return TelemetryCadResult::RxActive;
  }
  const uint16_t irq = radio.getIrqStatus();
  if (irq & (RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED |
             RADIOLIB_SX126X_IRQ_HEADER_VALID)) {
    ++macCadDiag.rxGuards;
    return TelemetryCadResult::RxActive;
  }
  announceCadTrialProfile();
  // 3G4E2B4 diagnostic ONLY: sample instantaneous RSSI during RX,
  // immediately before CAD. Never use the value as a TX gate or threshold.
  // A single RSSI value is NOT a spectrum scan, and is not simultaneous with CAD.
  const float rssiBeforeCad = radio.getRSSI(false);
  macCadRssiBefore = rssiBeforeCad;
  macCadRssiSampleMs = millis();
  // Do NOT use the old -95 dBm instantaneous RSSI gate: it caused starvation.
  // Preserve EXACT RadioLib default algorithm in profile 0.
  const CadTrialProfile &trial=kCadTrialProfiles[kCadProfileId];
  const int16_t rc=(kCadProfileId==0)
    ? radio.startChannelScan()
    : radio.startChannelScan(
        trial.symbols==4 ? RADIOLIB_SX126X_CAD_ON_4_SYMB : RADIOLIB_SX126X_CAD_ON_2_SYMB,
        trial.detPeak,trial.detMin);
  if (rc != RADIOLIB_ERR_NONE) {
    ++macCadDiag.startErrors;
    startReceive(); // restore continuous RX if scanning could not start
    return TelemetryCadResult::Error;
  }
  macCadScanActive = true;
  ++macCadDiag.started;
  macCadScanStartedAt = millis();
  return TelemetryCadResult::Scanning;
}

TelemetryCadResult pollTelemetryCad() {
  printMacCadDiagnostics();
  if (macCadInterrupted != TelemetryCadResult::None) {
    const auto outcome = macCadInterrupted;
    macCadInterrupted = TelemetryCadResult::None;
    return outcome;
  }
  if (!macCadScanActive) return TelemetryCadResult::None;
  // A CAD completion IRQ is NOT an RX_DONE IRQ; never call readData on it.
  if (digitalRead(LORA_DIO1) == HIGH) {
    // Capture IRQ BEFORE startReceive clears/reconfigures the RX IRQ mapping.
    // Do not print until RX has been restored (avoid prolonging deaf time).
    const int pinAtCompletion=digitalRead(LORA_DIO1);
    const uint16_t irq=radio.getIrqStatus();
    const int16_t rc = radio.getChannelScanResult();
    macCadScanEndedAt=millis();
    const uint32_t elapsed=uint32_t(macCadScanEndedAt-macCadScanStartedAt);
    macCadScanActive = false;
    startReceive(); // restore RX regardless of result or next action
    const bool done=(irq & RADIOLIB_SX126X_IRQ_CAD_DONE)!=0;
    const bool detected=(irq & RADIOLIB_SX126X_IRQ_CAD_DETECTED)!=0;
    if (rc == RADIOLIB_LORA_DETECTED) ++macCadDiag.busy;
    else if (rc == RADIOLIB_CHANNEL_FREE) ++macCadDiag.free;
    else ++macCadDiag.error;
    if ((rc == RADIOLIB_LORA_DETECTED && !detected) ||
        (rc == RADIOLIB_CHANNEL_FREE && !done) ||
        (detected && !done)) ++macCadDiag.inconsistent;
    Serial.printf("[CAD-IRQ] id=%lu raw=0x%04X dio=%d done=%u detected=%u "
                  "rxDone=%u rc=%d elapsedMs=%lu profile=%u startMs=%lu endMs=%lu rssiBeforeDbm=%.1f rssiSampleMs=%lu\n",
      (unsigned long)VesselIdentity::nodeId(), (unsigned)irq,
      pinAtCompletion, (unsigned)done, (unsigned)detected,
      (unsigned)((irq & RADIOLIB_SX126X_IRQ_RX_DONE)!=0),
      (int)rc, (unsigned long)elapsed, (unsigned)kCadProfileId,
      (unsigned long)macCadScanStartedAt,(unsigned long)macCadScanEndedAt,
      (double)macCadRssiBefore,(unsigned long)macCadRssiSampleMs);
    Serial.printf("[CAD-RSSI] id=%lu profile=%u decision=%s rxRssiDbm=%.1f sampleMs=%lu cadStartMs=%lu cadEndMs=%lu\n",
      (unsigned long)VesselIdentity::nodeId(),(unsigned)kCadProfileId,
      rc==RADIOLIB_LORA_DETECTED ? "BUSY" : rc==RADIOLIB_CHANNEL_FREE ? "FREE" : "ERROR",
      (double)macCadRssiBefore,(unsigned long)macCadRssiSampleMs,
      (unsigned long)macCadScanStartedAt,(unsigned long)macCadScanEndedAt);
    if (rc == RADIOLIB_CHANNEL_FREE) return TelemetryCadResult::Free;
    if (rc == RADIOLIB_LORA_DETECTED) return TelemetryCadResult::Busy;
    return TelemetryCadResult::Error;
  }
  if (uint32_t(millis() - macCadScanStartedAt) >= kMacCadTimeoutMs) {
    const uint16_t irq=radio.getIrqStatus();
    const uint32_t elapsed=uint32_t(millis()-macCadScanStartedAt);
    macCadScanActive = false;
    ++macCadDiag.timeouts;
    startReceive();
    Serial.printf("[CAD-IRQ] id=%lu result=TIMEOUT raw=0x%04X elapsedMs=%lu\n",
      (unsigned long)VesselIdentity::nodeId(), (unsigned)irq,
      (unsigned long)elapsed);
    return TelemetryCadResult::Timeout;
  }
  return TelemetryCadResult::Scanning;
}



// Never transmit under an absent/invalid identity, including automatic ACKs.
static int transmitWithIdentity(const uint8_t* data, size_t length) {
  if (!VesselIdentity::ready()) {
    Serial.println("[IDENTITY] TX BLOCKED: provisioned identity required");
    return -999;
  }
  interruptMacCadForControlTx();
  return radio.transmit(const_cast<uint8_t*>(data), length);
}

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
static uint32_t nextSOSTxAt = 0;
static uint8_t sosAttempts = 0;
static constexpr uint8_t SOS_MAX_ATTEMPTS = 4; // initial + up to 3 retries
static constexpr unsigned long SOS_RETRY_MS = 4500UL;

// Cancellation transaction is independent of any newer SOS. RAM only.
static bool cancelPending = false;
static VesselPacket pendingCancel = {};
static uint8_t cancelAttempts = 0;
static unsigned long lastCancelAttempt = 0;
static uint32_t nextCancelTxAt = 0;
static constexpr uint8_t CANCEL_MAX_ATTEMPTS = 4;
static constexpr unsigned long CANCEL_RETRY_MS = 4500UL;

#if THESIS_F05_RECOVERY == 1
// F05 bench-only persistent recovery. Both critical event IDs occupy a single
// versioned snapshot and alternate NVS keys. CRC detects accidental corruption;
// it is not authentication and does not authorize a radio relay.
namespace F05Recovery {
static State saved{};
static bool storageBlocked = false;
static bool recoveredCancelNeedsHold = false;

inline bool save(uint32_t active, uint32_t cancel, uint32_t attempts,
                 uint32_t status) {
  if (!nvsReady || storageBlocked || !VesselIdentity::ready()) return false;
  const uint32_t gen = saved.generation + 1UL;
  const State next = make(NODE_ID, gen ? gen : 1UL,
                          active, cancel, attempts, status);
  if (!valid(next, NODE_ID, nextEmergencyId)) {
    Serial.println("[F05-RECOVERY] INVALID_TRANSITION; TX BLOCKED");
    storageBlocked = true;
    return false;
  }
  const char* key = (next.generation & 1UL) ? "f05a" : "f05b";
  if (emergencyPrefs.putBytes(key, &next, sizeof(next)) != sizeof(next)) {
    Serial.println("[F05-RECOVERY] WRITE_FAILED; TX BLOCKED");
    storageBlocked = true;
    return false;
  }
  State roundtrip{};
  if (emergencyPrefs.getBytesLength(key) != sizeof(roundtrip) ||
      emergencyPrefs.getBytes(key, &roundtrip, sizeof(roundtrip)) != sizeof(roundtrip) ||
      memcmp(&next, &roundtrip, sizeof(next)) != 0 ||
      !valid(roundtrip, NODE_ID, nextEmergencyId)) {
    Serial.println("[F05-RECOVERY] READBACK_FAILED; TX BLOCKED");
    storageBlocked = true;
    return false;
  }
  saved = next;
  return true;
}

inline void restore() {
  if (!nvsReady || !VesselIdentity::ready()) {
    storageBlocked = true;
    Serial.println("[F05-RECOVERY] NVS_OR_IDENTITY_UNAVAILABLE; TX BLOCKED");
    return;
  }
  const size_t sizeA = emergencyPrefs.getBytesLength("f05a");
  const size_t sizeB = emergencyPrefs.getBytesLength("f05b");
  State a{}, b{};
  const bool validA = sizeA == sizeof(a) &&
    emergencyPrefs.getBytes("f05a", &a, sizeof(a)) == sizeof(a) &&
    valid(a, NODE_ID, nextEmergencyId);
  const bool validB = sizeB == sizeof(b) &&
    emergencyPrefs.getBytes("f05b", &b, sizeof(b)) == sizeof(b) &&
    valid(b, NODE_ID, nextEmergencyId);
  if (!validA && !validB) {
    if (sizeA || sizeB) {
      storageBlocked = true;
      Serial.println("[F05-RECOVERY] NO_VALID_NVS_SNAPSHOT; TX BLOCKED");
    } else {
      Serial.println("[F05-RECOVERY] CLEAN_BOOT no unresolved checkpoint");
    }
    return;
  }
  saved = (!validB || (validA && newer(a.generation, b.generation))) ? a : b;
  if ((sizeA && !validA) || (sizeB && !validB))
    Serial.println("[F05-RECOVERY] WARNING: one damaged slot; recovered older valid state");

  // Restoring identity must NEVER silently restart SOS or CANCEL radio TX.
  // The operator can deliberately hold GPIO2 to cancel the active SOS or
  // resume an older pending cancellation; no SD card is involved.
  activeEmergency = saved.activeId != 0;
  activeEmergencyId = saved.activeId;
  sosPending = false;
  cancelPending = saved.cancelStatus == kCancelPending;
  cancelAttempts = static_cast<uint8_t>(saved.cancelAttempts);
  recoveredCancelNeedsHold = cancelPending;
  if (cancelPending) {
    memset(&pendingCancel, 0, sizeof(pendingCancel));
    pendingCancel.magic = PACKET_MAGIC;
    pendingCancel.version = PACKET_VERSION;
    pendingCancel.packetType = PACKET_SOS_CANCEL;
    pendingCancel.originNodeId = NODE_ID;
    pendingCancel.sequenceNumber = saved.cancelId;
    pendingCancel.timestamp = millis() / 1000UL;
    strncpy(pendingCancel.nodeName, NODE_NAME, sizeof(pendingCancel.nodeName) - 1);
    memcpy(pendingCancel.message, &saved.cancelId, 4);
    const uint32_t origin = NODE_ID;
    memcpy(pendingCancel.message + 4, &origin, 4);
  }
  Serial.printf("[F05-RECOVERY] RESTORED active=%lu pendingCancel=%lu "
                "cancelAttempts=%lu status=%lu AUTO_TX=OFF HOLD=REQUIRED\n",
                (unsigned long)saved.activeId, (unsigned long)saved.cancelId,
                (unsigned long)saved.cancelAttempts, (unsigned long)saved.cancelStatus);
  if (saved.cancelStatus == kCancelUnconfirmed)
    Serial.println("[F05-RECOVERY] CANCEL_UNCONFIRMED; retry budget exhausted");
}
} // namespace F05Recovery
#endif // THESIS_F05_RECOVERY


static MultiPeerTelemetry::Table peerTelemetry;

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



// Phase 5.2.2C-2F: snapshot a live peer for display. No TX or neighbor mutation.
// Iterate numerical identities: at most 9999 small bounded lookup operations.
// Selection is most recently updated ACTIVE peer; ties prefer lower ID.
bool getLivePeerSnapshot(PeerDisplaySnapshot &out) {
  out = PeerDisplaySnapshot{};
  const uint32_t now = millis();
  const MultiPeerTelemetry::Peer* chosen = nullptr;
  for (uint32_t id=1; id<=9999; ++id) {
    const auto* peer = peerTelemetry.find(id);
    if (!peer || !peer->active(now)) continue;
    if (!chosen || uint32_t(now-peer->lastSeenMs)<uint32_t(now-chosen->lastSeenMs) ||
        (peer->lastSeenMs==chosen->lastSeenMs && peer->id<chosen->id)) chosen=peer;
  }
  if (!chosen) return false;
  out.present = true;
  out.id = chosen->id;
  out.active = true;
  out.gpsValid = chosen->gpsValid;
  out.latitude = chosen->latitude;
  out.longitude = chosen->longitude;
  out.rssi = chosen->rssi;
  out.snr = chosen->snr;
  out.unique = chosen->unique;
  out.duplicates = chosen->duplicates;
  out.missing = chosen->missingEstimate();
  out.pdr = chosen->pdrEstimate();
  out.ageSeconds = uint32_t(now-chosen->lastSeenMs)/1000UL;
  return true;
}
bool getPeerSnapshotById(uint32_t id, PeerDisplaySnapshot &out) {
  out=PeerDisplaySnapshot{};
  const auto* x=peerTelemetry.find(id);
  if(!x) return false;
  const uint32_t now=millis();
  out.present=true;out.active=x->active(now);out.id=x->id;
  out.gpsValid=out.active && x->gpsValid;
  out.latitude=x->latitude;out.longitude=x->longitude;
  out.rssi=x->rssi;out.snr=x->snr;
  out.unique=x->unique;out.duplicates=x->duplicates;
  out.missing=x->missingEstimate();out.pdr=x->pdrEstimate();
  out.ageSeconds=uint32_t(now-x->lastSeenMs)/1000UL;
  return true;
}
bool nextActivePeerId(uint32_t current, uint32_t &next) {
  uint32_t first=0, after=0;
  for(uint32_t id=1;id<=9999;++id) {
    const auto* x=peerTelemetry.find(id);
    if(!x || !x->active(millis())) continue;
    if(!first)first=id;
    if(id>current && !after) after=id;
  }
  if(!first)return false;
  next=after ? after:first;
  return true;
}
size_t getTrackedPeerCount() { return peerTelemetry.count(); }
uint32_t getTotalPeerTelemetryReceived() {
  uint64_t total = 0;
  for (uint32_t id=1; id<=9999; ++id) {
    const auto* peer = peerTelemetry.find(id);
    if (peer) total += peer->received;
  }
  return total > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(total);
}

// Phase 3G-4E1: read-only MAC peer count. A 120s horizon avoids a brief
// missed transmission immediately shrinking the estimated network load.
uint32_t getRecentMacPeerCount(uint32_t nowMs, uint32_t horizonMs) {
  uint32_t count = 0;
  for (uint32_t id = 1; id <= 9999; ++id) {
    const auto* peer = peerTelemetry.find(id);
    if (peer && uint32_t(nowMs - peer->lastSeenMs) <= horizonMs) ++count;
  }
  return count;
}


// Helper forward declarations (kept private to this translation unit).
static void processReceivedPacket(const uint8_t*, size_t, int16_t, float);
static void trackBSequence(uint32_t);
static bool bSequenceWasSeen(uint32_t);
static void rememberBSequence(uint32_t);
static void resetBSequenceSession();
static void calculateBSequencePDR();

// Received distress record is volatile RAM, never authoritative after reboot.
static MultiPeerSOS::Table peerSOSTable;
// R2-B1 dry-run state is volatile, bounded, and NEVER submitted to radio TX.
static Relay52::DryRunScheduler<VesselPacket> relayDryRun;
static RelayPolicy::Cache relaySeen;
// R2-B3: local, unauthenticated neighbor-ACK evidence only.
static RelayReceipt::Tracker relayReceipt;
static ReceivedSOSView incomingBSOS = {};
size_t getSOSRecordCount() {return peerSOSTable.count();}
size_t getActiveSOSCount() {return peerSOSTable.activeCount();}
bool getSOSRecordByIndex(size_t index,SOSRecordSnapshot &out) {
 out=SOSRecordSnapshot{};
 const auto* slot=peerSOSTable.at(index);
 if(!slot)return false;
 out.known=true;out.originId=slot->origin;out.eventId=slot->event;
 out.receivedAtMs=slot->receivedAtMs;out.cancelled=slot->cancelled;
 out.positionValid=slot->positionValid;out.latitude=slot->latitude;
 out.longitude=slot->longitude;
 return true;
}

static bool incomingBSOSNew = false;
ReceivedSOSView getReceivedSOSView() { return incomingBSOS; }
bool consumeNewReceivedSOS() {
  bool pending = incomingBSOSNew;
  incomingBSOSNew = false;
  return pending;
}

// PHASE 5.2.2C-3G-1: receive health diagnostics (no recovery TX).
static uint32_t diagIrqCount = 0;
static uint32_t diagReadOk = 0;
static uint32_t diagReadErr = 0;
static uint32_t diagInvalidLen = 0;
static uint32_t diagStartReceiveErr = 0;
static uint32_t diagLastRxMs = 0;
// PHASE 5.2.2C-3G-4A: timestamp-only telemetry TX/RX diagnostics.
// millis() is LOCAL device uptime, not synchronized across vessels.
static uint32_t traceTxTelemetry = 0;
static uint32_t traceTxTelemetryFail = 0;
static uint32_t traceTxTotalDurationMs = 0;
static uint32_t traceRxTelemetry = 0;
static void printPeerReceiveDiagnostics();

// R2-B1: only authoritative NEW SOS or CANCELLED events may enter the
// preparation queue. Invalid, stale, duplicate and unknown events cannot
// consume the relay duplicate cache. No radio transmission happens here.
static void prepareRelayAfterSOSState(const VesselPacket& p,
                                      bool stateAccepted, const char* stateLabel) {
  if (!stateAccepted) {
    Serial.printf("[RELAY52] origin=0x%lX event=%lu type=%u state=%s result=STATE_REJECTED TX=OFF\n",
      (unsigned long)p.originNodeId, (unsigned long)p.sequenceNumber,
      (unsigned)p.packetType, stateLabel);
    return;
  }
  const Relay52::Admit result = relayDryRun.offer(p, NODE_ID, true);
  Serial.printf("[RELAY52] origin=0x%lX event=%lu type=%u hops=%u result=%s queued=%u TX=NOT_AT_ADMISSION\n",
    (unsigned long)p.originNodeId, (unsigned long)p.sequenceNumber,
    (unsigned)p.packetType, (unsigned)p.hopCount,
    Relay52::name(result), (unsigned)relayDryRun.size());
  // Relay51 remains diagnostic; inspect only after successful queue admission
  // so FULL/rejected frames cannot poison its separate observation cache.
  if (result == Relay52::Admit::Queued) {
    const RelayPolicy::Decision decision = RelayPolicy::inspect(p, NODE_ID, relaySeen);
    Serial.printf("[RELAY51] origin=0x%lX event=%lu type=%u hops=%u decision=%s; FORWARD=NOT_AT_ADMISSION\n",
      (unsigned long)p.originNodeId, (unsigned long)p.sequenceNumber,
      (unsigned)p.packetType, (unsigned)p.hopCount, RelayPolicy::name(decision));
  }
}
void pollLoRaReceive() {
  // DIO1 is CAD_DONE/CAD_DETECTED while a channel scan is running.
  if (macCadScanActive) return; // do not misread CAD as a received packet
  if (digitalRead(LORA_DIO1) == HIGH) {
    ++diagIrqCount;
    size_t len = radio.getPacketLength();
    if (len == 0 || len > Phase516::kMaxReceiveWireBytes) ++diagInvalidLen;
    if (len > 0 && len <= Phase516::kMaxReceiveWireBytes) {
      uint8_t buffer[Phase516::kMaxReceiveWireBytes];
      int state = radio.readData(buffer, len);
      if (state == RADIOLIB_ERR_NONE) {
        ++diagReadOk;
        diagLastRxMs = millis();
        int16_t rssi = static_cast<int16_t>(radio.getRSSI());
        float snr = radio.getSNR();
        processReceivedPacket(buffer, len, rssi, snr);
      } else {
        ++diagReadErr;
        Serial.print("[RX-A] readData error: ");
        Serial.println(state);
        invalidPackets++;
      }
    }
    startReceive();
  }
  // R2-B2: reject queued events superseded/cancelled while they were waiting.
  // This checks only volatile SOS state; it is not authentication or a TX grant.
  VesselPacket prepared;
  if (relayDryRun.poll(millis(), prepared)) {
    const Relay52::Preflight gate = Relay52::preflightPrepared(prepared, peerSOSTable);
    if (gate != Relay52::Preflight::Ready) {
      Serial.printf("[RELAY52] DROP origin=0x%lX event=%lu type=%u reason=%s TX=OFF\n",
        (unsigned long)prepared.originNodeId, (unsigned long)prepared.sequenceNumber,
        (unsigned)prepared.packetType, Relay52::preflightName(gate));
    } else {
#if THESIS_LAB_RELAY_TX == 1
      // BENCH ONLY: fixed three-node route 001 -> 002 -> 003. This is NOT
      // a cryptographic security authorization, nor suitable for sea trials.
      if (ThesisLabRelay::permits(prepared, NODE_ID)) {
        const int relayRc = transmitWithIdentity(
          reinterpret_cast<const uint8_t*>(&prepared), sizeof(prepared));
        startReceive();
        SD_EVENT(true, "relay_tx", prepared.originNodeId,
                 prepared.sequenceNumber, relayRc, 0, 0,
                 prepared.latitude_e7, prepared.longitude_e7,
                 prepared.flags, prepared.hopCount);
        Serial.printf("[LAB-RELAY-TX] origin=%lu event=%lu type=%u hops=%u rc=%d attempts=1 SECURITY=UNAUTHENTICATED\n",
          (unsigned long)prepared.originNodeId,
          (unsigned long)prepared.sequenceNumber,
          (unsigned)prepared.packetType, (unsigned)prepared.hopCount, relayRc);
      } else {
        Serial.printf("[LAB-RELAY] BLOCKED self=%lu origin=%lu event=%lu type=%u hops=%u\n",
          (unsigned long)NODE_ID, (unsigned long)prepared.originNodeId,
          (unsigned long)prepared.sequenceNumber,
          (unsigned)prepared.packetType, (unsigned)prepared.hopCount);
      }
#else
      Serial.printf("[RELAY52] PREPARED origin=0x%lX event=%lu type=%u outgoing_hops=%u state=READY readiness=%s contract=%s TX=OFF\n",
        (unsigned long)prepared.originNodeId, (unsigned long)prepared.sequenceNumber,
        (unsigned)prepared.packetType, (unsigned)prepared.hopCount,
        RelayReadiness::blockerName(RelayReadiness::primaryBlocker()),
        RelayContract::statusName(RelayContract::assess(
          RelayContract::evidenceUnavailable(NODE_ID, prepared.originNodeId))));
#endif
    }
  }
  printPeerReceiveDiagnostics();
}

// Every 15 seconds, print RX activity and active peers. This intentionally
// never calls radio.startReceive(), resets the radio, or transmits anything.
static void printPeerReceiveDiagnostics() {
  static uint32_t lastPrintMs = 0;
  const uint32_t now = millis();
  if (uint32_t(now - lastPrintMs) < 15000UL) return;
  lastPrintMs = now;
  size_t active = 0;
  for (uint32_t id = 1; id <= 9999; ++id) {
    const auto* p = peerTelemetry.find(id);
    if (p && p->active(now)) ++active;
  }
  Serial.printf("[RX-DIAG] ms=%lu dio1=%d irq=%lu readOK=%lu readERR=%lu invalidLen=%lu rxStartERR=%lu lastRXAgeMs=%ld known=%u active=%u peers=",
    (unsigned long)now, digitalRead(LORA_DIO1),
    (unsigned long)diagIrqCount, (unsigned long)diagReadOk,
    (unsigned long)diagReadErr, (unsigned long)diagInvalidLen,
    (unsigned long)diagStartReceiveErr,
    diagReadOk ? (long)uint32_t(now-diagLastRxMs) : -1L,
    (unsigned)peerTelemetry.count(), (unsigned)active);
  bool any = false;
  for (uint32_t id = 1; id <= 9999; ++id) {
    const auto* p = peerTelemetry.find(id);
    if (!p || !p->active(now)) continue;
    if (any) Serial.print(',');
    Serial.printf("%lu:%lus", (unsigned long)id,
                  (unsigned long)(uint32_t(now-p->lastSeenMs)/1000UL));
    any = true;
  }
  if (!any) Serial.print("none");
  Serial.println();
  Serial.printf("[RF-TRACE] nowMs=%lu txTelemetry=%lu txFail=%lu txTotalMs=%lu rxTelemetry=%lu crcOrReadErr=%lu rxStartErr=%lu\n",
    (unsigned long)now, (unsigned long)traceTxTelemetry,
    (unsigned long)traceTxTelemetryFail, (unsigned long)traceTxTotalDurationMs,
    (unsigned long)traceRxTelemetry, (unsigned long)diagReadErr,
    (unsigned long)diagStartReceiveErr);
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
    ++diagStartReceiveErr;
    Serial.print("[RX-A] startReceive error: ");
    Serial.println(state);
  }
}

// ============================================================
// TRANSMISSION
// ============================================================
void createPacket(VesselPacket &p, bool consumeTelemetrySequence) {
  memset(&p, 0, sizeof(p));
  p.magic = PACKET_MAGIC;
  p.version = PACKET_VERSION;
  p.packetType = PACKET_TELEMETRY;
  p.originNodeId = NODE_ID;
  p.sequenceNumber = consumeTelemetrySequence ? txSequence++ : 0;
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
  if (!VesselIdentity::ready()) return;
  VesselPacket p;
  createPacket(p);
  Serial.print("[TX-A] Seq="); Serial.print(p.sequenceNumber);
  Serial.print(" GPS="); Serial.print((p.flags & FLAG_GPS_FIX) ? "VALID" : "NO_FIX");
  Serial.print(" Packet="); Serial.println(POSITION_V2_TX_ENABLED ? "POSITION_V2" : "TELEMETRY_V1");

  const uint32_t traceBeginMs = millis();
  Serial.printf("[TX-TRACE] id=%lu seq=%lu beginMs=%lu\n",
    (unsigned long)VesselIdentity::nodeId(), (unsigned long)p.sequenceNumber,
    (unsigned long)traceBeginMs);
  // Routine POSITION v2 is 25 bytes. Preserve the original v1 SOS path.
  const uint8_t* telemetryWire = reinterpret_cast<const uint8_t*>(&p);
  size_t telemetryBytes = sizeof(p);
#if POSITION_V2_TX_ENABLED
  CompactPositionV2::State pos;
  pos.originId = p.originNodeId;
  pos.sequence = p.sequenceNumber;
  pos.uptimeSeconds = p.timestamp;
  pos.gpsValid = (p.flags & FLAG_GPS_FIX) != 0;
  pos.latitudeE7 = p.latitude_e7;
  pos.longitudeE7 = p.longitude_e7;
  uint8_t positionWire[CompactPositionV2::kWireSize] = {};
  if (!CompactPositionV2::encode(positionWire, sizeof(positionWire), pos)) {
    ++txFailures;
    lastTXSuccess = false;
    Serial.printf("[PKT-POS2] id=%lu seq=%lu ERROR=ENCODE_INVALID, no TX\n",
      (unsigned long)pos.originId,(unsigned long)pos.sequence);
    startReceive();
    return;
  }
  telemetryWire = positionWire;
  telemetryBytes = sizeof(positionWire);
#endif
  static bool packetModePrinted = false;
  if (!packetModePrinted) {
    packetModePrinted = true;
    Serial.printf("[PKT-SCHEMA] tx=POSITION_%s txBytes=%u legacySOSBytes=%u rx=V1_OR_V2\n",
      POSITION_V2_TX_ENABLED ? "V2" : "V1",
      (unsigned)telemetryBytes,(unsigned)sizeof(VesselPacket));
  }
  Serial.printf("[PKT-TX] id=%lu seq=%lu type=%s bytes=%u\n",
    (unsigned long)p.originNodeId,(unsigned long)p.sequenceNumber,
    POSITION_V2_TX_ENABLED ? "POSITION_V2" : "TELEMETRY_V1",(unsigned)telemetryBytes);
  int state = transmitWithIdentity(telemetryWire, telemetryBytes);
  const uint32_t traceEndMs = millis();
  const uint32_t traceDurationMs = uint32_t(traceEndMs - traceBeginMs);
  ++traceTxTelemetry;
  if (state != RADIOLIB_ERR_NONE) ++traceTxTelemetryFail;
  traceTxTotalDurationMs += traceDurationMs;
  Serial.printf("[TX-TRACE] id=%lu seq=%lu endMs=%lu durationMs=%lu rc=%d\n",
    (unsigned long)VesselIdentity::nodeId(), (unsigned long)p.sequenceNumber,
    (unsigned long)traceEndMs, (unsigned long)traceDurationMs, state);
  SD_EVENT(false, "telemetry_tx", p.originNodeId, p.sequenceNumber,
           state, 0, 0, p.latitude_e7, p.longitude_e7, p.flags);
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
  const int state = transmitWithIdentity(reinterpret_cast<uint8_t*>(&pendingSOS), sizeof(pendingSOS));
  if (state == RADIOLIB_ERR_NONE) { ++txCount; lastTXSuccess = true;
    Serial.println("[SOS-A] RADIO TX SUCCESS; AWAITING B ACK");
  } else { ++txFailures; lastTXSuccess = false;
    Serial.printf("[SOS-A] RADIO TX FAILED=%d\n", state);
  }
  SD_EVENT(true, "sos_tx_attempt", pendingSOS.originNodeId,
           pendingSOS.sequenceNumber, state, 0, 0,
           pendingSOS.latitude_e7, pendingSOS.longitude_e7,
           pendingSOS.flags, pendingSOS.hopCount, sosAttempts);
  relayReceipt.sosAttempt(NODE_ID, pendingSOS.sequenceNumber, state == RADIOLIB_ERR_NONE);
  Serial.printf("[RECEIPT] SOS event=%lu neighbor=%s farEnd=%s txOK=%u\n",
    (unsigned long)pendingSOS.sequenceNumber,
    RelayReceipt::neighborName(relayReceipt.snapshot().neighbor),
    RelayReceipt::farEndName(relayReceipt.snapshot().farEnd),
    (unsigned)(state == RADIOLIB_ERR_NONE));
  startReceive();
  nextSOSTxAt = millis() + SOS_RETRY_MS + ControlRetry::sosSlotDelayMs(NODE_ID, pendingSOS.sequenceNumber, sosAttempts);
}

void setupEmergencyIdentity() {
  nvsReady = emergencyPrefs.begin("sos-identity", false);
  if (!nvsReady) {
    Serial.println("[SOS-ID] ERROR NVS unavailable; SOS ID persistence unavailable");
    return;
  }
  nextEmergencyId = emergencyPrefs.getUInt("nextId", 1);
  Serial.printf("[SOS-ID] Next persistent ID=%lu\n", (unsigned long)nextEmergencyId);
#if THESIS_F05_RECOVERY == 1
  F05Recovery::restore();
#endif
}

void transmitSOS() {
#if THESIS_F05_RECOVERY == 1
  if (F05Recovery::storageBlocked) { Serial.println("[F05-RECOVERY] SOS BLOCKED, NVS unsafe"); return; }
#endif
  if (!VesselIdentity::ready()) { Serial.println("[IDENTITY] SOS BLOCKED: not provisioned"); return; }
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
#if THESIS_F05_RECOVERY == 1
  // SOS identity is reserved first. Persist its active state before any RF TX.
  if (!F05Recovery::save(selectedId, F05Recovery::saved.cancelId,
                         F05Recovery::saved.cancelAttempts,
                         F05Recovery::saved.cancelStatus)) {
    Serial.println("[F05-RECOVERY] SOS NOT STARTED; NVS checkpoint failed");
    return;
  }
#endif
  createPacket(pendingSOS, false);
  pendingSOS.sequenceNumber = selectedId;
  activeEmergencyId = selectedId;
  activeEmergency = true;
  pendingSOS.packetType = PACKET_SOS;
  strncpy(pendingSOS.message, "SOS", sizeof(pendingSOS.message) - 1);
  pendingSOS.message[sizeof(pendingSOS.message) - 1] = '\0';
  sosPending = true;
  SD_EVENT(true, "sos_local_start", NODE_ID, selectedId, 0, 0, 0,
           pendingSOS.latitude_e7, pendingSOS.longitude_e7, pendingSOS.flags);
  relayReceipt.begin(NODE_ID, selectedId);
  Serial.printf("[RECEIPT] SOS event=%lu neighbor=%s farEnd=%s originAuth=UNVERIFIED\n",
    (unsigned long)selectedId,
    RelayReceipt::neighborName(relayReceipt.snapshot().neighbor),
    RelayReceipt::farEndName(relayReceipt.snapshot().farEnd));
  sosAttempts = 0;
  nextSOSTxAt = millis() + ControlRetry::sosSlotDelayMs(NODE_ID, pendingSOS.sequenceNumber, 0);
  Serial.printf("[SOS-A] Scheduled first attempt in %lu ms\n", (unsigned long)(nextSOSTxAt - millis()));
}

// One cancellation in flight. Refuse a second until ACK or timeout,
// rather than silently replacing the older event's cancellation.
static void sendPendingCancel() {
  if (!cancelPending) return;
#if THESIS_F05_RECOVERY == 1
  if (F05Recovery::recoveredCancelNeedsHold || F05Recovery::storageBlocked) return;
  // Reserve this RF attempt before transmitting; reboot cannot reset budget.
  if (!F05Recovery::save(F05Recovery::saved.activeId, pendingCancel.sequenceNumber,
                         (uint32_t)cancelAttempts + 1UL, F05Recovery::kCancelPending)) return;
#endif
  ++cancelAttempts;
  lastCancelAttempt = millis();
  const int state = transmitWithIdentity(reinterpret_cast<uint8_t*>(&pendingCancel), sizeof(pendingCancel));
  startReceive();
  SD_EVENT(true, "cancel_tx_attempt", NODE_ID,
           pendingCancel.sequenceNumber, state, 0, 0, 0, 0, 0, 0, cancelAttempts);
  nextCancelTxAt = millis() + CANCEL_RETRY_MS + ControlRetry::slotDelayMs(NODE_ID, pendingCancel.sequenceNumber, cancelAttempts);
  Serial.printf("[CANCEL-A] ATTEMPT %u/%u event=%lu radio=%d; ACK PENDING\n",
    cancelAttempts, CANCEL_MAX_ATTEMPTS,
    (unsigned long)pendingCancel.sequenceNumber, state);
}

bool hasActiveLocalSOS() {
#if THESIS_F05_RECOVERY == 1
  // A recovered pending cancellation requires a deliberate 5s hold.
  return activeEmergency || F05Recovery::recoveredCancelNeedsHold;
#else
  return activeEmergency;
#endif
}

void cancelActiveSOS() {
#if THESIS_F05_RECOVERY == 1
  if (F05Recovery::storageBlocked) { Serial.println("[F05-RECOVERY] CANCEL BLOCKED, NVS unsafe"); return; }
  if (F05Recovery::recoveredCancelNeedsHold && cancelPending) {
    F05Recovery::recoveredCancelNeedsHold = false;
    nextCancelTxAt = millis() + ControlRetry::slotDelayMs(
        NODE_ID, pendingCancel.sequenceNumber, cancelAttempts);
    Serial.printf("[F05-RECOVERY] MANUAL_CANCEL_RESUMED event=%lu remaining=%u; NOT RESCUE RESOLVED\n",
      (unsigned long)pendingCancel.sequenceNumber, (unsigned)(CANCEL_MAX_ATTEMPTS - cancelAttempts));
    return;
  }
#endif
  if (!activeEmergency) {
    Serial.println("[SOS-A] No active emergency to cancel");
    return;
  }
  if (cancelPending) {
    Serial.println("[CANCEL-A] Previous cancel pending; cannot queue another cancellation yet");
    return;
  }
#if THESIS_F05_RECOVERY == 1
  // Durable intent before clearing active distress or scheduling CANCEL RF.
  if (!F05Recovery::save(0, activeEmergencyId, 0, F05Recovery::kCancelPending)) {
    Serial.println("[F05-RECOVERY] LOCAL CANCEL NOT APPLIED; NVS write failed");
    return;
  }
#endif
  memset(&pendingCancel, 0, sizeof(pendingCancel));
  pendingCancel.magic = PACKET_MAGIC;
  pendingCancel.version = PACKET_VERSION;
  pendingCancel.packetType = PACKET_SOS_CANCEL;
  pendingCancel.originNodeId = NODE_ID;
  pendingCancel.sequenceNumber = activeEmergencyId;
  pendingCancel.timestamp = millis() / 1000UL;
  strncpy(pendingCancel.nodeName, NODE_NAME, sizeof(pendingCancel.nodeName) - 1);
  memcpy(pendingCancel.message, &activeEmergencyId, 4);
  const uint32_t origin = NODE_ID;
  memcpy(pendingCancel.message + 4, &origin, 4);
  cancelPending = true;
  SD_EVENT(true, "sos_local_cancel", NODE_ID, activeEmergencyId, 0);
  cancelAttempts = 0;
  sosPending = false; // Deliberate cancellation stops the prior SOS retries.
  activeEmergency = false; // New SOS can begin before this cancel is ACKed.
  relayReceipt.localCancel(NODE_ID, activeEmergencyId);
  Serial.printf("[RECEIPT] CANCEL event=%lu local=YES farEnd=%s\n",
    (unsigned long)activeEmergencyId,
    RelayReceipt::farEndName(relayReceipt.snapshot().farEnd));
  Serial.printf("[SOS-A] LOCAL CANCEL event=%lu; NEW SOS ENABLED; REMOTE NOT YET CONFIRMED\n",
    (unsigned long)activeEmergencyId);
  nextCancelTxAt = millis() + ControlRetry::slotDelayMs(NODE_ID, pendingCancel.sequenceNumber, 0);
  Serial.printf("[CANCEL-A] Scheduled first cancellation in %lu ms\n", (unsigned long)(nextCancelTxAt - millis()));
}

void processCancelRetries() {
#if THESIS_F05_RECOVERY == 1
  if (F05Recovery::storageBlocked || F05Recovery::recoveredCancelNeedsHold) return;
#endif
  if (!cancelPending || !ControlRetry::due(millis(), nextCancelTxAt)) return;
  if (cancelAttempts >= CANCEL_MAX_ATTEMPTS) {
#if THESIS_F05_RECOVERY == 1
    if (!F05Recovery::save(F05Recovery::saved.activeId, pendingCancel.sequenceNumber,
                           cancelAttempts, F05Recovery::kCancelUnconfirmed)) return;
#endif
    Serial.printf("[CANCEL-A] UNCONFIRMED after %u attempts event=%lu; REMOTE STATE UNKNOWN\n",
      cancelAttempts, (unsigned long)pendingCancel.sequenceNumber);
    SD_EVENT(true, "cancel_unconfirmed", NODE_ID, pendingCancel.sequenceNumber, cancelAttempts);
    cancelPending = false;
    return;
  }
  sendPendingCancel();
}

void processSOSRetries() {
#if THESIS_F05_RECOVERY == 1
  if (F05Recovery::storageBlocked) return;
#endif
  if (!sosPending || !ControlRetry::due(millis(), nextSOSTxAt)) return;
  if (sosAttempts >= SOS_MAX_ATTEMPTS) {
    Serial.printf("[SOS-A] UNCONFIRMED after %u attempts; emergency NOT resolved\n", sosAttempts);
    relayReceipt.retriesExhausted(NODE_ID, pendingSOS.sequenceNumber);
    Serial.printf("[RECEIPT] SOS event=%lu neighbor=%s farEnd=%s\n",
      (unsigned long)pendingSOS.sequenceNumber,
      RelayReceipt::neighborName(relayReceipt.snapshot().neighbor),
      RelayReceipt::farEndName(relayReceipt.snapshot().farEnd));
    SD_EVENT(true, "sos_unconfirmed", NODE_ID, pendingSOS.sequenceNumber, sosAttempts);
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
  // Phase 5.1.6: format-only, fail-closed RX gate. NO signature verifier yet.
  // v3 frames are quarantined before any SOS state change, ACK, or relay queue.
  const Phase516::Decision gate = Phase516::decide(buffer, length);
  if (gate == Phase516::Decision::Reject) {
    ++invalidPackets;
    Serial.println("[RX-GATE] REJECT invalid wire format");
    return;
  }
  if (gate == Phase516::Decision::QuarantineV3) {
    // Phase 5.1.9D -- receive-only shadow inspection, no trusted keys provisioned.
    // The immutable runtime trust callback fails closed before Ed25519 or SOS state.
    static Phase519::Ledger v3ShadowLedger{};
    const Phase519::Result shadowResult = Phase519D::inspect(buffer, length, v3ShadowLedger);
    Serial.printf("[RX-519D] v3 shadowOutcome=%u verifyVerdict=%u trust=UNPROVISIONED durable=NO sos=NO ack=NO relay=OFF\n",
       (unsigned)shadowResult.outcome, (unsigned)shadowResult.signature);
    Serial.printf("[RX-GATE] V3_QUARANTINED kind=0x%02X bytes=%u auth=NOT_IMPLEMENTED ack=NO relay=OFF\n",
       (unsigned)buffer[3], (unsigned)length);
    return;
  }
  // Legacy v1 is UNAUTHENTICATED and retains its existing behavior.
  // POSITION v2 is UNAUTHENTICATED telemetry. Never treat either as v3.
  // Compact POSITION packets are decoded into the existing telemetry pipeline.
  // This keeps peer tracking, OLED navigation and PDR accounting unchanged.
  VesselPacket normalizedPosition = {};
  if (length == CompactPositionV2::kWireSize) {
    CompactPositionV2::State pos;
    if (!CompactPositionV2::decode(buffer,length,pos)) {
      ++invalidPackets;
      Serial.println("[RX-POS2] Invalid v2 position frame");
      return;
    }
    normalizedPosition.magic = PACKET_MAGIC;
    normalizedPosition.version = PACKET_VERSION;
    normalizedPosition.packetType = PACKET_TELEMETRY;
    normalizedPosition.originNodeId = pos.originId;
    normalizedPosition.sequenceNumber = pos.sequence;
    normalizedPosition.timestamp = pos.uptimeSeconds;
    normalizedPosition.flags = pos.gpsValid ? FLAG_GPS_FIX : 0;
    normalizedPosition.latitude_e7 = pos.latitudeE7;
    normalizedPosition.longitude_e7 = pos.longitudeE7;
    normalizedPosition.weather.weatherState = WEATHER_CALIBRATING;
    Serial.printf("[RX-POS2] origin=%lu seq=%lu gps=%u bytes=%u\n",
      (unsigned long)pos.originId,(unsigned long)pos.sequence,
      (unsigned)pos.gpsValid,(unsigned)length);
    buffer = reinterpret_cast<const uint8_t*>(&normalizedPosition);
    length = sizeof(normalizedPosition);
  }
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

  if (p.originNodeId < 1 || p.originNodeId > 9999) {
    Serial.print("[RX-A] Reject invalid numeric node ID 0x");
    Serial.println(p.originNodeId, HEX);
    return;
  }

  // Phase 5.2.2C-2D: per-origin SOS state; still no relay TX.
  if (p.packetType == PACKET_SOS_CANCEL) {
    uint32_t cancelId=0, cancelOrigin=0;
    memcpy(&cancelId,p.message,4);
    memcpy(&cancelOrigin,p.message+4,4);
    if (cancelOrigin!=p.originNodeId || cancelId!=p.sequenceNumber) {
      Serial.println("[PEER-SOS] CANCEL_MISMATCH payload; NO ACK"); prepareRelayAfterSOSState(p, false, "CANCEL_PAYLOAD"); return;
    }
    const auto result=peerSOSTable.cancel(p.originNodeId,cancelId,millis());
    SD_EVENT(true, "cancel_rx", p.originNodeId, cancelId, (int32_t)result,
             rssi, (int16_t)(snr * 10.0f), p.latitude_e7,
             p.longitude_e7, p.flags, p.hopCount);
    prepareRelayAfterSOSState(p, result==MultiPeerSOS::Result::Cancelled, MultiPeerSOS::name(result));
    Serial.printf("[PEER-SOS] CANCEL origin=%lu event=%lu result=%s\n",
      (unsigned long)p.originNodeId,(unsigned long)cancelId,MultiPeerSOS::name(result));
    if (result!=MultiPeerSOS::Result::Cancelled && result!=MultiPeerSOS::Result::CancelDuplicate)return;
    if (incomingBSOS.known && incomingBSOS.originId==p.originNodeId &&
        incomingBSOS.eventId==cancelId)incomingBSOS.cancelled=true;
    VesselPacket ack={};ack.magic=PACKET_MAGIC;ack.version=PACKET_VERSION;
    ack.packetType=PACKET_SOS_CANCEL_ACK;ack.originNodeId=NODE_ID;
    ack.timestamp=millis()/1000UL;
    strncpy(ack.nodeName,NODE_NAME,sizeof(ack.nodeName)-1);
    encodeSOSAck(ack,cancelId,cancelOrigin);
    const int rc=transmitWithIdentity(reinterpret_cast<uint8_t*>(&ack),sizeof(ack));
    startReceive();
    SD_EVENT(true, "cancel_ack_tx", NODE_ID, cancelId, rc);
    Serial.printf("[PEER-SOS] CANCEL ACK origin=%lu event=%lu radio=%d; NOT RESCUE RESOLVED\n",
      (unsigned long)p.originNodeId,(unsigned long)cancelId,rc);
    return;
  }
  if (p.packetType == PACKET_SOS) {
    const auto result=peerSOSTable.receiveSOS(p.originNodeId,p.sequenceNumber,millis());
    SD_EVENT(true, "sos_rx", p.originNodeId, p.sequenceNumber,
             (int32_t)result, rssi, (int16_t)(snr * 10.0f),
             p.latitude_e7, p.longitude_e7, p.flags, p.hopCount);
    prepareRelayAfterSOSState(p, result==MultiPeerSOS::Result::New, MultiPeerSOS::name(result));
    Serial.printf("[PEER-SOS] SOS origin=%lu event=%lu result=%s\n",
      (unsigned long)p.originNodeId,(unsigned long)p.sequenceNumber,MultiPeerSOS::name(result));
    if(result==MultiPeerSOS::Result::Stale||result==MultiPeerSOS::Result::Full)return;
    if(result==MultiPeerSOS::Result::New){
      bool validPosition = (p.flags & FLAG_GPS_FIX)!=0;
      const double latitudeSOS=p.latitude_e7/1e7;
      const double longitudeSOS=p.longitude_e7/1e7;
      validPosition = validPosition && latitudeSOS >= -90.0 && latitudeSOS <= 90.0 &&
        longitudeSOS >= -180.0 && longitudeSOS <= 180.0 &&
        !(latitudeSOS==0.0 && longitudeSOS==0.0);
      peerSOSTable.setPositionForNew(p.originNodeId,p.sequenceNumber,millis(),validPosition,latitudeSOS,longitudeSOS);
      incomingBSOS.known=true;incomingBSOS.cancelled=false;
      incomingBSOS.originId=p.originNodeId;incomingBSOS.eventId=p.sequenceNumber;
      incomingBSOS.receivedAtMs=millis();incomingBSOS.positionValid=(p.flags&FLAG_GPS_FIX)!=0;
      incomingBSOS.latitude=p.latitude_e7/1e7;incomingBSOS.longitude=p.longitude_e7/1e7;
      incomingBSOSNew=true;
    }
    VesselPacket ack={};ack.magic=PACKET_MAGIC;ack.version=PACKET_VERSION;
    ack.packetType=PACKET_SOS_ACK;ack.originNodeId=NODE_ID;ack.timestamp=millis()/1000UL;
    strncpy(ack.nodeName,NODE_NAME,sizeof(ack.nodeName)-1);
    encodeSOSAck(ack,p.sequenceNumber,p.originNodeId);
    const int rc=transmitWithIdentity(reinterpret_cast<uint8_t*>(&ack),sizeof(ack));
    startReceive();
    SD_EVENT(true, "sos_ack_tx", NODE_ID, p.sequenceNumber, rc);
    Serial.printf("[PEER-SOS] SOS ACK origin=%lu event=%lu radio=%d (direct-neighbor receipt)\n",
      (unsigned long)p.originNodeId,(unsigned long)p.sequenceNumber,rc);
    return;
  }
  if (p.packetType == PACKET_SOS_CANCEL_ACK) {
    uint32_t ackId = 0, ackOrigin = 0;
    memcpy(&ackId, p.message, 4);
    memcpy(&ackOrigin, p.message + 4, 4);
    if (cancelPending && ackOrigin == NODE_ID &&
        ackId == pendingCancel.sequenceNumber) {
#if THESIS_F05_RECOVERY == 1
      // Matching neighbor ACK: clear durable pending cancellation first.
      if (!F05Recovery::save(F05Recovery::saved.activeId, 0, 0, F05Recovery::kCancelNone)) {
        Serial.println("[F05-RECOVERY] ACK OBSERVED BUT CHECKPOINT FAILED");
        return;
      }
      F05Recovery::recoveredCancelNeedsHold = false;
#endif
      Serial.printf("[CANCEL-A] ACK RECEIVED from Vessel-B for event=%lu after %u attempt(s)\n",
        (unsigned long)ackId, cancelAttempts);
      cancelPending = false;
      SD_EVENT(true, "cancel_ack_rx", p.originNodeId, ackId, 0,
               rssi, (int16_t)(snr * 10.0f));
      if (relayReceipt.cancelAck(NODE_ID, ackId, p.originNodeId)) {
        Serial.printf("[RECEIPT] CANCEL event=%lu neighborCancelAck=CLAIMED sender=%lu farEnd=UNVERIFIED originAuth=UNVERIFIED\n",
          (unsigned long)ackId, (unsigned long)p.originNodeId);
      }
    } else {
      Serial.println("[CANCEL-A] Ignored unrelated/late cancellation ACK");
    }
    return; // Control packet, not telemetry PDR.
  }
  if (p.packetType == PACKET_SOS_ACK) {
    if (sosPending && matchesSOSAck(p, pendingSOS.sequenceNumber, pendingSOS.originNodeId)) {
      if (relayReceipt.neighborAck(NODE_ID, pendingSOS.sequenceNumber, p.originNodeId)) {
        Serial.printf("[RECEIPT] SOS event=%lu neighbor=%s sender=%lu farEnd=%s originAuth=UNVERIFIED\n",
          (unsigned long)pendingSOS.sequenceNumber,
          RelayReceipt::neighborName(relayReceipt.snapshot().neighbor),
          (unsigned long)p.originNodeId,
          RelayReceipt::farEndName(relayReceipt.snapshot().farEnd));
      }
      Serial.printf("[SOS-A] ACK RECEIVED from Vessel-B for SOS seq=%lu after %u attempt(s)\n",
        (unsigned long)pendingSOS.sequenceNumber, sosAttempts);
      sosPending = false;
      SD_EVENT(true, "sos_ack_rx", p.originNodeId, pendingSOS.sequenceNumber,
               0, rssi, (int16_t)(snr * 10.0f));
    } else {
      Serial.println("[SOS-A] Ignored unrelated or expired SOS ACK");
    }
    return; // ACK is not ordinary telemetry; do not affect telemetry PDR.
  }
  if (p.packetType != PACKET_TELEMETRY && p.packetType != PACKET_SOS) {
    ++invalidPackets; return;
  }

  // 5.2.2C-2E: observe telemetry by numerical vessel ID independently.
  // Legacy Board-A-derived UI counters remain until the next migration.
  if (p.packetType == PACKET_TELEMETRY) {
    bool validPosition = false;
    double peerLat = 0.0, peerLon = 0.0;
    if (p.flags & FLAG_GPS_FIX) {
      peerLat = p.latitude_e7 / 10000000.0;
      peerLon = p.longitude_e7 / 10000000.0;
      validPosition = peerLat >= -90.0 && peerLat <= 90.0 &&
                      peerLon >= -180.0 && peerLon <= 180.0 &&
                      !(peerLat == 0.0 && peerLon == 0.0);
    }
    const uint32_t traceReceiveMs = millis();
    ++traceRxTelemetry;
    Serial.printf("[RX-TRACE] at=%lu origin=%lu seq=%lu senderUptimeS=%lu RSSI=%d SNR=%.2f\n",
      (unsigned long)traceReceiveMs, (unsigned long)p.originNodeId,
      (unsigned long)p.sequenceNumber, (unsigned long)p.timestamp, rssi, snr);
    const MultiPeerTelemetry::Result result = peerTelemetry.observe(
        p.originNodeId, p.sequenceNumber, traceReceiveMs, rssi, snr,
        validPosition, peerLat, peerLon, p.weather.weatherState, p.timestamp);
    SD_EVENT(false, "telemetry_rx", p.originNodeId, p.sequenceNumber,
             (int32_t)result, rssi, (int16_t)(snr * 10.0f),
             p.latitude_e7, p.longitude_e7, p.flags, p.hopCount);
    const MultiPeerTelemetry::Peer* peer = peerTelemetry.find(p.originNodeId);
    if (result == MultiPeerTelemetry::Result::Restart) {
      Serial.printf("[PEER-SESSION] RESTART id=%lu session=%lu uptime=%lu seq=%lu; PDR baseline reset\n",
        (unsigned long)p.originNodeId, (unsigned long)(peer ? peer->session : 0),
        (unsigned long)p.timestamp, (unsigned long)p.sequenceNumber);
    }
    Serial.printf("[PEER-TLM] id=%lu seq=%lu result=%s peers=%u unique=%lu dup=%lu missing=%lu pdr=%.1f%%\n",
      (unsigned long)p.originNodeId, (unsigned long)p.sequenceNumber,
      MultiPeerTelemetry::name(result), (unsigned)peerTelemetry.count(),
      (unsigned long)(peer ? peer->unique : 0),
      (unsigned long)(peer ? peer->duplicates : 0),
      (unsigned long)(peer ? peer->missingEstimate() : 0),
      peer ? peer->pdrEstimate() : 0.0f);
  }

  // Phase 5.2.2C-2H: do not mix multiple vessel sequences into the
  // historical Board-B-only counters; the authoritative state is peerTelemetry.
  if (p.packetType == PACKET_TELEMETRY) {
    Serial.printf("[RX-PEER] origin=%lu seq=%lu RSSI=%d SNR=%.2f\n",
      (unsigned long)p.originNodeId, (unsigned long)p.sequenceNumber,
      rssi, snr);
  }

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