#pragma once
// F24B read-only replay seam for CLOSED F06 journal snapshots; host-only.
// No SD mount, SPI, network, HTTP, sidecar writes or upload permission here.
// A future on-device Volume must open FILE_READ only; a real Digest verifier
// must compute SHA256 from all source bytes; Gate must enforce hardware,
// vessel ownership, closure and SD-owner consent. Mock tests grant NONE.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dtn_ack_ledger.h"
#include "dtn_jsonl_snapshot.h"
#include "dtn_f06_record_gate.h"

namespace DTNF06ClosedReplay {
static constexpr uint64_t kMaxJournalBytes = 1048576U; // existing F06 export bound
static constexpr size_t kPathCapacity = 96U;

enum class Open : uint8_t {
  Ready, BadStream, NotAuthorized, Missing, Oversize, DigestMismatch,
  ChangedSource, LedgerMismatch
};
enum class Read : uint8_t {
  CandidateUnverified, End, WindowBlocked, IncompleteTail, Oversize,
  Malformed, WrongVessel, IoError, SourceChanged, LedgerMismatch, NotOpen, NotAuthorized
};
struct Candidate {
  DTNJsonlSnapshot::Record frame{};
  DTNF06RecordGate::Fields fields{};
};
inline bool pathFor(const DTNAckLedger::Stream& stream, char* out, size_t capacity) {
  if (!out || !capacity || !DTNAckLedger::validStream(stream)) return false;
  const char* name = stream.journal==DTNAckLedger::Journal::SosEvents ?
                     "sos_events.jsonl" : "telemetry.jsonl";
  const int count=snprintf(out,capacity,"/THESIS/F06TEST/V%03lu/%s/%s",
      static_cast<unsigned long>(stream.vessel),stream.session,name);
  return count>0 && static_cast<size_t>(count)<capacity;
}
inline DTNJsonlSnapshot::Journal journalOf(DTNAckLedger::Journal type) {
  return type==DTNAckLedger::Journal::SosEvents ?
     DTNJsonlSnapshot::Journal::SosEvents : DTNJsonlSnapshot::Journal::Telemetry;
}

// File: bool isOpen() const; uint64_t size() const;
//       bool readAt(uint64_t offset,uint8_t& out) const; void close().
// Volume: bool openReadOnly(const char* path, File& out).
// Gate: bool allowClosedJournal(const Stream&,const char* exactPath) const;
//       bool allowClosedJournalBytes(const Stream&,const char*,uint64_t) const.
// Digest: bool verifyCompleteSha256(const File&,uint64_t bytes,const uint8_t expected[32]) const.
// Ledger: F24A Ledger<Store>, already loaded, bound to this exact stream.
// Return CandidateUnverified is never automatic production upload approval.
template<class File, class Volume, class Gate, class Digest, class Ledger>
class Reader {
public:
  ~Reader(){close();}
  Reader(const Reader&)=delete;
  Reader& operator=(const Reader&)=delete;
  Reader()=default;

  Open begin(Volume& volume,const DTNAckLedger::Stream& stream,
             const Gate& gate,const Digest& digest,const Ledger& ledger) {
    close();
    if (!pathFor(stream,path_,sizeof(path_))) return Open::BadStream;
    if (!ledger.ready() || !DTNAckLedger::sameStream(ledger.state().stream,stream))
      return Open::LedgerMismatch;
    if (!gate.allowClosedJournal(stream,path_)) return Open::NotAuthorized;
    if (!volume.openReadOnly(path_,file_) || !file_.isOpen()) {
      file_.close();return Open::Missing;
    }
    const uint64_t bytes=file_.size();
    if (bytes>kMaxJournalBytes) {close();return Open::Oversize;}
    if (!gate.allowClosedJournalBytes(stream,path_,bytes)) {
      close();return Open::NotAuthorized;
    }
    if (!digest.verifyCompleteSha256(file_,bytes,stream.sha256)) {
      close();return Open::DigestMismatch;
    }
    if (file_.size()!=bytes) {close();return Open::ChangedSource;}
    bytes_=bytes;stream_=stream;cursor_=DTNJsonlSnapshot::Cursor{};
    opened_=true;
    return Open::Ready;
  }
  void close(){
    file_.close();opened_=false;bytes_=0;cursor_=DTNJsonlSnapshot::Cursor{};
    path_[0]='\0';stream_=DTNAckLedger::Stream{};revoked_=false;
  }
  bool opened()const{return opened_ && file_.isOpen();}
  const char* path()const{return opened()?path_:"";}
  uint64_t frozenBytes()const{return opened()?bytes_:0U;}
  DTNJsonlSnapshot::Cursor cursor()const{return cursor_;}

