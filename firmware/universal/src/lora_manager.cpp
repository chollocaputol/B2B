// F23D SD begin
#include "../../shared/relay/f23d_sd_capture.h"
#if THESIS_F23D_SD_EVIDENCE
#include <Arduino.h>
static bool f23dSDRadioCalled=false;
static int f23dSDLastRxRc=-999;
static void f23dSDPacket(const char* stage,const VesselPacket& p,int rc=0,uint8_t attempt=0) {
 F23DSD::Record row{};if(!F23DSD::describe(row,stage)){thesisF23dSDFormatLoss();return;}
 F23DSD::packet(row,p);row.ms=millis();row.rc=rc;row.rx_rc=f23dSDLastRxRc;
 row.used=attempt;row.called=!strcmp(stage,"TX_RESULT") && f23dSDRadioCalled;
 (void)thesisF23dSDCapture(row);
}
#endif
// F23D SD end
// F23D physical begin
#include "../../shared/relay/f23d_physical_contract.h"
// F23D physical end
// F23D readiness begin
#include "../../shared/relay/f23d_readiness_contract.h"
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
#include "../../shared/relay/f23d_lab_diagnostics.h"
#include "../../shared/relay/f23d_evidence_queue.h"
static F23DReadiness::EvidenceQueue f23dEvidenceQueue;
static uint32_t f23dMaxWriteUs=0,f23dMaxEnqueueUs=0,f23dReportedLost=0;
#endif
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE == 1
#if THESIS_F23D_RELAY_EVIDENCE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
#include "../../shared/relay/f23d_run_evidence.h"
static F23DReadiness::RunEvidence f23dRunEvidence;
static int f23dLastRxRestoreRc=-999;
static uint32_t f23dLastRxReadyAt=0;
static void f23dQueueRunEvidence(const char*,int,const VesselPacket*);
#endif
// F23D readiness end
// F23D begin
#include "../../shared/relay/f23d_bench_contract.h"
// F23D end
// F23C Phase 1 begin
#include "../../shared/relay/f23c_lab_contract.h"
// F23C Phase 1 end
#include "../../shared/hardware/tbeam_pins.h"
#include "../../shared/hardware/vessel_identity.h"
#include "../../shared/relay/relay_policy.h"
#include "../../shared/relay/relay_queue.h"
#include "../../shared/relay/relay_dryrun_scheduler.h"
#include "../../shared/relay/relay_preflight.h"
#include "../../shared/relay/relay_receipt_state.h"
#include "../../shared/relay/relay_tx_readiness.h"
#include "../../shared/relay/relay_authorization_contract.h"
#include "../../shared/relay/lab_relay_gate.h"
#include "lora_manager.h"
#include "../../shared/communication/control_retry_schedule.h"
#include "weather_manager.h"
#include "../../shared/communication/compact_position_v2.h"
#ifndef THESIS_MULTIHOP_DEV_TEST
#define THESIS_MULTIHOP_DEV_TEST 0
#endif
#ifndef THESIS_MULTIHOP_BENCH_RF
#define THESIS_MULTIHOP_BENCH_RF 0
#endif
#if THESIS_MULTIHOP_BENCH_RF != 0 && THESIS_MULTIHOP_BENCH_RF != 1
#error "Invalid supervised bench RF flag"
#endif
#if THESIS_MULTIHOP_BENCH_RF == 1
#if THESIS_MULTIHOP_DEV_TEST != 1 || THESIS_F22_FLEET_OPERATIONAL != 1 || THESIS_V003_F19_DUAL_DISPLAY != 1 || THESIS_LAB_RELAY_TX != 0 || THESIS_F23D_CANDIDATE != 0 || THESIS_F23D_PHYSICAL_CANDIDATE != 0
#error "Supervised bench RF requires F22+DEV display; excludes F23D candidates and legacy lab relay"
#endif
#include "../../shared/relay/multihop_supervised_bench.h"
#endif
#if THESIS_MULTIHOP_DEV_TEST == 1
#include "../../shared/relay/compact_position_relay.h"
#include "../../shared/relay/compact_position_hop1.h"
#include "../../shared/relay/multihop_priority.h"
#include "../../shared/relay/multihop_dev_test.h"
#if THESIS_F22_FLEET_OPERATIONAL != 1 || THESIS_LAB_RELAY_TX != 0 || THESIS_F23D_PHYSICAL_CANDIDATE != 0
#error "DEV relay observation requires F22 and all physical relay transmission OFF"
#endif

