#pragma once
#include <stdint.h>
#include <stddef.h>

// Phase 5.1 protocol contract. Does not transmit or alter existing SOS logic.
// Compatible with current 62-byte, version-1 VesselPacket on both boards.
namespace RelayPolicy {
static constexpr uint8_t kMaxHops = 2;  // at most two transmissions via relays
static constexpr size_t kCacheSize = 32;

enum class Decision : uint8_t {
  Invalid, OwnOrigin, NotRelayable, HopLimit, Duplicate, Candidate
};
inline const char* name(Decision d) {
  switch (d) {
    case Decision::Invalid: return "INVALID";
    case Decision::OwnOrigin: return "OWN_ORIGIN";
    case Decision::NotRelayable: return "NOT_RELAYABLE";
    case Decision::HopLimit: return "HOP_LIMIT";
    case Decision::Duplicate: return "DUPLICATE";
    case Decision::Candidate: return "CANDIDATE";
  }
  return "UNKNOWN";
}
struct Entry { uint32_t origin; uint32_t event; uint8_t kind; bool used; };
// Cache is RAM-only. Reboot persistence, authenticated origins, and durable
// replay protection are deferred, so do not enable real SOS relaying yet.
class Cache {
public:
  Cache() : cursor_(0) { for (size_t i=0;i<kCacheSize;++i) entries_[i]={0,0,0,false}; }
  bool contains(uint32_t origin, uint32_t event, uint8_t kind) const {
    for (size_t i=0;i<kCacheSize;++i)
      if (entries_[i].used && entries_[i].origin==origin &&
          entries_[i].event==event && entries_[i].kind==kind) return true;
    return false;
  }
  void remember(uint32_t origin,uint32_t event,uint8_t kind) {
    if (contains(origin,event,kind)) return;
    entries_[cursor_] = {origin,event,kind,true};
    cursor_=(cursor_+1)%kCacheSize;
  }
private:
  Entry entries_[kCacheSize];
  size_t cursor_;
};
// Protocol types: 1 telemetry, 2 SOS, 3 SOS-ACK, 4 cancel, 5 cancel-ACK.
// Phase 5 design: only SOS and cancellation are candidates. Control ACKs
// remain single-hop pending an explicit end-to-end acknowledgment protocol.
inline bool eligible(uint8_t kind) { return kind==2 || kind==4; }

template <typename Packet>
inline Decision inspect(const Packet& p, uint32_t localNodeId, Cache& cache) {
  if (p.magic!=0x4D45 || p.version!=1 || p.originNodeId==0 ||
      p.packetType<1 || p.packetType>5) return Decision::Invalid;
  if (p.originNodeId==localNodeId) return Decision::OwnOrigin;
  if (!eligible(p.packetType)) return Decision::NotRelayable;
  if (p.hopCount>=kMaxHops) return Decision::HopLimit;
  if (cache.contains(p.originNodeId,p.sequenceNumber,p.packetType)) return Decision::Duplicate;
  cache.remember(p.originNodeId,p.sequenceNumber,p.packetType);
  return Decision::Candidate;
}
} // namespace RelayPolicy
