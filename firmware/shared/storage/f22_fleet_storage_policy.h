#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../hardware/f22_universal_capability_policy.h"

// F22R3B: PURE software-side authorization/path policy, NOT the SD driver.
// NO Arduino runtime, SD mounting, GPIO, SPI, PMU, NVS, radio or filesystem I/O.
// The only enabled runtime use in F22R3B is an opt-in diagnostic printing
// DENIED mount and write access. Real universal storage is a later milestone.
namespace F22FleetStoragePolicy {
struct Evidence {
  uint32_t vesselId = 0;
  bool provisionedIdentity = false;  // validated NVS identity from VesselIdentity::load
  bool boardAndSdPinsReviewed = false;
  bool sdMediaPresent = false;
  bool sdMediaReviewed = false;       // includes review of existing journals
  bool historyProtectionApproved = false;
  bool sdExplicitOptIn = false;
  bool isolatedF06SessionConfirmed = false;
  bool continuousResearchConsent = false;
  bool sosOrTailActive = false;       // authoritative F19 hybrid state, not forecast
  bool legacyV002ReadOnlyOptIn = false;
};

struct Decision {
  bool identityAccepted = false;
  uint32_t checkedVesselId = 0;   // binds the decision to the evaluated vessel
  bool mountEligible = false;        // policy only; does NOT mount storage
  bool newSessionWriteEligible = false;
  bool urgentJournalEligible = false;
  bool routineJournalEligible = false;
  bool researchJournalEligible = false;
  bool ownF06HistoryReadEligible = false;
  bool legacyV002HistoryReadEligible = false;
  bool formatOrEraseAllowed = false;
  bool physicalOutputsEnabled = false;
  bool autoEnroll = false;
};

inline Decision evaluate(const Evidence& e) {
  Decision d;
  d.identityAccepted = F22UniversalCapability::validIdentity(
      e.provisionedIdentity, e.vesselId);
  if (d.identityAccepted) d.checkedVesselId = e.vesselId;
  d.mountEligible = d.identityAccepted && e.boardAndSdPinsReviewed &&
      e.sdMediaPresent && e.sdMediaReviewed && e.historyProtectionApproved &&
      e.sdExplicitOptIn;
  d.newSessionWriteEligible = d.mountEligible && e.isolatedF06SessionConfirmed;
  d.urgentJournalEligible = d.newSessionWriteEligible;
  d.routineJournalEligible = d.newSessionWriteEligible &&
      (e.continuousResearchConsent || e.sosOrTailActive);
  d.researchJournalEligible = d.newSessionWriteEligible &&
      e.continuousResearchConsent;
  d.ownF06HistoryReadEligible = d.mountEligible;
  // Preserve V002 historical reads. Other vessels never get legacy access.
  d.legacyV002HistoryReadEligible = d.mountEligible &&
      e.vesselId == 2U && e.legacyV002ReadOnlyOptIn;
  return d;
}

inline bool ownSandboxRoot(char* out, size_t cap, uint32_t vesselId) {
  if (!out || cap == 0) return false;
  out[0] = '\0';
  if (!F22UniversalCapability::validIdentity(true, vesselId)) return false;
  const int n = snprintf(out, cap, "/THESIS/F06TEST/V%03lu", (unsigned long)vesselId);
  if (n < 0 || static_cast<size_t>(n) >= cap) { out[0]='\0'; return false; }
  return true;
}

inline bool safeSessionName(const char* session) {
  if (!session || session[0] != 'S') return false;
  for (size_t i=1; i<8; ++i) {
    const char ch = session[i];
    if (!((ch>='0' && ch<='9') || (ch>='A' && ch<='F'))) return false;
  }
  return session[8] == '\0';
}

inline bool ownSessionPath(char* out, size_t cap, uint32_t vesselId,
                           const char* session) {
  if (!out || cap == 0) return false;
  out[0] = '\0';
  if (!safeSessionName(session)) return false;
  char root[40];
  if (!ownSandboxRoot(root,sizeof(root),vesselId)) return false;
  const int n=snprintf(out,cap,"%s/%s",root,session);
  if (n<0 || static_cast<size_t>(n)>=cap) { out[0]='\0'; return false; }
  return true;
}

inline bool legacyV002ReadPath(char* out, size_t cap, const Decision& d,
                               uint32_t vesselId) {
  if (!out || cap==0) return false;
  out[0]='\0';
  if (!d.legacyV002HistoryReadEligible || vesselId!=2U ||
      d.checkedVesselId != vesselId) return false;
  const int n=snprintf(out,cap,"/THESIS/VESSEL_002");
  if (n<0 || static_cast<size_t>(n)>=cap) { out[0]='\0'; return false; }
  return true;
}

static_assert(F22UniversalCapability::validIdentity(true,1U),"first vessel ID");
static_assert(F22UniversalCapability::validIdentity(true,9999U),"last vessel ID");
static_assert(!F22UniversalCapability::validIdentity(true,10000U),"no ID overflow");
} // namespace F22FleetStoragePolicy
