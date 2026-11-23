#pragma once
// THESIS F24K — permanently denied SD facade for compile-only interposition.
// No Arduino/SD includes, global SD object, SPI, SD.open, credentials, or
// physical authorization. This type cannot be switched to a permissive mode.
// All concrete legacy logger SD *entrypoints* are routed through the same
// named facade; ordinary (F24K=0) firmware aliases its existing SD singleton.
// F23D independent writer and open File handle methods remain separate work.
#include <stdint.h>
#include <stddef.h>

namespace DTNF24K {

enum class Operation : uint8_t {
  Begin, CardSize, Exists, Mkdir, Open, Remove, Rename, Count
};

// A deny-only observation counter is NOT a physical witness, journal lease,
// mount epoch, media identity or indication that writes are interposed.
struct DenialAudit {
  uint32_t total = 0;
  uint32_t byOperation[static_cast<unsigned>(Operation::Count)]{};
  Operation last = Operation::Count;
};

template<class FileType> class DenySdFacade final {
 public:
  static constexpr bool kPhysicalAuthority = false;
  static constexpr bool kWriterExclusionProven = false;
  static constexpr bool kAllWritesInterposed = false;
  static constexpr bool kReplayPermitted = false;

  DenySdFacade() = default;
  DenySdFacade(const DenySdFacade&) = delete;
  DenySdFacade& operator=(const DenySdFacade&) = delete;

  // Deliberately consume arguments with templates so any ESP32 Arduino SD
  // method signature remains compile-checkable, but no argument is forwarded
  // to a storage provider. Never return an open File object.
  template<class... Args> bool begin(Args&&...) {
    record(Operation::Begin); return false;
  }
  template<class... Args> uint64_t cardSize(Args&&...) {
    record(Operation::CardSize); return 0U;
  }
  template<class... Args> bool exists(Args&&...) {
    record(Operation::Exists); return false;
  }
  template<class... Args> bool mkdir(Args&&...) {
    record(Operation::Mkdir); return false;
  }
  template<class... Args> FileType open(Args&&...) {
    record(Operation::Open); return FileType{};
  }
  // Explicit deny for common SD APIs if future logger code starts using them.
  // Stage-time inventory still rejects any new SD.<method> calls without review.
  template<class... Args> bool remove(Args&&...) {
    record(Operation::Remove); return false;
  }
  template<class... Args> bool rename(Args&&...) {
    record(Operation::Rename); return false;
  }
  DenialAudit audit() const {return audit_;}
  void resetAudit() {audit_ = DenialAudit{};}
 private:
  DenialAudit audit_{};
  void record(Operation op) {
    if (audit_.total != UINT32_MAX) ++audit_.total;
    const unsigned k = static_cast<unsigned>(op);
    if (audit_.byOperation[k] != UINT32_MAX) ++audit_.byOperation[k];
    audit_.last = op;
  }
};
} // namespace DTNF24K
