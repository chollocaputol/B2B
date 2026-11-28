// ============================================================
// SETUP
// ============================================================
#if THESIS_SD_LOGGER == 1
// The single logger instance is owned by main.cpp; radio code only queues.
bool thesisLogEvent(bool urgent, const char* kind, uint32_t origin,
                    uint32_t seq, int32_t result, int16_t rssi,
                    int16_t snrX10, int32_t latE7, int32_t lonE7,
                    uint8_t flags, uint8_t hops, uint8_t attempt) {
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
    if(!strcmp(kind,"sos_ack_tx") || !strcmp(kind,"cancel_ack_tx") ||
       !strcmp(kind,"sos_ack_rx") || !strcmp(kind,"cancel_ack_rx")){
      F23DSD::Record row{};F23DSD::describe(row,strstr(kind,"_tx")?"ACK_TX":"ACK_RX");
      row.ms=millis();row.origin=origin;row.event=seq;row.rc=result;
      row.rssi=rssi;row.snr_x10=snrX10;row.type=strstr(kind,"cancel")?PACKET_SOS_CANCEL_ACK:PACKET_SOS_ACK;
      (void)ThesisSDLogger::f23dJournal.capture(row);
    }
#endif
// F23D SD end
    return ThesisSDLogger::queueEvent(urgent, kind, origin, seq, result,
                                     rssi, snrX10, latE7, lonE7,
                                     flags, hops, attempt);
}
#endif

#if THESIS_STUDY_SF_BUILD_ONLY == 1
// Capture the actual acquisition time; no synthetic UTC or upload timestamp.
static const char* studyUtc(char (&out)[21]) {
  if(gps.date.isValid() && gps.time.isValid() && gps.date.age()<=10000UL && gps.time.age()<=10000UL &&
     snprintf(out,sizeof(out),"%04u-%02u-%02uT%02u:%02u:%02uZ",(unsigned)gps.date.year(),(unsigned)gps.date.month(),(unsigned)gps.date.day(),(unsigned)gps.time.hour(),(unsigned)gps.time.minute(),(unsigned)gps.time.second())==20) return out;
  return nullptr;
}
static void studyMacroCapture(){char utc[21]{};const auto& w=weatherMeasurement;
  char macro[160], z[8]="null";
  if(w.inputsOK && w.historyCount>=10 && w.weatherState!=0 && isfinite(w.pressureDeltaHpa))
    snprintf(z,sizeof(z),"%u",unsigned(w.zambretti));
  const int n=snprintf(macro,sizeof(macro),"{\"input_valid\":%s,\"history_count\":%u,\"state\":%u,\"zambretti_index\":%s}",w.inputsOK?"true":"false",unsigned(w.historyCount),unsigned(w.weatherState),z);
  if(n>0 && size_t(n)<sizeof(macro))
    (void)thesisStudySfWeather(w.uptimeMs,studyUtc(utc),w.selectedTempC,w.selectedRH,w.stationPressureHpa,w.pressureDeltaHpa,w.weatherState,nullptr,nullptr,false,macro);
}
#endif

// F24M: the actual LoRa SOS call sites only copy exact packet bytes into RAM.
// No SD File operation or forwarding permission exists in this bridge.
#if THESIS_DTN_F24M_BUILD_ONLY == 1
#if THESIS_SD_LOGGER != 1
#error "F24M capture requires enabled logger in DENY-only preview"
#endif
bool thesisF24MObserveSOSPacket(const VesselPacket& packet,uint8_t source){
#if THESIS_STUDY_SF_BUILD_ONLY == 1
  char utc[21]{};return thesisStudySfControl(millis(),studyUtc(utc),packet,source);
#else
#if THESIS_DTN_F25A_BUILD_ONLY == 1
  const auto result=thesisF25ACapture(ThesisSDLogger::f24mSosCapture,packet,source);
#else
  const auto result=ThesisSDLogger::f24mSosCapture.capture(packet,source);
#endif
  return result==DTNF24M::CaptureResult::Queued ||
         result==DTNF24M::CaptureResult::Duplicate;
#endif
}
#if THESIS_DTN_F25C_BUILD_ONLY == 1
bool thesisF25CObserveCancelPacket(const VesselPacket& packet,uint8_t source){
  return packet.packetType==PACKET_SOS_CANCEL && thesisF24MObserveSOSPacket(packet,source);
}
#endif
#endif

// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
extern bool f23dSDRadioBusy();
bool thesisF23dSDCapture(const F23DSD::Record& row){return ThesisSDLogger::f23dJournal.capture(row);}
void thesisF23dSDFormatLoss(){ThesisSDLogger::f23dJournal.formatLost();}
bool thesisF23dSDHealthy(){return ThesisSDLogger::f23dJournal.permissionHealthy() && ThesisSDLogger::ready &&
  ThesisSDLogger::isAuthorizedVessel(THESIS_F23D_SD_ROLE) && !ThesisSDLogger::lostUrgent;}
bool thesisF23dSDCommand(const char* line) {
  if(!strcmp(line,"R23D SD START")){if(!f23dSDRadioBusy())(void)ThesisSDLogger::f23dJournal.start(millis());return true;}
  if(!strcmp(line,"R23D SD SEAL")){(void)ThesisSDLogger::f23dJournal.requestSeal();return true;}
  if(!strcmp(line,"R23D SD STATUS")){
    Serial.printf("[F23D-SD] local=%lu path=%s session=%s ready=%u active=%u sealed=%u pending=%u written=%lu lost=%lu health=%lu\n",
      (unsigned long)VesselIdentity::nodeId(),ThesisSDLogger::f23dWriter.path(),ThesisSDLogger::f23dJournal.session(),unsigned(ThesisSDLogger::f23dJournal.readyToStart()),
      unsigned(ThesisSDLogger::f23dJournal.captureActive()),unsigned(ThesisSDLogger::f23dJournal.sealed()),
      unsigned(ThesisSDLogger::f23dJournal.pending()),(unsigned long)ThesisSDLogger::f23dJournal.written(),
      (unsigned long)ThesisSDLogger::f23dJournal.lost(),(unsigned long)ThesisSDLogger::f23dJournal.health());
    return true;
  }
  return false;
}
#endif
// F23D SD end
#if THESIS_SD_LOGGER == 1
// Existing TEST commands remain handled by processSerialTestCommands().
bool pollSDExportCommand() {
  static char command[24];
  static uint8_t used = 0;
  if (!Serial.available() || (used == 0 && Serial.peek() != 'S')) return false;
  // Only runs after the first 'S' has arrived; await the newline across loops.
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\n' || c == '\r') {
      command[used] = '\0';
#if THESIS_STUDY_SF_BUILD_ONLY == 1
      Serial.println("[STUDY-SF] LEGACY_SD_COMMAND_DISABLED");
#else
      if (strcmp(command, "SD EXPORT") == 0) {
#if THESIS_F06_SD_SANDBOX == 1
        if (ThesisSDLogger::historyReadActive())
          Serial.println("[SDH] ERROR HISTORY_BUSY");
        else
#endif
          ThesisSDLogger::requestExport();
      }
#if THESIS_F06_SD_SANDBOX == 1
      else if (strcmp(command, "SD LIST") == 0)
        ThesisSDLogger::listHistory();
      else if (strncmp(command, "SD READ ", 8) == 0)
        ThesisSDLogger::requestHistory(command + 8);
#endif
      else Serial.println("[SDX] ERROR UNKNOWN_COMMAND");
#endif
      used = 0;
      return true;
    }
    if (used >= sizeof(command) - 1) {
      used = 0;
      Serial.println("[SDX] ERROR COMMAND_TOO_LONG");
      return true;
    }
    command[used++] = (char)c;
  }
  return true;
}
#endif

#if THESIS_V003_F15_MACRO_SD_SANDBOX == 1
// A completed F11 median already represents five physical sensor readings.
// Stamp a fresh GNSS UTC value only when date AND time are valid and recent;
// otherwise the logger must serialize gps_utc as JSON null. No extra I2C read.
static void queuePhysicalMacroMedianForStorage() {
  weatherMeasurement.gpsUtcValid = false;
  weatherMeasurement.gpsUtcIso8601[0] = '\0';
  if (gps.date.isValid() && gps.time.isValid() &&
      gps.date.age() <= 10000UL && gps.time.age() <= 10000UL) {
    const int n = snprintf(weatherMeasurement.gpsUtcIso8601,
        sizeof(weatherMeasurement.gpsUtcIso8601),
        "%04u-%02u-%02uT%02u:%02u:%02uZ",
        (unsigned)gps.date.year(), (unsigned)gps.date.month(),
        (unsigned)gps.date.day(), (unsigned)gps.time.hour(),
        (unsigned)gps.time.minute(), (unsigned)gps.time.second());
    weatherMeasurement.gpsUtcValid = (n == 20);
  }
#if THESIS_V003_F15_SD_PILOT_ARMED == 1
  const bool queued = ThesisSDLogger::queueWeatherSample(weatherMeasurement);
  Serial.printf("[F15-WX] median_queued=%u inputs=%u history=%u utc_valid=%u\n",
                unsigned(queued), unsigned(weatherMeasurement.inputsOK),
                unsigned(weatherMeasurement.historyCount),
                unsigned(weatherMeasurement.gpsUtcValid));
#else
  Serial.println("[F15-WX] BUILD_ONLY_NO_SD_QUEUE");
#endif
}
#endif

