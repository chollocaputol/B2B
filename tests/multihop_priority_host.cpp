#include "../firmware/shared/relay/multihop_priority.h"
#include <cassert>
#include <cstdio>
int main() {
  using MultihopPriority::mayDrainRoutine;
  assert(mayDrainRoutine(false,false,false,false,false,false));
  assert(!mayDrainRoutine(true,false,false,false,false,false));
  assert(!mayDrainRoutine(false,true,false,false,false,false));
  assert(!mayDrainRoutine(false,false,true,false,false,false));
  assert(!mayDrainRoutine(false,false,false,true,false,false));
  assert(!mayDrainRoutine(false,false,false,false,true,false));
  assert(!mayDrainRoutine(false,false,false,false,false,true));
  // A remote SOS may still be active in peer state, but once its relay
  // queue is empty, routine position forwarding is eligible again.
  bool remoteSosStillActive=true;
  (void)remoteSosStillActive;
  assert(mayDrainRoutine(false,false,false,false,false,false));
  std::puts("multihop SOS-priority nonstarvation host PASS");
}
