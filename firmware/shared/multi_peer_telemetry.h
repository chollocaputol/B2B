#pragma once
#include <stddef.h>
#include <stdint.h>

// Phase 5.2.2C-2E: RAM-only independent per-origin telemetry counters.
// A bounded, no-eviction table preserves existing active peers. PDR is an
// observed sequence-span estimate, not proof of RF-layer delivery reliability.
namespace MultiPeerTelemetry {
static constexpr size_t kMaxPeers = 10;
static constexpr size_t kRecentSequences = 64;
static constexpr uint32_t kActiveMs = 15000;
enum class Result : uint8_t { NewPeer, Unique, Duplicate, Restart, Full, Invalid };
inline const char* name(Result x) {
 switch(x) {
 case Result::NewPeer:return "NEW_PEER";
 case Result::Unique:return "UNIQUE";
 case Result::Duplicate:return "DUPLICATE";
 case Result::Restart:return "RESTART";
 case Result::Full:return "FULL";
 case Result::Invalid:return "INVALID";
 }
 return "UNKNOWN";
}
struct Peer {
 bool used=false;
 uint32_t id=0,received=0,unique=0,duplicates=0,firstSequence=0,highestSequence=0;
 uint32_t lastSeenMs=0;
 uint32_t lastRemoteUptimeSec=0, session=1;
 uint64_t sequenceSpan=0; // contiguous modular sequence range, including gaps
 bool restartCandidate=false;
 uint32_t candidateSequence=0,candidateUptime=0;
 bool uptimeObserved=false;
 // Sliding 64-sequence bitmap anchored at highestSequence. Bit 0 is newest.
 // Retains already-seen holes even if many later packets arrive.
 uint64_t seenWindow=0;
 int16_t rssi=0;
 float snr=0;
 bool gpsValid=false;
 double latitude=0,longitude=0;
 uint8_t weatherState=0;
 bool active(uint32_t now) const { return used && uint32_t(now-lastSeenMs)<=kActiveMs; }
 uint32_t missingEstimate() const {
   if(!unique) return 0;
   const uint64_t span=sequenceSpan;
   const uint64_t missing=span>unique ? span-unique : 0;
   return missing>UINT32_MAX ? UINT32_MAX : uint32_t(missing);
 }
 float pdrEstimate() const {
   if(!unique) return 0.0f;
   const uint64_t span=sequenceSpan;
   return span ? 100.0f*float(unique)/float(span) : 0.0f;
 }
};
class Table {
 Peer peers_[kMaxPeers]{};
public:
 const Peer* find(uint32_t id) const {
   for(const auto& p:peers_) if(p.used && p.id==id) return &p;
   return nullptr;
 }
 size_t count() const { size_t n=0;for(const auto& p:peers_)if(p.used)++n;return n; }
 Result observe(uint32_t id,uint32_t seq,uint32_t now,int16_t rssi,float snr,
                bool gps,double lat,double lon,uint8_t weather,
                uint32_t remoteUptimeSec) {
   if(id<1 || id>9999) return Result::Invalid;
   Peer* p=nullptr;
   for(auto& x:peers_) if(x.used && x.id==id) {p=&x;break;}
   bool created=false;
   if(!p) {
     for(auto& x:peers_) if(!x.used) {p=&x;created=true;break;}
     if(!p) return Result::Full;
     *p=Peer{};p->used=true;p->id=id;
   }
   // First classify modular sequence movement. A forward step across 0xffffffff
   // is NOT a reboot. Half the uint32 range is deliberately treated as stale.
   const bool ahead=!created && seq!=p->highestSequence &&
     uint32_t(seq-p->highestSequence)<0x80000000UL;
   const uint32_t backwards=created ? 0 : uint32_t(p->highestSequence-seq);
   const bool behind=!created && seq!=p->highestSequence && !ahead;
   // No wire-level boot nonce exists. Require two consecutive low-uptime,
   // increasing low-sequence observations before inferring a restart. A single
   // delayed old frame must never reset a session. This remains heuristic.
   const bool lowStart=behind && seq<=16UL && remoteUptimeSec<=30UL &&
     p->uptimeObserved && uint64_t(remoteUptimeSec)+5ULL<p->lastRemoteUptimeSec;
   bool reboot=false;
   uint32_t firstRebootSeq=0;
   if(p->restartCandidate && lowStart &&
      seq==p->candidateSequence+1UL &&
      remoteUptimeSec>=p->candidateUptime &&
      remoteUptimeSec<=p->candidateUptime+30UL) {
     reboot=true;
     firstRebootSeq=p->candidateSequence;
     const uint32_t oldSession=p->session;
     const uint32_t vesselId=p->id;
     *p=Peer{};
     p->used=true;p->id=vesselId;
     p->session=oldSession+1;
   } else if(lowStart && !p->restartCandidate) {
     p->restartCandidate=true;
     p->candidateSequence=seq;
     p->candidateUptime=remoteUptimeSec;
     // Quarantine ambiguous frame; do not corrupt PDR or live session.
     return Result::Duplicate;
   } else {
     p->restartCandidate=false;
   }
   if(behind && !reboot && backwards>=kRecentSequences) {
     // Too old to enter the bounded reordering window; never expand PDR span.
     return Result::Duplicate;
   }
   // On reordering, preserve the highest known sender uptime rather than
   // treating an older frame as the latest sender time.
   if(!p->uptimeObserved || reboot || remoteUptimeSec>p->lastRemoteUptimeSec)
      p->lastRemoteUptimeSec=remoteUptimeSec;
   p->uptimeObserved=true;
   ++p->received;
   p->lastSeenMs=now;
   p->rssi=rssi;p->snr=snr;p->gpsValid=gps;p->weatherState=weather;
   if(gps) {p->latitude=lat;p->longitude=lon;}
   // A 64-bit sequence-indexed receive window prevents re-counting a sequence
   // merely because it fell out of a FIFO of the last 64 arrivals.
   // Every sequence position stays represented until the highest advances
   // beyond the 64-position window. Positions outside it are ignored.
   if(p->unique==0) {
     p->firstSequence=reboot ? firstRebootSeq : seq;
     p->highestSequence=seq;
     p->sequenceSpan=reboot ? 2 : 1;
     p->seenWindow=reboot ? 3ULL : 1ULL; // first candidate + confirmed frame
     if(reboot) ++p->unique;
   } else if(seq==p->highestSequence) {
     ++p->duplicates;
     return Result::Duplicate;
   } else if(ahead) {
     const uint32_t step=uint32_t(seq-p->highestSequence);
     p->seenWindow=(step>=kRecentSequences ? 0ULL : p->seenWindow<<step)|1ULL;
     p->sequenceSpan+=step;
     p->highestSequence=seq;
   } else if(behind) {
     // backwards is guaranteed to be in [1,63] by the earlier guard.
     const uint64_t bit=1ULL<<backwards;
     if((p->seenWindow&bit)!=0ULL) {
       ++p->duplicates;
       return Result::Duplicate;
     }
     p->seenWindow|=bit;
     // A genuine earlier observation can expand the sequence span backwards.
     // Do so only for values inside the bounded reordering window.
     const uint32_t beforeFirst=uint32_t(p->firstSequence-seq);
     if(beforeFirst>0 && beforeFirst<kRecentSequences) {
       p->sequenceSpan+=beforeFirst;
       p->firstSequence=seq;
     }
   }
   ++p->unique;
   return created ? Result::NewPeer : (reboot ? Result::Restart : Result::Unique);
 }
};
} // namespace MultiPeerTelemetry