#if THESIS_V003_F19_DUAL_DISPLAY == 1 && THESIS_V003_F15_SD_PILOT_ARMED == 1
// Sampling/persistence are intentionally separate: this queues only RAM records.
// Never alters LoRa, radio scheduler, SOS control, NVS, or the sensor bus.
static void capturePeriodicResearchSnapshot() {
  static uint32_t lastCaptureMs=0;
  const uint32_t now=millis();
  if (!ThesisSDLogger::ready || !ThesisSDLogger::hybridIsContinuous()) {
    lastCaptureMs=now;return;
  }
  if (uint32_t(now-lastCaptureMs)<30000UL) return;
  lastCaptureMs=now;
  ThesisSDLogger::ResearchSystemSnapshot s{};
  s.uptimeMs=now;s.txOk=txCount;s.txFail=txFailures;
  s.rxTelemetry=getTotalPeerTelemetryReceived();
  s.invalidPackets=invalidPackets;s.peerCount=(uint32_t)getTrackedPeerCount();
  s.gpsFix=gpsHasFix;
  s.positionValid=gpsHasFix && gps.location.isValid() && gps.location.age()<=10000UL;
  if (s.positionValid) {s.latitude=latitude;s.longitude=longitude;}
  s.satellitesValid=gps.satellites.isValid() && gps.satellites.age()<=10000UL;
  if (s.satellitesValid) s.satellites=(uint32_t)gps.satellites.value();
  s.hdopValid=gps.hdop.isValid() && gps.hdop.age()<=10000UL;
  if (s.hdopValid) s.hdop=gps.hdop.hdop();
  s.speedValid=gps.speed.isValid() && gps.speed.age()<=10000UL;
  if (s.speedValid) s.speedKmph=gps.speed.kmph();
  if (gps.date.isValid() && gps.time.isValid() &&
      gps.date.age()<=10000UL && gps.time.age()<=10000UL) {
    const int n=snprintf(s.gpsUtc,sizeof(s.gpsUtc),
        "%04u-%02u-%02uT%02u:%02u:%02uZ",
        (unsigned)gps.date.year(),(unsigned)gps.date.month(),(unsigned)gps.date.day(),
        (unsigned)gps.time.hour(),(unsigned)gps.time.minute(),(unsigned)gps.time.second());
    s.utcValid=(n==20);
  }
  const bool systemQueued=ThesisSDLogger::queueResearchSystem(s);
  uint32_t peerQueued=0,peerRejected=0;
  const size_t peerCount=getTrackedPeerCount();
  // At most 10 bounded peer snapshots per 30 seconds (even inactive peers).
  for(size_t i=0;i<peerCount && i<10U;++i) {
    PeerDisplaySnapshot peer{};
    if(!getPeerSnapshotByIndex(i,peer)) {++peerRejected;continue;}
    if(ThesisSDLogger::queueResearchPeer(peer.id,peer.session,peer.received,
         peer.unique,peer.duplicates,peer.missing,peer.pdr,
         peer.rssi,peer.snr,peer.ageSeconds,peer.active)) ++peerQueued;
    else ++peerRejected;
  }
  Serial.printf("[F19R3-RESEARCH] system_queued=%u peer_queued=%lu peer_rejected=%lu "
                "peers=%u mode=CONTINUOUS pdr=SEQUENCE_SPAN_ESTIMATE\n",
      unsigned(systemQueued),(unsigned long)peerQueued,(unsigned long)peerRejected,
      (unsigned)peerCount);
}
#endif // F19R3 research capture

