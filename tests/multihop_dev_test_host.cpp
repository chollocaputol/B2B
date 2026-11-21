#include "../firmware/shared/relay/multihop_dev_test.h"
#include <cassert>
#include <cstdio>
int main(){
  MultihopDevTest::Control c;
  assert(!c.requested() && !c.mayPrepare(2,true));
  assert(!c.toggle(1,true,false) && !c.requested());
  assert(!c.toggle(3,true,false) && !c.requested());
  assert(!c.toggle(2,false,false) && !c.requested());
  assert(!c.toggle(2,true,true) && !c.requested());
  assert(c.toggle(2,true,false));
  assert(c.requested() && c.mayPrepare(2,true));
  assert(!c.mayPrepare(1,true));
  assert(!c.mayPrepare(2,false));
  assert(!c.toggle(2,true,true) && c.requested());
  assert(c.toggle(2,true,false) && !c.requested());
  std::puts("Multihop DEV control: 12 assertions PASS");
  return 0;
}
