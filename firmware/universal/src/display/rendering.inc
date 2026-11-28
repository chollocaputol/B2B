// ============================================================
// OLED PAGES
// ============================================================
#if THESIS_V003_F19_DUAL_DISPLAY == 1
// 128x64 landscape, six rows of 6x10 font. Optional portrait hardware
// orientation is NOT enabled until physically inspected and separately tested.
// These renderers use cached sensor/peer/SOS state only; never perform I/O.
void oledF19HomeMenu() {
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"HOME MENU OP 1/4");
  const char *choices[]={"SCREEN OFF","DEVELOPMENT","PORTRAIT","LANDSCAPE","BACK"};
  const uint8_t selected=f19Display.homeMenuCursor();
  const uint8_t first=selected>=3U?uint8_t(selected-2U):0U;
  for (uint8_t row=0;row<3U && first+row<V003F19Display::kHomeChoiceCount;++row) {
    const uint8_t idx=uint8_t(first+row);
    const bool active=(idx==2U && f19Display.portrait()) ||
                      (idx==3U && !f19Display.portrait());
    char line[24];
    snprintf(line,sizeof(line),"%c%c%s",idx==selected?'>':' ',
             active?'*':' ',choices[idx]);
    u8g2.drawStr(0,21+12*row,line);
  }
  u8g2.drawStr(0,62,"TAP:MOVE HOLD:SET");
  u8g2.sendBuffer();
}

void oledF19GPSPage() {
  if (f19Display.homeMenuOpen()) { oledF19HomeMenu(); return; }
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  char line[40];
  if (VesselIdentity::ready())
    snprintf(line,sizeof(line),"V%03lu BAT:-- OP1/4",
             (unsigned long)VesselIdentity::nodeId());
  else strcpy(line,"V--- BAT:-- OP1/4");
  u8g2.drawStr(0,9,line);
  // Apply the same GNSS trust gate to the F19 fallback screen.
  char pht[8];displayGnssLocalClock.phtText(millis(),pht,sizeof(pht));
  if(strcmp(pht,"--:--")==0)strcpy(line,"TIME: --:-- NO SYNC");
  else snprintf(line,sizeof(line),"TIME:%s PHT",pht);
  u8g2.drawStr(0,19,line);
  char uptime[40]; formatUptime(uptime,sizeof(uptime));
  snprintf(line,sizeof(line),"UP:%.17s",uptime);
  u8g2.drawStr(0,29,line);
  // No home anchor or battery fuel-gauge calibration accepted yet.
  u8g2.drawStr(0,39,"H DST: -- (NOT SET)");
  unsigned livePeers=0; uint32_t previous=0,next=0;
  while (livePeers<32U && nextActivePeerId(previous,next) && next>previous) {
    ++livePeers; previous=next;
  }
  snprintf(line,sizeof(line),"GPS:%s PEERS:%u",gpsHasFix?"FIX":"NO FIX",livePeers);
  u8g2.drawStr(0,49,line);
  u8g2.drawStr(0,59,"HOLD:MENU TAP:NEXT");
  u8g2.sendBuffer();
}

void oledF19WeatherPage() {
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];
  u8g2.drawStr(0,9,"WEATHER OP 2/4");
  const bool fresh=weatherSampleAttempted && weatherInputsValid &&
      uint32_t(millis()-lastWeatherTime)<=720000UL;
  // Current observational availability, NOT a direct sea-state diagnosis.
  if (!weatherSampleAttempted) strcpy(line,"CURRENT: WAITING");
  else if (!weatherInputsValid) strcpy(line,"CURRENT: SENSOR FAIL");
  else if (!fresh) strcpy(line,"CURRENT: DATA STALE");
  else strcpy(line,"CURRENT: DATA VALID");
  u8g2.drawStr(0,19,line);
  const bool ready=fresh && weatherState!=WEATHER_CALIBRATING &&
      weatherMeasurement.historyCount>=WEATHER_HISTORY_SIZE;
  if (!fresh) strcpy(line,"OUTLOOK: UNAVAILABLE");
  else if (!ready) strcpy(line,"OUTLOOK: CALIBRATING");
  else snprintf(line,sizeof(line),"OUTLOOK: %.12s",shortWeatherName(weatherState));
  u8g2.drawStr(0,29,line);
  if (ready) snprintf(line,sizeof(line),"Z:%u HIST:%u/%u",
      unsigned(zambrettiIndex),unsigned(weatherMeasurement.historyCount),
      unsigned(WEATHER_HISTORY_SIZE));
  else snprintf(line,sizeof(line),"Z:-- HIST:%u/%u",
      unsigned(weatherMeasurement.historyCount),unsigned(WEATHER_HISTORY_SIZE));
  u8g2.drawStr(0,39,line);
  if (!weatherSampleAttempted) strcpy(line,"UPDATE: NOT YET");
  else snprintf(line,sizeof(line),"UPDATE: %lum AGO",
      (unsigned long)(uint32_t(millis()-lastWeatherTime)/60000UL));
  u8g2.drawStr(0,49,line);
  u8g2.drawStr(0,59,"WEATHER GUIDANCE ONLY");
  u8g2.sendBuffer();
}

void oledF19PeerMenu() {
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"SELECT PEER OP 3/4");
  const uint8_t total=oledMenuPeerCount+2U;
  const uint8_t first=oledMenuCursor>=3U?oledMenuCursor-3U:0U;
  for (uint8_t row=0;row<4U && first+row<total;++row) {
    const uint8_t idx=first+row;
    char label[28];
    if (idx==0) strcpy(label,"AUTO");
    else if (idx==oledMenuPeerCount+1U) strcpy(label,"BACK");
    else snprintf(label,sizeof(label),"VESSEL-%03lu",
        (unsigned long)oledMenuPeerIds[idx-1U]);
    char line[32]; snprintf(line,sizeof(line),"%c %s",
        idx==oledMenuCursor?'>':' ',label);
    u8g2.drawStr(0,20+10*row,line);
  }
  u8g2.drawStr(0,60,"TAP:MOVE HOLD:SET");
  u8g2.sendBuffer();
}

void oledF19NearbyPage() {
  if (oledPeerMenuOpen) { oledF19PeerMenu(); return; }
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  char line[36];
  unsigned activeCount=0; uint32_t previous=0,next=0;
  while (activeCount<32U && nextActivePeerId(previous,next) && next>previous) {
    ++activeCount; previous=next;
  }
  snprintf(line,sizeof(line),"PEERS:%u OP 3/4",activeCount);
  u8g2.drawStr(0,9,line);
  if (selectedOLEDPeerId!=0U) {
    PeerDisplaySnapshot peer{};
    if (!getPeerSnapshotById(selectedOLEDPeerId,peer) || !peer.active) {
      u8g2.drawStr(0,20,"SELECTED PEER LOST");
      u8g2.drawStr(0,30,"DIST: UNKNOWN");
      u8g2.drawStr(0,40,"DIR: UNKNOWN");
    } else {
      snprintf(line,sizeof(line),"SELECTED: V%03lu",(unsigned long)peer.id);
      u8g2.drawStr(0,20,line);
      if (gpsHasFix && peer.gpsValid && isfinite(latitude) &&
          isfinite(longitude) && isfinite(peer.latitude) && isfinite(peer.longitude)) {
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
                            peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,
                            peer.latitude,peer.longitude);
        if (isfinite(km)) snprintf(line,sizeof(line),"DIST:%.2fkm",km);
        else strcpy(line,"DIST: UNKNOWN");
        u8g2.drawStr(0,30,line);
        snprintf(line,sizeof(line),"DIR:%s TRUE",V003F19Display::cardinal8(brg));
      } else {
        u8g2.drawStr(0,30,"DIST: UNKNOWN");
        strcpy(line,"DIR: UNKNOWN");
      }
      u8g2.drawStr(0,40,line);
      snprintf(line,sizeof(line),"LAST:%lus",(unsigned long)peer.ageSeconds);
      u8g2.drawStr(0,50,line);
    }
  } else {
    previous=0;next=0;
    for (uint8_t row=0;row<3U && nextActivePeerId(previous,next) && next>previous;++row) {
      previous=next;
      PeerDisplaySnapshot peer{};
      if (!getPeerSnapshotById(next,peer) || !peer.active) continue;
      if (gpsHasFix && peer.gpsValid && isfinite(latitude) && isfinite(longitude) &&
          isfinite(peer.latitude) && isfinite(peer.longitude)) {
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
                           peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,
                           peer.latitude,peer.longitude);
        if (isfinite(km)) snprintf(line,sizeof(line),"V%03lu %.1fkm %s",
           (unsigned long)peer.id,km,V003F19Display::cardinal8(brg));
        else snprintf(line,sizeof(line),"V%03lu DIST:--",(unsigned long)peer.id);
      } else snprintf(line,sizeof(line),"V%03lu DIST:--",(unsigned long)peer.id);
      u8g2.drawStr(0,20+10*row,line);
    }
    if (!activeCount) u8g2.drawStr(0,30,"NO LIVE PEERS");
  }
  u8g2.drawStr(0,60,"HOLD:SELECT TAP:NEXT");
  u8g2.sendBuffer();
}

