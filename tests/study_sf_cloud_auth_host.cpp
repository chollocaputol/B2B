#include "../firmware/universal/include/study_sf_cloud_auth.h"
#include <cassert>
#include <cstdio>
#include <string>
int main(){using namespace StudyCloudAuth;Session s;Retry r;
 assert(!s.valid(0));assert(!s.accept(nullptr,3600,0));
 const char* jwt="abcdefghijklmnop.abc.def";
 assert(!s.accept(jwt,59,0));assert(!s.accept(jwt,86401,0));
 assert(!s.accept("abcdefghijklmnop.abc",3600,0));
 assert(!s.accept("abcdefghijklmnop..abc.def",3600,0));
 assert(!s.accept("abcdefghijklmnop.abc.de\nf",3600,0));
 assert(!s.accept((std::string(2000,'a')+".b.c").c_str(),3600,0));
 assert(s.accept(jwt,3600,100));assert(s.valid(100));assert(s.valid(3570099));assert(!s.valid(3570100));
 s.clear();assert(!s.valid(100));assert(s.token[0]==0);
 assert(s.accept(jwt,60,0xfffffff0U));assert(s.valid(5U));assert(!s.valid(uint32_t(0xfffffff0U+30000U)));
 assert(r.ready(0));r.mark(0xfffffff0U);assert(!r.ready(5));assert(r.ready(uint32_t(0xfffffff0U+30000U)));
 puts("CLOUD AUTH SESSION POLICY PASS (RAM token, expiry, wraparound, retry)");}
