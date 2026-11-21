#include "../firmware/shared/hardware/gnss_pht_clock.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using GnssPhtClock::Tracker;
using GnssPhtClock::Utc;
static void expect(Tracker& t,uint32_t ms,const char* expected){
 char s[8]={};t.phtText(ms,s,sizeof(s));assert(std::strcmp(s,expected)==0);
}
int main(){
 Tracker t;
 const Utc zero{2026,11,22,0,0,0,true};
 expect(t,0,"--:--");
 t.observe(zero,1000);expect(t,1000,"--:--");
 t.observe(zero,9000);expect(t,9000,"--:--");
 t.observe(zero,11001);expect(t,11001,"--:--");
 t.observe(Utc{2026,11,22,0,0,1,true},12000);expect(t,12000,"08:00");
 t.observe(Utc{2026,11,22,0,0,2,true},13000);expect(t,13000,"08:00");
 t.observe(Utc{2026,11,22,0,1,0,true},14000);expect(t,14000,"--:--");
 t.observe(Utc{2026,11,22,0,1,1,true},15000);expect(t,15000,"08:01");
 expect(t,25001,"--:--");
 t.observe(Utc{2026,11,22,0,1,1,false},26000);expect(t,26000,"--:--");
 assert(!GnssPhtClock::plausible(Utc{2026,2,30,12,0,1,true}));
 assert(!GnssPhtClock::plausible(Utc{2010,2,1,12,0,1,true}));
 assert(!GnssPhtClock::plausible(Utc{2026,2,1,24,0,1,true}));
 assert(GnssPhtClock::plausible(Utc{2028,2,29,23,59,59,true}));
 assert(!GnssPhtClock::plausible(Utc{2026,2,29,23,59,59,true}));
 Tracker midnight;
 midnight.observe(Utc{2028,2,29,23,59,59,true},100);
 midnight.observe(Utc{2028,3,1,0,0,0,true},1100);
 expect(midnight,1100,"08:00");
 Tracker night;
 night.observe(Utc{2026,11,22,15,59,59,true},100);
 night.observe(Utc{2026,11,22,16,0,0,true},1100);
 expect(night,1100,"00:00");
 Tracker wrap;
 wrap.observe(Utc{2026,11,22,12,0,0,true},UINT32_MAX-500);
 wrap.observe(Utc{2026,11,22,12,0,1,true},499);
 expect(wrap,499,"20:00");
 assert(!GnssPhtClock::plausible(Utc{2026,11,22,12,0,60,true}));
 std::puts("GNSS PHT startup trust/freshness/rollover: PASS");
}