void oledF19SOSPage() {
  const bool local=hasActiveLocalSOS();
  const unsigned incoming=getActiveSOSCount();
  const bool distress=local || incoming>0U;
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  char line[40];
  u8g2.drawStr(0,9,distress?"!! SOS ACTIVE !!":"SOS STATUS OP 4/4");
  u8g2.drawStr(0,19,local?"LOCAL: ACTIVE":"LOCAL: INACTIVE");
  snprintf(line,sizeof(line),"INCOMING:%u",incoming);
  u8g2.drawStr(0,29,line);
  if (local) {
    u8g2.drawStr(0,39,"BOAT REPLY: UNKNOWN");
    u8g2.drawStr(0,49,gpsHasFix?"GPS: FIX":"GPS: NO FIX");
    u8g2.drawStr(0,59,"GPIO2: SOS CONTROL");
    u8g2.sendBuffer(); return;
  }
  if (!incoming) {
    u8g2.drawStr(0,39,"NO ACTIVE DISTRESS");
    u8g2.drawStr(0,49,"GPIO2: SOS CONTROL");
    u8g2.drawStr(0,59,"TAP: NEXT");
    u8g2.sendBuffer(); return;
  }
  SOSRecordSnapshot chosen{}; bool found=false;
  for (size_t i=0;i<kSOSMenuCapacity;++i) {
    SOSRecordSnapshot rec{};
    if (!getSOSRecordByIndex(i,rec) || !rec.known || rec.cancelled) continue;
    if (!found || int32_t(rec.receivedAtMs-chosen.receivedAtMs)>0) {
      chosen=rec;found=true;
    }
  }
  if (!found) {
    u8g2.drawStr(0,39,"RX SOS: DATA PENDING");
    u8g2.drawStr(0,49,"DIST: UNKNOWN");
    u8g2.drawStr(0,59,"DIR: UNKNOWN");
    u8g2.sendBuffer();return;
  }
  snprintf(line,sizeof(line),"FROM:V%03lu EVENT:%lu",
      (unsigned long)chosen.originId,(unsigned long)chosen.eventId);
  u8g2.drawStr(0,39,line);
  if (gpsHasFix && chosen.positionValid && isfinite(latitude) && isfinite(longitude) &&
      isfinite(chosen.latitude) && isfinite(chosen.longitude)) {
    const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
                     chosen.latitude,chosen.longitude)/1000.0;
    const double bearing=TinyGPSPlus::courseTo(latitude,longitude,
                     chosen.latitude,chosen.longitude);
    if (isfinite(km)) snprintf(line,sizeof(line),"DIST:%.2fkm (LAST)",km);
    else strcpy(line,"DIST: UNKNOWN");
    u8g2.drawStr(0,49,line);
    snprintf(line,sizeof(line),"DIR:%s TRUE (LAST)",
             V003F19Display::cardinal8(bearing));
    u8g2.drawStr(0,59,line);
  } else {
    u8g2.drawStr(0,49,"DIST: UNKNOWN");
    u8g2.drawStr(0,59,"DIR: UNKNOWN");
  }
  u8g2.sendBuffer();
}
#endif // F19 renderer

