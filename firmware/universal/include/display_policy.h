#pragma once
// F21R4: PURE OLED-only formatting; no I/O, radio, SD, GPIO, or weather changes.
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>
namespace V003F21R4UI {
static constexpr double kHomeLatitude=8.275013;
static constexpr double kHomeLongitude=124.262401;
inline bool validBatteryPercent(int pct) { return pct>=0 && pct<=100; }
inline void batteryText(int pct,char* out,size_t n) {
  if(!out||!n)return;
  if(validBatteryPercent(pct))snprintf(out,n,"%d%%",pct);
  else snprintf(out,n,"--%%");
}
inline bool freshPosition(bool fix,bool valid,uint32_t ageMs,double lat,double lon) {
  return fix&&valid&&ageMs<=10000UL&&isfinite(lat)&&isfinite(lon)&&
         lat>=-90.0&&lat<=90.0&&lon>=-180.0&&lon<=180.0;
}
inline void uptimeText(uint32_t uptimeMs,char* out,size_t n) {
  if(!out||!n)return;
  uint32_t seconds=uptimeMs/1000UL;
  uint32_t h=seconds/3600UL, m=(seconds/60UL)%60UL,s=seconds%60UL;
  if(h>=100UL)snprintf(out,n,"UP 100h+"); // Do not silently wrap at 99:59:59
  else snprintf(out,n,"UP %02lu:%02lu:%02lu",(unsigned long)h,(unsigned long)m,(unsigned long)s);
}
inline void shortUptime(uint32_t uptimeMs,char* out,size_t n) {
  if(!out||!n)return;
  const uint32_t seconds=uptimeMs/1000UL;
  if(seconds<3600UL)snprintf(out,n,"UP %lum",(unsigned long)(seconds/60UL));
  else snprintf(out,n,"UP %luh",(unsigned long)(seconds/3600UL));
}
inline void distanceText(double km,char* out,size_t n) {
  if(!out||!n)return;
  if(!isfinite(km)||km<0.0){snprintf(out,n,"HOME --");return;}
  if(km<1.0)snprintf(out,n,"HOME %.0fm",km*1000.0);
  else if(km<1000.0)snprintf(out,n,"HOME %.1fkm",km);
  else snprintf(out,n,"HOME 999+km");
}
}
