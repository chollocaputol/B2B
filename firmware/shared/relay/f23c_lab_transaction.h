#pragma once
#include "f23c_lab_contract.h"
#include "../communication/packet_protocol.h"

namespace F23CLab {
enum class Admission : uint8_t {
  Disarmed, IdentityDenied, Expired, Invalid, WrongRoute, WrongEvent,
  StateRejected, UnknownCancel, Duplicate, Accepted
};
enum class Output : uint8_t { None, Prepared };
enum class RadioResult : uint8_t { Disabled };

// Deliberately has no RadioLib object, function pointer, or hardware callback.
struct DisabledRadio {
  RadioResult submit(const VesselPacket&) const { return RadioResult::Disabled; }
};

// One event per object/run. Terminal state is retained, never evicted.
// Only the host fixture arms this model; firmware has no arming command.
// A new object/reboot loses history: external run control is still required.
class Transaction {
public:
  bool arm(uint32_t run, uint32_t event, bool identityReady,
           uint32_t self, uint32_t now) {
    if (armUsed_ || !run || !event || !identityReady || self != 2U) return false;
    armUsed_ = true;
    armed_ = true;
    event_ = event;
    startedAt_ = now;
    return true;
  }

  void disarm() {
    armed_ = false;
    sosPending_ = false;
    cancelPending_ = false;
  }

  Admission offer(const VesselPacket& p, bool identityReady, uint32_t self,
                  bool stateAccepted, uint32_t now) {
    const Admission context = checkContext(identityReady, self, now);
    if (context != Admission::Accepted) return context;
    if (p.magic != PACKET_MAGIC || p.version != PACKET_VERSION ||
        !p.sequenceNumber) return Admission::Invalid;
    if (p.originNodeId != 1U || p.hopCount != 0U ||
        (p.packetType != PACKET_SOS && p.packetType != PACKET_SOS_CANCEL))
      return Admission::WrongRoute;
    if (p.sequenceNumber != event_) return Admission::WrongEvent;
    if (p.packetType == PACKET_SOS_CANCEL) {
      uint32_t event = 0, origin = 0;
      memcpy(&event, p.message, 4);
      memcpy(&origin, p.message + 4, 4);
      if (event != event_ || origin != 1U) return Admission::Invalid;
    }
    if (!stateAccepted) return Admission::StateRejected;
    if (p.packetType == PACKET_SOS) {
      if (sosSeen_ || cancelSeen_) return Admission::Duplicate;
      sosSeen_ = true;
      sosPending_ = true;
      sos_ = p;
      sosQueuedAt_ = now;
    } else {
      if (!sosSeen_) return Admission::UnknownCancel;
      if (cancelSeen_) return Admission::Duplicate;
      cancelSeen_ = true;
      cancelPending_ = true;
      sosPending_ = false; // cancellation wins before a staged SOS is emitted
      cancel_ = p; // independent of later operational peer-table replacement
      cancelQueuedAt_ = now;
    }
    return Admission::Accepted;
  }

  Output poll(uint32_t now, bool identityReady, uint32_t self,
              bool localControlBusy, VesselPacket& prepared) {
    if (checkContext(identityReady, self, now) != Admission::Accepted ||
        localControlBusy || attempts_ >= kMaxModelAttempts) return Output::None;
    if (haveOutput_ && uint32_t(now - lastOutputAt_) < kModelSeparationMs)
      return Output::None;
    if (cancelPending_) {
      if (uint32_t(now - cancelQueuedAt_) < kPreparationDelayMs) return Output::None;
      prepared = cancel_;
      cancelPending_ = false;
    } else if (sosPending_ && !cancelSeen_) {
      if (uint32_t(now - sosQueuedAt_) < kPreparationDelayMs) return Output::None;
      prepared = sos_;
      sosPending_ = false;
    } else {
      return Output::None;
    }
    // Reserve the modeled attempt BEFORE the caller invokes any mock sink.
    ++attempts_;
    haveOutput_ = true;
    lastOutputAt_ = now;
    prepared.hopCount = 1;
    return Output::Prepared;
  }

  bool armed() const { return armed_; }
  bool cancelled() const { return cancelSeen_; }
  uint8_t attempts() const { return attempts_; }

private:
  Admission checkContext(bool identityReady, uint32_t self, uint32_t now) {
    if (!armed_) return Admission::Disarmed;
    if (!identityReady || self != 2U) {
      disarm(); // identity failure latches closed; restoration cannot restart work
      return Admission::IdentityDenied;
    }
    if (uint32_t(now - startedAt_) >= kRunLifetimeMs) {
      disarm();
      return Admission::Expired;
    }
    return Admission::Accepted;
  }

  VesselPacket sos_{}, cancel_{};
  uint32_t event_ = 0, startedAt_ = 0, sosQueuedAt_ = 0, cancelQueuedAt_ = 0;
  uint32_t lastOutputAt_ = 0;
  uint8_t attempts_ = 0;
  bool armUsed_ = false, armed_ = false, sosSeen_ = false, cancelSeen_ = false;
  bool sosPending_ = false, cancelPending_ = false, haveOutput_ = false;
};
}
