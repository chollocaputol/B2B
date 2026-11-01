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
 bool uptimeObserved=false;
 uint32_t recent[kRecentSequences]{};
 size_t cursor=0, recentCount=0;
 int16_t rssi=0;
 float snr=0;
 bool gpsValid=false;
 double latitude=0,longitude=0;
 uint8_t weatherState=0;
 bool active(uint32_t now) const { return used && uint32_t(now-lastSeenMs)<=kActiveMs; }
 uint32_t missingEstimate() const {
   if(!unique) return 0;
   const uint64_t span=uint64_t(highestSequence)-firstSequence+1;
   return span>unique ? uint32_t(span-unique) : 0;
 }
 float pdrEstimate() const {
   if(!unique) return 0.0f;
   const uint64_t span=uint64_t(highestSequence)-firstSequence+1;
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
   // PHASE 3G-3: restart-aware per-peer telemetry session.
   // Require BOTH decreasing remote uptime and decreasing sequence to confirm
   // a new sender session. A small uptime decrease alone can be packet reordering.
   bool reboot=false;
   if(!created && p->uptimeObserved &&
      remoteUptimeSec + uint64_t(2) < p->lastRemoteUptimeSec &&
      seq < p->highestSequence) {
     reboot=true;
     const uint32_t oldSession=p->session;
     const uint32_t vesselId=p->id;
     *p=Peer{};
     p->used=true;p->id=vesselId;
     p->session=oldSession+1;
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
   // Only retain last 64 sequence IDs; older replay can eventually be counted
   // as an out-of-window arrival, so this is NOT an authentication mechanism.
   for(size_t i=0;i<p->recentCount;++i)
     if(p->recent[i]==seq) {++p->duplicates;return Result::Duplicate;}
   p->recent[p->cursor]=seq;
   p->cursor=(p->cursor+1)%kRecentSequences;
   if(p->recentCount<kRecentSequences) ++p->recentCount;
   if(p->unique==0) p->firstSequence=p->highestSequence=seq;
   else {
     if(seq<p->firstSequence) p->firstSequence=seq;
     if(seq>p->highestSequence) p->highestSequence=seq;
   }
   ++p->unique;
   return created ? Result::NewPeer : (reboot ? Result::Restart : Result::Unique);
 }
};
} // namespace MultiPeerTelemetry
