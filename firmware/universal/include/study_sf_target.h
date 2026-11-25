#pragma once
#if THESIS_STUDY_SF_BUILD_ONLY == 1
#include "../../shared/communication/packet_protocol.h"
#include <stdint.h>
bool thesisStudySfConfigured();
bool thesisStudySfStart(uint32_t node);
void thesisStudySfSetContinuous(bool continuous);
bool thesisStudySfIsContinuous();
bool thesisStudySfStorageReady();
bool thesisStudySfWeather(uint32_t ms,const char* utc,double t,double rh,double p,double trend,unsigned state,const char* micro=nullptr,const char* pattern=nullptr,bool valid=false);
bool thesisStudySfGps(uint32_t ms,const char* utc,bool fix,double lat,double lon);
bool thesisStudySfControl(uint32_t ms,const char* utc,const VesselPacket&,uint8_t source);
#endif
