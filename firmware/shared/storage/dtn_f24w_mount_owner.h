#pragma once
// F24W: single-caller, synthetic mount lifecycle and owner-held slot inventory.
// No raw File crosses this API. Backend success is explicit, NOT durability.
// The firmware uses only NeverBackend + DenyWitness. A real backend and
// task serialization, path controls and physical qualification are future gates.
#include <stdint.h>
#include <stddef.h>
#include <utility>
#include <type_traits>
#include "dtn_f24q_cross_writer_arbiter.h"
namespace DTNF24W {
using DTNF24Q::Lane;
using DTNF24Q::Mode;
using DTNF24Q::Action;
struct Identity { uint64_t media=0, epoch=0, owner=0; bool authorized=false, exclusive=false, allPathsInterposed=false; };
struct DenyWitness { Identity view()const{return {};} };
enum class State:uint8_t { Detached, Active, Draining, Faulted };
struct NeverBackend {
 bool mount(){return false;}
 bool open(uint8_t,const char*,Mode){return false;}
 bool child(uint8_t,uint8_t){return false;}
 bool io(uint8_t,Action,uint8_t*,size_t){return false;}
 bool flush(uint8_t){return false;}
 bool close(uint8_t){return false;}
 bool unmount(){return false;}
};
// Backend contract: exclusive private owner, no implicit backend I/O on
// destruction; true means operation completed, false means possibly partial.
// Backend must not expose raw resources through an external alias. This is
// not enforced for arbitrary injected backends. No production backend exists.
template<class Backend,class Witness> class MountOwner final {
 static_assert(std::is_trivially_destructible<Backend>::value, "Backend must not own implicit destructor I/O");
 public:
 class Token final {
  friend class MountOwner;
  Token(const MountOwner* p,uint8_t slot,uint64_t stamp):owner_(p),slot_(slot),stamp_(stamp){}
  const MountOwner* owner_=nullptr; uint8_t slot_=0; uint64_t stamp_=0;
  public:
  Token()=default;
  Token(const Token&)=delete; Token& operator=(const Token&)=delete;
  Token(Token&& o)noexcept:owner_(o.owner_),slot_(o.slot_),stamp_(o.stamp_){o.owner_=nullptr;}
  Token& operator=(Token&&)=delete;
  // A token is only a key, never a raw resource. Destruction does NOT close.
  // Owner must outlive all tokens; truthiness means syntactic key only.
  // Abandoned slots remain in the owner inventory until an orderly drain.
  explicit operator bool()const{return owner_!=nullptr;}
 };
 template<class... Args> explicit MountOwner(const Witness& w,Args&&... args):w_(w),backend_(std::forward<Args>(args)...){ }
 MountOwner(const MountOwner&)=delete; MountOwner& operator=(const MountOwner&)=delete;
 MountOwner(MountOwner&&)=delete; MountOwner& operator=(MountOwner&&)=delete;
 State state()const{return state_;}
 unsigned outstanding()const{return unsigned(slots_[0].used)+unsigned(slots_[1].used);}
 Lane lane()const{return lane_;}
 bool mount(){
  Call c(busy_); if(!c.ok || state_!=State::Detached)return false;
  const Identity i=w_.view();
  if(!valid(i) || i.epoch<=lastEpoch_)return false;
  bound_=i; lastEpoch_=i.epoch;
  if(!backend_.mount() || !same())return fail();
  state_=State::Active;return true;
 }
 Token open(Lane lane,const char* path,Mode mode){
  Call c(busy_); if(!c.ok || state_!=State::Active || !live())return {};
  if(!path || !*path || !validLane(lane) || slots_[0].used || slots_[1].used)return {};
  if((mode!=Mode::Read && mode!=Mode::Append) || (mode==Mode::Append && !writer(lane)) ||
     (mode==Mode::Read && (lane==Lane::LegacyAppend || lane==Lane::F23D)))return {};
  lane_=lane;
  if(!reserve(0,mode) || !backend_.open(0,path,mode) || !same()){fail();return {};}
  return Token(this,0,slots_[0].stamp);
 }
 Token child(const Token& parent,const char* verifyPath=nullptr){
  Call c(busy_);if(!c.ok || state_!=State::Active || !live() || !matches(parent) || parent.slot_!=0 || slots_[1].used)return {};
  if(lane_!=Lane::History && lane_!=Lane::F23D)return {};
  if(lane_==Lane::F23D && (!verifyPath || !*verifyPath))return {};
  if(!reserve(1,Mode::Read)){fail();return {};}
  const bool ok=lane_==Lane::History?backend_.child(0,1):backend_.open(1,verifyPath,Mode::Read);
  if(!ok || !same()){fail();return {};}
  return Token(this,1,slots_[1].stamp);
 }
 bool io(const Token& t,Action a,uint8_t* data,size_t n){
  Call c(busy_);if(!c.ok || state_!=State::Active || !live() || !matches(t))return false;
  const bool wr=slots_[t.slot_].mode==Mode::Append;
  if(!data || !n || (a!=Action::Write && a!=Action::Read) || (wr!=(a==Action::Write)))return false;
  if(!backend_.io(t.slot_,a,data,n) || !same())return fail();
  return true;
 }
 bool close(const Token& t){
  Call c(busy_);if(!c.ok || state_!=State::Active || !live() || !matches(t))return false;
  if(t.slot_==0 && slots_[1].used)return false;
  return closeSlot(t.slot_);
 }
 // Planned unmount: reject all new work, drain even abandoned token slots.
 // Any failure or identity change sticks permanently; never retry/unmount.
 bool shutdown(){
  Call c(busy_);if(!c.ok || state_!=State::Active || !live())return false;
  state_=State::Draining;
  if(slots_[1].used && !closeSlot(1))return false;
  if(slots_[0].used && !closeSlot(0))return false;
  if(!live() || !backend_.unmount() || !same())return fail();
  state_=State::Detached;bound_={};return true;
 }
 private:
 struct Call { bool& busy; bool ok; explicit Call(bool& b):busy(b),ok(!b){if(ok)busy=true;} ~Call(){if(ok)busy=false;} };
 struct Slot {bool used=false; Mode mode=Mode::Read; uint64_t stamp=0;};
 const Witness& w_; Backend backend_; State state_=State::Detached;
 Identity bound_{}; uint64_t lastEpoch_=0,stamp_=0; Slot slots_[2]{}; Lane lane_=Lane::None; bool busy_=false;
 static bool valid(const Identity& i){return i.authorized && i.exclusive && i.allPathsInterposed && i.media && i.epoch && i.owner;}
 static bool validLane(Lane l){return l==Lane::Sos || l==Lane::LegacyAppend || l==Lane::Export || l==Lane::History || l==Lane::Audit || l==Lane::F23D;}
 static bool writer(Lane l){return l==Lane::Sos || l==Lane::LegacyAppend || l==Lane::F23D;}
 bool same()const{const Identity i=w_.view();return valid(i) && i.media==bound_.media && i.epoch==bound_.epoch && i.owner==bound_.owner;}
 bool fail(){state_=State::Faulted;return false;}
 bool live(){return state_!=State::Faulted && (same() || fail());}
 bool matches(const Token& t)const{return t.owner_==this && t.slot_<2 && slots_[t.slot_].used && slots_[t.slot_].stamp==t.stamp_;}
 bool reserve(uint8_t s,Mode m){if(stamp_==UINT64_MAX)return false;slots_[s]={true,m,++stamp_};return true;}
 bool closeSlot(uint8_t s){
  if(!live())return false;
  if(slots_[s].mode==Mode::Append && (!backend_.flush(s) || !same()))return fail();
  if(!backend_.close(s) || !same())return fail();
  slots_[s]={};if(!outstanding())lane_=Lane::None;return true;
 }
};
// Firmware seam: only a permanently denied owner and no physical backend.
// Empty Arduino File results preserve existing signatures, not live aliases.
template<class FileType> class DenyMountFacade final {
 public:
 template<class... A> bool begin(A&&...){(void)owner_.mount();return false;}
 template<class... A> uint64_t cardSize(A&&...){return 0;}
 template<class... A> bool exists(A&&...){return false;}
 template<class... A> bool mkdir(A&&...){return false;}
 template<class... A> FileType open(A&&...){return FileType{};}
 template<class... A> bool remove(A&&...){return false;}
 template<class... A> bool rename(A&&...){return false;}
 bool end(){return owner_.shutdown();}
 State state()const{return owner_.state();}
 private:
 const DenyWitness witness_{};
 MountOwner<NeverBackend,DenyWitness> owner_{witness_};
};
} // namespace DTNF24W
