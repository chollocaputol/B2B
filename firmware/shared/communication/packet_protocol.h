#pragma once
#include <stdint.h>
#include <string.h>

// Phase 5.2.2A: common on-air packet contract for all T-Beam Supreme vessels.
// Keep byte layout unchanged for backward compatibility.
#define PACKET_MAGIC 0x4D45
#define PACKET_VERSION 1
#define FLAG_GPS_FIX 0x01
#define FLAG_TEMP_HUMIDITY_VALID 0x02
#define FLAG_PRESSURE_VALID 0x04

enum PacketType : uint8_t {
  PACKET_TELEMETRY = 1,
  PACKET_SOS = 2,
  PACKET_SOS_ACK = 3,
  PACKET_SOS_CANCEL = 4,
  PACKET_SOS_CANCEL_ACK = 5
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
static_assert(sizeof(VesselPacket) == 62, "On-air protocol must remain 62 bytes");

inline void encodeSOSAck(VesselPacket &ack, uint32_t sequence, uint32_t origin) {
  memcpy(ack.message, &sequence, sizeof(sequence));
  memcpy(ack.message + 4, &origin, sizeof(origin));
}
inline bool matchesSOSAck(const VesselPacket &ack, uint32_t sequence, uint32_t origin) {
  if (ack.packetType != PACKET_SOS_ACK) return false;
  uint32_t receivedSequence = 0, receivedOrigin = 0;
  memcpy(&receivedSequence, ack.message, 4);
  memcpy(&receivedOrigin, ack.message + 4, 4);
  return receivedSequence == sequence && receivedOrigin == origin;
}
