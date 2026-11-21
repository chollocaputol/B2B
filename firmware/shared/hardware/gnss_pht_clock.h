#pragma once
// HOME clock trust gate. No radio, NVS, Wi-Fi or SD access.
// GNSS supplies UTC; display PHT only after plausible advancing UTC.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
namespace GnssPhtClock {
static constexpr uint32_t kMaxFreshMs=10000UL;
static constexpr uint32_t kMaxStepSeconds=10UL;
struct Utc {
 uint16_t year=0;
 uint8_t month=0,day=0,hour=0,minute=0,second=0;
 bool fresh=false;
};
inline bool leap(uint16_t y){return y%4U==0U && (y%100U!=0U||y%400U==0U);}
inline uint8_t daysInMonth(uint16_t y,uint8_t m){
 static const uint8_t days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
 if(m<1U||m>12U)return 0U;
 return static_cast<uint8_t>(days[m-1U]+(m==2U&&leap(y)?1U:0U));
}
inline bool plausible(const Utc& s){
 return s.fresh&&s.year>=2020U&&s.year<=2099U&&s.month>=1U&&s.month<=12U&&
 s.day>=1U&&s.day<=daysInMonth(s.year,s.month)&&s.hour<24U&&s.minute<60U&&s.second<60U;
}
inline uint32_t secondsSince2020(const Utc& s){
 uint32_t days=0;
 for(uint16_t y=2020U;y<s.year;++y)days+=leap(y)?366U:365U;
 for(uint8_t m=1U;m<s.month;++m)days+=daysInMonth(s.year,m);
 days+=static_cast<uint32_t>(s.day-1U);
 return days*86400UL+static_cast<uint32_t>(s.hour)*3600UL+
        static_cast<uint32_t>(s.minute)*60UL+s.second;
}
class Tracker {
 public:
 void observe(const Utc& s,uint32_t nowMs){
  if(!plausible(s)){reset();return;}
  const uint32_t stamp=secondsSince2020(s);
  if(!candidate_){baseline(stamp,s,nowMs);return;}
  if(static_cast<uint32_t>(nowMs-lastAdvanceMs_)>kMaxFreshMs){
    baseline(stamp,s,nowMs);return;
  }
  if(stamp==lastStamp_)return;
  if(stamp>lastStamp_&&stamp-lastStamp_<=kMaxStepSeconds){
    lastStamp_=stamp;lastAdvanceMs_=nowMs;hour_=s.hour;minute_=s.minute;trusted_=true;
  }else{
    // A sudden switch to real UTC requires another advancing sample.
    baseline(stamp,s,nowMs);
  }
 }
 bool trusted(uint32_t nowMs)const{
  return candidate_&&trusted_&&static_cast<uint32_t>(nowMs-lastAdvanceMs_)<=kMaxFreshMs;
 }
 void phtText(uint32_t nowMs,char* out,size_t cap)const{
  if(!out||!cap)return;
  if(!trusted(nowMs)){snprintf(out,cap,"--:--");return;}
  snprintf(out,cap,"%02u:%02u",unsigned((hour_+8U)%24U),unsigned(minute_));
 }
 private:
 void reset(){candidate_=false;trusted_=false;}
 void baseline(uint32_t stamp,const Utc& s,uint32_t nowMs){
  candidate_=true;trusted_=false;lastStamp_=stamp;lastAdvanceMs_=nowMs;
  hour_=s.hour;minute_=s.minute;
 }
 bool candidate_=false,trusted_=false;
 uint32_t lastStamp_=0,lastAdvanceMs_=0;
 uint8_t hour_=0,minute_=0;
};
}