#if THESIS_V003_F19_DUAL_DISPLAY == 1
// F21R3: OLED-only icon/phone-style OP presentation. Do not draw page numbers.
// Physical OLED 128x64 (R0), logical 64x128 portrait (R1).
// NO weather, SOS, GPIO, LoRa, or SD processing is performed here.
namespace F21Icon {enum Kind : uint8_t { Boat, Battery, Pin, Clock, Home, Peers, Cloud, Radio, Alert, Compass };}
// Minimal vector icons; monochrome, 8-10px, readable on SH1106.
void oledF21Icon(F21Icon::Kind kind, int x, int y) {
  switch(kind) {
    case F21Icon::Boat:
      u8g2.drawHLine(x+1,y+6,8);u8g2.drawLine(x+2,y+4,x+8,y+4);
      u8g2.drawLine(x+1,y+5,x+3,y+8);u8g2.drawLine(x+3,y+8,x+7,y+8);
      u8g2.drawLine(x+7,y+8,x+9,y+5);u8g2.drawBox(x+4,y+1,3,3);break;
    case F21Icon::Battery:
      u8g2.drawFrame(x,y+1,9,6);u8g2.drawBox(x+9,y+3,1,2);break;
    case F21Icon::Pin:
      u8g2.drawCircle(x+5,y+3,3);u8g2.drawDisc(x+5,y+3,1);
      u8g2.drawLine(x+3,y+6,x+5,y+9);u8g2.drawLine(x+7,y+6,x+5,y+9);break;
    case F21Icon::Clock:
      u8g2.drawCircle(x+5,y+5,4);u8g2.drawLine(x+5,y+5,x+5,y+2);
      u8g2.drawLine(x+5,y+5,x+7,y+6);break;
    case F21Icon::Home:
      u8g2.drawLine(x,y+4,x+5,y);u8g2.drawLine(x+5,y,x+10,y+4);
      u8g2.drawFrame(x+2,y+4,7,5);break;
    case F21Icon::Peers:
      u8g2.drawCircle(x+3,y+3,2);u8g2.drawCircle(x+8,y+3,2);
      u8g2.drawHLine(x,y+8,5);u8g2.drawHLine(x+6,y+8,5);break;
    case F21Icon::Cloud:
      u8g2.drawCircle(x+4,y+4,3);u8g2.drawCircle(x+7,y+3,2);
      u8g2.drawHLine(x+1,y+7,10);break;
    case F21Icon::Radio:
      u8g2.drawLine(x+5,y+9,x+5,y+2);u8g2.drawLine(x+2,y+9,x+8,y+9);
      u8g2.drawCircle(x+5,y+2,1);u8g2.drawLine(x+2,y+1,x,y+3);
      u8g2.drawLine(x+8,y+1,x+10,y+3);break;
    case F21Icon::Alert:
      u8g2.drawLine(x+5,y,x,y+9);u8g2.drawLine(x,y+9,x+10,y+9);
      u8g2.drawLine(x+10,y+9,x+5,y);u8g2.drawLine(x+5,y+3,x+5,y+6);
      u8g2.drawDisc(x+5,y+8,1);break;
    case F21Icon::Compass:
      u8g2.drawCircle(x+5,y+5,4);u8g2.drawLine(x+5,y+1,x+5,y+8);
      u8g2.drawLine(x+5,y+1,x+3,y+4);u8g2.drawLine(x+5,y+1,x+7,y+4);break;
  }
}
void oledF21P(int x,int baseline,const char* msg) {
  if(!msg||x<0||x>=64) return;
  char b[13];const int cap=(64-x)/5;
  snprintf(b,sizeof(b),"%.*s",cap<12?cap:12,msg);u8g2.drawStr(x,baseline,b);
}
void oledF21L(int x,int baseline,const char* msg) {
  if(!msg||x<0||x>=128) return;
  char b[22];const int cap=(128-x)/6;
  snprintf(b,sizeof(b),"%.*s",cap<21?cap:21,msg);u8g2.drawStr(x,baseline,b);
}
F21Icon::Kind oledF21HeaderIcon(const char* title) {
  if(strncmp(title,"SOS",3)==0||strstr(title,"SOS"))return F21Icon::Alert;
  if(strstr(title,"WEATHER"))return F21Icon::Cloud;
  if(strstr(title,"PEER")||strstr(title,"NEARBY"))return F21Icon::Radio;
  return F21Icon::Boat;
}
void oledF21PortraitTop(const char* title,bool alert=false,bool vesselHome=false) {
  u8g2.clearBuffer();u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawBox(0,0,64,13);u8g2.setDrawColor(0);
  // Keep the full-width portrait HOME title (no boat icon) for EVERY
  // provisioned vessel, not just the original hardcoded V003.
  if(vesselHome)oledF21P(3,10,title);
  else{oledF21Icon(oledF21HeaderIcon(title),2,1);oledF21P(13,10,title);}
  u8g2.setDrawColor(1);
  if(alert)u8g2.drawFrame(0,14,64,102);
}
void oledF21LandscapeTop(const char* title,bool alert=false) {
  u8g2.clearBuffer();u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawBox(0,0,128,13);u8g2.setDrawColor(0);
  oledF21Icon(oledF21HeaderIcon(title),3,1);oledF21L(17,10,title);
  u8g2.setDrawColor(1);
  if(alert)u8g2.drawFrame(0,14,128,40);
}
void oledF21Dots(bool portrait,uint8_t page) {
  const int y=portrait?124:60;const int center=portrait?32:107;
  // Persistent distress cue whenever the operator browses another OP page.
  // Page 3 already has the unmistakable SOS header and SELECT hint.
  if(page!=3U && V003F19Display::emergencyActive(
       hasActiveLocalSOS(),getActiveSOSCount())) {
    u8g2.setFont(u8g2_font_5x8_tf);
    if(portrait)u8g2.drawStr(0,126,"SOS"); // x0..14; dots start at x17
    else u8g2.drawStr(3,62,"SOS ACTIVE"); // ends before dots at x92
  }
  for(uint8_t i=0;i<4U;++i){const int x=center-15+10*i;
    if(i==page)u8g2.drawDisc(x,y,2);else u8g2.drawCircle(x,y,2);
  }
  u8g2.sendBuffer();
}
void oledF21MenuFooter(bool portrait) {
  u8g2.setFont(u8g2_font_5x8_tf);
  if(portrait){u8g2.drawHLine(0,113,64);oledF21P(2,124,"TAP/HOLD:SET");}
  else{u8g2.drawHLine(0,54,128);oledF21P(3,63,"TAP MOVE");oledF21L(68,63,"HOLD SET");}
  u8g2.sendBuffer();
}
void oledF21PortraitHomeMenu(){
  const bool distress=V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount());
  oledF21PortraitTop(distress?"SOS MENU":"DISPLAY");
  const char*const names[]={"SCREEN OFF","DEV MODE","PORTRAIT","LANDSCAPE","BACK"};
  for(uint8_t i=0;i<V003F19Display::kHomeChoiceCount;++i){
    const bool active=(i==2U&&f19Display.portrait())||(i==3U&&!f19Display.portrait());
    const bool locked=distress && i<2U;
    char b[22];snprintf(b,sizeof(b),"%c%c%s",i==f19Display.homeMenuCursor()?'>':' ',
      locked?'X':active?'*':' ',names[i]);oledF21P(2,26+17*i,b);
  }
  oledF21MenuFooter(true);
}
void oledF21LandscapeHomeMenu(){
  const bool distress=V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount());
  oledF21LandscapeTop(distress?"SOS HOME MENU":"DISPLAY MENU");
  const char*const names[]={"SCREEN OFF","DEVELOPMENT","PORTRAIT","LANDSCAPE","BACK"};
  const uint8_t chosen=f19Display.homeMenuCursor();
  const uint8_t first=chosen>=3U?uint8_t(chosen-2U):0U;
  for(uint8_t row=0;row<3U&&first+row<V003F19Display::kHomeChoiceCount;++row){
    const uint8_t i=uint8_t(first+row);
    const bool active=(i==2U&&f19Display.portrait())||(i==3U&&!f19Display.portrait());
    const bool locked=distress && i<2U;
    char b[26];snprintf(b,sizeof(b),"%c%c %s",i==chosen?'>':' ',locked?'X':active?'*':' ',names[i]);
    oledF21L(5,25+12*row,b);
  }
  oledF21MenuFooter(false);
}
// F21R4: cached PMU gauge estimate, never read I2C from a renderer.
void oledF21BatteryLabel(int x,int y,bool portrait) {
  char label[8];V003F21R4UI::batteryText(cachedDisplayBatteryPercent,label,sizeof(label));
  // Battery percentage: 6x10 in portrait, 5x8 in landscape (unchanged).
  const uint8_t *font=portrait?u8g2_font_6x10_tf:u8g2_font_5x8_tf;
  u8g2.setFont(font);u8g2.drawStr(x,y,label);u8g2.setFont(u8g2_font_5x8_tf);
}
void oledF21HomeLabel(char* dst,size_t n) {
  const bool good=V003F21R4UI::freshPosition(gpsHasFix,gps.location.isValid(),
                   gps.location.age(),latitude,longitude);
  if(!good){V003F21R4UI::distanceText(NAN,dst,n);return;}
  const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
      V003F21R4UI::kHomeLatitude,V003F21R4UI::kHomeLongitude)/1000.0;
  V003F21R4UI::distanceText(km,dst,n);
}
void oledF21PortraitGPSPage(){
  if(f19Display.homeMenuOpen()){oledF21PortraitHomeMenu();return;}
  const uint32_t vesselId=VesselIdentity::nodeId();
  const bool vesselReady=VesselIdentity::ready();
  char vesselTitle[8];FleetHomeTitle::shortTitle(vesselId,vesselReady,
                                                vesselTitle,sizeof(vesselTitle));
  char b[42];oledF21PortraitTop(vesselTitle,false,true);
  // For V001..V999, preserve the V003 portrait layout exactly:
  // title x3, battery icon x26, percentage x38. V1000+ or NO ID gets
  // the full title and the percentage without an overlapping icon.
  u8g2.setDrawColor(0);
  if(FleetHomeTitle::portraitBatteryIconFits(vesselId,vesselReady))
    oledF21Icon(F21Icon::Battery,26,2);
  oledF21BatteryLabel(38,11,true);u8g2.setDrawColor(1);
  u8g2.drawFrame(1,16,62,35);
  u8g2.setFont(u8g2_font_5x8_tf);oledF21P(5,26,"TIME / PHT");
  // Display valid advancing GNSS time only; never invent a startup clock.
  displayGnssLocalClock.phtText(millis(),b,sizeof(b));
  u8g2.setFont(u8g2_font_7x14B_tf);u8g2.drawStr(10,45,b);
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawHLine(2,55,60);
  oledF21P(3,67,gpsHasFix?"GPS FIX":"GPS NO FIX");
  u8g2.drawHLine(2,71,60);
  V003F21R4UI::uptimeText(millis(),b,sizeof(b));oledF21P(3,82,b);
  u8g2.drawHLine(2,86,60);
  oledF21HomeLabel(b,sizeof(b));oledF21P(3,97,b);
  u8g2.drawHLine(2,101,60);
  unsigned peers=0;uint32_t prev=0,nxt=0;
  while(peers<32U&&nextActivePeerId(prev,nxt)&&nxt>prev){++peers;prev=nxt;}
  snprintf(b,sizeof(b),"PEERS %u",peers);oledF21P(3,112,b);
  // No portrait footer instructions; the navigation dots are sufficient.
  oledF21Dots(true,0);
}
void oledF21LandscapeGPSPage(){
  if(f19Display.homeMenuOpen()){oledF21LandscapeHomeMenu();return;}
  char vesselTitle[16];FleetHomeTitle::landscapeTitle(
      VesselIdentity::nodeId(),VesselIdentity::ready(),
      vesselTitle,sizeof(vesselTitle));
  char b[42];oledF21LandscapeTop(vesselTitle);
  u8g2.setDrawColor(0);oledF21Icon(F21Icon::Battery,91,2);
  oledF21BatteryLabel(104,10,false);u8g2.setDrawColor(1);
  u8g2.drawFrame(1,15,62,39);u8g2.drawFrame(66,15,61,39);
  u8g2.setFont(u8g2_font_5x8_tf);oledF21P(6,23,"TIME PHT");
  // Display valid advancing GNSS time only; never invent a startup clock.
  displayGnssLocalClock.phtText(millis(),b,sizeof(b));
  u8g2.setFont(u8g2_font_7x14B_tf);u8g2.drawStr(6,41,b);
  u8g2.setFont(u8g2_font_5x8_tf);
  oledF21HomeLabel(b,sizeof(b));
  if(strlen(b)>11U)strcpy(b,"HOME >99km");
  oledF21P(6,50,b);
  oledF21Icon(F21Icon::Pin,68,18);oledF21L(81,25,gpsHasFix?"GPS FIX":"GPS --");
  unsigned peers=0;uint32_t prev=0,nxt=0;
  while(peers<32U&&nextActivePeerId(prev,nxt)&&nxt>prev){++peers;prev=nxt;}
  oledF21Icon(F21Icon::Peers,68,30);
  snprintf(b,sizeof(b),"PEERS %u",peers);oledF21L(81,38,b);
  oledF21Icon(F21Icon::Clock,68,42);
  V003F21R4UI::shortUptime(millis(),b,sizeof(b));oledF21L(81,50,b);
  // Keep the landscape help text, as requested.
  if(!V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount()))
    u8g2.drawStr(3,62,"HOLD MENU");
  oledF21Dots(false,0);
}
V003F21WeatherDisplay::View oledF21WeatherView(){
  V003F21WeatherDisplay::Input x{};
  x.nowMs=millis();x.macroAttempted=weatherSampleAttempted;
  x.macroInputsValid=weatherInputsValid;x.macroLastStartMs=lastWeatherTime;
  x.macroHistoryCount=weatherMeasurement.historyCount;x.macroState=weatherState;
  x.macroPressureFinite=isfinite(pressureDelta);
  x.microLastMs=lastF10MicroTime;x.microHistoryCount=f10Forecaster.count();
  x.microState=static_cast<uint8_t>(f10Forecaster.state());
  return V003F21WeatherDisplay::select(x);
}
void oledF21PortraitWeatherPage(){
  oledF21PortraitTop("WEATHER");
  const auto v=oledF21WeatherView();
  u8g2.drawFrame(1,16,62,38);
  u8g2.setFont(u8g2_font_5x8_tf);oledF21P(5,26,"CURRENT");
  const char*label=V003F21WeatherDisplay::currentText(v.current);
  const int w=int(strlen(label))*7;
  u8g2.setFont(u8g2_font_7x14B_tf);u8g2.drawStr(w<=60?(64-w)/2:2,46,label);
  u8g2.drawFrame(1,56,62,49);
  u8g2.setFont(u8g2_font_5x8_tf);oledF21P(5,66,"OUTLOOK");
  const char*state=V003F21WeatherDisplay::outlookText(v.outlook);
  if(v.outlook==V003F21WeatherDisplay::Outlook::FrontSignal||
     v.outlook==V003F21WeatherDisplay::Outlook::SquallSignal){
    oledF21P(5,81,v.outlook==V003F21WeatherDisplay::Outlook::FrontSignal?"FRONT":"SQUALL");
    oledF21P(5,90,"SIGNAL");
  }else oledF21P(5,84,state);
  oledF21P(5,100,V003F21WeatherDisplay::horizonText(v.horizon));
  // F21R4: remove cosmetic GAP/GUIDE footer; status labels remain authoritative.
  oledF21Dots(true,1);
}
void oledF21LandscapeWeatherPage(){
  oledF21LandscapeTop("WEATHER");
  const auto v=oledF21WeatherView();
  u8g2.drawFrame(1,15,61,39);u8g2.drawFrame(65,15,62,39);
  u8g2.setFont(u8g2_font_5x8_tf);u8g2.drawStr(4,24,"CURRENT");
  const char*label=V003F21WeatherDisplay::currentText(v.current);
  u8g2.setFont(u8g2_font_7x14B_tf);u8g2.drawStr(4,43,label);
  u8g2.setFont(u8g2_font_5x8_tf);u8g2.drawStr(68,24,"OUTLOOK");
  const char*state=V003F21WeatherDisplay::outlookText(v.outlook);
  if(v.outlook==V003F21WeatherDisplay::Outlook::FrontSignal||
     v.outlook==V003F21WeatherDisplay::Outlook::SquallSignal){
    u8g2.drawStr(68,34,v.outlook==V003F21WeatherDisplay::Outlook::FrontSignal?"FRONT":"SQUALL");
    u8g2.drawStr(68,42,"SIGNAL");
  }else{
    // Bound the 62px card rather than silently run into the right edge.
    char shortState[12];snprintf(shortState,sizeof(shortState),"%.11s",state);
    u8g2.drawStr(68,38,shortState);
  }
  u8g2.drawStr(68,51,V003F21WeatherDisplay::horizonText(v.horizon));
  // F21R4: no cosmetic weather guidance footer.
  oledF21Dots(false,1);
}
void oledF21PortraitPeerMenu(){
  oledF21PortraitTop(V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount())?
                     "SOS PEERS":"PEER MENU");
  const uint8_t total=uint8_t(oledMenuPeerCount+2U);
  const uint8_t first=oledMenuCursor>=7U?uint8_t(oledMenuCursor-6U):0U;
  for(uint8_t row=0;row<8U && uint8_t(first+row)<total;++row){
    const uint8_t idx=uint8_t(first+row);
    char label[24],b[28];
    if(idx==0U)strcpy(label,"AUTO");
    else if(idx==oledMenuPeerCount+1U)strcpy(label,"BACK");
    else snprintf(label,sizeof(label),"V%03lu",(unsigned long)oledMenuPeerIds[idx-1U]);
    snprintf(b,sizeof(b),"%c %s",idx==oledMenuCursor?'>':' ',label);
    oledF21P(3,23+11*row,b);
  }
  oledF21MenuFooter(true);
}
void oledF21LandscapePeerMenu(){
  oledF21LandscapeTop(V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount())?
                      "SOS SELECT PEER":"SELECT PEER");
  const uint8_t total=uint8_t(oledMenuPeerCount+2U);
  const uint8_t first=oledMenuCursor>=3U?uint8_t(oledMenuCursor-2U):0U;
  for(uint8_t row=0;row<3U && uint8_t(first+row)<total;++row){
    const uint8_t idx=uint8_t(first+row);
    char label[24],b[28];
    if(idx==0U)strcpy(label,"AUTO");
    else if(idx==oledMenuPeerCount+1U)strcpy(label,"BACK");
    else snprintf(label,sizeof(label),"V%03lu",(unsigned long)oledMenuPeerIds[idx-1U]);
    snprintf(b,sizeof(b),"%c %s",idx==oledMenuCursor?'>':' ',label);
    oledF21L(5,24+12*row,b);
  }
  oledF21MenuFooter(false);
}
void oledF21PortraitNearbyPage(){
  if(oledPeerMenuOpen){oledF21PortraitPeerMenu();return;}
  oledF21PortraitTop("PEERS");
  unsigned active=0;uint32_t prev=0,nxt=0;
  while(active<32U && nextActivePeerId(prev,nxt) && nxt>prev){++active;prev=nxt;}
  u8g2.drawFrame(1,16,62,23);
  char b[40];snprintf(b,sizeof(b),"LIVE %u",active);oledF21P(5,25,b);
  oledF21P(5,35,"HOLD SELECT");
  if(selectedOLEDPeerId){
    PeerDisplaySnapshot peer{};
    if(!getPeerSnapshotById(selectedOLEDPeerId,peer)||!peer.active){
      oledF21P(5,58,"PEER LOST");oledF21P(5,80,"DIST --");oledF21P(5,99,"DIR --");
    }else{
      oledF21Icon(F21Icon::Boat,3,47);
      snprintf(b,sizeof(b),"V%03lu",(unsigned long)peer.id);oledF21P(17,57,b);
      if(gpsHasFix && peer.gpsValid && isfinite(latitude) && isfinite(longitude) &&
         isfinite(peer.latitude) && isfinite(peer.longitude)){
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,peer.latitude,peer.longitude);
        if(isfinite(km))snprintf(b,sizeof(b),"%.1f km",km);
        else strcpy(b,"DIST --");
        oledF21P(5,79,b);
        snprintf(b,sizeof(b),"DIR %s TRUE",V003F19Display::cardinal8(brg));oledF21P(5,97,b);
      }else{oledF21P(5,79,"DIST --");oledF21P(5,97,"DIR --");}
      snprintf(b,sizeof(b),"AGE %lus",(unsigned long)peer.ageSeconds);oledF21P(5,113,b);
    }
  }else if(!active){
    oledF21P(5,65,"NO PEERS");
  }else{
    prev=0;nxt=0;
    for(uint8_t row=0;row<3U && nextActivePeerId(prev,nxt) && nxt>prev;++row){
      prev=nxt;PeerDisplaySnapshot peer{};
      if(!getPeerSnapshotById(nxt,peer)||!peer.active)continue;
      const int y=49+22*row;
      oledF21Icon(F21Icon::Boat,2,y);
      if(gpsHasFix&&peer.gpsValid&&isfinite(latitude)&&isfinite(longitude)&&
         isfinite(peer.latitude)&&isfinite(peer.longitude)){
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,peer.latitude,peer.longitude);
        if(isfinite(km)&&km<100.0)snprintf(b,sizeof(b),"%03lu %.1fkm",(unsigned long)peer.id,km);
        else snprintf(b,sizeof(b),"%03lu DIST --",(unsigned long)peer.id);
        oledF21P(15,y+7,b);
        snprintf(b,sizeof(b),"DIR %s",V003F19Display::cardinal8(brg));
        oledF21P(15,y+16,b);
      }else{
        snprintf(b,sizeof(b),"%03lu DIST--",(unsigned long)peer.id);oledF21P(15,y+7,b);
        oledF21P(15,y+16,"DIR --");
      }
      if(row<2U)u8g2.drawHLine(2,y+19,60);
    }
  }
  oledF21Dots(true,2);
}
void oledF21LandscapeNearbyPage(){
  if(oledPeerMenuOpen){oledF21LandscapePeerMenu();return;}
  oledF21LandscapeTop("PEERS");
  unsigned active=0;uint32_t prev=0,nxt=0;
  while(active<32U && nextActivePeerId(prev,nxt)&&nxt>prev){++active;prev=nxt;}
  u8g2.drawFrame(1,15,40,39);u8g2.drawFrame(44,15,83,39);
  char b[40];snprintf(b,sizeof(b),"LIVE %u",active);oledF21L(4,27,b);
  oledF21L(4,41,"HOLD");oledF21L(4,51,"SELECT");
  if(selectedOLEDPeerId){
    PeerDisplaySnapshot peer{};
    if(!getPeerSnapshotById(selectedOLEDPeerId,peer)||!peer.active){
      oledF21L(48,29,"PEER LOST");oledF21L(48,42,"DIST --");
      oledF21L(48,51,"AGE --");
    }else{
      oledF21Icon(F21Icon::Boat,47,18);
      snprintf(b,sizeof(b),"V%03lu",(unsigned long)peer.id);oledF21L(62,27,b);
      if(gpsHasFix&&peer.gpsValid&&isfinite(latitude)&&isfinite(longitude)&&
         isfinite(peer.latitude)&&isfinite(peer.longitude)){
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,peer.latitude,peer.longitude);
        if(isfinite(km))snprintf(b,sizeof(b),"%.1fkm %s",km,V003F19Display::cardinal8(brg));
        else strcpy(b,"DIST --");
        oledF21L(48,42,b);
      }else oledF21L(48,42,"DIST -- DIR --");
      snprintf(b,sizeof(b),"AGE %lus",(unsigned long)peer.ageSeconds);
      oledF21L(48,51,b);
    }
  }else if(!active)oledF21L(48,34,"NO LIVE PEERS");
  else{
    prev=0;nxt=0;
    for(uint8_t row=0;row<3U&&nextActivePeerId(prev,nxt)&&nxt>prev;++row){
      prev=nxt;PeerDisplaySnapshot peer{};
      if(!getPeerSnapshotById(nxt,peer)||!peer.active)continue;
      const int y=17+12*row;
      oledF21Icon(F21Icon::Boat,46,y);
      if(gpsHasFix&&peer.gpsValid&&isfinite(latitude)&&isfinite(longitude)&&
         isfinite(peer.latitude)&&isfinite(peer.longitude)){
        const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
        const double brg=TinyGPSPlus::courseTo(latitude,longitude,peer.latitude,peer.longitude);
        if(isfinite(km)&&km<100.0)snprintf(b,sizeof(b),"%03lu %.1f %s",(unsigned long)peer.id,km,V003F19Display::cardinal8(brg));
        else snprintf(b,sizeof(b),"%03lu DIST --",(unsigned long)peer.id);
      }else snprintf(b,sizeof(b),"%03lu DIST --",(unsigned long)peer.id);
      u8g2.setFont(u8g2_font_5x8_tf);oledF21L(61,y+8,b);
    }
  }
  if(!V003F19Display::emergencyActive(hasActiveLocalSOS(),getActiveSOSCount())) {
    u8g2.setFont(u8g2_font_5x8_tf);u8g2.drawStr(3,62,"HOLD SELECT");
  }
  oledF21Dots(false,2);
}
// F21R4: one read-only event selector for both OP orientations.
// AUTO shows most recent ACTIVE received SOS; manual selection is origin+event.
// Restore AUTO if a formerly selected incoming event was cancelled/evicted.
// A still-active MY SOS must never incorrectly show "NO ACTIVE SOS".
void validateDisplayedSOSSelection(){
  if(selectedSOSOrigin==0U)return;
  SOSRecordSnapshot selected{};
  if(!findSOSRecord(selectedSOSOrigin,selectedSOSEvent,selected) ||
     !selected.known || selected.cancelled){
    selectedSOSOrigin=0;selectedSOSEvent=0;
  }
}
bool chooseDisplayedSOSRecord(SOSRecordSnapshot &chosen){
  if(selectedSOSOrigin!=0U){
    if(findSOSRecord(selectedSOSOrigin,selectedSOSEvent,chosen) &&
       chosen.known && !chosen.cancelled)return true;
    // A cancelled/evicted selected event must never remain an active SOS screen.
    selectedSOSOrigin=0;selectedSOSEvent=0;
  }
  return displaySOSRecord(chosen);
}
void oledF21PortraitSOSMenu(){
  oledF21PortraitTop("SOS SELECT",true);
  const uint8_t total=uint8_t(sosMenuCount+2U);
  const uint8_t first=sosMenuCursor>=8U?uint8_t(sosMenuCursor-7U):0U;
  for(uint8_t row=0;row<8U && uint8_t(first+row)<total;++row){
    const uint8_t idx=uint8_t(first+row);
    char b[32];
    if(idx==0U)strcpy(b,hasActiveLocalSOS()?"MY SOS":"AUTO");
    else if(idx==sosMenuCount+1U)strcpy(b,"BACK");
    else snprintf(b,sizeof(b),"V%03lu #%lu",
        (unsigned long)sosMenuOrigins[idx-1U],(unsigned long)sosMenuEvents[idx-1U]);
    char line[36];snprintf(line,sizeof(line),"%c%s",idx==sosMenuCursor?'>':' ',b);
    oledF21P(3,25+11*row,line);
  }
  oledF21MenuFooter(true);
}
void oledF21LandscapeSOSMenu(){
  oledF21LandscapeTop("SOS SELECT",true);
  const uint8_t total=uint8_t(sosMenuCount+2U);
  const uint8_t first=sosMenuCursor>=3U?uint8_t(sosMenuCursor-2U):0U;
  for(uint8_t row=0;row<3U && uint8_t(first+row)<total;++row){
    const uint8_t idx=uint8_t(first+row);
    char b[32];
    if(idx==0U)strcpy(b,hasActiveLocalSOS()?"MY SOS (AUTO)":"AUTO LATEST");
    else if(idx==sosMenuCount+1U)strcpy(b,"BACK");
    else snprintf(b,sizeof(b),"V%03lu EVENT%lu",
        (unsigned long)sosMenuOrigins[idx-1U],(unsigned long)sosMenuEvents[idx-1U]);
    char line[36];snprintf(line,sizeof(line),"%c %s",idx==sosMenuCursor?'>':' ',b);
    oledF21L(5,24+12*row,line);
  }
  oledF21MenuFooter(false);
}
void oledF21PortraitSOSPage(){
  const bool local=hasActiveLocalSOS();const unsigned incoming=getActiveSOSCount();
  const bool distress=local || incoming>0U;
  if(sosMenuOpen){oledF21PortraitSOSMenu();return;}
  validateDisplayedSOSSelection();
  oledF21PortraitTop(distress?"SOS ACTIVE":"SOS STATUS",distress);
  u8g2.drawFrame(2,17,60,34);
  char b[40];
  const bool showLocal=local && selectedSOSOrigin==0U;
  oledF21P(6,29,showLocal?"MY SOS":distress?"INCOMING":"NO DISTRESS");
  snprintf(b,sizeof(b),"ACTIVE %u",incoming+(local?1U:0U));oledF21P(6,43,b);
  if(showLocal){
    oledF21P(5,65,"BOAT REPLY");
    oledF21P(5,79,"UNKNOWN");
    oledF21P(5,94,gpsHasFix?"GPS FIX":"GPS --");
    oledF21P(5,108,"HOLD SELECT");oledF21Dots(true,3);return;
  }
  if(!incoming){oledF21P(5,77,"NO ACTIVE SOS");oledF21Dots(true,3);return;}
  SOSRecordSnapshot chosen{};
  if(!chooseDisplayedSOSRecord(chosen)){
    oledF21P(5,65,"SOS DATA WAIT");oledF21P(5,82,"DIST --");
    oledF21P(5,99,"DIR --");u8g2.sendBuffer();return;
  }
  snprintf(b,sizeof(b),"FROM V%03lu",(unsigned long)chosen.originId);oledF21P(5,61,b);
  snprintf(b,sizeof(b),"EVENT %lu",(unsigned long)chosen.eventId);oledF21P(5,73,b);
  if(gpsHasFix && chosen.positionValid && isfinite(latitude) && isfinite(longitude) &&
     isfinite(chosen.latitude) && isfinite(chosen.longitude)){
    const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
                                  chosen.latitude,chosen.longitude)/1000.0;
    if(isfinite(km))snprintf(b,sizeof(b),"DIST %.1fkm",km);
    else strcpy(b,"DIST --");
    oledF21P(5,85,b);
    const double brg=TinyGPSPlus::courseTo(latitude,longitude,
                                  chosen.latitude,chosen.longitude);
    snprintf(b,sizeof(b),"DIR %s",V003F19Display::cardinal8(brg));
    oledF21P(5,97,b);
  }else{oledF21P(5,85,"DIST --");oledF21P(5,97,"DIR --");}
  snprintf(b,sizeof(b),"AGE %lus",(unsigned long)(uint32_t(millis()-chosen.receivedAtMs)/1000UL));
  oledF21P(5,111,b);
  oledF21Dots(true,3);
}
void oledF21LandscapeSOSPage(){
  const bool local=hasActiveLocalSOS();const unsigned incoming=getActiveSOSCount();
  const bool distress=local || incoming>0U;
  if(sosMenuOpen){oledF21LandscapeSOSMenu();return;}
  validateDisplayedSOSSelection();
  oledF21LandscapeTop(distress?"SOS ACTIVE":"SOS STATUS",distress);
  char b[48];u8g2.drawFrame(1,15,126,39);
  if(local && selectedSOSOrigin==0U){
    oledF21L(5,26,"MY SOS ACTIVE");
    oledF21L(5,39,"BOAT REPLY: UNKNOWN");
    oledF21L(5,49,gpsHasFix?"GPS FIX":"GPS --");
    u8g2.drawStr(3,62,"HOLD SELECT");oledF21Dots(false,3);return;
  }
  if(!incoming){oledF21L(5,35,"NO ACTIVE DISTRESS");oledF21Dots(false,3);return;}
  SOSRecordSnapshot chosen{};
  if(!chooseDisplayedSOSRecord(chosen)){
    oledF21L(5,34,"SOS DATA WAIT");u8g2.sendBuffer();return;
  }
  snprintf(b,sizeof(b),"FROM V%03lu EVENT %lu",
      (unsigned long)chosen.originId,(unsigned long)chosen.eventId);
  oledF21L(5,25,b);
  if(gpsHasFix && chosen.positionValid && isfinite(latitude) && isfinite(longitude) &&
     isfinite(chosen.latitude) && isfinite(chosen.longitude)){
    const double km=TinyGPSPlus::distanceBetween(latitude,longitude,
                            chosen.latitude,chosen.longitude)/1000.0;
    if(isfinite(km))snprintf(b,sizeof(b),"DIST %.1fkm",km);
    else strcpy(b,"DIST --");
    oledF21L(5,37,b);
    const double brg=TinyGPSPlus::courseTo(latitude,longitude,
                            chosen.latitude,chosen.longitude);
    snprintf(b,sizeof(b),"DIR %s TRUE",V003F19Display::cardinal8(brg));
    oledF21L(75,37,b);
  }else{oledF21L(5,37,"DIST --");oledF21L(75,37,"DIR --");}
  snprintf(b,sizeof(b),"AGE %lus",(unsigned long)(uint32_t(millis()-chosen.receivedAtMs)/1000UL));
  oledF21L(5,49,b);
  u8g2.drawStr(3,62,"HOLD SELECT");oledF21Dots(false,3);
}
#endif // F21R3 icon renderers

