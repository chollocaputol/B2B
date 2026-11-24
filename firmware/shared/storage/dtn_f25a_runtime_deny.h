#pragma once
// F25A: actual cooperative-loop admission to the F24Z service. Permanent DENY.
// No session identity, mount start, replay, receipt, or storage permission.
#include "dtn_f24z_session_service.h"
#include <atomic>
namespace DTNF25A {
using Queue=DTNF24M::PendingQueue<8>;
enum class Tick:uint8_t { ContextDenied, Busy, NotDue, Denied, Unexpected };
struct Report { size_t pending=0; uint32_t lost=0; };
template<class Backend> class Runtime final {
 const DTNF24W::DenyWitness mountWitness_{};
 const DTNF24O::DenyWitness evidenceWitness_{};
 DTNF24Z::Service<Backend,DTNF24W::DenyWitness,DTNF24O::DenyWitness> service_;
 std::atomic<uintptr_t> owner_{0};
 std::atomic_flag entry_=ATOMIC_FLAG_INIT;
 bool attempted_=false, halted_=false; uint32_t last_=0;
 struct Entry {
  std::atomic_flag& flag; bool ok;
  explicit Entry(std::atomic_flag& f):flag(f),ok(!f.test_and_set(std::memory_order_acquire)){}
  ~Entry(){if(ok)flag.clear(std::memory_order_release);}
 };
 bool owns(uintptr_t token) const {return token && owner_.load(std::memory_order_acquire)==token;}
 public:
 template<class... Args> explicit Runtime(Args&&... args):service_(mountWitness_,evidenceWitness_,std::forward<Args>(args)...){ }
 bool bind(uintptr_t token){
  if(!token)return false;
  uintptr_t none=0;
  return owner_.compare_exchange_strong(none,token,std::memory_order_release,std::memory_order_relaxed);
 }
 DTNF24M::CaptureResult capture(uintptr_t token,Queue& queue,const VesselPacket& packet,uint8_t source){
  if(!owns(token))return DTNF24M::CaptureResult::Faulted;
  Entry guard(entry_);if(!guard.ok)return DTNF24M::CaptureResult::Faulted;
  if(halted_)return DTNF24M::CaptureResult::Faulted;
  return queue.capture(packet,source);
 }
 Tick poll(uintptr_t token,Queue& queue,uint32_t now,bool radioBusy,Report& report){
  if(!owns(token))return Tick::ContextDenied;
  Entry guard(entry_);if(!guard.ok)return Tick::ContextDenied;
  if(halted_)return Tick::NotDue;
  if(radioBusy)return Tick::Busy;
  if(attempted_ && uint32_t(now-last_)<30000U)return Tick::NotDue;
  attempted_=true;last_=now;
  // No legitimate current session exists. Exercise actual F24Z admission,
  // not a fabricated start. Rejected must leave RAM intact, even if empty.
  const auto pending=queue.pending(); const auto lost=queue.lost();
  const auto result=service_.append(queue);
  report={queue.pending(),queue.lost()};
  if(result==DTNF24Z::Result::Rejected && pending==report.pending && lost==report.lost)return Tick::Denied;
  halted_=true;return Tick::Unexpected;
 }
};
} // namespace DTNF25A
