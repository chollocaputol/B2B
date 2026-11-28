#pragma once
// F24E portable single-owner scheduling policy for F24D's CLOSED F06 SHA job.
// External Gate MUST verify immutable closure, media continuity, uploader/vessel
// authorization and that ThesisSDLogger is the sole SD owner. This class proves
// NONE of those. Neither a verified digest nor a host pass permits an upload.
// No Arduino, SPI, SD mount, file writes, HTTP, RF, or checkpoint changes here.
#include <stddef.h>
#include <stdint.h>
#include "dtn_closed_sha256_job.h"

namespace DTNF24E {
enum class Phase : uint8_t {
  Idle, Queued, WaitingForOwner, Preempted, Hashing, Verified, Denied, Failed
};
// Volume, Gate, and the Scheduler must outlive the request. The device owner
// must keep a Scheduler instance static rather than destroying it during SD I/O.
// Gate::allowClosedJournal(...) is a RAM-only decision: it MUST NOT touch SPI.
template<class File, class Volume> class Scheduler {
public:
  using Stream = DTNAckLedger::Stream;
  Scheduler()=default;
  Scheduler(const Scheduler&)=delete;
  Scheduler& operator=(const Scheduler&)=delete;

  template<class Gate> Phase queue(Volume& volume, const Stream& stream,
                                   const Gate& gate) {
    if(phase_!=Phase::Idle)return phase_;
    char path[DTNF06ClosedReplay::kPathCapacity]{};
    if(!DTNF06ClosedReplay::pathFor(stream,path,sizeof(path))) {
      last_=DTNF24D::Result::BadStream;return phase_=Phase::Failed;
    }
    if(!gate.allowClosedJournal(stream,path)) {
      last_=DTNF24D::Result::Denied;return phase_=Phase::Denied;
    }
    volume_=&volume;stream_=stream;
    last_=DTNF24D::Result::Idle;
    return phase_=Phase::Queued; // no SD access here
  }

  // Call only from ThesisSDLogger's main-loop owner. sdOwnerGranted must mean
  // exclusive access is NOW available, not just that SD was once mounted.
  // When owner/priority defers, no File/Volume method is invoked, including
  // size()/close(); permission revocation is latched until owner is restored.
  template<class Gate> Phase poll(const Gate& gate,bool sdOwnerGranted,
                                   bool urgentOrRadio,size_t budget=DTNF24D::kChunkBytes) {
    if(phase_==Phase::Idle||phase_==Phase::Failed)return phase_;
    if(phase_==Phase::Verified){
      char previous[DTNF06ClosedReplay::kPathCapacity]{};
      if(!DTNF06ClosedReplay::pathFor(stream_,previous,sizeof(previous)) ||
         !gate.allowClosedJournal(stream_,previous)){
        last_=DTNF24D::Result::Denied;
        return phase_=Phase::Denied;
      }
      return phase_; // no fresh I/O or authorization grant; scan-time status only
    }
    if(phase_==Phase::Denied){
      if(needsClose_&&sdOwnerGranted){job_.clear();needsClose_=false;}
      return phase_;
    }
    char path[DTNF06ClosedReplay::kPathCapacity]{};
    // An invalid path cannot be treated as a successful certificate.
    if(!DTNF06ClosedReplay::pathFor(stream_,path,sizeof(path)) ||
       !gate.allowClosedJournal(stream_,path)) {
      last_=DTNF24D::Result::Denied;
      if(sdOwnerGranted)job_.clear();
      else needsClose_=begun_;
      return phase_=Phase::Denied;
    }
    if(!sdOwnerGranted)return phase_=Phase::WaitingForOwner;
    if(urgentOrRadio)return phase_=Phase::Preempted;
    if(!volume_){last_=DTNF24D::Result::ReadError;return phase_=Phase::Failed;}
    if(!begun_){
      last_=job_.begin(*volume_,stream_,gate);
      if(last_!=DTNF24D::Result::Running){
        return phase_= last_==DTNF24D::Result::Denied ? Phase::Denied : Phase::Failed;
      }
      begun_=true;
    }
    // F24D caps the requested budget to <= 512 bytes, and rechecks the gate.
    last_=job_.step(gate,false,budget);
    switch(last_){
      case DTNF24D::Result::Running:return phase_=Phase::Hashing;
      case DTNF24D::Result::Verified:return phase_=Phase::Verified;
      case DTNF24D::Result::Denied:return phase_=Phase::Denied;
      default:return phase_=Phase::Failed;
    }
  }

  // Reuse only when the caller has actual SD ownership, since releasing a
  // live File handle may touch the SD driver. No automatic retry after faults.
  bool reset(bool sdOwnerGranted){
    if(!sdOwnerGranted)return false;
    job_.clear();volume_=nullptr;stream_=Stream{};begun_=false;
    needsClose_=false;last_=DTNF24D::Result::Idle;phase_=Phase::Idle;
    return true;
  }
  Phase phase()const{return phase_;}
  DTNF24D::Result detail()const{return last_;}
  uint64_t hashedBytes()const{return job_.bytesProcessed();}
  // A match is valid only at the time of scan, subject to EXTERNAL ongoing
  // immutable-media proof. No replay, backend acceptance, or ACK occurs here.
  const Stream* verifiedAtScan()const{return phase_==Phase::Verified?&stream_:nullptr;}
private:
  DTNF24D::Job<File,Volume> job_{};
  Volume* volume_=nullptr;
  Stream stream_{};
  Phase phase_=Phase::Idle;
  DTNF24D::Result last_=DTNF24D::Result::Idle;
  bool begun_=false,needsClose_=false;
};
} // namespace DTNF24E
