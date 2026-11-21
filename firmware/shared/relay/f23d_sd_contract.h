#pragma once
#include "f23d_physical_contract.h"
#ifndef THESIS_F23D_SD_ROLE
#define THESIS_F23D_SD_ROLE 0
#endif
#if THESIS_F23D_SD_ROLE < 0 || THESIS_F23D_SD_ROLE > 3
#error "F23D SD role must be 0,1,2,3"
#endif
#define THESIS_F23D_SD_EVIDENCE (THESIS_F23D_SD_ROLE != 0)
#if THESIS_F23D_SD_EVIDENCE
#if THESIS_F22_FLEET_OPERATIONAL != 1 || THESIS_SD_LOGGER != 1 || THESIS_F06_SD_SANDBOX != 1 || THESIS_SD_FLEET != 1 || THESIS_F05_RECOVERY != 1 || THESIS_LAB_RELAY_TX != 0 || THESIS_V003_F13_LIVE_OUTPUTS != 0
#error "F23D SD requires authorized F22/F06 storage; legacy relay/GPIO OFF"
#endif
#if THESIS_F23D_SD_ROLE == 1 && THESIS_F23D_EVENT_QUERY != 1
#error "V001 SD requires event query role"
#endif
#if THESIS_F23D_SD_ROLE == 3 && THESIS_F23D_RX_OBSERVER != 1
#error "V003 SD requires receiver observer role"
#endif
#if THESIS_F23D_SD_ROLE == 2 && THESIS_F23D_RELAY_EVIDENCE != 1 && THESIS_F23D_PHYSICAL_CANDIDATE != 1
#error "V002 SD requires offline evidence or separately guarded physical candidate"
#endif
#endif