#endif
#include "../../shared/communication/receive_format_guard.h"
#include "../../shared/communication/device_verification_contract.h"
#include "../../shared/communication/device_shadow_receiver.h"
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
#include "../../shared/communication/multi_peer_telemetry.h"
#include "../../shared/communication/multi_peer_sos.h"
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
#include "../../shared/storage/f05_sos_recovery.h"
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
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
static bool f23dFinalRadioGate(const uint8_t*,size_t);
#endif
// F23D physical end
static int transmitWithIdentity(const uint8_t* data, size_t length) {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  f23dSDRadioCalled=false;
#endif
// F23D SD end
  if (!VesselIdentity::ready()) {
    Serial.println("[IDENTITY] TX BLOCKED: provisioned identity required");
    return -999;
  }
  interruptMacCadForControlTx();
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
  if(!f23dFinalRadioGate(data,length))return -997;
#endif
// F23D physical end
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  f23dSDRadioCalled=true;
#endif
// F23D SD end
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
  out.session = chosen->session;
  out.received = chosen->received;
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
  out.session=x->session;out.received=x->received;
  out.missing=x->missingEstimate();out.pdr=x->pdrEstimate();
  out.ageSeconds=uint32_t(now-x->lastSeenMs)/1000UL;
  return true;
}
// Read-only bounded snapshot enumeration for research; never mutates PDR.
// IDs are bounded by the existing protocol (1..9999), max 10 tracked peers.
bool getPeerSnapshotByIndex(size_t index, PeerDisplaySnapshot &out) {
  out=PeerDisplaySnapshot{};
  if(index>=getTrackedPeerCount()) return false;
  size_t seen=0;
  for(uint32_t id=1;id<=9999;++id) {
    if (!peerTelemetry.find(id)) continue;
    if (seen++==index) return getPeerSnapshotById(id,out);
  }
  return false;
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
// F23C Phase 1 begin
#if THESIS_F23C_BUILD_ONLY == 1
#include "../../shared/relay/f23c_lab_transaction.h"
static F23CLab::Transaction f23cTransaction; // never armed by firmware
static const F23CLab::DisabledRadio f23cRadio;
#endif
// F23C Phase 1 end
// F23D begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_CANDIDATE == 1
#if THESIS_F23D_CANDIDATE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
#include "../../shared/relay/f23d_run_control.h"
static F23DBench::RunControl f23dControl;
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
#include "../../shared/relay/f23d_physical_permission.h"
// Actual firmware provider. Missing platform evidence always denies; no GPIO,
// MAC, credential, serial command or NVS value can manufacture a grant.
static F23DPhysical::Permission f23dPermission;
static const VesselPacket* f23dPhysicalLease=nullptr;
#endif
// F23D physical end
static F23DBench::Context f23dContext() {
  F23DBench::Context c;
  c.identityReady=VesselIdentity::ready();
  c.self=NODE_ID;
  c.physicalPermission=F23DBench::kPhysicalPermissionInstalled;
  c.recoverySafe=nvsReady && !F05Recovery::storageBlocked &&
                 !F05Recovery::recoveredCancelNeedsHold;
  c.localControlBusy=activeEmergency || sosPending || cancelPending ||
                    macCadScanActive || digitalRead(LORA_DIO1)==HIGH;
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
  c.physicalPermission=f23dPermission.evaluate(c,millis());
#endif
// F23D physical end
  return c;
}
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D final gate begin
static bool f23dFinalRadioGate(const uint8_t* bytes,size_t length) {
  if(!f23dPhysicalLease)return true;
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  // Cached RAM only: missing/unsealed capture, loss or withdrawal vetoes relay.
  if(!thesisF23dSDHealthy()){
    f23dPermission.revoke(F23DPhysical::Reason::EvidenceLost);f23dControl.revoke();return false;
  }
#endif
// F23D SD end
  const auto c=f23dContext();
  const bool allowed=length==sizeof(VesselPacket) &&
      bytes==reinterpret_cast<const uint8_t*>(f23dPhysicalLease) &&
      f23dControl.maySubmit(*f23dPhysicalLease,c) &&
      f23dPermission.beforeRadio(c,millis(),f23dEvidenceQueue.lost());
  // No diagnostic formatting or serial I/O between this check and radio call.
  if(!allowed){f23dPermission.revoke();f23dControl.revoke();}
  return allowed;
}
// F23D final gate end
#endif
// F23D physical end
// F23D adapter body begin
struct F23DBenchRadio {
  int submit(const VesselPacket& packet) {
    const auto context=f23dContext();
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE == 1
#if THESIS_F23D_RELAY_EVIDENCE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
    f23dRunEvidence.reserved(packet.packetType);
    f23dQueueRunEvidence("RESERVED",int(f23dControl.attempts()),&packet);
    if(!f23dControl.maySubmit(packet,context))
      f23dQueueRunEvidence("ADAPTER_REJECTED",-998,&packet);
#endif
// F23D readiness end
    if (!f23dControl.maySubmit(packet, context)) return -998;
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE == 1
#if THESIS_F23D_RELAY_EVIDENCE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
    f23dQueueRunEvidence("RADIO_INVOKE",0,&packet);
#endif
// F23D readiness end
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
    f23dPhysicalLease=&packet;
#endif
// F23D physical end
    const int rc=transmitWithIdentity(
        reinterpret_cast<const uint8_t*>(&packet),sizeof(packet));
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
    f23dPhysicalLease=nullptr;
    if(rc!=RADIOLIB_ERR_NONE){f23dPermission.revoke();f23dControl.revoke();}
#endif
// F23D physical end
    startReceive();
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
    f23dQueueRunEvidence(rc==-997?"AUTH_FINAL_REJECTED":"AUTH_RADIO_RETURN",rc,&packet);
#endif
// F23D physical end
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE == 1
#if THESIS_F23D_RELAY_EVIDENCE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
    if(f23dLastRxRestoreRc!=RADIOLIB_ERR_NONE)f23dControl.revoke();
    f23dQueueRunEvidence("RADIO_RESULT",rc,&packet);
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
    if(f23dLastRxRestoreRc!=RADIOLIB_ERR_NONE ||
       !f23dPermission.evaluate(f23dContext(),millis())){
      f23dPermission.revoke();f23dControl.revoke();
      f23dQueueRunEvidence("AUTH_REVOKED",int(f23dPermission.reason()),&packet);
    }
#endif
// F23D physical end
#endif
// F23D readiness end
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE != 1
#if THESIS_F23D_RELAY_EVIDENCE != 1 && THESIS_F23D_PHYSICAL_CANDIDATE != 1
// F23D physical end
// F23D readiness end
    Serial.printf("[F23D-TX] ms=%lu origin=%lu event=%lu kind=%u hop=%u attempt=%u rc=%d AUTH=UNVERIFIED FAR_END=UNVERIFIED\n",
        (unsigned long)millis(),(unsigned long)packet.originNodeId,
        (unsigned long)packet.sequenceNumber,unsigned(packet.packetType),
        unsigned(packet.hopCount),unsigned(f23dControl.attempts()),rc);
// F23D readiness begin
#endif
// F23D readiness end
    return rc;
  }
};
// F23D adapter body end
static F23DBenchRadio f23dRadio;
bool f23dHandleRunCommand(const char* line) {
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
  const uint32_t commandAt=millis();
  const auto result=f23dControl.command(line,f23dContext(),commandAt,esp_random() | 1U);
  if(result==F23DBench::Control::Selected){
    uint32_t run=0;const char* p=line+12;
    while(*p>='0' && *p<='9'){run=run*10U+uint32_t(*p-'0');++p;}
    f23dPermission.selected(run,f23dControl.event(),commandAt);
  }
  if(result==F23DBench::Control::Armed)f23dPermission.armed(commandAt);
  if(result==F23DBench::Control::Disarmed || result==F23DBench::Control::Denied)
    f23dPermission.revoke();
  if(result!=F23DBench::Control::NotCommand)
    f23dQueueRunEvidence("AUTH_COMMAND",int(f23dPermission.reason()),nullptr);
#else
// F23D physical end
  const auto result=f23dControl.command(line,f23dContext(),millis(),esp_random() | 1U);
// F23D physical begin
#endif
// F23D physical end
  if (result==F23DBench::Control::NotCommand) return false;
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE == 1
#if THESIS_F23D_RELAY_EVIDENCE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
  f23dRunEvidence.command(line,result,millis());
  f23dQueueRunEvidence(F23DReadiness::RunEvidence::controlName(result),int(result),nullptr);
#endif
// F23D readiness end
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE != 1
#if THESIS_F23D_RELAY_EVIDENCE != 1 && THESIS_F23D_PHYSICAL_CANDIDATE != 1
// F23D physical end
// F23D readiness end
  Serial.printf("[F23D-RUN] result=%u event=%lu challenge=%lu armed=%u attempts=%u PHYSICAL_PERMISSION=UNAVAILABLE AUTH=UNVERIFIED\n",
      unsigned(result),(unsigned long)f23dControl.event(),
      (unsigned long)f23dControl.challenge(),unsigned(f23dControl.armed()),
      unsigned(f23dControl.attempts()));
// F23D readiness begin
#endif
// F23D readiness end
  return true;
}
#endif
// F23D end
#if THESIS_MULTIHOP_DEV_TEST == 1
static CompactPositionRelay::Queue multihopDevQueue;
#if THESIS_MULTIHOP_BENCH_RF == 1
static MultihopBench::Session multihopBenchSession;
static int multihopBenchLastRxRc=RADIOLIB_ERR_NONE;
static int multihopBenchTransmit(const VesselPacket&);
#else
static MultihopDevTest::Control multihopDevControl;
#endif
bool multihopDevRequested() {
#if THESIS_MULTIHOP_BENCH_RF == 1
  return multihopBenchSession.preparing(millis());
#else
  return multihopDevControl.requested();
#endif
}
const char* multihopDevStatus() {
#if THESIS_MULTIHOP_BENCH_RF == 1
  return multihopBenchSession.label(millis());
#else
  return multihopDevControl.requested()?"PREP":"OFF";
#endif
}
bool multihopDevToggle(){
  const bool busy=hasActiveLocalSOS() || getActiveSOSCount()>0 ||
                  sosPending || cancelPending;
#if THESIS_MULTIHOP_BENCH_RF == 1
  const bool changed=multihopBenchSession.press(NODE_ID,VesselIdentity::ready(),busy,millis());
#else
  const bool changed=multihopDevControl.toggle(NODE_ID,VesselIdentity::ready(),busy);
#endif
  if(changed)multihopDevQueue=CompactPositionRelay::Queue{};
  return changed;
}
#if THESIS_MULTIHOP_BENCH_RF == 1
static bool multihopRadioBusy() {
  if(macCadScanActive || digitalRead(LORA_DIO1)==HIGH)return true;
  const uint16_t irq=radio.getIrqStatus();
  return (irq & (RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED |
                 RADIOLIB_SX126X_IRQ_HEADER_VALID))!=0;
}
static int multihopBenchTransmit(const VesselPacket& p) {
  if(p.magic!=PACKET_MAGIC || p.version!=PACKET_VERSION ||
     p.originNodeId!=1U || p.hopCount!=1U ||
     (p.packetType!=PACKET_TELEMETRY && !ThesisLabRelay::permits(p,NODE_ID)))
    return -995;
  const bool localBusy=activeEmergency || sosPending || cancelPending;
  const uint32_t started=millis();
  if(!multihopBenchSession.reserve(started,NODE_ID,VesselIdentity::ready(),
                                     localBusy,multihopRadioBusy()))
    return -996;
  // This is the *only* bench radio submission: no formatting or SD I/O
  // may come between the final operator/identity/time check and TX.
  const int rc=transmitWithIdentity(reinterpret_cast<const uint8_t*>(&p),sizeof(p));
  startReceive();
  const uint32_t ms=uint32_t(millis()-started);
  const bool rxOk=multihopBenchLastRxRc==RADIOLIB_ERR_NONE;
  const bool keep=multihopBenchSession.completed(ms,rc==RADIOLIB_ERR_NONE,rxOk);
  SD_EVENT(p.packetType!=PACKET_TELEMETRY,"multihop_bench_tx",
     p.originNodeId,p.sequenceNumber,rc,0,0,
     p.latitude_e7,p.longitude_e7,p.flags,p.hopCount);
  SD_EVENT(p.packetType!=PACKET_TELEMETRY,"multihop_bench_tx_to_rx_ms",
     p.originNodeId,p.sequenceNumber,(int32_t)ms,0,0,
     p.latitude_e7,p.longitude_e7,p.flags,p.hopCount);
  Serial.printf("[MULTIHOP-BENCH] ms=%lu origin=%lu seq=%lu type=%u hop=%u tx_rc=%d tx_to_rx_ms=%lu rx_rc=%d still_live=%u far_end=UNVERIFIED\n",
      (unsigned long)millis(),(unsigned long)p.originNodeId,
      (unsigned long)p.sequenceNumber,unsigned(p.packetType),
      unsigned(p.hopCount),rc,(unsigned long)ms,multihopBenchLastRxRc,unsigned(keep));
  return rc;
}
#endif
void multihopDevPollPrepared(){
  if(!multihopDevRequested())return;
  // Prepared SOS/CANCEL preempt routine traffic. A peer SOS remaining
  // ACTIVE after its relay queue drains must not starve routine telemetry.
  if(!MultihopPriority::mayDrainRoutine(activeEmergency,sosPending,
      cancelPending,relayDryRun.size()>0,macCadScanActive,
      digitalRead(LORA_DIO1)==HIGH))return;
  CompactPositionRelay::Frame f{};
  if(!multihopDevQueue.take(f,false))return;
  VesselPacket p{};
  if(!CompactPositionRelay::toHopOneTelemetry(f,NODE_ID,p)){
    Serial.println("[MULTIHOP-DEV] DROP=INVALID_CONVERSION TX=OFF");return;
  }
#if THESIS_MULTIHOP_BENCH_RF == 1
  if(multihopBenchSession.live(millis())) {
    const int rc=multihopBenchTransmit(p);
    if(rc<0)Serial.printf("[MULTIHOP-BENCH] POSITION_UNSENT seq=%lu rc=%d\n",
                          (unsigned long)p.sequenceNumber,rc);
    return;
  }
#endif
  Serial.printf("[MULTIHOP-DEV] PREPARED origin=%lu relay=2 seq=%lu hop=%u bytes=%u TX=OFF\n",
    (unsigned long)p.originNodeId,(unsigned long)p.sequenceNumber,
    unsigned(p.hopCount),unsigned(sizeof(p)));
  SD_EVENT(false,"relay_position_preview",p.originNodeId,p.sequenceNumber,
    0,0,0,p.latitude_e7,p.longitude_e7,p.flags,p.hopCount);
}
#endif
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

// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
void f23dPollEvidence() {
  struct Writer {
    int availableForWrite(){return Serial.availableForWrite();}
    size_t write(const uint8_t* data,size_t length){return Serial.write(data,length);}
  } writer;
  const uint32_t began=micros();
  (void)f23dEvidenceQueue.drain(writer);
  const uint32_t elapsed=micros()-began;
  if(elapsed>f23dMaxWriteUs)f23dMaxWriteUs=elapsed;
  if(f23dEvidenceQueue.lost()!=f23dReportedLost &&
      f23dEvidenceQueue.size()<F23DReadiness::EvidenceQueue::kDepth) {
    char row[128];
    snprintf(row,sizeof(row),"[F23D-EVIDENCE-LOSS] lost=%lu peak=%u write_max_us=%lu RUN_EVIDENCE_INVALID=1",
        (unsigned long)f23dEvidenceQueue.lost(),unsigned(f23dEvidenceQueue.peak()),
        (unsigned long)f23dMaxWriteUs);
    if(f23dEvidenceQueue.push(row))f23dReportedLost=f23dEvidenceQueue.lost();
  }
}
#endif
#if THESIS_F23D_EVENT_QUERY == 1
// F23D query collector begin
static F23DReadiness::EventSnapshot f23dCollectEventSnapshot() {
  F23DReadiness::EventSnapshot s;
  s.self=VesselIdentity::nodeId();s.provisioned=VesselIdentity::ready();
  s.next=nextEmergencyId;s.counterAvailable=nvsReady;
  s.active=activeEmergency;s.pending=sosPending;s.cancelPending=cancelPending;
  s.storageSafe=nvsReady && !F05Recovery::storageBlocked;
  s.recoveryHold=F05Recovery::recoveredCancelNeedsHold;
  return s;
}
// F23D query collector end
#endif
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
// F23D query command begin
bool f23dHandleDiagnosticCommand(const char* line) {
#if THESIS_F23D_EVENT_QUERY == 1
  if(!strcmp(line,"R23D NEXT")) {
    const auto s=f23dCollectEventSnapshot();
    char row[F23DReadiness::EvidenceQueue::kRow];
    if(F23DReadiness::eventRow(row,sizeof(row),s,millis()))f23dEvidenceQueue.push(row);
    return true;
  }
#endif
  if(!strcmp(line,"R23D EVIDENCE")) {
    char row[160];
    snprintf(row,sizeof(row),"[F23D-EVIDENCE-STATS] queued=%u peak=%u lost=%lu write_max_us=%lu enqueue_max_us=%lu PHYSICAL_MEASUREMENT_REQUIRED=1",
        unsigned(f23dEvidenceQueue.size()),unsigned(f23dEvidenceQueue.peak()),
        (unsigned long)f23dEvidenceQueue.lost(),(unsigned long)f23dMaxWriteUs,
        (unsigned long)f23dMaxEnqueueUs);
    f23dEvidenceQueue.push(row);return true;
  }
  return false;
}
// F23D query command end
#endif
// F23D SD begin
// F23D SD restore: #if THESIS_F23D_RX_OBSERVER == 1
#if THESIS_F23D_RX_OBSERVER == 1 || THESIS_F23D_SD_ROLE == 2
// F23D SD end
static void f23dQueueRxEvidence(const uint8_t* bytes,size_t length,int16_t rssi,
                               float snr,const char* validation,const char* outcome) {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  {
  if(!VesselIdentity::ready() || NODE_ID!=THESIS_F23D_SD_ROLE)return;
  F23DSD::Record row{};
  if(!F23DSD::describe(row,"RX",validation,outcome)){thesisF23dSDFormatLoss();return;}
  row.ms=millis();row.rssi=rssi;row.snr_x10=snr>=-100.0f && snr<=100.0f?int16_t(snr*10.0f):32767;
  row.bytes=length<=99?uint16_t(length):0;
  if(length==sizeof(VesselPacket)){VesselPacket p{};memcpy(&p,bytes,sizeof(p));F23DSD::packet(row,p);}
  else if(length>=4 && length<=99)row.type=bytes[3];
#if THESIS_F23D_SD_ROLE == 2
  row.run=f23dRunEvidence.run();
#endif
  (void)thesisF23dSDCapture(row);return;
  }
#endif
// F23D SD end
  if(!VesselIdentity::ready() || NODE_ID!=3U)return;
  const uint32_t began=micros();
  const int snrX10=snr>=-100.0f && snr<=100.0f?int(snr*10.0f):32767;
  char row[F23DReadiness::EvidenceQueue::kRow];
  if(F23DReadiness::rxRow(row,sizeof(row),NODE_ID,millis(),bytes,length,
                         rssi,snrX10,validation,outcome))f23dEvidenceQueue.push(row);
  const uint32_t elapsed=micros()-began;
  if(elapsed>f23dMaxEnqueueUs)f23dMaxEnqueueUs=elapsed;
}
#endif
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE == 1
#if THESIS_F23D_RELAY_EVIDENCE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
// F23D run row begin
static void f23dQueueRunEvidence(const char* stage,int code,const VesselPacket* packet) {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  {
  F23DSD::Record row{};
  if(!F23DSD::describe(row,stage)){thesisF23dSDFormatLoss();return;}
  const auto context=f23dContext();
  row.ms=millis();row.run=f23dRunEvidence.run();row.event=f23dControl.event();row.rc=code;
  row.permit=context.physicalPermission;row.used=f23dControl.attempts();
  row.sos_left=f23dRunEvidence.sosRemaining();row.cancel_left=f23dRunEvidence.cancelRemaining();
  row.rx_rc=f23dLastRxRestoreRc;row.called=!strcmp(stage,"RADIO_RESULT") && f23dSDRadioCalled;
  if(packet)F23DSD::packet(row,*packet);
  (void)thesisF23dSDCapture(row);
  if(!packet && (!strcmp(stage,"SELECTED") || !strcmp(stage,"ARMED") ||
      !strcmp(stage,"STATUS") || !strcmp(stage,"DENIED") || !strcmp(stage,"LOCKED") ||
      !strcmp(stage,"DISARMED") || !strcmp(stage,"INVALID"))){
    char reply[192];
    const int n=snprintf(reply,sizeof(reply),
      "[F23D-CONTROL] result=%s run=%lu event=%lu challenge=%lu armed=%u permit=%u",
      stage,(unsigned long)f23dRunEvidence.run(),(unsigned long)f23dControl.event(),
      (unsigned long)f23dControl.challenge(),unsigned(f23dControl.armed()),unsigned(context.physicalPermission));
    if(n>0 && size_t(n)<sizeof(reply))f23dEvidenceQueue.push(reply);
  }
  return;
  }
#endif
// F23D SD end
  const uint32_t began=micros();
  const auto c=f23dContext();
  const uint32_t now=millis();
  char row[F23DReadiness::EvidenceQueue::kRow];
  const int n=snprintf(row,sizeof(row),
      "[F23D-RUNOBS] local=%lu ms=%lu run=%lu event=%lu stage=%s code=%d armed=%u permit=%u select_due=%lu run_due=%lu left_ms=%lu used=%u sos_left=%u cancel_left=%u kind=%u hop=%u origin=%lu packet_event=%lu canonical_fnv=%08lX rx_rc=%d rx_ready_ms=%lu challenge=%lu FAR_END=UNVERIFIED",
      (unsigned long)c.self,(unsigned long)now,(unsigned long)f23dRunEvidence.run(),
      (unsigned long)f23dControl.event(),stage,code,unsigned(f23dControl.armed()),
      unsigned(c.physicalPermission),(unsigned long)f23dRunEvidence.selectDeadline(),
      (unsigned long)f23dRunEvidence.deadline(),
      (unsigned long)(f23dControl.armed()?f23dRunEvidence.remaining(now):0),
      unsigned(f23dControl.attempts()),f23dRunEvidence.sosRemaining(),
      f23dRunEvidence.cancelRemaining(),packet?unsigned(packet->packetType):0U,
      packet?unsigned(packet->hopCount):0U,
      packet?(unsigned long)packet->originNodeId:0UL,
      packet?(unsigned long)packet->sequenceNumber:0UL,
      packet?(unsigned long)F23DReadiness::fingerprint(reinterpret_cast<const uint8_t*>(packet),sizeof(*packet),true):0UL,
      f23dLastRxRestoreRc,
      (unsigned long)f23dLastRxReadyAt,(unsigned long)f23dControl.challenge());
  if(n>=0 && size_t(n)<sizeof(row))f23dEvidenceQueue.push(row);
  else f23dEvidenceQueue.push("[F23D-EVIDENCE-LOSS] FORMAT_OVERFLOW RUN_EVIDENCE_INVALID=1");
  const uint32_t elapsed=micros()-began;
  if(elapsed>f23dMaxEnqueueUs)f23dMaxEnqueueUs=elapsed;
}
// F23D run row end
#endif
// F23D readiness end
// R2-B1: only authoritative NEW SOS or CANCELLED events may enter the
// preparation queue. Invalid, stale, duplicate and unknown events cannot
// consume the relay duplicate cache. No radio transmission happens here.
static void prepareRelayAfterSOSState(const VesselPacket& p,
                                      bool stateAccepted, const char* stateLabel) {
// F23C Phase 1 begin
#if THESIS_F23C_BUILD_ONLY == 1
  const auto modelAdmission = f23cTransaction.offer(
      p, VesselIdentity::ready(), NODE_ID, stateAccepted, millis());
  Serial.printf("[F23C-MODEL] admission=%u REAL_RELAY_TX=OFF ARMED=0\n",
                unsigned(modelAdmission));
#endif
// F23C Phase 1 end
// F23D begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_CANDIDATE == 1
#if THESIS_F23D_CANDIDATE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
  const auto f23dAdmission=f23dControl.observe(p,f23dContext(),stateAccepted,millis());
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE == 1
#if THESIS_F23D_RELAY_EVIDENCE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
  if(f23dAdmission==F23CLab::Admission::Accepted && p.packetType==PACKET_SOS_CANCEL)
    f23dRunEvidence.cancellationAccepted();
  f23dQueueRunEvidence(F23DReadiness::RunEvidence::admissionName(f23dAdmission),int(f23dAdmission),&p);
#endif
// F23D readiness end
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE != 1
#if THESIS_F23D_RELAY_EVIDENCE != 1 && THESIS_F23D_PHYSICAL_CANDIDATE != 1
// F23D physical end
// F23D readiness end
  Serial.printf("[F23D-RX] ms=%lu origin=%lu event=%lu kind=%u hop=%u admission=%u PHYSICAL_TX=OFF\n",
      (unsigned long)millis(),(unsigned long)p.originNodeId,
      (unsigned long)p.sequenceNumber,unsigned(p.packetType),
      unsigned(p.hopCount),unsigned(f23dAdmission));
// F23D readiness begin
#endif
// F23D readiness end
#endif
// F23D end
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
#if THESIS_MULTIHOP_BENCH_RF == 1
  if(!activeEmergency && !sosPending && !cancelPending &&
     relayDryRun.poll(millis(),prepared)) {
#else
  if (relayDryRun.poll(millis(), prepared)) {
#endif
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
#elif THESIS_MULTIHOP_BENCH_RF == 1
      if(multihopBenchSession.live(millis()) &&
         ThesisLabRelay::permits(prepared,NODE_ID)) {
        const int rc=multihopBenchTransmit(prepared);
        if(rc<0)Serial.printf("[MULTIHOP-BENCH] SOS_CANCEL_UNSENT event=%lu rc=%d\n",
                                (unsigned long)prepared.sequenceNumber,rc);
      } else {
        Serial.printf("[MULTIHOP-BENCH] SOS_CANCEL_PREVIEW event=%lu type=%u TX=OFF\n",
            (unsigned long)prepared.sequenceNumber,unsigned(prepared.packetType));
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
// F23C Phase 1 begin
#if THESIS_F23C_BUILD_ONLY == 1
  VesselPacket modelPrepared{};
  if (f23cTransaction.poll(millis(), VesselIdentity::ready(), NODE_ID,
                          activeEmergency || sosPending || cancelPending,
                          modelPrepared) == F23CLab::Output::Prepared) {
    // DisabledRadio cannot transmit, even if a future caller arms the model.
    (void)f23cRadio.submit(modelPrepared);
  }
#endif
// F23C Phase 1 end
// F23D begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_CANDIDATE == 1
#if THESIS_F23D_CANDIDATE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
  int f23dResult=0;
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE == 1
#if THESIS_F23D_RELAY_EVIDENCE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
  const bool f23dWasArmed=f23dControl.armed();
  if(f23dWasArmed && f23dContext().localControlBusy && f23dRunEvidence.deferralDue(millis()))
    f23dQueueRunEvidence("LOCAL_CONTROL_DEFERRED",0,nullptr);
#endif
// F23D readiness end
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
  if(f23dEvidenceQueue.lost()){
    f23dPermission.revoke(F23DPhysical::Reason::EvidenceLost);f23dControl.revoke();
  }
#endif
// F23D physical end
  (void)f23dControl.dispatch(f23dContext(),millis(),f23dRadio,f23dResult);
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE == 1
#if THESIS_F23D_RELAY_EVIDENCE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE == 1
  if(f23dWasArmed && !f23dControl.armed())
    f23dQueueRunEvidence("AUTH_REVOKED",int(f23dPermission.reason()),nullptr);
#endif
// F23D physical end
  if(f23dWasArmed && !f23dControl.armed())
    f23dQueueRunEvidence(f23dRunEvidence.remaining(millis())?"REVOKED":"EXPIRED",0,nullptr);
#endif
// F23D readiness end
#endif
// F23D end
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
#if THESIS_MULTIHOP_BENCH_RF == 1
  multihopBenchLastRxRc=state;
#endif
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  f23dSDLastRxRc=state;
#endif
// F23D SD end
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_RELAY_EVIDENCE == 1
#if THESIS_F23D_RELAY_EVIDENCE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
  f23dLastRxRestoreRc=state;f23dLastRxReadyAt=millis();
#endif
// F23D readiness end
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
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  f23dSDPacket("TX_BEGIN",pendingSOS,0,sosAttempts);
#endif
// F23D SD end
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
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  f23dSDPacket("TX_RESULT",pendingSOS,state,sosAttempts);
#endif
// F23D SD end
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
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  F23DSD::Record row{};F23DSD::describe(row,"ALLOCATED");row.ms=millis();
  row.origin=NODE_ID;row.event=selectedId;row.type=PACKET_SOS;
  (void)thesisF23dSDCapture(row);
#endif
// F23D SD end
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
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  f23dSDPacket("LOCAL_SOS",pendingSOS);
#endif
// F23D SD end
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
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  f23dSDPacket("TX_BEGIN",pendingCancel,0,cancelAttempts);
#endif
// F23D SD end
  const int state = transmitWithIdentity(reinterpret_cast<uint8_t*>(&pendingCancel), sizeof(pendingCancel));
  startReceive();
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  f23dSDPacket("TX_RESULT",pendingCancel,state,cancelAttempts);
#endif
// F23D SD end
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
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  f23dSDPacket("LOCAL_CANCEL",pendingCancel);
#endif
// F23D SD end
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
// F23D readiness begin
// F23D SD begin
// F23D SD restore: #if THESIS_F23D_RX_OBSERVER == 1
#if THESIS_F23D_RX_OBSERVER == 1 || THESIS_F23D_SD_ROLE == 2
// F23D SD end
    f23dQueueRxEvidence(buffer,length,rssi,snr,"BAD_FORMAT","REJECTED");
#endif
// F23D readiness end
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
#if THESIS_MULTIHOP_DEV_TEST == 1
    // V002-only routine route preparation, after the existing format gate.
    // Must use ORIGINAL 25-byte bytes before conversion to a 62-byte local view.
    if (multihopDevRequested() && VesselIdentity::ready() && NODE_ID==2U) {
      const auto offered=multihopDevQueue.offer(buffer,length,NODE_ID);
      Serial.printf("[MULTIHOP-DEV] OFFER origin=%lu seq=%lu result=%s queued=%u TX=NOT_AT_ADMISSION\n",
                    (unsigned long)pos.originId,(unsigned long)pos.sequence,
                    CompactPositionRelay::name(offered),unsigned(multihopDevQueue.pending()));
    }
#endif
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
// F23D readiness begin
// F23D SD begin
// F23D SD restore: #if THESIS_F23D_RX_OBSERVER == 1
#if THESIS_F23D_RX_OBSERVER == 1 || THESIS_F23D_SD_ROLE == 2
// F23D SD end
  if(p.originNodeId==NODE_ID)
    f23dQueueRxEvidence(buffer,length,rssi,snr,"SELF_ORIGIN","REJECTED");
  else if(p.originNodeId<1 || p.originNodeId>9999)
    f23dQueueRxEvidence(buffer,length,rssi,snr,"BAD_ORIGIN","REJECTED");
#endif
// F23D readiness end
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
// F23D readiness begin
// F23D SD begin
// F23D SD restore: #if THESIS_F23D_RX_OBSERVER == 1
#if THESIS_F23D_RX_OBSERVER == 1 || THESIS_F23D_SD_ROLE == 2
// F23D SD end
      f23dQueueRxEvidence(buffer,length,rssi,snr,"BAD_CANCEL","REJECTED");
#endif
// F23D readiness end
      Serial.println("[PEER-SOS] CANCEL_MISMATCH payload; NO ACK"); prepareRelayAfterSOSState(p, false, "CANCEL_PAYLOAD"); return;
    }
    const auto result=peerSOSTable.cancel(p.originNodeId,cancelId,millis());
// F23D readiness begin
// F23D SD begin
// F23D SD restore: #if THESIS_F23D_RX_OBSERVER == 1
#if THESIS_F23D_RX_OBSERVER == 1 || THESIS_F23D_SD_ROLE == 2
// F23D SD end
    f23dQueueRxEvidence(buffer,length,rssi,snr,"LEGACY_UNAUTHENTICATED",MultiPeerSOS::name(result));
#endif
// F23D readiness end
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
// F23D readiness begin
// F23D SD begin
// F23D SD restore: #if THESIS_F23D_RX_OBSERVER == 1
#if THESIS_F23D_RX_OBSERVER == 1 || THESIS_F23D_SD_ROLE == 2
// F23D SD end
    f23dQueueRxEvidence(buffer,length,rssi,snr,"LEGACY_UNAUTHENTICATED",MultiPeerSOS::name(result));
#endif
// F23D readiness end
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

// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
bool f23dSDRadioBusy() {
  return activeEmergency || sosPending || cancelPending || macCadScanActive ||
#if THESIS_F23D_CANDIDATE == 1 || THESIS_F23D_PHYSICAL_CANDIDATE == 1
    f23dControl.armed() ||
#endif
    digitalRead(LORA_DIO1)==HIGH;
}
#endif
// F23D SD end
float bSequencePDR() {
  if (!haveBSequence) return 0.0f;
  uint64_t expected = static_cast<uint64_t>(highestBSequence) - firstBSequence + 1ULL;
  if (expected == 0) return 0.0f;
  return 100.0f * static_cast<float>(uniqueFromB) / static_cast<float>(expected);
}