void updateOLED() {
  if (!oledAvailable) return;
  // Ensure OLED is targeted before drawing
  switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  // Screen sleep affects OLED ONLY. Distress wakes it and takes precedence.
  const bool f19SOS=V003F19Display::emergencyActive(
      hasActiveLocalSOS(),getActiveSOSCount());
  const bool f19WantOff=f19Display.screenAsleep() && !f19SOS;
  if (f19PhysicalScreenAsleep!=f19WantOff) {
    u8g2.setPowerSave(f19WantOff?1:0);
    f19PhysicalScreenAsleep=f19WantOff;
  }
  if (f19WantOff) return;
  // F21 rotation is display-only and never runs on development pages.
  const bool f21WantPortrait=(f19Display.operational() || f19SOS) && f19Display.portrait();
  if (f21WantPortrait!=f21PhysicalPortrait) {
    u8g2.setDisplayRotation(f21WantPortrait?U8G2_R1:U8G2_R0);
    f21PhysicalPortrait=f21WantPortrait;
  }
  if (f19SOS && f21r42SOSBrowse.forceSOS()) {
    if(f21WantPortrait)oledF21PortraitSOSPage();
    else oledF21LandscapeSOSPage();
    return;
  }
  if (f19Display.operational() || f19SOS) {
    const uint8_t opPage=f19SOS ? f21r42SOSBrowse.page() : f19Display.operationalPage();
    if(f21WantPortrait) {
      switch(opPage) {
        case 0: oledF21PortraitGPSPage(); break;
        case 1: oledF21PortraitWeatherPage(); break;
        case 2: oledF21PortraitNearbyPage(); break;
        case 3: oledF21PortraitSOSPage(); break;
        default: oledF21PortraitGPSPage(); break;
      }
      return;
    }
    switch (opPage) {
      case 0: oledF21LandscapeGPSPage(); break;
      case 1: oledF21LandscapeWeatherPage(); break;
      case 2: oledF21LandscapeNearbyPage(); break;
      case 3: oledF21LandscapeSOSPage(); break;
      default: oledF21LandscapeGPSPage(); break;
    }
    return;
  }
#endif
  switch (oledPage) {
    case 0: oledGPSPage(); break;
    case 1: oledWeatherPage(); break;
    case 2: oledLinkPage(); break;
    case 3: oledPDRPage(); break;
    case 4: oledAlertPage(); break;
    case 5: oledUptimePage(); break;
    case 6: oledSOSRescuePage(); break;
#if THESIS_SD_OLED_UI == 1
    case 7: oledSDStoragePage(); break;
#endif
#if THESIS_STUDY_SF_CLOUD == 1
    case 8: {
      u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tf);
      char b[32];
      u8g2.drawStr(0,9,"DTN SYNC 9/9");
      snprintf(b,sizeof(b),"MODE: %s",StudyShore::modeName(studyShore.mode()));u8g2.drawStr(0,20,b);
      if(studyShore.known())snprintf(b,sizeof(b),"GPS: %s %.0fm",studyShore.near()?"NEAR":"FAR",studyShore.distance());
      else strcpy(b,"GPS: UNKNOWN");
      u8g2.drawStr(0,31,b);
      u8g2.drawStr(0,42,studyShore.allowed()?"GATE: ALLOWED":"GATE: BLOCKED");
      const uint8_t net=thesisStudySfNetworkState();
      u8g2.drawStr(0,53,net==2?"NET: CONNECTED":net==1?"NET: CONNECTING":net==3?"NET: STOP PENDING":"NET: OFF");
      u8g2.drawStr(0,63,"HOLD:MODE TAP:NEXT");u8g2.sendBuffer();break;
    }
#endif
  }
}

void oledGPSPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];
  snprintf(line, sizeof(line), "%s GPS 1/" OLED_PAGE_TOTAL, VesselIdentity::ready() ? VesselIdentity::label() : "NO ID");
  u8g2.drawStr(0, 9, line);
  u8g2.drawStr(0, 20, gpsHasFix ? "GPS: FIX" : "GPS: SEARCH");
  if (gps.satellites.isValid()) snprintf(line, sizeof(line), "SAT: %lu", gps.satellites.value());
  else strcpy(line, "SAT: ?");
  u8g2.drawStr(0, 32, line);
  if (gpsHasFix) {
    snprintf(line, sizeof(line), "%.6f", latitude); u8g2.drawStr(0, 44, line);
    snprintf(line, sizeof(line), "%.6f", longitude); u8g2.drawStr(0, 56, line);
  } else {
    u8g2.drawStr(0, 44, "NO POSITION FIX");
    u8g2.drawStr(0, 56, "MOVE TO OPEN SKY");
  }
  u8g2.sendBuffer();
}

#if THESIS_V003_F18_WEATHER_INTERFACE == 1
// F18 OLED CONSISTENCY: shared six-row 6x10 layout with macro view.
// Separate experimental scientific state; no weather or output side effects.
void oledMicroWeatherPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];
  u8g2.drawStr(0, 9, "MICRO WX 2/8 EXP");
  const auto state = f10Forecaster.state();
  const char* label = "INVALID";
  switch (state) {
    case V003F10Micro::State::WARMUP: label = "BASELINE"; break;
    case V003F10Micro::State::NO_PATTERN: label = "NO_PATTERN"; break;
    case V003F10Micro::State::CANDIDATE: label = "CANDIDATE"; break;
    case V003F10Micro::State::RISK_WATCH: label = "RISK_WATCH"; break;
    default: break;
  }
  snprintf(line, sizeof(line), "STATE:%.15s", label);
  u8g2.drawStr(0, 19, line);
  const char* pattern = "--";
  if (state != V003F10Micro::State::INVALID &&
      state != V003F10Micro::State::WARMUP) {
    switch (f10Forecaster.pattern()) {
      case V003F10Micro::Pattern::PRESSURE_RISE: pattern = "RISE"; break;
      case V003F10Micro::Pattern::PRESSURE_FALL: pattern = "FALL"; break;
      default: pattern = "NONE"; break;
    }
  }
  snprintf(line, sizeof(line), "H:%u/11 PAT:%s",
           unsigned(f10Forecaster.count()), pattern);
  u8g2.drawStr(0, 29, line);
  const bool trendReady = f10Forecaster.count() == V003F10Micro::kWindowCount &&
      state != V003F10Micro::State::INVALID &&
      state != V003F10Micro::State::WARMUP &&
      isfinite(f10Forecaster.deltaTemperature()) &&
      isfinite(f10Forecaster.deltaHumidity()) &&
      isfinite(f10Forecaster.deltaPressure());
  if (trendReady) {
    snprintf(line, sizeof(line), "dT:%+.1fC dRH:%+.0fpp",
             (double)f10Forecaster.deltaTemperature(),
             (double)f10Forecaster.deltaHumidity());
    u8g2.drawStr(0, 39, line);
    snprintf(line, sizeof(line), "dP:%+.2fhPa",
             (double)f10Forecaster.deltaPressure());
    u8g2.drawStr(0, 49, line);
  } else {
    u8g2.drawStr(0, 39, "TRENDS: NOT READY");
    u8g2.drawStr(0, 49, "NO SAFETY ALL-CLEAR");
  }
  u8g2.drawStr(0, 59, "EXPERIMENTAL 10-30m");
  u8g2.sendBuffer();
}
#endif

