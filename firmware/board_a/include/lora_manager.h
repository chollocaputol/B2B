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

void setupLoRa();
void startReceive();
void pollLoRaReceive();
void createPacket(VesselPacket &packet);
void transmitPacket();
void transmitSOS(); // Bench SOS: single high-priority packet, no ACK/relay yet
float bSequencePDR();