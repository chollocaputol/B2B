#pragma once
#include <stdint.h>
#include "packet_protocol.h"
// Laboratory routing restriction, NOT origin authentication.
// Additional guards in radio path include VesselIdentity::ready(), event
// admission/duplicate cache and state preflight. This is NOT a security proof.
namespace ThesisLabRelay {
inline bool permits(const VesselPacket& p, uint32_t self) {
  return self == 2 && p.originNodeId == 1 &&
    p.magic == PACKET_MAGIC && p.version == PACKET_VERSION &&
    (p.packetType == PACKET_SOS || p.packetType == PACKET_SOS_CANCEL) &&
    p.hopCount == 1 && p.sequenceNumber != 0 &&
    (p.packetType != PACKET_SOS_CANCEL || (
      [] (const VesselPacket& pkt) {
        uint32_t event=0, origin=0;
        memcpy(&event,pkt.message,4);
        memcpy(&origin,pkt.message+4,4);
        return event==pkt.sequenceNumber && origin==pkt.originNodeId;
      }(p)));
}
}