void oledWeatherPage() {
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
  if (f18WeatherMicroView) { oledMicroWeatherPage(); return; }
  {  // Isolate F18 renderer locals from legacy renderer.
  // F18 OLED CONSISTENCY: no changes to legacy F10/F11 calculations or to
  // earlier F10-F17 profiles, which retain their original macro renderer.
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];
  u8g2.drawStr(0, 9, "MACRO WX 2/8 EMP");
  const char* macroState = !weatherSampleAttempted ? "CHECKING"
      : !weatherInputsValid ? "SENSOR_FAIL" : shortWeatherName(weatherState);
  snprintf(line, sizeof(line), "STATE:%.15s", macroState);
  u8g2.drawStr(0, 19, line);
  snprintf(line, sizeof(line), "H:%u/%u D:%s B:%s",
           unsigned(weatherMeasurement.historyCount), unsigned(WEATHER_HISTORY_SIZE),
           dhtReadingValid ? "OK" : "--",
           bmePressureReadingValid ? "OK" : "--");
  u8g2.drawStr(0, 29, line);
  if (tempHumidityReadingValid) {
    snprintf(line, sizeof(line), "T:%.1fC RH:%.1f%%",
             (double)temperatureC, (double)humidityPercent);
  } else {
    strcpy(line, "T:--.-C RH:--.-%");
  }
  u8g2.drawStr(0, 39, line);
  if (bmePressureReadingValid && isfinite(pressureStationHpa)) {
    snprintf(line, sizeof(line), "P:%.1fhPa", (double)pressureStationHpa);
  } else {
    strcpy(line, "P:-- hPa (BME FAIL)");
  }
  u8g2.drawStr(0, 49, line);
  if (!weatherSampleAttempted) {
    strcpy(line, "WAIT: FIRST READ");
  } else if (!weatherInputsValid) {
    strcpy(line, "FORECAST: DISABLED");
  } else if (weatherState == WEATHER_CALIBRATING ||
             weatherMeasurement.historyCount < WEATHER_HISTORY_SIZE) {
    strcpy(line, "WAIT: MACRO HISTORY");
  } else if (isfinite(pressureDelta)) {
    snprintf(line, sizeof(line), "Z:%u dP:%+.1fhPa",
             unsigned(zambrettiIndex), (double)pressureDelta);
  } else {
    strcpy(line, "Z:-- dP:--");
  }
  u8g2.drawStr(0, 59, line);
  u8g2.sendBuffer();
  }
  return;
