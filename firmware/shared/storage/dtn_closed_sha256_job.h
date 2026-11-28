#pragma once
// F24D: cooperative, read-only SHA256 of an externally authorized CLOSED F06
// journal. NO writes, mount, radio, Wi-Fi, ACK update, SD reclaim or upload.
// Gate must prove media/vessel/uploader ownership AND closure at every step.
// A hash match is file integrity, NOT LoRa-origin authentication or upload OK.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "dtn_closed_replay.h"
#include "dtn_sha256_portable.h"
namespace DTNF24D {
static constexpr size_t kChunkBytes=256U;
static constexpr size_t kMaxStepBytes=512U;
enum class Result:uint8_t { Idle,Running,Paused,Verified,Denied,BadStream,Missing,
  Oversize,ChangedSource,ReadError,DigestMismatch };
// File: bool isOpen(), uint64_t size(), bool readBlockAt(offset, uint8_t*, n), close()
// Volume: bool openReadOnly(const char*,File&)
// Gate: bool allowClosedJournal(const Stream&, const char*) const;
//    MUST also enforce exclusive SD main-loop ownership and closed-session proof.
template<class File,class Volume> class Job {
public:
  Job()=default;
  Job(const Job&)=delete;Job& operator=(const Job&)=delete;
  ~Job(){file_.close();}
  template<class Gate> Result begin(Volume& volume,const DTNAckLedger::Stream& stream,
                                    const Gate& gate){
    clear();
    if(!DTNF06ClosedReplay::pathFor(stream,path_,sizeof(path_)))return fail(Result::BadStream);
    if(!gate.allowClosedJournal(stream,path_))return fail(Result::Denied);
    if(!volume.openReadOnly(path_,file_)||!file_.isOpen())return fail(Result::Missing);
    size_=file_.size();
    if(size_>DTNF06ClosedReplay::kMaxJournalBytes)return fail(Result::Oversize);
    stream_=stream;hash_.begin();return state_=Result::Running;
  }
  template<class Gate> Result step(const Gate& gate,bool urgentOrRadioWork,
                                   size_t budget=kChunkBytes){
    if(state_!=Result::Running&&state_!=Result::Paused)return state_;
    if(!gate.allowClosedJournal(stream_,path_))return fail(Result::Denied);
    if(!file_.isOpen()||file_.size()!=size_)return fail(Result::ChangedSource);
    if(urgentOrRadioWork)return state_=Result::Paused;
    state_=Result::Running;
    if(!budget)return state_;
    if(budget>kMaxStepBytes)budget=kMaxStepBytes;
    uint8_t buffer[kChunkBytes]{};
    size_t remaining=budget;
    while(remaining>0U&&read_<size_){
      if(!gate.allowClosedJournal(stream_,path_))return fail(Result::Denied);
      if(file_.size()!=size_)return fail(Result::ChangedSource);
      size_t count=remaining;
      if(count>sizeof(buffer))count=sizeof(buffer);
      if(uint64_t(count)>size_-read_)count=static_cast<size_t>(size_-read_);
      if(!file_.readBlockAt(read_,buffer,count)||!hash_.update(buffer,count))
        return fail(Result::ReadError);
      read_+=count;remaining-=count;
    }
    if(read_!=size_)return state_;
    if(file_.size()!=size_)return fail(Result::ChangedSource);
    uint8_t actual[32]{};
    if(!hash_.end(actual))return fail(Result::ReadError);
    const bool same=DTNSha256::equals32(actual,stream_.sha256);
    memset(actual,0,sizeof(actual));file_.close();path_[0]='\0';
    return state_=same?Result::Verified:Result::DigestMismatch;
  }
  Result status()const{return state_;}
  uint64_t bytesProcessed()const{return read_;}
  uint64_t totalBytes()const{return size_;}
  void clear(){file_.close();stream_=DTNAckLedger::Stream{};
    memset(path_,0,sizeof(path_));size_=read_=0U;hash_.begin();state_=Result::Idle;}
private:
  Result fail(Result r){file_.close();path_[0]='\0';return state_=r;}
  File file_{};DTNAckLedger::Stream stream_{};
  char path_[DTNF06ClosedReplay::kPathCapacity]{};
  DTNSha256::Hasher hash_{};uint64_t size_=0U,read_=0U;
  Result state_=Result::Idle;
};
} // DTNF24D
