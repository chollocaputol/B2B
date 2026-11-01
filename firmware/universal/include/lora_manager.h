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
size_t getTrackedPeerCount();
uint32_t getTotalPeerTelemetryReceived();

void setupLoRa();
void startReceive();
void pollLoRaReceive();
void createPacket(VesselPacket &packet);
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
ReceivedSOSView getReceivedSOSView();
bool consumeNewReceivedSOS();