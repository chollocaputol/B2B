#pragma once
#include "f23d_bench_contract.h"
#include "f23c_lab_transaction.h"
#include "lab_relay_gate.h"
#include <string.h>

namespace F23DBench {
struct Context {
  bool identityReady = false;
  uint32_t self = 0;
  bool physicalPermission = false;
  bool recoverySafe = false;
  bool localControlBusy = false;
};
enum class Control : uint8_t {
  NotCommand, Invalid, Selected, Armed, Denied, Locked, Status, Disarmed
};
class RunControl {
public:
  // Pure local parser; no stream, NVS, GPIO, SD, enrollment or radio operations.
  Control command(const char* line, const Context& c, uint32_t now, uint32_t challenge) {
    if (strncmp(line, "R23D", 4) != 0) return Control::NotCommand;
    if (!strcmp(line, "R23D STATUS")) return Control::Status;
    if (!strcmp(line, "R23D DISARM")) { revoke(); return Control::Disarmed; }
    if (!strncmp(line, "R23D SELECT ", 12)) {
      const char* next = line + 12;
      uint32_t run = 0, event = 0;
      if (!number(next,run) || *next++ != ' ' || !number(next,event) || *next)
        return Control::Invalid;
      if (selected_ || terminal_) return Control::Locked; // one slot, never evicted
      if (!identity(c) || !c.recoverySafe || c.localControlBusy || !challenge)
        return Control::Denied;
      selected_ = true; run_ = run; event_ = event; nonce_ = challenge; selectedAt_ = now;
      return Control::Selected;
    }
    if (!strncmp(line, "R23D ARM ", 9)) {
      const char* next = line + 9; uint32_t nonce = 0;
      if (!number(next,nonce) || *next) return Control::Invalid;
      if (!selected_ || terminal_ || transaction_.armed()) return Control::Locked;
      // Failed authorization closes this object permanently; never auto-retry.
      if (nonce != nonce_ || !eligible(c) || c.localControlBusy ||
          uint32_t(now-selectedAt_) >= F23CLab::kRunLifetimeMs) {
        revoke(); return Control::Denied;
      }
      if (!transaction_.arm(run_,event_,c.identityReady,c.self,now)) {
        revoke(); return Control::Denied;
      }
      nonce_ = 0;
      armedOnce_ = true;
      return Control::Armed;
    }
    return Control::Invalid;
  }

  F23CLab::Admission observe(const VesselPacket& p, const Context& c,
                            bool operationalAccepted, uint32_t now) {
    if (!selected_ || !armedOnce_ || terminal_) return F23CLab::Admission::Disarmed;
    if (!eligible(c)) { revoke(); return F23CLab::Admission::Disarmed; }
    // A matching cancellation belongs to the accepted selected transaction even
    // after a newer operational peer event replaces the original table record.
    const bool accepted = operationalAccepted ||
        (sawSOS_ && p.packetType == PACKET_SOS_CANCEL);
    const auto result=transaction_.offer(p,c.identityReady,c.self,accepted,now);
    if (result==F23CLab::Admission::Accepted && p.packetType==PACKET_SOS) sawSOS_=true;
    if (!transaction_.armed()) terminal_=true;
    return result;
  }

  template<class Adapter>
  bool dispatch(const Context& c, uint32_t now, Adapter& adapter, int& result) {
    if (!selected_ || !armedOnce_ || terminal_) return false;
    if (!eligible(c)) { revoke(); return false; }
    VesselPacket packet{};
    if (transaction_.poll(now,c.identityReady,c.self,c.localControlBusy,packet)
        != F23CLab::Output::Prepared) {
      if (selected_ && !transaction_.armed()) terminal_=true;
      return false;
    }
    // The reused transaction reserved the bounded attempt BEFORE this adapter.
    if (!ThesisLabRelay::permits(packet,c.self) || packet.sequenceNumber!=event_) {
      revoke(); return false;
    }
    dispatching_=true;
    leasedKind_=packet.packetType;
    result=adapter.submit(packet);
    dispatching_=false;
    return true;
  }

  bool maySubmit(const VesselPacket& p, const Context& c) const {
    return eligible(c) && !c.localControlBusy && transaction_.armed() &&
        !terminal_ && dispatching_ && p.packetType==leasedKind_ &&
        transaction_.attempts()>0 &&
        p.sequenceNumber==event_ && ThesisLabRelay::permits(p,c.self);
  }
  void revoke() { transaction_.disarm(); terminal_=true; dispatching_=false; nonce_=0; }
  bool armed() const { return transaction_.armed() && !terminal_; }
  uint32_t challenge() const { return nonce_; }
  uint32_t event() const { return event_; }
  uint8_t attempts() const { return transaction_.attempts(); }
private:
  static bool identity(const Context& c) { return c.identityReady && c.self==2U; }
  static bool eligible(const Context& c) { return identity(c) && c.recoverySafe && c.physicalPermission; }
  static bool number(const char*& text, uint32_t& value) {
    value=0;
    if (*text<'0' || *text>'9') return false;
    while (*text>='0' && *text<='9') {
      const uint32_t digit=uint32_t(*text-'0');
      if (value>(UINT32_MAX-digit)/10U) return false;
      value=value*10U+digit; ++text;
    }
    return value!=0;
  }
  F23CLab::Transaction transaction_;
  uint32_t run_=0,event_=0,nonce_=0,selectedAt_=0;
  uint8_t leasedKind_=0;
  bool selected_=false,terminal_=false,sawSOS_=false,armedOnce_=false,dispatching_=false;
};
}
