#pragma once
// Phase 3G-4E3A: POSITION-only on-air payload, independent from v1 SOS frames.
// Explicit little-endian encoding; do NOT transmit a native C++ struct directly.
#include <stddef.h>
#include <stdint.h>
#include "packet_protocol.h"

namespace CompactPositionV2 {
static constexpr uint8_t kVersion = 2;
static constexpr uint8_t kType = 6; // Distinct from legacy v1 types 1..5.
static constexpr size_t kWireSize = 25;
static constexpr uint8_t kGpsValid = FLAG_GPS_FIX;

struct State {
  uint32_t originId = 0;
  uint32_t sequence = 0;
  uint32_t uptimeSeconds = 0;
  bool gpsValid = false;
  int32_t latitudeE7 = 0;
  int32_t longitudeE7 = 0;
};

inline void write16(uint8_t *p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8);
}
inline void write32(uint8_t *p, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8*i));
}
inline uint16_t read16(const uint8_t *p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
inline uint32_t read32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
         (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
// Preserve signed bit patterns across host/ESP32 compilers without type punning.
inline int32_t asSigned(uint32_t u) {
  if (u <= 0x7FFFFFFFUL) return static_cast<int32_t>(u);
  return -1 - static_cast<int32_t>(~u);
}
inline bool coordinateValid(int32_t lat, int32_t lon) {
  return lat >= -900000000 && lat <= 900000000 &&
         lon >= -1800000000 && lon <= 1800000000 &&
         !(lat == 0 && lon == 0); // Matches existing receiver's missing-fix rule.
}

// Wire offsets: 0 magic(2), 2 version(1), 3 type(1), 4 origin(4),
// 8 telemetry-seq(4), 12 uptime-sec(4), 16 flags(1), 17 lat-e7(4), 21 lon-e7(4).
inline bool encode(uint8_t *out, size_t capacity, const State &p) {
  if (out == nullptr || capacity < kWireSize || p.originId < 1 || p.originId > 9999) return false;
  if (p.gpsValid && !coordinateValid(p.latitudeE7,p.longitudeE7)) return false;
  write16(out+0,PACKET_MAGIC);
  out[2] = kVersion;
  out[3] = kType;
  write32(out+4,p.originId);
  write32(out+8,p.sequence);
  write32(out+12,p.uptimeSeconds);
  out[16] = p.gpsValid ? kGpsValid : 0;
  write32(out+17,p.gpsValid ? static_cast<uint32_t>(p.latitudeE7) : 0);
  write32(out+21,p.gpsValid ? static_cast<uint32_t>(p.longitudeE7) : 0);
  return true;
}

inline bool decode(const uint8_t *in, size_t length, State &out) {
  if (in == nullptr || length != kWireSize || read16(in+0) != PACKET_MAGIC ||
      in[2] != kVersion || in[3] != kType || (in[16] & ~kGpsValid)) return false;
  State p;
  p.originId=read32(in+4);
  if (p.originId < 1 || p.originId > 9999) return false;
  p.sequence=read32(in+8);
  p.uptimeSeconds=read32(in+12);
  p.gpsValid=(in[16]&kGpsValid)!=0;
  p.latitudeE7=asSigned(read32(in+17));
  p.longitudeE7=asSigned(read32(in+21));
  if (p.gpsValid) {
    if (!coordinateValid(p.latitudeE7,p.longitudeE7)) return false;
  } else if (p.latitudeE7 != 0 || p.longitudeE7 != 0) {
    return false; // Unfixed beacons must not contain misleading coordinates.
  }
  out=p;
  return true;
}
} // namespace CompactPositionV2
