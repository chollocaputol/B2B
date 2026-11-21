#include "../firmware/shared/relay/multihop_supervised_bench.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace MultihopBench;
int main(){
 Session s;
 assert(!s.live(0)&&!s.reserve(0,2,true,false,false));
 assert(!s.press(1,true,false,0)&&!s.press(2,false,false,0));
 assert(!s.press(2,true,true,0));
 assert(s.press(2,true,false,100));
 assert(!strcmp(s.label(100),"PREP"));
 assert(!s.reserve(100,2,true,false,false));
 assert(s.press(2,true,false,200)&&s.live(201));
 assert(!s.reserve(201,3,true,false,false));
 assert(!s.reserve(201,2,false,false,false));
 assert(!s.reserve(201,2,true,true,false));
 assert(!s.reserve(201,2,true,false,true));
 assert(s.reserve(201,2,true,false,false)&&s.attempts()==1);
 assert(s.completed(240,true,true));
 assert(s.live(200+Session::kSessionMs-1));
 assert(!s.live(200+Session::kSessionMs));
 assert(!strcmp(s.label(200+Session::kSessionMs),"EXPIRED"));
 assert(s.press(2,true,false,200+Session::kSessionMs));
 assert(s.press(2,true,false,UINT32_MAX-1));
 assert(s.press(2,true,false,UINT32_MAX));
 assert(s.reserve(0,2,true,false,false));
 assert(!s.completed(5001,true,true));
 assert(s.press(2,true,false,10)&&s.press(2,true,false,20));
 assert(s.reserve(21,2,true,false,false));
 assert(!s.completed(100,false,true));
 assert(s.press(2,true,false,10)&&s.press(2,true,false,20));
 assert(s.reserve(21,2,true,false,false));
 assert(!s.completed(100,true,false));
 assert(s.press(2,true,false,10)&&s.press(2,true,false,20));
 for(unsigned i=0;i<Session::kMaxAttempts;++i)
  assert(s.reserve(21+i,2,true,false,false));
 assert(!s.reserve(500,2,true,false,false));
 assert(!s.live(500));
 // The OLED's SOS path must still be able to disarm V002 immediately.
 Session emergencyStop;
 assert(!emergencyStop.press(2,true,true,1)); // emergency cannot activate
 assert(emergencyStop.press(2,true,false,2)); // OFF -> PREP
 assert(emergencyStop.press(2,true,true,3));  // PREP -> OFF during SOS
 assert(!emergencyStop.preparing(3));
 assert(emergencyStop.press(2,true,false,4));
 assert(emergencyStop.press(2,true,false,5)); // LIVE
 assert(emergencyStop.live(6));
 assert(emergencyStop.press(2,true,true,7));  // LIVE -> OFF during SOS
 assert(!emergencyStop.live(8));
 assert(!emergencyStop.reserve(8,2,true,false,false));
 std::puts("supervised V002 bench gate host PASS");
}
