#include "../firmware/shared/hardware/fleet_home_title.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
  char label[16];
  FleetHomeTitle::shortTitle(1,true,label,sizeof(label)); assert(!strcmp(label,"V001"));
  FleetHomeTitle::shortTitle(2,true,label,sizeof(label)); assert(!strcmp(label,"V002"));
  FleetHomeTitle::shortTitle(3,true,label,sizeof(label)); assert(!strcmp(label,"V003"));
  FleetHomeTitle::landscapeTitle(2,true,label,sizeof(label)); assert(!strcmp(label,"V002 HOME"));
  FleetHomeTitle::landscapeTitle(3,true,label,sizeof(label)); assert(!strcmp(label,"V003 HOME"));
  FleetHomeTitle::shortTitle(999,true,label,sizeof(label)); assert(!strcmp(label,"V999"));
  FleetHomeTitle::shortTitle(1000,true,label,sizeof(label)); assert(!strcmp(label,"V1000"));
  FleetHomeTitle::shortTitle(9999,true,label,sizeof(label)); assert(!strcmp(label,"V9999"));
  FleetHomeTitle::shortTitle(0,true,label,sizeof(label)); assert(!strcmp(label,"NO ID"));
  FleetHomeTitle::shortTitle(2,false,label,sizeof(label)); assert(!strcmp(label,"NO ID"));
  FleetHomeTitle::landscapeTitle(0,false,label,sizeof(label)); assert(!strcmp(label,"NO ID HOME"));
  assert(FleetHomeTitle::portraitBatteryIconFits(2,true));
  assert(FleetHomeTitle::portraitBatteryIconFits(999,true));
  assert(!FleetHomeTitle::portraitBatteryIconFits(1000,true));
  assert(!FleetHomeTitle::portraitBatteryIconFits(0,false));
  char narrow[3]{};
  FleetHomeTitle::shortTitle(2,true,narrow,sizeof(narrow)); assert(!strcmp(narrow,"V0"));
  FleetHomeTitle::shortTitle(2,true,nullptr,0);
  std::puts("fleet HOME identity labels and portrait battery fit: PASS");
}
