#pragma once
// F24N — SOS-only cold-restart journal replay and verified delivery ledger.
// Portable C++17; no Arduino, SD singleton, RF, credentials, network or
// physical authorization. The only firmware profile enabling it is F24K's
// hard-denied SD facade. Host tests inject disposable filesystem and fake shore.
// CRC32 detects accidental corruption, NOT authenticity. Receipt verifier is
// independently injected; a missing verifier must NEVER mark delivery.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "dtn_f24m_sos_packet_store.h"

namespace DTNF24N {
static constexpr size_t kReceiptBytes=40;
static constexpr size_t kMaxJournalBytes=DTNF24M::kFrameBytes*32U;
static constexpr size_t kMaxReceiptJournalBytes=kReceiptBytes*32U;
struct Receipt {
 uint32_t origin=0,event=0,packetCrc=0;
 uint64_t serverId=0;
 uint32_t serverTag=0;
};
inline bool receiptValid(const Receipt& r){
 return r.origin>=1 && r.origin<=9999 && r.event!=0 &&
        r.packetCrc!=0 && r.serverId!=0 && r.serverTag!=0;
}
inline void le64(uint8_t* p,uint64_t v){for(unsigned i=0;i<8;++i)p[i]=uint8_t(v>>(8U*i));}
inline uint64_t rd64(const uint8_t* p){uint64_t x=0;for(unsigned i=0;i<8;++i)x|=uint64_t(p[i])<<(8U*i);return x;}
inline bool encodeReceipt(const Receipt& r,uint8_t (&out)[kReceiptBytes]){
 if(!receiptValid(r))return false;
 memset(out,0,sizeof(out));
 memcpy(out,"F24N",4);out[4]=1;out[5]=1;
 DTNF24M::le32(out+8,r.origin);DTNF24M::le32(out+12,r.event);
 DTNF24M::le32(out+16,r.packetCrc);le64(out+20,r.serverId);
 DTNF24M::le32(out+28,r.serverTag);
 DTNF24M::le32(out+36,DTNF24M::crc32(out,36));return true;
}
inline bool decodeReceipt(const uint8_t* in,size_t n,Receipt& out){
 if(!in || n!=kReceiptBytes || memcmp(in,"F24N",4) || in[4]!=1 || in[5]!=1 ||
    in[6] || in[7] || in[32] || in[33] || in[34] || in[35] ||
    DTNF24M::rd32(in+36)!=DTNF24M::crc32(in,36))return false;
 Receipt r{};r.origin=DTNF24M::rd32(in+8);r.event=DTNF24M::rd32(in+12);
 r.packetCrc=DTNF24M::rd32(in+16);r.serverId=rd64(in+20);
 r.serverTag=DTNF24M::rd32(in+28);
 if(!receiptValid(r))return false;
 out=r;return true;
}
inline bool receiptPath(char* out,size_t cap,const char* session,uint32_t vessel){
 char packetPath[96]{};
 if(!DTNF24M::sandboxPath(packetPath,sizeof(packetPath),session,vessel))return false;
 const int n=snprintf(out,cap,"%s/dtn_shore_receipts.f24n",session);
 return n>0 && size_t(n)<cap;
}

// Recorded state is conservative: a neighbor radio ACK is not an end-to-end
// receipt. Exact byte identity is required for duplicate (origin,event) records.
enum class Add : uint8_t {Inserted,Duplicate,Conflict,Invalid,Full,Faulted};
enum class ReceiptResult : uint8_t {Verified,Duplicate,Unverified,Unknown,Conflict,Invalid,Faulted};
struct Record {
 DTNF24M::PacketEntry packet{};
 bool delivered=false;
 uint64_t serverId=0;
};
struct DenyReceiptVerifier {
 bool operator()(const Receipt&,const DTNF24M::PacketEntry&)const{return false;}
};
// This is a separate precondition from receipt verification. Firmware has no
// independent media lease and supplies this permanent-deny reader witness.
struct DenyReadAuthority {bool operator()()const{return false;}};
template<size_t Capacity>class Ledger final{
 static_assert(Capacity>0 && Capacity<=64,"Bounded SOS replay ledger");
 public:
 Add accept(const DTNF24M::PacketEntry& p){
  if(fault_)return Add::Faulted;
  uint8_t frame[DTNF24M::kFrameBytes]{};
  if(!DTNF24M::encode(p,frame))return Add::Invalid;
  for(size_t i=0;i<count_;++i){
   const auto& prior=rows_[i].packet;
   if(prior.origin==p.origin && prior.event==p.event){
    if(memcmp(prior.bytes,p.bytes,DTNF24M::kWireBytes)){
     fault_=true;return Add::Conflict;
    }
    return Add::Duplicate;
   }
  }
  if(count_==Capacity){fault_=true;return Add::Full;}
  rows_[count_++].packet=p;return Add::Inserted;
 }
 template<class Verifier>ReceiptResult apply(const Receipt& r,const Verifier& verify){
  if(fault_)return ReceiptResult::Faulted;
  if(!receiptValid(r))return ReceiptResult::Invalid;
  for(size_t i=0;i<count_;++i){
   Record& e=rows_[i];
   if(e.packet.origin!=r.origin || e.packet.event!=r.event)continue;
   if(r.packetCrc!=DTNF24M::crc32(e.packet.bytes,DTNF24M::kWireBytes)){
    fault_=true;return ReceiptResult::Conflict;
   }
   if(!verify(r,e.packet))return ReceiptResult::Unverified;
   if(e.delivered){
    if(e.serverId==r.serverId)return ReceiptResult::Duplicate;
    fault_=true;return ReceiptResult::Conflict;
   }
   e.delivered=true;e.serverId=r.serverId;return ReceiptResult::Verified;
  }
  fault_=true;return ReceiptResult::Unknown;
 }
 void fault(){fault_=true;}
 bool faulted()const{return fault_;}
 size_t count()const{return count_;}
 size_t pending()const{
  if(fault_)return 0;
  size_t n=0;
  for(size_t i=0;i<count_;++i)if(!rows_[i].delivered)++n;
  return n;
 }
 size_t delivered()const{
  if(fault_)return 0;
  size_t n=0;
  for(size_t i=0;i<count_;++i)if(rows_[i].delivered)++n;
  return n;
 }
 const Record* at(size_t i)const{return !fault_ && i<count_?&rows_[i]:nullptr;}
 const DTNF24M::PacketEntry* nextPending()const{
  if(fault_)return nullptr;
  for(size_t i=0;i<count_;++i)if(!rows_[i].delivered)return &rows_[i].packet;
  return nullptr;
 }
 private:Record rows_[Capacity]{};size_t count_=0;bool fault_=false;
};

enum class Poll : uint8_t {Progress,Completed,Failed};
// Current session is excluded. This scanner may only be supplied by a
// separately verified *read-only* SD owner. One exact frame per poll; no
// retained File handle and no opening any prior session in append mode.
template<size_t Capacity,size_t MaxSessions>class Replay final{
 static_assert(MaxSessions>=1 && MaxSessions<=32,"Bounded old-session inventory");
 public:
 bool begin(uint32_t vessel,const char* current){
  if(started_)return fail();
  started_=true;vessel_=vessel;
  char p[96]{};
  if(!DTNF24M::sandboxPath(p,sizeof(p),current,vessel))return fail();
  strncpy(current_,current,sizeof(current_)-1);
  return true;
 }
 bool addClosed(const char* dir){
  if(!started_ || locked_ || failed_ || !dir || count_>=MaxSessions)return fail();
  char p[96]{};
  if(!DTNF24M::sandboxPath(p,sizeof(p),dir,vessel_) || strcmp(dir,current_)==0)return fail();
  for(size_t i=0;i<count_;++i)if(strcmp(sessions_[i],dir)==0)return fail();
  strncpy(sessions_[count_++],dir,sizeof(sessions_[0])-1);return true;
 }
 bool lock(){if(!started_ || locked_ || failed_)return fail();locked_=true;return true;}
 template<class SDLike,class Verifier,class ReaderAuthority>
 Poll poll(SDLike& sd,const char* readFlag,const Verifier& verifier,
           const ReaderAuthority& readAuthority){
  if(failed_ || !locked_ || !readFlag || !readAuthority())return failedPoll();
  if(completed_)return Poll::Completed;
  if(index_>=count_){
   if(phase_==0){phase_=1;index_=0;offset_=0;return Poll::Progress;}
   completed_=true;return Poll::Completed;
  }
  char path[96]{};
  if(phase_==0){if(!DTNF24M::sandboxPath(path,sizeof(path),sessions_[index_],vessel_))return failedPoll();}
  else if(!receiptPath(path,sizeof(path),sessions_[index_],vessel_))return failedPoll();
  const size_t frameSize=phase_==0?DTNF24M::kFrameBytes:kReceiptBytes;
  const size_t maxSize=phase_==0?kMaxJournalBytes:kMaxReceiptJournalBytes;
  if(!sd.exists(path)){advance();return Poll::Progress;}
  auto f=sd.open(path,readFlag);
  if(!f)return failedPoll();
  const size_t bytes=f.size();
  if(bytes==0 || bytes>maxSize || bytes%frameSize || offset_>=bytes){f.close();return failedPoll();}
  // Once observed, a closed journal may not grow or shrink across poll steps.
  size_t& observed=observedSizes_[phase_][index_];
  if(observed && observed!=bytes){f.close();return failedPoll();}
  if(!observed)observed=bytes;
  uint8_t data[DTNF24M::kFrameBytes]{};
  const bool good=f.seek(offset_) && f.read(data,frameSize)==int(frameSize);
  f.close();
  if(!good || !readAuthority())return failedPoll();
  if(phase_==0){
   DTNF24M::PacketEntry p{};
   if(!DTNF24M::decode(data,frameSize,p))return failedPoll();
   const Add outcome=ledger_.accept(p);
   if(outcome!=Add::Inserted && outcome!=Add::Duplicate)return failedPoll();
  }else{
   Receipt r{};
   if(!decodeReceipt(data,frameSize,r))return failedPoll();
   const ReceiptResult outcome=ledger_.apply(r,verifier);
   if(outcome!=ReceiptResult::Verified && outcome!=ReceiptResult::Duplicate &&
      outcome!=ReceiptResult::Unverified)return failedPoll();
   if(outcome==ReceiptResult::Unverified)++unverified_;
  }
  offset_+=frameSize;
  if(offset_==bytes)advance();
  return Poll::Progress;
 }
 const Ledger<Capacity>& ledger()const{return ledger_;}
 Ledger<Capacity>& ledger(){return ledger_;}
 bool completed()const{return completed_ && !failed_;}
 bool failed()const{return failed_;}
 uint32_t unverified()const{return unverified_;}
 size_t sessions()const{return count_;}
 private:
 bool started_=false,locked_=false,completed_=false,failed_=false;
 uint32_t vessel_=0,unverified_=0;
 char current_[64]{},sessions_[MaxSessions][64]{};
 size_t count_=0,index_=0,offset_=0;
 size_t observedSizes_[2][MaxSessions]{};
 uint8_t phase_=0;Ledger<Capacity> ledger_{};
 bool fail(){failed_=true;ledger_.fault();return false;}
 Poll failedPoll(){fail();return Poll::Failed;}
 void advance(){++index_;offset_=0;}
};

// Receipt writer is only useful if an independent transport supplied an
// authentic, verified end-to-end server receipt. The firmware profile has
// DENY verification and does not call this writer. Host injects fake verifier.
class ReceiptWriter final{
 public:
 template<class SDLike,class LedgerType,class Verifier>
 bool appendVerified(SDLike& sd,const char* dir,uint32_t vessel,
      LedgerType& ledger,const Receipt& receipt,const Verifier& verifier,
      const char* appendFlag,const char* readFlag){
  if(fault_)return false;
  const Record* match=nullptr;
  for(size_t i=0;i<ledger.count();++i){
   const Record* row=ledger.at(i);
   if(row && row->packet.origin==receipt.origin && row->packet.event==receipt.event){match=row;break;}
  }
  if(!match || match->delivered ||
     receipt.packetCrc!=DTNF24M::crc32(match->packet.bytes,DTNF24M::kWireBytes) ||
     !verifier(receipt,match->packet))return false;
  uint8_t frame[kReceiptBytes]{};
  if(!encodeReceipt(receipt,frame)){fault_=true;return false;}
  if(!initialized_){
   if(!receiptPath(path_,sizeof(path_),dir,vessel) || sd.exists(path_)){
    fault_=true;return false;
   }
   auto created=sd.open(path_,appendFlag);
   if(!created){fault_=true;return false;}
   const bool empty=created.size()==0;
   created.close();
   if(!empty){fault_=true;return false;}
   initialized_=true;
  }
  auto handle=sd.open(path_,appendFlag);
  if(!handle){fault_=true;return false;}
  if(handle.size()!=written_){handle.close();fault_=true;return false;}
  const bool complete=handle.write(frame,kReceiptBytes)==kReceiptBytes;
  handle.flush();const bool wrote=complete && handle.getWriteError()==0;
  handle.close();
  if(!wrote){fault_=true;return false;}
  auto proof=sd.open(path_,readFlag);
  if(!proof){fault_=true;return false;}
  uint8_t actual[kReceiptBytes]{};
  const bool same=proof.size()==written_+kReceiptBytes &&
      proof.seek(written_) && proof.read(actual,kReceiptBytes)==int(kReceiptBytes) &&
      memcmp(actual,frame,kReceiptBytes)==0;
  proof.close();
  if(!same){fault_=true;return false;}
  written_+=kReceiptBytes;
  return ledger.apply(receipt,verifier)==ReceiptResult::Verified;
 }
 bool faulted()const{return fault_;}
 const char* path()const{return path_;}
 size_t written()const{return written_;}
 private:char path_[96]{};size_t written_=0;bool initialized_=false,fault_=false;
};
} // namespace DTNF24N
