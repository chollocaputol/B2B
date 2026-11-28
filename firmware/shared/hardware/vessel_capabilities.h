#pragma once
#include <stdint.h>

// F22R1 policy scaffold only. PURE host-testable decisions, NO hardware I/O.
// NOT wired to any production profile. Do not use this to arm GPIO or SD.
// Vessel identities are provisioned outside the firmware image in NVS.
namespace F22UniversalCapability {

enum class SensorWiring : uint8_t {
  Unverified = 0,
  LegacySplit = 1,    // BME680 GPIO21/48; DHT20 GPIO38/39
  Shared2148 = 2,    // BME680 + DHT20 GPIO21/48
};

struct Evidence {
  uint32_t vesselId = 0;
  bool provisionedIdentity = false;   // caller must use validated VesselIdentity::load()
  bool boardRevisionReviewed = false; // independent wiring / power review
  SensorWiring sensorWiring = SensorWiring::Unverified;
  bool sensorPinsReviewed = false;
  bool bme680Responded = false;       // actual probe after safe bus selection
  bool dht20Responded = false;
  bool sdCardPresent = false;
  bool sdMediaReviewed = false;       // media/partition/history preserved
  bool sdExplicitOptIn = false;
};

struct Pins {
  uint8_t bmeSda;
  uint8_t bmeScl;
  uint8_t dhtSda;
  uint8_t dhtScl;
};

struct Decision {
  bool identityAccepted = false;
  bool sensorBusEligible = false;     // NOT permission to probe unspecified pins
  bool weatherInputSetComplete = false;
  bool sdMountEligible = false;       // NOT a mount operation
  bool gpio38and39ReservedForDht = false;
  bool physicalOutputsEnabled = false; // F22R1 permanently OFF
  bool autoEnrolled = false;           // F22R1 never creates or guesses identities
};

constexpr bool validIdentity(bool provisioned, uint32_t id) {
  return provisioned && id >= 1U && id <= 9999U;
}
constexpr bool knownWiring(SensorWiring p) {
  return p == SensorWiring::LegacySplit || p == SensorWiring::Shared2148;
}
constexpr Pins pins(SensorWiring p) {
  return p == SensorWiring::LegacySplit ? Pins{21,48,38,39} :
         p == SensorWiring::Shared2148 ? Pins{21,48,21,48} : Pins{0,0,0,0};
}
inline Decision evaluate(const Evidence& e) {
  Decision d;
  d.identityAccepted = validIdentity(e.provisionedIdentity, e.vesselId);
  const bool reviewedBoard = d.identityAccepted && e.boardRevisionReviewed;
  d.sensorBusEligible = reviewedBoard && e.sensorPinsReviewed && knownWiring(e.sensorWiring);
  d.weatherInputSetComplete = d.sensorBusEligible && e.bme680Responded && e.dht20Responded;
  d.sdMountEligible = reviewedBoard && e.sdCardPresent && e.sdMediaReviewed && e.sdExplicitOptIn;
  d.gpio38and39ReservedForDht = d.sensorBusEligible && e.sensorWiring == SensorWiring::LegacySplit;
  // No experimental weather, radio/SOS, GPIO, rail, NVS, or SD writes in F22R1.
  d.physicalOutputsEnabled = false;
  d.autoEnrolled = false;
  return d;
}
static_assert(validIdentity(true, 1) && validIdentity(true, 9999), "valid identity range");
static_assert(!validIdentity(true, 0) && !validIdentity(true, 10000) && !validIdentity(false, 3), "invalid identity fails closed");
static_assert(pins(SensorWiring::LegacySplit).dhtSda == 38 && pins(SensorWiring::LegacySplit).dhtScl == 39, "legacy wiring contract");
static_assert(pins(SensorWiring::Shared2148).dhtSda == 21 && pins(SensorWiring::Shared2148).dhtScl == 48, "shared wiring contract");

} // namespace F22UniversalCapability
