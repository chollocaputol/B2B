#pragma once
// F25B/A-05: strict lifecycle adapter on the existing F24Z service and owner.
// Independent caller-supplied reservations/closure claims; no authority provider.
// Single serialized service caller, immutable witness snapshot during each call.
// Host fixtures only for positive paths; no card, authentication or power-fail proof.
#include "dtn_f24z_session_service.h"
namespace DTNF25B {
struct Closure {
 uint64_t media=0,closedEpoch=0,closeSerial=0;
 uint32_t vessel=0;
 size_t packetBytes=0,receiptBytes=0; // zero means file must be absent
};
inline bool same(const Closure& a,const Closure& b){return a.media==b.media && a.closedEpoch==b.closedEpoch && a.closeSerial==b.closeSerial && a.vessel==b.vessel && a.packetBytes==b.packetBytes && a.receiptBytes==b.receiptBytes;}
struct DenyWitness : DTNF24O::DenyWitness {
 bool fresh(uint32_t,const char*)const{return false;}
 bool closed(const char*,Closure&)const{return false;}
 bool closedCount(size_t&)const{return false;}
};
template<class Witness> struct Policy {
 const Witness& w;uint32_t vessel=0;uint64_t media=0,epoch=0,serial=0;
 char current[64]{};DTNF24W::SessionSnapshot snapshot{};Closure claims[12]{};
 bool prepared=false,locked=false;
#if THESIS_DTN_F26B_BUILD_ONLY == 1
 size_t reservedClosedCount=0;
 bool reservedInventory()const{size_t n=0;return w.closedCount(n) && n==reservedClosedCount && n<DTNF26B::kClosedLimit;}
#endif
 explicit Policy(const Witness& witness):w(witness){}
 bool identity()const{const auto e=w.view();return e.vessel==vessel && e.mediaTag==media && e.mountEpoch==epoch && e.ownerSerial==serial && e.mounted && e.independentOwner && e.exclusive && e.f23dClosed && e.otherWriterClosed && e.closedSessionsVerified;}
 bool reserve(uint32_t v,const char* dir){
  if(prepared)return false;
  char journal[96]{};const auto e=w.view();
  if(!DTNF24M::sandboxPath(journal,sizeof(journal),dir,v) || !DTNF24O::validEvidence(e,v) || !w.fresh(v,dir))return false;
#if THESIS_DTN_F26B_BUILD_ONLY == 1
  // Reserve one of the 12 future closed-session slots before mount/mkdir.
  if(!w.closedCount(reservedClosedCount) || reservedClosedCount>=DTNF26B::kClosedLimit)return false;
#endif
  vessel=v;media=e.mediaTag;epoch=e.mountEpoch;serial=e.ownerSerial;strncpy(current,dir,sizeof(current)-1);prepared=true;return fresh();
 }
 bool fresh()const{
#if THESIS_DTN_F26B_BUILD_ONLY == 1
  if(!reservedInventory())return false;
#endif
  return prepared && identity() && DTNF24O::validEvidence(w.view(),vessel) && w.fresh(vessel,current) && identity() && DTNF24O::validEvidence(w.view(),vessel);}
 bool valid(const Closure& c)const{
  return c.vessel==vessel && c.media==media && c.closedEpoch && c.closedEpoch<epoch && c.closeSerial &&
   c.packetBytes<=DTNF24N::kMaxJournalBytes && c.packetBytes%DTNF24M::kFrameBytes==0 &&
   c.receiptBytes<=DTNF24N::kMaxReceiptJournalBytes && c.receiptBytes%DTNF24N::kReceiptBytes==0;
 }
 bool lock(const DTNF24W::SessionSnapshot& s){
  size_t count=0;
#if THESIS_DTN_F26B_BUILD_ONLY == 1
  if(!reservedInventory() || s.count!=reservedClosedCount)return false;
#endif
  if(locked || s.count>12 || !identity() || !w.closedCount(count) || count!=s.count)return false;
  for(size_t i=0;i<s.count;++i){
   char path[96]{};
   if(!DTNF24M::sandboxPath(path,sizeof(path),s.paths[i],vessel) || !strcmp(current,s.paths[i]) || !w.closed(s.paths[i],claims[i]) || !valid(claims[i]))return false;
   for(size_t j=0;j<i;++j)if(!strcmp(s.paths[i],s.paths[j]))return false;
  }
  snapshot=s;locked=true;return stable();
 }
 bool stable()const{
  size_t count=0;
  if(!identity() || !locked || !w.closedCount(count) || count!=snapshot.count)return false;
  for(size_t i=0;i<snapshot.count;++i){Closure c{};if(!w.closed(snapshot.paths[i],c) || !same(c,claims[i]))return false;}
  return identity();
 }
 bool expected(const char* path,bool& old,size_t& n)const{
  bool isCurrent=false;n=0;old=false;
  if(!identity() || !DTNF24O::supportedPath(path,current,vessel,isCurrent))return false;
  if(isCurrent)return true;
  old=true;if(!stable())return false;
  for(size_t i=0;i<snapshot.count;++i){
   char p[96]{};
   if(DTNF24M::sandboxPath(p,sizeof(p),snapshot.paths[i],vessel)&&!strcmp(p,path)){n=claims[i].packetBytes;return true;}
   if(DTNF24N::receiptPath(p,sizeof(p),snapshot.paths[i],vessel)&&!strcmp(p,path)){n=claims[i].receiptBytes;return true;}
  }
  return false;
 }
};
// Adapter is held privately by F24W MountOwner, not an alternate filesystem owner.
template<class Backend,class Witness> class CheckedBackend final {
 Policy<Witness>& p_;Backend backend_;bool fault_=false;bool old_[2]{};size_t bytes_[2]{},limit_[2]{};
 bool fail(){fault_=true;return false;}
 bool live(bool old=false)const{const auto e=p_.w.view();return !fault_ && p_.identity() && DTNF24O::validEvidence(e,p_.vessel) && (!old || (!e.urgentPending && p_.stable()));}
 public:
 template<class... Args> CheckedBackend(Policy<Witness>& p,Args&&... args):p_(p),backend_(std::forward<Args>(args)...){ }
 bool mount(){return (!fault_ && p_.fresh() && backend_.mount() && p_.fresh() && backend_.createSession(p_.vessel,p_.current) && p_.fresh()) || fail();}
 bool exists(const char* path,bool& found){
  bool old=false;size_t n=0;found=false;
  return (live() && p_.expected(path,old,n) && backend_.exists(path,found) && live(old) && (!old || found==(n!=0))) || fail();
 }
 bool open(uint8_t s,const char* path,DTNF24W::Mode m){
  bool old=false;size_t n=0;
  if(s>=2 || !live() || !p_.expected(path,old,n) || (old && (m!=DTNF24W::Mode::Read || !n)))return fail();
  if(!backend_.open(s,path,m) || !live(old))return fail();
  old_[s]=old;bytes_[s]=n;
  limit_[s]=strstr(path,"/dtn_sos_packets.f24m")?DTNF24N::kMaxJournalBytes:DTNF24N::kMaxReceiptJournalBytes;size_t actual=0;
  return (!old || (backend_.size(s,actual) && actual==n && live(true))) || fail();
 }
 bool child(uint8_t,uint8_t){return fail();}
 bool size(uint8_t s,size_t& n){return (s<2 && live(old_[s]) && backend_.size(s,n) && (!old_[s] || n==bytes_[s]) && live(old_[s])) || fail();}
 bool seek(uint8_t s,size_t n){return (s<2 && live(old_[s]) && backend_.seek(s,n) && live(old_[s])) || fail();}
 bool io(uint8_t s,DTNF24W::Action a,uint8_t* data,size_t n){
  if(s>=2 || !live(old_[s]))return fail();
  if(a==DTNF24W::Action::Write){size_t current=0;if(old_[s] || !backend_.size(s,current) || current>limit_[s] || n>limit_[s]-current || !live())return fail();}
  return (backend_.io(s,a,data,n) && live(old_[s])) || fail();
 }
 bool flush(uint8_t s){return (s<2 && !old_[s] && live() && backend_.flush(s) && live()) || fail();}
 bool close(uint8_t s){return (s<2 && live(old_[s]) && backend_.close(s) && live(old_[s])) || fail();}
 bool unmount(){return (live() && backend_.unmount() && live()) || fail();}
 template<class Guard>bool discover(uint32_t v,const char* current,DTNF24W::SessionSnapshot& out,const Guard& guard){
  out={};DTNF24W::SessionSnapshot copy{};
  if(!live() || !guard() || !backend_.discover(v,current,copy,guard) || !guard() || !p_.lock(copy) || !guard())return fail();
  out=copy;return true;
 }
};
using DTNF24Z::Result;
// Exactly one existing F24Z service. Outer guard covers policy and service;
// no new queue, writer, replay ledger or backend alias.
template<class Backend,class MountWitness,class Witness> class Service final {
 Policy<Witness> policy_;DTNF24Z::Service<CheckedBackend<Backend,Witness>,MountWitness,Witness> service_;
 std::atomic_flag gate_=ATOMIC_FLAG_INIT;bool failed_=false,started_=false,stopped_=false;
 struct Entry{std::atomic_flag& flag;bool ok;explicit Entry(std::atomic_flag& f):flag(f),ok(!f.test_and_set(std::memory_order_acquire)){}~Entry(){if(ok)flag.clear(std::memory_order_release);}};
 Result observe(Result r){if(r==Result::Fault)failed_=true;return r;}
 Result stable(){if(failed_)return Result::Fault;if(policy_.locked&&!policy_.stable()){failed_=true;return Result::Fault;}return Result::Done;}
 public:
 template<class... Args> Service(const MountWitness& m,const Witness& w,Args&&... a):policy_(w),service_(m,w,policy_,std::forward<Args>(a)...){ }
 Result start(uint32_t v,const char* dir){
  Entry e(gate_);if(!e.ok)return Result::Deferred;if(failed_)return Result::Fault;if(started_||stopped_)return Result::Rejected;
  const auto evidence=policy_.w.view();if(evidence.radioBusy||evidence.exportActive||evidence.historyActive)return Result::Deferred;
  if(!policy_.reserve(v,dir)){return Result::Rejected;}
  const auto r=observe(service_.start(v,dir));if(r==Result::Done)started_=true;else {failed_=true;return Result::Fault;}return r;
 }
 Result discover(){Entry e(gate_);if(!e.ok)return Result::Deferred;if(stable()!=Result::Done)return Result::Fault;return observe(service_.discover());}
 template<class Q>Result append(Q& q){Entry e(gate_);if(!e.ok)return Result::Deferred;if(stable()!=Result::Done)return Result::Fault;return observe(service_.append(q));}
 template<class V>Result poll(const V& v){Entry e(gate_);if(!e.ok)return Result::Deferred;if(stable()!=Result::Done)return Result::Fault;return observe(service_.poll(v));}
 template<class V>Result receipt(const DTNF24N::Receipt& r,const V& v){Entry e(gate_);if(!e.ok)return Result::Deferred;if(stable()!=Result::Done)return Result::Fault;return observe(service_.receipt(r,v));}
 Result summary(DTNF24Z::Summary& s){Entry e(gate_);if(!e.ok)return Result::Deferred;s={};if(stopped_)return Result::Rejected;if(stable()!=Result::Done)return Result::Fault;return observe(service_.summary(s));}
 Result shutdown(){Entry e(gate_);if(!e.ok)return Result::Deferred;if(stable()!=Result::Done)return Result::Fault;auto r=observe(service_.shutdown());if(r==Result::Done)stopped_=true;return r;}
};
} // namespace DTNF25B