#endif
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[32];

  const char* wxHeader = !weatherSampleAttempted ? "CHECKING"
      : !weatherInputsValid ? "SENSOR FAIL" : shortWeatherName(weatherState);
  snprintf(line, sizeof(line), "WX 2/" OLED_PAGE_TOTAL ": %s", wxHeader);
  u8g2.drawStr(0, 9, line);

  if (tempHumidityReadingValid) {
    snprintf(line, sizeof(line), "T:%.1fC RH:%.1f%%", temperatureC, humidityPercent);
  } else {
    strcpy(line, "T:--.-C RH:--.-%");
  }
  u8g2.drawStr(0, 19, line);

  if (bmePressureReadingValid) {
    // Show the live pressure sensor result, even when T/RH is unavailable.
    snprintf(line, sizeof(line), "P:%.1f hPa", pressureStationHpa);
  } else {
    strcpy(line, "P:-- hPa (BME FAIL)");
  }
  u8g2.drawStr(0, 29, line);

  if (!weatherSampleAttempted) {
    snprintf(line, sizeof(line), "DHT:%s BME:%s",
             dhtAvailable ? "FOUND" : "MISS",
             bmeAvailable ? "FOUND" : "MISS");
  } else {
    snprintf(line, sizeof(line), "DHT:%s BME-P:%s",
             dhtReadingValid ? "OK" : "FAIL",
             bmePressureReadingValid ? "OK" : "FAIL");
  }
  u8g2.drawStr(0, 39, line);

  if (!weatherSampleAttempted) strcpy(line, "WAIT: FIRST READ");
  else if (!dhtReadingValid && !bmePressureReadingValid) strcpy(line, "DHT+BME: READ FAIL");
  else if (!dhtReadingValid) strcpy(line, "DHT20: READ FAIL");
  else if (!bmePressureReadingValid) strcpy(line, "BME680: READ FAIL");
  else strcpy(line, "T/RH: DHT20 ONLY");
  u8g2.drawStr(0, 49, line);

  if (!weatherSampleAttempted) strcpy(line, "WAIT: FIRST SAMPLE");
  else if (!weatherInputsValid) strcpy(line, "FORECAST: DISABLED");
  else if (weatherState == WEATHER_CALIBRATING ||
           weatherMeasurement.historyCount < WEATHER_HISTORY_SIZE) {
    snprintf(line, sizeof(line), "CALIBRATING %u/%u",
             (unsigned)weatherMeasurement.historyCount, (unsigned)WEATHER_HISTORY_SIZE);
  } else {
    snprintf(line, sizeof(line), "Z:%u DP:%+.1f", zambrettiIndex, pressureDelta);
  }
  u8g2.drawStr(0, 59, line);
  u8g2.sendBuffer();
}

void oledLinkPage() {
  if (oledPeerMenuOpen) { drawOLEDPeerMenu(); return; }
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,selectedOLEDPeerId ? "SELECT PEER 3/" OLED_PAGE_TOTAL : "LIVE PEER 3/" OLED_PAGE_TOTAL);
  char line[32];
  PeerDisplaySnapshot peer;
  if (!selectedOLEDPeer(peer)) {
    snprintf(line,sizeof(line),"KNOWN:%u ACTIVE:0",(unsigned)getTrackedPeerCount());
    u8g2.drawStr(0,22,line);
    u8g2.drawStr(0,39,selectedOLEDPeerId ? "SELECTED PEER LOST" : "NO LIVE TELEMETRY");
    u8g2.drawStr(0,56,"HOLD: CYCLE PEER");
  } else {
    snprintf(line,sizeof(line),"ID:%lu OF %u",(unsigned long)peer.id,(unsigned)getTrackedPeerCount());
    u8g2.drawStr(0,21,line);
    if (gpsHasFix && peer.gpsValid) {
      const double km=TinyGPSPlus::distanceBetween(latitude,longitude,peer.latitude,peer.longitude)/1000.0;
      snprintf(line,sizeof(line),"DIST:%.2f km",km);
    } else snprintf(line,sizeof(line),"DIST: UNKNOWN");
    u8g2.drawStr(0,34,line);
    snprintf(line,sizeof(line),"RSSI:%d SNR:%.1f",(int)peer.rssi,peer.snr);
    u8g2.drawStr(0,47,line);
    snprintf(line,sizeof(line),"LAST:%lus",(unsigned long)peer.ageSeconds);
    u8g2.drawStr(0,60,line);
  }
  u8g2.sendBuffer();
}

void oledPDRPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"PEER PDR 4/" OLED_PAGE_TOTAL);
  char line[32];
  PeerDisplaySnapshot peer;
  if (!selectedOLEDPeer(peer)) {
    u8g2.drawStr(0,28,selectedOLEDPeerId ? "SELECTED PEER LOST" : "NO LIVE PEER");
    u8g2.drawStr(0,44,"PDR: UNKNOWN");
  } else {
    snprintf(line,sizeof(line),"ID:%lu UNIQUE:%lu",(unsigned long)peer.id,(unsigned long)peer.unique);
    u8g2.drawStr(0,21,line);
    snprintf(line,sizeof(line),"MISS:%lu DUP:%lu",(unsigned long)peer.missing,(unsigned long)peer.duplicates);
    u8g2.drawStr(0,34,line);
    snprintf(line,sizeof(line),"PDR EST:%.1f%%",peer.pdr);
    u8g2.drawStr(0,47,line);
  }
  snprintf(line,sizeof(line),"TX FAIL:%lu",(unsigned long)txFailures);
  u8g2.drawStr(0,60,line);
  u8g2.sendBuffer();
}
// ============================================================
// ALERT INDICATOR DISPLAY (SIMULATED LED/BUZZER ONLY)
// ============================================================
void oledAlertPage() {
  uint8_t indicatorState = weatherState;
  if (alertDemoMode == 1) indicatorState = WEATHER_NORMAL;
  if (alertDemoMode == 2) indicatorState = WEATHER_FRONT;
  if (alertDemoMode == 3) indicatorState = WEATHER_SQUALL;

  const bool greenLed = (indicatorState == WEATHER_NORMAL);
  const bool redLed = (indicatorState == WEATHER_SQUALL);
  const bool yellowLed = (indicatorState == WEATHER_FRONT);
  const bool buzzer = (indicatorState == WEATHER_SQUALL);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, alertDemoMode ? "ALERT DEMO 5/" OLED_PAGE_TOTAL : "ALERT 5/" OLED_PAGE_TOTAL);
  char line[30];
  snprintf(line, sizeof(line), "STATE: %s", shortWeatherName(indicatorState));
  u8g2.drawStr(0, 20, line);
  snprintf(line, sizeof(line), "G:%s Y:%s R:%s", greenLed ? "ON" : "OFF",
           yellowLed ? "ON" : "OFF", redLed ? "ON" : "OFF");
  u8g2.drawStr(0, 34, line);
  snprintf(line, sizeof(line), "BUZZER: %s", buzzer ? "ON" : "OFF");
  u8g2.drawStr(0, 47, line);
  u8g2.drawStr(0, 61, "Hold:TEST  Tap:NEXT");
  u8g2.sendBuffer();
}

// ============================================================
// 64-BIT UPTIME (does not wrap after 49.7 days like millis())
// ============================================================
void formatUptime(char *out, size_t outSize) {
  const uint64_t totalSeconds = static_cast<uint64_t>(esp_timer_get_time()) / 1000000ULL;
  const unsigned long days = static_cast<unsigned long>(totalSeconds / 86400ULL);
  const unsigned hours = static_cast<unsigned>((totalSeconds / 3600ULL) % 24ULL);
  const unsigned minutes = static_cast<unsigned>((totalSeconds / 60ULL) % 60ULL);
  const unsigned seconds = static_cast<unsigned>(totalSeconds % 60ULL);
  snprintf(out, outSize, "%lud %02u:%02u:%02u", days, hours, minutes, seconds);
}

void oledUptimePage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "VESSEL UPTIME 6/" OLED_PAGE_TOTAL);
  char line[40];
  formatUptime(line, sizeof(line));
  u8g2.drawStr(0, 22, line);
  snprintf(line, sizeof(line), "WX 2/" OLED_PAGE_TOTAL ": %s", shortWeatherName(weatherState));
  u8g2.drawStr(0, 34, line);
  snprintf(line, sizeof(line), "TX:%lu RX:%lu",
           static_cast<unsigned long>(txCount),
           static_cast<unsigned long>(getTotalPeerTelemetryReceived()));
  u8g2.drawStr(0, 46, line);
  u8g2.drawStr(0, 60, "Tap: NEXT PAGE");
  u8g2.sendBuffer();
}


// Phase 3.5.2: rescue navigation to last reported Vessel-B SOS position.
// Bearings are TRUE-north referenced; not bow-relative heading.
void oledSOSRescuePage() {
  if(sosMenuOpen){drawSOSMenu();return;}
  SOSRecordSnapshot sos;
  const bool present=displaySOSRecord(sos);
  u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,9,"SOS / RESCUE 7/" OLED_PAGE_TOTAL);
  if(!present) {
    u8g2.drawStr(0,23,selectedSOSOrigin?"EVENT REPLACED":"NO ACTIVE SOS");
    u8g2.drawStr(0,42,"Hold: SELECT SOS");
    u8g2.sendBuffer();return;
  }
  char line[40];
  snprintf(line,sizeof(line),"FROM: VESSEL-%03lu",(unsigned long)sos.originId);
  u8g2.drawStr(0,20,line);
  snprintf(line,sizeof(line),"EVENT:%lu %s",(unsigned long)sos.eventId,
           selectedSOSOrigin?"SELECT":"AUTO");
  u8g2.drawStr(0,30,line);
  if(sos.cancelled)u8g2.drawStr(0,40,"CANCEL BY SENDER");
  else if(millis()-sos.receivedAtMs>=60000UL)u8g2.drawStr(0,40,"OLD / UNRESOLVED");
  else u8g2.drawStr(0,40,"SOS / UNRESOLVED");
  if(gpsHasFix && sos.positionValid) {
    const double meters=TinyGPSPlus::distanceBetween(latitude,longitude,sos.latitude,sos.longitude);
    const double bearing=TinyGPSPlus::courseTo(latitude,longitude,sos.latitude,sos.longitude);
    snprintf(line,sizeof(line),"DIST: %.2f km",meters/1000.0);
    u8g2.drawStr(0,51,line);
    snprintf(line,sizeof(line),"BRG: %.0f deg TRUE",bearing);
    u8g2.drawStr(0,62,line);
  } else {
    u8g2.drawStr(0,51,"DIST: UNKNOWN");
    u8g2.drawStr(0,62,"BRG: UNKNOWN");
  }
  u8g2.sendBuffer();
}

