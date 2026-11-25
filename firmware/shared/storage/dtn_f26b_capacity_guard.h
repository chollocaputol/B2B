#pragma once
// A08-02a: bounded admission and non-destructive archive preflight.
// No archive authority, deletion, session rotation, storage grant or transport.
#include "dtn_f24n_sos_restart_recovery.h"
#if THESIS_DTN_F25C_BUILD_ONLY != 1 || THESIS_DTN_F25B_BUILD_ONLY != 1
#error F26B requires typed F25C records and strict F25B lifecycle
#endif
namespace DTNF26B {
constexpr size_t kCapacity=32, kClosedLimit=12;
enum class Admission:uint8_t {New,AlreadyPresent,Blocked,Conflict,Invalid};
enum class ArchiveState:uint8_t { Empty, Unresolved, Unverified, NeedsClosedArchive };
struct Status {
 size_t occupied=0,missingPartners=0,activeFrames=0,activeReceipts=0;
 bool overBudget=false;
 ArchiveState archive=ArchiveState::Empty;
};
// Identity includes packet type; never infer authenticity from bytes/CRC.
inline bool eventMatch(const DTNF24M::PacketEntry& a,const DTNF24M::PacketEntry& b){return a.origin==b.origin && a.event==b.event;}
template<size_t C> bool partner(const DTNF24N::Ledger<C>& ledger,const DTNF24M::PacketEntry& p){
 for(size_t i=0;i<ledger.count();++i){const auto* r=ledger.at(i);if(r && eventMatch(r->packet,p) && DTNF24M::recordType(r->packet)!=DTNF24M::recordType(p))return true;}
 return false;
}
template<size_t C> Status inspect(const DTNF24N::Ledger<C>& ledger,size_t frames,size_t receipts,uint32_t unverified){
 static_assert(C==kCapacity,"F26B service capacity contract");
 Status s{};s.occupied=ledger.count();s.activeFrames=frames;s.activeReceipts=receipts;
 if(ledger.faulted()){s.overBudget=true;s.archive=ArchiveState::Unresolved;return s;}
 for(size_t i=0;i<ledger.count();++i){const auto* row=ledger.at(i);if(row && !partner(ledger,row->packet))++s.missingPartners;}
 s.overBudget=s.occupied+s.missingPartners>C || frames>C || receipts>C;
 if(unverified)s.archive=ArchiveState::Unverified;
 else if(ledger.pending() || s.missingPartners)s.archive=ArchiveState::Unresolved;
 else if(s.occupied)s.archive=ArchiveState::NeedsClosedArchive;
 // NeedsClosedArchive is NOT retirement eligibility. Full closed inventory,
 // authentic receipts and durable replay/cancellation index still required.
 return s;
}
template<size_t C> Admission admit(const DTNF24N::Ledger<C>& ledger,const DTNF24M::PacketEntry& p,size_t frames){
 uint8_t encoded[DTNF24M::kFrameBytes]{};
 if(ledger.faulted() || !DTNF24M::encode(p,encoded))return Admission::Invalid;
 for(size_t i=0;i<ledger.count();++i){const auto* row=ledger.at(i);if(!row)return Admission::Invalid;
  const auto& prior=row->packet;
  if(eventMatch(prior,p) && DTNF24M::recordType(prior)==DTNF24M::recordType(p))return memcmp(prior.bytes,p.bytes,DTNF24M::kWireBytes)?Admission::Conflict:Admission::AlreadyPresent;
 }
 if(ledger.count()>=C || frames>=C)return Admission::Blocked;
 // Close an existing pair if there is space, even for a legacy over-budget
 // inventory. Admit a new event only with a reserved counterpart slot.
 if(partner(ledger,p))return Admission::New;
 const auto s=inspect(ledger,frames,0,0);
 return s.occupied+s.missingPartners+2<=C?Admission::New:Admission::Blocked;
}
} // namespace DTNF26B
