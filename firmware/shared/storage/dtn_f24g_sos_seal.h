#pragma once
// F24G: BUILD-ONLY, pure-C++ SOS session-close/seal contract.
// NO physical logger retirement, SD port, snapshot, card-CID proof, RF,
// Wi-Fi, HTTP, sidecar writer, or F24F independent approval is implemented.
// A seal with CRC is accidental-corruption evidence, NOT a signature and
// NOT proof that the physical journal stays immutable after a hash scan.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dtn_f24f_sos_closed_lease.h"
#include "dtn_f24d_closed_sha256_job.h"

namespace DTNF24G {
using Stream = DTNAckLedger::Stream;
static constexpr uint32_t kMagic = 0x47343246UL; // ASCII F24G in LE
static constexpr uint32_t kVersion = 1U;
static constexpr size_t kSealBytes = 106U;
static constexpr size_t kSealPathBytes = 104U;

// The sole SD logger MUST generate these facts after retiring a previous
// session. No current firmware supplies them. Synthesizing flags is unsafe.
struct CloseFacts {
  Stream stream{};
  uint64_t fileBytes = 0U;
  uint64_t mediaTag = 0U;      // independently verified media binding
  uint64_t mountEpoch = 0U;    // current mount, not persisted in seal
  uint64_t sourceEpoch = 0U;   // must change on every possible source write
  uint64_t closeSerial = 0U;   // durable monotonic closed-session generation
  uint64_t ownerLeaseSerial = 0U;
  bool retiredSession = false;
  bool appendQueueDrained = false;
  bool flushSucceeded = false;
  bool allWriterHandlesClosed = false;
  bool finalNewlineVerified = false;
  bool protectedAgainstWrites = false;
};
struct Seal {
  uint32_t magic = 0U;
  uint32_t version = 0U;
  Stream stream{};
  uint64_t fileBytes = 0U;
  uint64_t mediaTag = 0U;
  uint64_t sourceEpoch = 0U;
  uint64_t closeSerial = 0U;
  uint32_t crc = 0U;
};
enum class Result : uint8_t {
  ReadyCandidate, NotAuthorized, BadStream, BadClose, WrongOwner,
  HashNotVerified, InvalidSeal, ChangedSource
};
enum class Load : uint8_t {
  Ready, Missing, Corrupt, WrongBinding, Denied, IoError
};
enum class Publish : uint8_t {
  WrittenAndReadBack, IdenticalExisting, Denied, Conflict, IoError
};

inline bool sameSeal(const Seal& a,const Seal& b) {
  return a.magic==b.magic && a.version==b.version &&
    DTNAckLedger::sameStream(a.stream,b.stream) &&
    a.fileBytes==b.fileBytes && a.mediaTag==b.mediaTag &&
    a.sourceEpoch==b.sourceEpoch && a.closeSerial==b.closeSerial &&
    a.crc==b.crc;
}
inline uint32_t checksum(const Seal& s) {
  uint32_t crc=0xFFFFFFFFUL;
  DTNAckLedger::crc32(crc,s.magic);
  DTNAckLedger::crc32(crc,s.version);
  for(uint8_t b:s.stream.uploader.bytes)DTNAckLedger::crcByte(crc,b);
  DTNAckLedger::crc32(crc,s.stream.vessel);
  for(char b:s.stream.session)DTNAckLedger::crcByte(crc,static_cast<uint8_t>(b));
  DTNAckLedger::crcByte(crc,static_cast<uint8_t>(s.stream.journal));
  for(uint8_t b:s.stream.sha256)DTNAckLedger::crcByte(crc,b);
  DTNAckLedger::crc64(crc,s.fileBytes);
  DTNAckLedger::crc64(crc,s.mediaTag);
  DTNAckLedger::crc64(crc,s.sourceEpoch);
  DTNAckLedger::crc64(crc,s.closeSerial);
  return crc^0xFFFFFFFFUL;
}
inline bool valid(const Seal& s) {
  return s.magic==kMagic && s.version==kVersion &&
    DTNAckLedger::validStream(s.stream) &&
    s.stream.journal==DTNAckLedger::Journal::SosEvents &&
    s.fileBytes<=DTNF06ClosedReplay::kMaxJournalBytes &&
    s.mediaTag!=0U && s.sourceEpoch!=0U && s.closeSerial!=0U &&
    s.crc==checksum(s);
}
inline bool sealPath(const Stream& stream,char* out,size_t capacity){
  if(!out||capacity==0U||!DTNAckLedger::validStream(stream) ||
     stream.journal!=DTNAckLedger::Journal::SosEvents)return false;
  const int n=snprintf(out,capacity,"/THESIS/DTNSYNC/V%03lu/%s/sos.close",
                       static_cast<unsigned long>(stream.vessel),stream.session);
  return n>0 && static_cast<size_t>(n)<capacity;
}
inline bool ownerContinuity(const Stream& stream,const CloseFacts& facts,
                            const DTNF24F::OwnerNow& now) {
  return DTNAckLedger::sameStream(stream,facts.stream) &&
    facts.mediaTag!=0U && facts.mountEpoch!=0U && facts.sourceEpoch!=0U &&
    facts.closeSerial!=0U && facts.ownerLeaseSerial!=0U &&
    now.mounted && now.mediaIdentityVerified && now.soleMainLoopOwner &&
    now.sourceReadProtected && !now.writerActive && !now.pendingAppends &&
    !now.currentSession && !now.urgentOrRadio &&
    now.vessel==stream.vessel &&
    DTNAckLedger::sameId(now.uploader,stream.uploader) &&
    now.mediaTag==facts.mediaTag && now.mountEpoch==facts.mountEpoch &&
    now.sourceEpoch==facts.sourceEpoch &&
    now.exclusiveLeaseSerial==facts.ownerLeaseSerial;
}
inline bool closeComplete(const CloseFacts& facts) {
  return facts.retiredSession && facts.appendQueueDrained &&
    facts.flushSucceeded && facts.allWriterHandlesClosed &&
    facts.finalNewlineVerified && facts.protectedAgainstWrites &&
    facts.fileBytes<=DTNF06ClosedReplay::kMaxJournalBytes;
}
// Accept F24D's VERIFIED result ONLY after the scan completed using the
// SAME exclusive source and without any unobserved append/mount/media change.
// The returned Seal is a CANDIDATE, not F24F ClosureProof/permission.
inline Result prepare(const Stream& stream,const CloseFacts& facts,
                      const DTNF24F::OwnerNow& now,
                      DTNF24D::Result freshHashStatus,uint64_t hashedFileBytes,
                      Seal& out) {
  out=Seal{};
  if(!DTNAckLedger::validStream(stream) ||
     stream.journal!=DTNAckLedger::Journal::SosEvents)return Result::BadStream;
  if(!closeComplete(facts))return Result::BadClose;
  if(!ownerContinuity(stream,facts,now))return Result::WrongOwner;
  if(freshHashStatus!=DTNF24D::Result::Verified)
    return Result::HashNotVerified;
  if(hashedFileBytes!=facts.fileBytes)return Result::ChangedSource;
  Seal candidate{};
  candidate.magic=kMagic;candidate.version=kVersion;
  candidate.stream=stream;candidate.fileBytes=facts.fileBytes;
  candidate.mediaTag=facts.mediaTag;candidate.sourceEpoch=facts.sourceEpoch;
  candidate.closeSerial=facts.closeSerial;
  candidate.crc=checksum(candidate);
  out=candidate;return Result::ReadyCandidate;
}
// After a reboot, the caller must independently establish fresh owner/media
// facts and recompute SHA256 from ALL source bytes again. A stored CRC/digest
// is never a substitute for those checks or F24F independent approval.
inline Result recheckAtScan(const Seal& stored,const Stream& stream,
                            const CloseFacts& current,
                            const DTNF24F::OwnerNow& now,
                            DTNF24D::Result newHashStatus,
                            uint64_t newHashedBytes) {
  if(!valid(stored))return Result::InvalidSeal;
  if(!DTNAckLedger::sameStream(stored.stream,stream))return Result::BadStream;
  if(!closeComplete(current))return Result::BadClose;
  if(!ownerContinuity(stream,current,now))return Result::WrongOwner;
  if(stored.mediaTag!=current.mediaTag ||
     stored.sourceEpoch!=current.sourceEpoch ||
     stored.closeSerial!=current.closeSerial)return Result::ChangedSource;
  if(newHashStatus!=DTNF24D::Result::Verified)
    return Result::HashNotVerified;
  if(stored.fileBytes!=current.fileBytes ||
     stored.fileBytes!=newHashedBytes)return Result::ChangedSource;
  return Result::ReadyCandidate; // scan-time integrity ONLY; NO REPLAY GRANT
}
inline void put32(uint8_t* out,size_t& at,uint32_t v){
  for(unsigned i=0U;i<4U;++i)out[at++]=static_cast<uint8_t>(v>>(8U*i));
}
inline void put64(uint8_t* out,size_t& at,uint64_t v){
  put32(out,at,static_cast<uint32_t>(v));
  put32(out,at,static_cast<uint32_t>(v>>32U));
}
inline uint32_t get32(const uint8_t* in,size_t& at){
  uint32_t v=0U;
  for(unsigned i=0U;i<4U;++i)v|=static_cast<uint32_t>(in[at++])<<(8U*i);
  return v;
}
inline uint64_t get64(const uint8_t* in,size_t& at){
  const uint64_t lo=get32(in,at),hi=get32(in,at);
  return lo|(hi<<32U);
}
// Wire is explicitly little-endian and packed by field (no struct padding).
inline bool encode(const Seal& s,uint8_t out[kSealBytes]) {
  if(!out||!valid(s))return false;
  size_t at=0U;
  put32(out,at,s.magic);put32(out,at,s.version);
  for(uint8_t b:s.stream.uploader.bytes)out[at++]=b;
  put32(out,at,s.stream.vessel);
  for(char b:s.stream.session)out[at++]=static_cast<uint8_t>(b);
  out[at++]=static_cast<uint8_t>(s.stream.journal);
  for(uint8_t b:s.stream.sha256)out[at++]=b;
  put64(out,at,s.fileBytes);put64(out,at,s.mediaTag);
  put64(out,at,s.sourceEpoch);put64(out,at,s.closeSerial);
  put32(out,at,s.crc);
  return at==kSealBytes;
}
inline bool decode(const uint8_t* in,size_t length,Seal& out) {
  out=Seal{};
  if(!in||length!=kSealBytes)return false;
  size_t at=0U;Seal s{};
  s.magic=get32(in,at);s.version=get32(in,at);
  for(uint8_t& b:s.stream.uploader.bytes)b=in[at++];
  s.stream.vessel=get32(in,at);
  for(char& b:s.stream.session)b=static_cast<char>(in[at++]);
  s.stream.journal=static_cast<DTNAckLedger::Journal>(in[at++]);
  for(uint8_t& b:s.stream.sha256)b=in[at++];
  s.fileBytes=get64(in,at);s.mediaTag=get64(in,at);
  s.sourceEpoch=get64(in,at);s.closeSerial=get64(in,at);
  s.crc=get32(in,at);
  if(at!=kSealBytes||!valid(s))return false;
  out=s;return true;
}

// This Store is a PORTABLE CONTRACT for MOCK tests. NO physical port is
// provided. Any later real port MUST enforce exact allowlisted sidecar path,
// actual SD owner, media identity, flush/close/readback, and crash safety.
// Gate::permitSealSidecar(Stream) is rechecked before each port action.
// Port::read(path,buf,capacity,outBytes,exists), ::writeOnce(path,buf,len).
// An uncertain write and malformed existing seal fail-stop. Never overwrite.
template<class Port,class Gate> class Store {
public:
  Store(Port& port,const Gate& gate):port_(port),gate_(gate){}
  Load load(const Stream& stream,Seal& out)const {
    out=Seal{};
    char path[kSealPathBytes]{};
    if(!sealPath(stream,path,sizeof(path)))return Load::WrongBinding;
    if(!gate_.permitSealSidecar(stream))return Load::Denied;
    uint8_t bytes[kSealBytes]{};size_t received=0U;bool exists=false;
    if(!port_.read(path,bytes,sizeof(bytes),received,exists))return Load::IoError;
    if(!gate_.permitSealSidecar(stream))return Load::Denied;
    if(!exists)return Load::Missing;
    Seal candidate{};
    if(!decode(bytes,received,candidate))return Load::Corrupt;
    if(!DTNAckLedger::sameStream(candidate.stream,stream))return Load::WrongBinding;
    out=candidate;return Load::Ready;
  }
  Publish publishOnce(const Seal& requested){
    if(!valid(requested))return Publish::Conflict;
    if(!gate_.permitSealSidecar(requested.stream))return Publish::Denied;
    Seal already{};
    const Load found=load(requested.stream,already);
    if(found==Load::Ready)
      return sameSeal(already,requested)?Publish::IdenticalExisting:Publish::Conflict;
    if(found==Load::Denied)return Publish::Denied;
    if(found!=Load::Missing)return found==Load::IoError?Publish::IoError:Publish::Conflict;
    if(!gate_.permitSealSidecar(requested.stream))return Publish::Denied;
    uint8_t bytes[kSealBytes]{};
    if(!encode(requested,bytes))return Publish::Conflict;
    char path[kSealPathBytes]{};
    if(!sealPath(requested.stream,path,sizeof(path)))return Publish::Conflict;
    if(!port_.writeOnce(path,bytes,sizeof(bytes)))return Publish::IoError;
    // Existence and byte-for-byte logical readback are mandatory.
    Seal saved{};
    const Load checked=load(requested.stream,saved);
    if(checked!=Load::Ready)return checked==Load::Denied?Publish::Denied:Publish::IoError;
    return sameSeal(saved,requested)?Publish::WrittenAndReadBack:Publish::Conflict;
  }
private:
  Port& port_;
  const Gate& gate_;
};
// Explicitly denies EVERY SD sidecar operation in firmware preview.
struct NoPhysicalSealPermission {
  bool permitSealSidecar(const Stream&)const{return false;}
};
} // namespace DTNF24G