#if THESIS_SD_OLED_UI == 1
// 128x64 SH1106 display. Read cached logger counters ONLY: no SPI, file
// opens, directory mutations, radio TX, or emergency state writes here.
void oledSDStoragePage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  char line[38];
#if THESIS_F06_SD_HYBRID == 1
  if (sdStorageMenuOpen) {
    u8g2.drawStr(0, 9, "SD STORAGE MENU 8/" OLED_PAGE_TOTAL);
    const char* menuItems[4] = {
        "SD JOURNALS", "SD USB READBACK", "RECORDING MODE", "BACK"};
    for (uint8_t index = 0; index < 4; ++index) {
      snprintf(line, sizeof(line), "%c %s",
          index == sdStorageMenuCursor ? '>' : ' ', menuItems[index]);
      u8g2.drawStr(0, 21 + 11 * index, line);
    }
    u8g2.drawStr(0, 64, "HOLD:OPEN TAP:MOVE");
    u8g2.sendBuffer();
    return;
  }
  if (sdHybridMenuOpen) {
    u8g2.drawStr(0, 9, "RECORDING MODE 8/" OLED_PAGE_TOTAL);
    const char* menuItems[3] = {"SOS ARMED", "CONTINUOUS", "BACK"};
    for (uint8_t index = 0; index < 3; ++index) {
      snprintf(line, sizeof(line), "%c %s",
          index == sdHybridMenuCursor ? '>' : ' ', menuItems[index]);
      u8g2.drawStr(0, 23 + 13 * index, line);
    }
    u8g2.drawStr(0, 62, "HOLD:SET TAP:MOVE");
    u8g2.sendBuffer();
    return;
  }
#endif
  if (sdOLEDSubview == 0) {
    u8g2.drawStr(0, 9, "SD STORAGE 8/" OLED_PAGE_TOTAL);
#if THESIS_F06_SD_HYBRID == 1
#if THESIS_STUDY_SF_BUILD_ONLY == 1
    const bool ready=thesisStudySfStorageReady();
    u8g2.drawStr(0,20,ready ? "CARD: MOUNTED" : "CARD: NOT READY");
    u8g2.drawStr(0,30,thesisStudySfIsContinuous() ? "MODE: CONTINUOUS" : "MODE: SOS ARMED");
    u8g2.drawStr(0,41,!ready ? "REC: UNAVAILABLE" : thesisStudySfIsContinuous() ? "REC: CONTINUOUS" : "REC: SOS/CANCEL");
    u8g2.drawStr(0,52,"FOLDER: STUDY_SF");
#else
    u8g2.drawStr(0, 20, ThesisSDLogger::ready ? "CARD: MOUNTED" : "CARD: NOT READY");
    u8g2.drawStr(0, 30, ThesisSDLogger::hybridIsContinuous()
        ? "MODE: CONTINUOUS" : "MODE: SOS ARMED");
    const char* recStatus = !ThesisSDLogger::ready ? "REC: UNAVAILABLE" :
        ThesisSDLogger::hybridSOSOverride() ? "REC: SOS OVERRIDE" :
        ThesisSDLogger::hybridIsContinuous() ? "REC: CONTINUOUS" : "REC: STANDBY";
    u8g2.drawStr(0, 41, recStatus);
    const char* slash = strrchr(ThesisSDLogger::DIRECTORY, '/');
    snprintf(line, sizeof(line), "FOLDER: %.12s", slash ? slash + 1 : "UNKNOWN");
    u8g2.drawStr(0, 52, line);
#endif
#else
    u8g2.drawStr(0, 20, ThesisSDLogger::ready ? "CARD: LOGGING" : "CARD: NOT READY");
#if THESIS_F06_SD_SANDBOX == 1
    u8g2.drawStr(0, 30, "MODE: F06 SANDBOX");
#else
    u8g2.drawStr(0, 30, "MODE: VESSEL JOURNAL");
#endif
    const char* slash = strrchr(ThesisSDLogger::DIRECTORY, '/');
    const char* folder = slash ? slash + 1 : "UNKNOWN";
    snprintf(line, sizeof(line), "FOLDER: %.12s", folder);
    u8g2.drawStr(0, 41, line);
    snprintf(line, sizeof(line), "Q SOS:%u TLM:%u",
      (unsigned)ThesisSDLogger::urgentQueue.count,
      (unsigned)ThesisSDLogger::telemetryQueue.count);
    u8g2.drawStr(0, 52, line);
#endif
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 63, "HOLD:MENU TAP:NEXT");
#else
    u8g2.drawStr(0, 63, "HOLD:FILES TAP:NEXT");
#endif
  } else if (sdOLEDSubview == 1) {
    u8g2.drawStr(0, 9, "SD JOURNALS 8/" OLED_PAGE_TOTAL);
#if THESIS_STUDY_SF_BUILD_ONLY == 1
    u8g2.drawStr(0,21,"STUDY JSONL FILES");
    u8g2.drawStr(0,33,thesisStudySfStorageReady() ? "STORAGE READY" : "STORAGE NOT READY");
    u8g2.drawStr(0,44,"NO LEGACY COUNTERS");
    u8g2.drawStr(0,55,"NO FILE DELETION");
#else
    if (ThesisSDLogger::oledAuditValid) {
      snprintf(line, sizeof(line), "TLM: %llu B",
        (unsigned long long)ThesisSDLogger::oledTelemetryBytes);
      u8g2.drawStr(0, 21, line);
      snprintf(line, sizeof(line), "SOS: %llu B",
        (unsigned long long)ThesisSDLogger::oledSOSBytes);
      u8g2.drawStr(0, 33, line);
    } else {
      u8g2.drawStr(0, 21, "FILE SIZES: WAIT");
      u8g2.drawStr(0, 33, "SD AUDIT (30 SEC)");
    }
#if THESIS_F06_SD_HYBRID == 1
    snprintf(line, sizeof(line), "EV:%lu SKIP:%lu",
      (unsigned long)ThesisSDLogger::eventCounter,
      (unsigned long)ThesisSDLogger::hybridSkippedTelemetry());
#else
    snprintf(line, sizeof(line), "EVENTS: %lu",
      (unsigned long)ThesisSDLogger::eventCounter);
#endif
    u8g2.drawStr(0, 44, line);
    snprintf(line, sizeof(line), "DROP S:%lu T:%lu",
      (unsigned long)ThesisSDLogger::lostUrgent,
      (unsigned long)ThesisSDLogger::lostTelemetry);
    u8g2.drawStr(0, 55, line);
#endif
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 64, "HOLD:MENU TAP:NEXT");
#else
    u8g2.drawStr(0, 64, "HOLD:USB TAP:NEXT");
#endif
  } else {
    u8g2.drawStr(0, 9, "SD USB READBACK 8/" OLED_PAGE_TOTAL);
#if THESIS_STUDY_SF_BUILD_ONLY == 1
    u8g2.drawStr(0,21,"STUDY JSONL");
    u8g2.drawStr(0,33,"USB READBACK OFF");
    u8g2.drawStr(0,45,"SYNC PROFILE: CSV");
#elif THESIS_V003_F16_SD_OLED_HYBRID == 1
    // F19R2 adds bounded V003 history, while F16 retains the old
    // active-session-only export view.
#if THESIS_V003_F19_DUAL_DISPLAY == 1
    u8g2.drawStr(0, 21, "PC: SD LIST/READ");
    u8g2.drawStr(0, 33, "USB SERIAL 115200");
    u8g2.drawStr(0, 45, "V003 HISTORY / EXPORT");
#else
    u8g2.drawStr(0, 21, "PC: SD EXPORT");
    u8g2.drawStr(0, 33, "USB SERIAL 115200");
    u8g2.drawStr(0, 45, "ACTIVE SESSION ONLY");
#endif
#elif THESIS_F06_SD_SANDBOX == 1
    u8g2.drawStr(0, 21, "PC COMMAND: SD LIST");
    u8g2.drawStr(0, 33, "SD READ Sxxxxxxx");
    u8g2.drawStr(0, 45, "OR: SD READ LEGACY");
#else
    u8g2.drawStr(0, 21, "PC: SD EXPORT");
    u8g2.drawStr(0, 33, "USB SERIAL 115200");
    u8g2.drawStr(0, 45, "READBACK TO PC");
#endif
    u8g2.drawStr(0, 55, "NO ACTION FROM OLED");
#if THESIS_F06_SD_HYBRID == 1
    u8g2.drawStr(0, 64, "HOLD:MENU TAP:NEXT");
#else
    u8g2.drawStr(0, 64, "HOLD:HOME TAP:NEXT");
#endif
  }
  u8g2.sendBuffer();
}
#endif // THESIS_SD_OLED_UI

