// F24K compile-only physical SD denial — self-contained C++17 host tests.
// There is no filesystem, Arduino, ESP32, radio, or SD card in this suite.
#include "dtn_f24k_sd_interposition.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <type_traits>
#include <utility>

namespace {
unsigned checks=0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"F24K FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
struct FakeFile {
  bool opened = false;
  explicit operator bool() const {return opened;}
};
using Deny = DTNF24K::DenySdFacade<FakeFile>;
struct MaliciousArgument {
  int content=55;
};
static_assert(!Deny::kPhysicalAuthority, "F24K must never confer physical authority");
static_assert(!Deny::kWriterExclusionProven, "F24K cannot prove writer exclusion");
static_assert(!Deny::kAllWritesInterposed, "F23D/external paths remain outside ownership");
static_assert(!Deny::kReplayPermitted, "F24K cannot grant replay");
static_assert(!std::is_copy_constructible<Deny>::value, "do not clone denied state");
static_assert(std::is_same<decltype(std::declval<Deny&>().open("/THESIS/x", "r")),FakeFile>::value,
              "denied open must return a closed File-compatible type");
static_assert(std::is_same<decltype(std::declval<Deny&>().cardSize()),uint64_t>::value,
              "cardSize ABI must be unsigned 64-bit");
}
int main(){
  Deny gate{};
  auto audit = gate.audit();
  CHECK(audit.total == 0);
  CHECK(audit.last == DTNF24K::Operation::Count);
  CHECK(!gate.begin(47, 1, 1000000, "/sd", 5, false));
  CHECK(!gate.begin());
  CHECK(gate.cardSize() == 0U);
  CHECK(!gate.exists("/THESIS"));
  CHECK(!gate.exists("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl"));
  CHECK(!gate.mkdir("/THESIS"));
  CHECK(!gate.mkdir("/THESIS/F06TEST"));
  CHECK(!gate.open("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl","a"));
  CHECK(!gate.open("/THESIS/VESSEL_002/sos_events.jsonl","r"));
  CHECK(!gate.open("/THESIS/F06TEST/V003/S0000001/telemetry.jsonl","w"));
  CHECK(!gate.open("/THESIS/F06TEST/V003/S0000001/diagnostics.jsonl","r",true));
  CHECK(!gate.remove("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl"));
  CHECK(!gate.rename("old","new"));
  const MaliciousArgument arg{};
  CHECK(!gate.begin(arg,arg,arg));
  CHECK(!gate.open(arg,arg));
  CHECK(!gate.exists(arg));
  CHECK(!gate.mkdir(arg));
  audit=gate.audit();
  CHECK(audit.total == 17U);
  CHECK(audit.byOperation[static_cast<unsigned>(DTNF24K::Operation::Begin)] == 3U);
  CHECK(audit.byOperation[static_cast<unsigned>(DTNF24K::Operation::CardSize)] == 1U);
  CHECK(audit.byOperation[static_cast<unsigned>(DTNF24K::Operation::Exists)] == 3U);
  CHECK(audit.byOperation[static_cast<unsigned>(DTNF24K::Operation::Mkdir)] == 3U);
  CHECK(audit.byOperation[static_cast<unsigned>(DTNF24K::Operation::Open)] == 5U);
  CHECK(audit.byOperation[static_cast<unsigned>(DTNF24K::Operation::Remove)] == 1U);
  CHECK(audit.byOperation[static_cast<unsigned>(DTNF24K::Operation::Rename)] == 1U);
  CHECK(audit.last == DTNF24K::Operation::Mkdir);
  // Denial must persist across repeated attempts and after diagnostic reset.
  for(unsigned i=0;i<100U;++i){
    CHECK(!gate.open("/THESIS/F06TEST/V002/S0000001/sos_events.jsonl","a"));
    CHECK(!gate.begin(47,1,1000000,"/sd",5,false));
    CHECK(gate.cardSize() == 0U);
  }
  audit=gate.audit();
  CHECK(audit.total == 317U);
  CHECK(audit.byOperation[static_cast<unsigned>(DTNF24K::Operation::Open)] == 105U);
  CHECK(audit.byOperation[static_cast<unsigned>(DTNF24K::Operation::Begin)] == 103U);
  gate.resetAudit();
  CHECK(gate.audit().total == 0U);
  CHECK(!gate.open("/THESIS/VESSEL_002/sos_events.jsonl","r"));
  CHECK(gate.audit().total == 1U);
  std::printf("F24K logger SD facade: PASS (%u checks; DENY-ONLY / NO PHYSICAL IO)\n",checks);
  return 0;
}
