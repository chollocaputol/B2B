#pragma once
#include <stdint.h>
#include <math.h>
namespace StudyShore {
// Operator-selected Acmac landing/sync reference, 26 Nov 2026.
// Provisional proximity thresholds, NOT measured hotspot coverage.
constexpr double kLatitude=8.275292, kLongitude=124.262623;
constexpr double kEnterM=100.0, kExitM=150.0;
constexpr uint32_t kPermitLeaseMs=3000U;
enum class Mode : uint8_t { Auto=0, SimShore=1, SimOffshore=2 };
inline const char* modeName(Mode m){return m==Mode::SimShore?"SIM SHORE":m==Mode::SimOffshore?"SIM OFFSHORE":"AUTO";}
inline bool permitFresh(uint32_t stamp,uint32_t now){return stamp!=0U&&uint32_t(now-stamp)<kPermitLeaseMs;}
inline double distanceM(double lat,double lon){
 if(!isfinite(lat)||!isfinite(lon)||lat<-90.0||lat>90.0||lon<-180.0||lon>180.0)return NAN;
 constexpr double rad=0.017453292519943295;
 const double dlat=(lat-kLatitude)*rad,dlon=(lon-kLongitude)*rad;
 double a=sin(dlat/2)*sin(dlat/2)+cos(kLatitude*rad)*cos(lat*rad)*sin(dlon/2)*sin(dlon/2);
 if(a<0)a=0;
 if(a>1)a=1;
 return 6371000.0*2*asin(sqrt(a));
}
// Main-loop only. No simulated coordinates are ever supplied to capture.
class Policy {
 Mode mode_=Mode::Auto;
 bool near_=false;
 double distance_=NAN;
public:
 Mode mode()const{return mode_;}
 double distance()const{return distance_;}
 bool known()const{return isfinite(distance_);}
 bool near()const{return known()&&near_;}
 void clearSimulation(){mode_=Mode::Auto;}
 void cycle(bool developmentAllowed){if(!developmentAllowed){clearSimulation();return;}
  mode_=static_cast<Mode>((static_cast<unsigned>(mode_)+1U)%3U);}
 void observe(bool freshFix,double lat,double lon,bool developmentAllowed){
  if(!developmentAllowed)clearSimulation();
  distance_=freshFix?distanceM(lat,lon):NAN;
  if(!known())near_=false;
  else near_=distance_<=(near_?kExitM:kEnterM);
 }
 bool allowed()const{return mode_==Mode::SimShore||(mode_==Mode::Auto&&near());}
};
} // namespace StudyShore
