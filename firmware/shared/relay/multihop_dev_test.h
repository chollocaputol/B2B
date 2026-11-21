#pragma once
// DEV-only relay preparation. This switch is NOT physical transmission permission.
// Its OFF/ON choice controls offline queue observation for the three-node lab.
#include <stdint.h>
namespace MultihopDevTest {
class Control {
 public:
  bool toggle(uint32_t self, bool identityReady, bool emergency) {
    if(self!=2U || !identityReady || emergency) return false;
    requested_=!requested_;
    return true;
  }
  bool requested() const { return requested_; }
  bool mayPrepare(uint32_t self, bool identityReady) const {
    return requested_ && self==2U && identityReady;
  }
 private:
  bool requested_=false; // startup OFF; no NVS setting, remote command, or radio grant
};
}
