#pragma once
#include <Arduino.h>
#include "packet_protocol.h"

// Board A communications and last received Vessel-B state.
extern uint32_t txSequence, txCount, txFailures;
extern bool lastTXSuccess;
extern uint32_t rxFromB, uniqueFromB, duplicateFromB, missingFromB;
extern uint32_t invalidPackets;
extern bool bGPSValid;
extern double bLatitude, bLongitude, distanceToBkm;
extern int16_t lastBRSSI;
extern float lastBSNR;
extern unsigned long lastBSeen;
extern uint8_t bWeatherState;

// Read-only OLED snapshot of the most recent active telemetry peer.
struct PeerDisplaySnapshot {
  bool present = false, active = false, gpsValid = false;
  uint32_t id = 0, unique = 0, duplicates = 0, missing = 0, ageSeconds = 0;
  double latitude = 0, longitude = 0;
  int16_t rssi = 0;
  float snr = 0, pdr = 0;
};
bool getLivePeerSnapshot(PeerDisplaySnapshot &out);
bool getPeerSnapshotById(uint32_t id, PeerDisplaySnapshot &out);
bool nextActivePeerId(uint32_t current, uint32_t &next);
size_t getTrackedPeerCount();
uint32_t getTotalPeerTelemetryReceived();
// Phase 3G-4E1: number of *recently heard* distinct peers, not 15s OLED state.
// Read-only; cannot create or evict peer table entries.
uint32_t getRecentMacPeerCount(uint32_t nowMs, uint32_t horizonMs);

void setupLoRa();
void startReceive();
void pollLoRaReceive();
// PHASE 5.2.2C-3G-4E2: CAD IRQ must not enter RX readData().
enum class TelemetryCadResult : uint8_t {
  None, Scanning, Free, Busy, RxActive, Error, Timeout, Interrupted
};
bool telemetryCadRunning();
TelemetryCadResult startTelemetryCad();
TelemetryCadResult pollTelemetryCad();
void createPacket(VesselPacket &packet, bool consumeTelemetrySequence = true);
void transmitPacket();
void setupEmergencyIdentity();
bool hasActiveLocalSOS();
void cancelActiveSOS();
void transmitSOS(); // Starts bounded SOS send-and-ACK session
void processSOSRetries();
void processCancelRetries();
float bSequencePDR();
// A packet received in the last 15 seconds means the neighbor is active.
// Historical GPS/RSSI remain stored but must not be presented as live.
bool isVesselBActive();
unsigned long vesselBLastSeenSeconds();

// Phase 3.5.2: in-memory last received SOS from Vessel-B (read-only to UI).
struct ReceivedSOSView {
  bool known;
  bool cancelled;
  bool positionValid;
  double latitude;
  double longitude;
  uint32_t originId;
  uint32_t eventId;
  unsigned long receivedAtMs;
};
// Read-only RAM SOS record snapshot; events keyed by origin and latest event ID.
struct SOSRecordSnapshot {
 uint32_t originId=0,eventId=0,receivedAtMs=0;
 bool known=false,cancelled=false,positionValid=false;
 double latitude=0,longitude=0;
};
size_t getSOSRecordCount();
size_t getActiveSOSCount();
bool getSOSRecordByIndex(size_t index,SOSRecordSnapshot &out);
ReceivedSOSView getReceivedSOSView();
bool consumeNewReceivedSOS();