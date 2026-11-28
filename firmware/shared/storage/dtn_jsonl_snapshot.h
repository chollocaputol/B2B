#pragma once
// Host-only, read-only framing model for THESIS F06 JSONL sessions.
// No SD, Arduino, Wi-Fi, JSON parsing, upload or checkpoint writes.
// The final incomplete line is never acknowledged or silently discarded.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace DTNJsonlSnapshot {
static constexpr size_t kMaxLineBytes = 1024U;
enum class Journal : uint8_t { SosEvents, Telemetry, Diagnostics };
enum class Outcome : uint8_t {
  Record, End, IncompleteTail, Oversize, Malformed, IoError, InvalidInput
};
struct Cursor {
  uint64_t offset = 0;
  uint64_t ordinal = 0; // per-file, zero-based record index
};
struct Record {
  uint64_t start = 0;
  uint64_t end = 0;     // first byte after the newline (exclusive)
  uint64_t ordinal = 0;
  size_t length = 0;    // excludes newline and optional CR
  uint32_t fnv1a = 0; // non-cryptographic diagnostic checksum only
};
inline const char* filename(Journal j) {
  switch (j) {
    case Journal::SosEvents: return "sos_events.jsonl";
    case Journal::Telemetry: return "telemetry.jsonl";
    case Journal::Diagnostics: return "diagnostics.jsonl";
  }
  return nullptr; // no arbitrary filename paths
}
inline bool validSession(uint32_t vessel, const char* id) {
  if (vessel < 1U || vessel > 9999U || !id || strlen(id)!=8U || id[0]!='S') return false;
  for (unsigned i=1U; i<8U; ++i) {
    const char c=id[i];
    if (!((c>='0'&&c<='9')||(c>='A'&&c<='F'))) return false;
  }
  return id[8]=='\0';
}
inline uint32_t fnv(const char* data,size_t length) {
  uint32_t h=2166136261UL;
  for(size_t i=0;i<length;++i)
    h=(h ^ static_cast<uint8_t>(data[i]))*16777619UL;
  return h;
}
inline bool framedObject(const char* data,size_t n) {
  if(n<2U || data[0]!='{' || data[n-1U]!='}') return false;
  // Only a bounded JSONL framing check; not JSON syntax/field validation.
  for(size_t i=0;i<n;++i){
    const uint8_t c=static_cast<uint8_t>(data[i]);
    if(c<0x20U || c==0x7FU)return false;
  }
  return true;
}
// Source must provide: uint64_t size() const;
// bool readByte(uint64_t offset, uint8_t& byte) const;
// snapshotBytes is a caller-owned immutable size captured for a *closed*
// F06 session, not an active journal that is being appended concurrently.
// On any non-Record result the cursor is unchanged. Record bytes are preserved.
template<class Source>
Outcome next(const Source& source,uint64_t snapshotBytes,Cursor& cursor,
             char* output,size_t capacity,Record& result) {
  if(!output || capacity<3U || capacity>kMaxLineBytes+1U ||
     cursor.offset>snapshotBytes)return Outcome::InvalidInput;
  if(snapshotBytes>source.size())return Outcome::IoError;
  if(cursor.offset==snapshotBytes)return Outcome::End;
  uint64_t pos=cursor.offset;
  size_t length=0;
  while(pos<snapshotBytes){
    uint8_t byte=0;
    if(!source.readByte(pos,byte))return Outcome::IoError;
    ++pos;
    if(byte==static_cast<uint8_t>('\n')){
      if(length && output[length-1U]=='\r')--length; // CRLF only
      if(!framedObject(output,length))return Outcome::Malformed;
      output[length]='\0';
      result.start=cursor.offset;
      result.end=pos;
      result.ordinal=cursor.ordinal;
      result.length=length;
      result.fnv1a=fnv(output,length);
      cursor.offset=pos;
      ++cursor.ordinal;
      return Outcome::Record;
    }
    if(length>=capacity-1U || length>=kMaxLineBytes)return Outcome::Oversize;
    output[length++]=static_cast<char>(byte);
  }
  return Outcome::IncompleteTail;
}
} // namespace DTNJsonlSnapshot