  // Gate is pure-RAM and MUST be rechecked before ANY File/Volume read.
  // After permission loss this reader is permanently revoked until close() +
  // begin() under a fresh lease. Closing an SD File requires the SD owner;
  // caller must NOT close or destroy a live reader after ownership is lost.
  Read next(const Gate& gate,const Ledger& ledger,char* json,size_t capacity,Candidate& candidate) {
    candidate=Candidate{};
    if (!opened_)return Read::NotOpen;
    if (revoked_)return Read::NotAuthorized;
    if (!gate.allowClosedJournal(stream_,path_)) {
      revoked_=true;return Read::NotAuthorized;
    }
    if (!file_.isOpen())return Read::NotOpen;
    if (!ledger.ready()||!DTNAckLedger::sameStream(ledger.state().stream,stream_))
      return Read::LedgerMismatch;
    if (file_.size()!=bytes_){revoked_=true;return Read::SourceChanged;}
    if (!json || capacity<3U || capacity>DTNJsonlSnapshot::kMaxLineBytes+1U)
      return Read::IoError;
    Source source{file_,gate,stream_,path_};
    for (;;) {
      // Revalidate between skipped ACKed records and before the next SD read.
      if (!gate.allowClosedJournal(stream_,path_)) {
        revoked_=true;return Read::NotAuthorized;
      }
      if (cursor_.offset==bytes_) {
        if (cursor_.ordinal<ledger.firstPending())return Read::LedgerMismatch;
        const uint64_t mask=ledger.ackMask();
        for (unsigned bit=0U;bit<DTNAckLedger::kAckWindow;++bit) {
          if (((mask>>bit)&UINT64_C(1))!=0U &&
              ledger.firstPending()+bit>=cursor_.ordinal)
            return Read::LedgerMismatch; // persisted ACK past immutable EOF
        }
        return Read::End;
      }
      if (cursor_.ordinal>=ledger.firstPending() &&
          cursor_.ordinal-ledger.firstPending()>=DTNAckLedger::kAckWindow)
        return Read::WindowBlocked;
      auto scratch=cursor_;
      DTNJsonlSnapshot::Record frame{};
      const auto status=DTNJsonlSnapshot::next(source,bytes_,scratch,json,capacity,frame);
      // A gate may be revoked *during* byte reads, not only between calls.
      // The scratch cursor is not committed until this post-read check passes.
      if (!gate.allowClosedJournal(stream_,path_)) {
        revoked_=true;return Read::NotAuthorized;
      }
      if (status!=DTNJsonlSnapshot::Outcome::Record) {
        switch(status){
          case DTNJsonlSnapshot::Outcome::End:return Read::End;
          case DTNJsonlSnapshot::Outcome::IncompleteTail:return Read::IncompleteTail;
          case DTNJsonlSnapshot::Outcome::Oversize:return Read::Oversize;
          case DTNJsonlSnapshot::Outcome::Malformed:return Read::Malformed;
          default:return Read::IoError;
        }
      }
      DTNF06RecordGate::Fields fields{};
      const auto verdict=DTNF06RecordGate::inspect(
          journalOf(stream_.journal),stream_.vessel,json,frame.length,fields);
      if (verdict==DTNF06RecordGate::Verdict::WrongVessel)return Read::WrongVessel;
      if (verdict!=DTNF06RecordGate::Verdict::CandidateUnverified)
        return Read::Malformed; // fail closed: do not advance past an invalid record
      cursor_=scratch;
      if (frame.ordinal<ledger.firstPending() || ledger.acknowledged(frame.ordinal))
        continue; // skip only explicitly durable ACKs for this bound stream
      if (!ledger.eligible(frame.ordinal))return Read::WindowBlocked;
      candidate.frame=frame;candidate.fields=fields;
      return Read::CandidateUnverified;
    }
  }
private:
  struct Source {
    const File& file;
    const Gate& gate;
    const DTNAckLedger::Stream& stream;
    const char* path;
    uint64_t size()const{
      return gate.allowClosedJournal(stream,path)?file.size():0U;
    }
    bool readByte(uint64_t position,uint8_t& out)const{
      return gate.allowClosedJournal(stream,path)&&file.readAt(position,out);
    }
  };
  File file_{};
  bool opened_=false;
  bool revoked_=false;
  char path_[kPathCapacity]{};
  uint64_t bytes_=0;
  DTNAckLedger::Stream stream_{};
  DTNJsonlSnapshot::Cursor cursor_{};
};
} // namespace DTNF06ClosedReplay
