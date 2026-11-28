#pragma once
#include <stdint.h>
#include "fleet_storage_policy.h"

// F22R3D: pure decisions for the real SD logger, not an approval mechanism.
// Firmware supplies no board/media/history approval evidence in this phase.
// Compile-time-only preview; never reads NVS, SD, SPI or pins here.
namespace F22FleetStorageRuntimeGate {
using Decision = F22FleetStoragePolicy::Decision;
using Evidence = F22FleetStoragePolicy::Evidence;

inline Decision unreviewed(uint32_t id, bool provisionedAndMatches) {
  Evidence e{};
  e.vesselId = id;
  e.provisionedIdentity = provisionedAndMatches;
  // All remaining fields FALSE. In particular, no SD-media approval, consent,
  // F06 session confirmation, or legacy-history permission is fabricated.
  return F22FleetStoragePolicy::evaluate(e);
}
inline bool bound(const Decision& d, uint32_t id) {
  return d.identityAccepted && d.checkedVesselId == id &&
      F22UniversalCapability::validIdentity(true,id);
}
inline bool mountAllowed(const Decision& d, uint32_t id) {
  return bound(d,id) && d.mountEligible;
}
inline bool urgentAllowed(const Decision& d, uint32_t id) {
  return bound(d,id) && d.urgentJournalEligible;
}
inline bool routineAllowed(const Decision& d, uint32_t id) {
  return bound(d,id) && d.routineJournalEligible;
}
inline bool researchAllowed(const Decision& d, uint32_t id, bool continuous) {
  return bound(d,id) && continuous && d.researchJournalEligible;
}
inline bool ownHistoryAllowed(const Decision& d, uint32_t id) {
  return bound(d,id) && d.ownF06HistoryReadEligible;
}
inline bool legacyV002Allowed(const Decision& d, uint32_t id) {
  return bound(d,id) && id==2U && d.legacyV002HistoryReadEligible;
}
static_assert(!F22UniversalCapability::validIdentity(true,0U),"no unprovisioned ID");
} // namespace F22FleetStorageRuntimeGate
