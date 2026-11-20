#include <cstdio>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include "firmware/shared/relay/f23c_lab_transaction.h"
#include "firmware/shared/communication/multi_peer_sos.h"
using namespace F23CLab;
static unsigned checks = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } ++checks; } while(0)
struct MockRadio {
  unsigned calls = 0;
  bool success = false;
  VesselPacket last{};
  bool submit(const VesselPacket& p) { ++calls; last = p; return success; }
};
static VesselPacket sos(uint32_t event = 42) {
  VesselPacket p{};
  p.magic = PACKET_MAGIC; p.version = PACKET_VERSION; p.packetType = PACKET_SOS;
  p.originNodeId = 1; p.sequenceNumber = event; p.timestamp = 123;
  p.flags = FLAG_GPS_FIX; p.latitude_e7 = 123456; p.longitude_e7 = -987654;
  std::strcpy(p.nodeName, "Vessel-001"); std::strcpy(p.message, "SOS");
  p.batteryLevel = 77; p.weather.temperature_x10 = 286;
  return p;
}
static VesselPacket cancel(uint32_t event = 42) {
  auto p = sos(event); p.packetType = PACKET_SOS_CANCEL;
  encodeSOSAck(p, event, 1); return p;
}
int main() {
  CHECK(sizeof(VesselPacket) == 62);
  CHECK(!kRealRelayTxEnabled && !kOriginAuthenticated && !kEndToEndReceiptVerified);
  Transaction cold; VesselPacket out = sos(999); auto p = sos();
  CHECK(cold.offer(p,true,2,true,0) == Admission::Disarmed);
  CHECK(cold.poll(1000,true,2,false,out) == Output::None);
  CHECK(out.sequenceNumber == 999);
  CHECK(!cold.arm(0,42,true,2,0));
  CHECK(!cold.arm(1,0,true,2,0));
  CHECK(!cold.arm(1,42,false,2,0));
  CHECK(!cold.arm(1,42,true,1,0));
  CHECK(!cold.arm(1,42,true,3,0));
  CHECK(cold.arm(1,42,true,2,0));
  CHECK(!cold.arm(2,43,true,2,0));
  CHECK(cold.offer(cancel(),true,2,true,0) == Admission::UnknownCancel);
  CHECK(cold.offer(p,true,2,false,0) == Admission::StateRejected);
  auto bad = p; bad.magic = 0;
  CHECK(cold.offer(bad,true,2,true,0) == Admission::Invalid);
  bad = p; bad.version = 3;
  CHECK(cold.offer(bad,true,2,true,0) == Admission::Invalid);
  bad = p; bad.sequenceNumber = 0;
  CHECK(cold.offer(bad,true,2,true,0) == Admission::Invalid);
  bad = p; bad.originNodeId = 3;
  CHECK(cold.offer(bad,true,2,true,0) == Admission::WrongRoute);
  for (uint8_t hop : {uint8_t(1),uint8_t(2),uint8_t(255)}) {
    bad = p; bad.hopCount = hop;
    CHECK(cold.offer(bad,true,2,true,0) == Admission::WrongRoute);
  }
  for (uint8_t kind : {uint8_t(1),uint8_t(3),uint8_t(5),uint8_t(99)}) {
    bad = p; bad.packetType = kind;
    CHECK(cold.offer(bad,true,2,true,0) == Admission::WrongRoute);
  }
  CHECK(cold.offer(sos(41),true,2,true,0) == Admission::WrongEvent);
  CHECK(cold.offer(sos(43),true,2,true,0) == Admission::WrongEvent);
  CHECK(cold.offer(p,true,2,true,10) == Admission::Accepted);
  CHECK(cold.offer(p,true,2,true,20) == Admission::Duplicate);
  CHECK(cold.poll(259,true,2,false,out) == Output::None);
  CHECK(cold.poll(260,true,2,true,out) == Output::None); // local control priority
  CHECK(cold.attempts() == 0);
  CHECK(cold.poll(260,true,2,false,out) == Output::Prepared);
  CHECK(cold.attempts() == 1);
  auto expected = p; expected.hopCount = 1;
  CHECK(std::memcmp(&expected,&out,sizeof(out)) == 0);
  MockRadio mock;
  CHECK(!mock.submit(out)); // error still consumes the attempt
  CHECK(mock.calls == 1);
  CHECK(cold.poll(600,true,2,false,out) == Output::None);
  auto c = cancel(); bad = c; bad.message[0] ^= 1;
  CHECK(cold.offer(bad,true,2,true,600) == Admission::Invalid);
  bad = c; bad.message[4] = 2;
  CHECK(cold.offer(bad,true,2,true,600) == Admission::Invalid);
  CHECK(cold.offer(c,true,2,false,600) == Admission::StateRejected);
  CHECK(cold.offer(c,true,2,true,600) == Admission::Accepted);
  CHECK(cold.cancelled());
  CHECK(cold.offer(c,true,2,true,700) == Admission::Duplicate);
  CHECK(cold.offer(p,true,2,true,700) == Admission::Duplicate);
  CHECK(cold.poll(1259,true,2,false,out) == Output::None);
  CHECK(cold.poll(1260,true,2,false,out) == Output::Prepared);
  CHECK(cold.attempts() == 2);
  mock.success = true;
  CHECK(mock.submit(out));
  expected = c; expected.hopCount = 1;
  CHECK(std::memcmp(&expected,&mock.last,sizeof(expected)) == 0);
  CHECK(mock.calls == 2);
  CHECK(cold.poll(5000,true,2,false,out) == Output::None);
  CHECK(cold.offer(p,true,2,true,5000) == Admission::Duplicate);
  CHECK(cold.attempts() == kMaxModelAttempts);
  cold.disarm();
  CHECK(!cold.arm(2,42,true,2,5000)); // disarm/re-arm never renews budget
  CHECK(cold.attempts() == 2);

  Transaction race;
  CHECK(race.arm(2,42,true,2,0));
  CHECK(race.offer(p,true,2,true,0) == Admission::Accepted);
  CHECK(race.offer(c,true,2,true,100) == Admission::Accepted);
  CHECK(race.poll(349,true,2,false,out) == Output::None);
  CHECK(race.poll(350,true,2,false,out) == Output::Prepared);
  CHECK(out.packetType == PACKET_SOS_CANCEL && race.attempts() == 1);
  CHECK(race.poll(2000,true,2,false,out) == Output::None);
  CHECK(race.offer(p,true,2,true,2000) == Admission::Duplicate);
  // New operational event must not erase the old transaction's cancellation.
  MultiPeerSOS::Table peers;
  CHECK(peers.receiveSOS(1,42,0) == MultiPeerSOS::Result::New);
  CHECK(peers.cancel(1,42,1) == MultiPeerSOS::Result::Cancelled);
  Transaction oldCancel;
  CHECK(oldCancel.arm(3,42,true,2,0));
  CHECK(oldCancel.offer(p,true,2,true,0) == Admission::Accepted);
  CHECK(oldCancel.offer(c,true,2,true,1) == Admission::Accepted);
  CHECK(peers.receiveSOS(1,43,2) == MultiPeerSOS::Result::New);
  CHECK(oldCancel.offer(sos(43),true,2,true,2) == Admission::WrongEvent);
  CHECK(oldCancel.poll(251,true,2,false,out) == Output::Prepared);
  CHECK(out.sequenceNumber == 42 && out.packetType == PACKET_SOS_CANCEL);
  CHECK(!peers.get(1)->cancelled && peers.get(1)->event == 43);
  // Document unchanged endpoint limitation: unknown cancel has no tombstone.
  MultiPeerSOS::Table endpoint;
  CHECK(endpoint.cancel(1,42,0) == MultiPeerSOS::Result::CancelMismatch);
  CHECK(endpoint.receiveSOS(1,42,1) == MultiPeerSOS::Result::New);

  Transaction expired;
  CHECK(expired.arm(4,42,true,2,0));
  CHECK(expired.offer(p,true,2,true,0) == Admission::Accepted);
  CHECK(expired.poll(kRunLifetimeMs,true,2,false,out) == Output::None);
  CHECK(!expired.armed());
  CHECK(!expired.arm(5,42,true,2,kRunLifetimeMs));
  Transaction offerExpired;
  CHECK(offerExpired.arm(4,42,true,2,0));
  CHECK(offerExpired.offer(p,true,2,true,kRunLifetimeMs) == Admission::Expired);
  Transaction identity;
  CHECK(identity.arm(5,42,true,2,0));
  CHECK(identity.offer(p,false,2,true,0) == Admission::IdentityDenied);
  CHECK(!identity.armed());
  CHECK(identity.poll(1000,true,2,false,out) == Output::None);
  Transaction wrongSelf;
  CHECK(wrongSelf.arm(5,42,true,2,0));
  CHECK(wrongSelf.offer(p,true,3,true,0) == Admission::IdentityDenied);
  Transaction pollIdentity;
  CHECK(pollIdentity.arm(5,42,true,2,0));
  CHECK(pollIdentity.offer(p,true,2,true,0) == Admission::Accepted);
  CHECK(pollIdentity.poll(250,false,2,false,out) == Output::None);
  CHECK(!pollIdentity.armed() && pollIdentity.attempts() == 0);
  Transaction wrap;
  const uint32_t start = UINT32_MAX - 100;
  CHECK(wrap.arm(6,42,true,2,start));
  CHECK(wrap.offer(p,true,2,true,start) == Admission::Accepted);
  CHECK(wrap.poll(uint32_t(start+249U),true,2,false,out) == Output::None);
  CHECK(wrap.poll(uint32_t(start+250U),true,2,false,out) == Output::Prepared);
  CHECK(wrap.offer(c,true,2,true,uint32_t(start+300U)) == Admission::Accepted);
  CHECK(wrap.poll(uint32_t(start+1249U),true,2,false,out) == Output::None);
  CHECK(wrap.poll(uint32_t(start+1250U),true,2,false,out) == Output::Prepared);
  CHECK(wrap.attempts() == 2);
  DisabledRadio disabled;
  CHECK(disabled.submit(out) == RadioResult::Disabled);
  Transaction reboot;
  CHECK(!reboot.armed() && reboot.attempts() == 0);
  CHECK(reboot.offer(p,true,2,true,0) == Admission::Disarmed);
  CHECK(reboot.poll(1000,true,2,false,out) == Output::None);
  std::printf("PASS: %u F23C transaction/mock checks; real relay RF disabled\n",checks);
}
