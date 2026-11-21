#include "../firmware/shared/relay/compact_position_hop1.h"
#include <cassert>
#include <cstdio>

int main() {
  CompactPositionV2::State v001{};
  v001.originId=1; v001.sequence=253; v001.uptimeSeconds=628;
  v001.gpsValid=true; v001.latitudeE7=82234567; v001.longitudeE7=1242345678;
  uint8_t wire[CompactPositionV2::kWireSize]{};
  assert(CompactPositionV2::encode(wire,sizeof(wire),v001));
  CompactPositionRelay::Queue q;
  assert(q.offer(wire,sizeof(wire),2)==CompactPositionRelay::Admission::Queued);
  CompactPositionRelay::Frame incoming{};
  assert(q.take(incoming,false));
  VesselPacket v003Input{};
  assert(CompactPositionRelay::toHopOneTelemetry(incoming,2,v003Input));
  assert(sizeof(v003Input)==62);
  assert(v003Input.magic==PACKET_MAGIC && v003Input.version==1);
  assert(v003Input.packetType==PACKET_TELEMETRY && v003Input.hopCount==1);
  assert(v003Input.originNodeId==1 && v003Input.sequenceNumber==253);
  assert(v003Input.timestamp==628 && v003Input.flags==FLAG_GPS_FIX);
  assert(v003Input.latitude_e7==82234567 && v003Input.longitude_e7==1242345678);
  assert(v003Input.weather.temperature_x10==0 && v003Input.batteryLevel==0);
  assert(v003Input.nodeName[0]==0 && v003Input.message[0]==0);
  assert(!CompactPositionRelay::toHopOneTelemetry(incoming,3,v003Input));
  auto damaged=incoming;
  ++damaged.sequence;
  assert(!CompactPositionRelay::toHopOneTelemetry(damaged,2,v003Input));
  damaged=incoming; damaged.bytes[2]=3;
  assert(!CompactPositionRelay::toHopOneTelemetry(damaged,2,v003Input));
  v001.gpsValid=false; v001.latitudeE7=0; v001.longitudeE7=0;
  assert(CompactPositionV2::encode(wire,sizeof(wire),v001));
  CompactPositionRelay::Frame noFix{};
  for(size_t i=0;i<sizeof(wire);++i) noFix.bytes[i]=wire[i];
  noFix.sequence=253; noFix.uptimeSeconds=628;
  assert(CompactPositionRelay::toHopOneTelemetry(noFix,2,v003Input));
  assert(v003Input.flags==0 && v003Input.latitude_e7==0 && v003Input.longitude_e7==0);
  std::puts("V001 POSITION_V2 -> V002 hop-one legacy telemetry: PASS");
}
