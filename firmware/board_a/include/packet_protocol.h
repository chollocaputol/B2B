#pragma once
#include <Arduino.h>

#define PACKET_MAGIC    0x4D45
#define PACKET_VERSION  1
#define FLAG_GPS_FIX                0x01
#define FLAG_TEMP_HUMIDITY_VALID    0x02
#define FLAG_PRESSURE_VALID         0x04

enum PacketType : uint8_t {
  PACKET_TELEMETRY = 1,
  PACKET_SOS = 2
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