#pragma once
// Convert accepted V001 POSITION_V2 into the existing v1 telemetry wire
// schema with hopCount=1. Packet preparation grants NO radio permission.
#include "compact_position_relay.h"
#include "../communication/packet_protocol.h"

namespace CompactPositionRelay {
inline bool toHopOneTelemetry(const Frame& incoming, uint32_t localRelay,
                              VesselPacket& forwarded) {
  if (localRelay != kSelectedRelay) return false;
  CompactPositionV2::State pos;
  if (!CompactPositionV2::decode(incoming.bytes, sizeof(incoming.bytes), pos) ||
      pos.originId != kSelectedOrigin ||
      incoming.sequence != pos.sequence ||
      incoming.uptimeSeconds != pos.uptimeSeconds) return false;
  VesselPacket next{};
  next.magic = PACKET_MAGIC;
  next.version = PACKET_VERSION;
  next.packetType = PACKET_TELEMETRY;
  next.originNodeId = pos.originId;
  next.sequenceNumber = pos.sequence;
  next.timestamp = pos.uptimeSeconds;
  next.hopCount = 1;
  next.flags = pos.gpsValid ? FLAG_GPS_FIX : 0;
  next.latitude_e7 = pos.gpsValid ? pos.latitudeE7 : 0;
  next.longitude_e7 = pos.gpsValid ? pos.longitudeE7 : 0;
  // Compact POSITION has no weather/battery/name: never invent these fields.
  // The flags indicating weather validity remain clear.
  forwarded = next;
  return true;
}
} // namespace CompactPositionRelay
