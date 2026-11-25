#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace StudyCloudAuth {
// A bounded, RAM-only bearer. No refresh token is persisted. Retry password sign-in
// after reconnect/expiry using the device's own provisioned credentials.
struct Session {
 char token[2000]{}; uint32_t issued=0, lifetime=0;
 void clear(){memset(token,0,sizeof(token));issued=0;lifetime=0;}
 bool accept(const char* s,uint32_t seconds,uint32_t now){
  clear();if(!s||seconds<60||seconds>86400)return false;
  size_t n=strlen(s);if(n<16||n>=sizeof(token))return false;
  unsigned dots=0;for(size_t i=0;i<n;++i){const char ch=s[i];
   if(ch=='.'){if(i==0||i==n-1||s[i-1]=='.')return false;++dots;}
   else if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-'))return false;
  }
  if(dots!=2)return false;
  memcpy(token,s,n+1);issued=now;lifetime=(seconds-30U)*1000U;return true;
 }
 bool valid(uint32_t now)const{return token[0]&&lifetime&&uint32_t(now-issued)<lifetime;}
};
struct Retry {bool attempted=false;uint32_t last=0;
 bool ready(uint32_t now)const{return !attempted||uint32_t(now-last)>=30000U;}
 void mark(uint32_t now){attempted=true;last=now;}
};
} // namespace StudyCloudAuth
