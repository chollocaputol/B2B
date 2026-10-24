#pragma once
#include <Arduino.h>
#include <string.h>

#define PACKET_MAGIC    0x4D45
#define PACKET_VERSION  1
#define FLAG_GPS_FIX                0x01
#define FLAG_TEMP_HUMIDITY_VALID    0x02
#define FLAG_PRESSURE_VALID         0x04

enum PacketType : uint8_t {
  PACKET_TELEMETRY = 1,
  PACKET_SOS = 2,
  PACKET_SOS_ACK = 3,
  PACKET_SOS_CANCEL = 4
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

// ACK payload is placed in message[8]: 4-byte original SOS sequence
// followed by 4-byte original SOS origin node ID. Same 62-byte frame.
inline void encodeSOSAck(VesselPacket &ack, uint32_t sosSeq, uint32_t sosOrigin) {
  memcpy(ack.message, &sosSeq, sizeof(sosSeq));
  memcpy(ack.message + 4, &sosOrigin, sizeof(sosOrigin));
}
inline bool matchesSOSAck(const VesselPacket &ack, uint32_t sosSeq, uint32_t sosOrigin) {
  if (ack.packetType != PACKET_SOS_ACK) return false;
  uint32_t ackSeq = 0, ackOrigin = 0;
  memcpy(&ackSeq, ack.message, sizeof(ackSeq));
  memcpy(&ackOrigin, ack.message + 4, sizeof(ackOrigin));
  return ackSeq == sosSeq && ackOrigin == sosOrigin;
}