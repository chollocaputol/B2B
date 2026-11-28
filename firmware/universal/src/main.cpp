#include "runtime/runtime_state.inc"

#include "hardware/i2c_bus.inc"

#include "storage/record_capture.inc"

void setup() {
  Serial.begin(115200);
#if THESIS_DTN_F25A_BUILD_ONLY == 1
  const bool f25aBound=thesisF25ABindLoop();
  Serial.printf("[F25A] loop_bound=%u physical=DENIED legacy=OFF\n",unsigned(f25aBound));
#endif
  delay(1200);
  Serial.println();
  Serial.println("=== UNIVERSAL VESSEL: SCHEDULED TX + RX ===");
  Serial.println("Packet: 62 bytes | MAC adaptive telemetry (min base 5s)");
  Serial.printf("[WEATHER] %s, sample interval %lu ms; history %u samples\n",
                WEATHER_FAST_TEST ? "ACCELERATED DEVELOPMENT (NOT REAL 100-MIN HISTORY)" : "FIELD",
                WEATHER_INTERVAL, WEATHER_HISTORY_SIZE);

  SOSControls::begin();
  PhysicalIndicators::begin();
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  buttonLastReading = digitalRead(USER_BUTTON_PIN);
  buttonStableState = buttonLastReading;
  buttonLastDebounceTime = millis();
  Serial.println("[OLED] Tap GPIO0: next page; on ALERT page hold >=900ms: LIVE/NORMAL/FRONT/SQUALL demo");

  setupPMU();
#if THESIS_SD_PROBE == 1
  ThesisSD::probe(pmu);
#endif
  setupOLED();
#if THESIS_V003_F08_INDICATOR_BENCH == 1
  // GPIO38/39 are NOT configured for sensor I2C in this build-only profile.
  // Weather alerts are unavailable; authoritative SOS remains independent.
  weatherInputsValid = false;
  weatherState = WEATHER_CALIBRATING;
  Serial.println("[F08-BENCH] DHT20/BME680 initialization SKIPPED");
#elif THESIS_V003_F09_SHARED_SENSOR_BUS == 1
  // Move only this profile's identity check before external sensor access.
  VesselIdentity::load();
  f09SensorAuthorized = V003F09SharedSensorBus::authorized(
      VesselIdentity::ready(), VesselIdentity::nodeId());
#if THESIS_F22_FLEET_OPERATIONAL == 1
  // Override the legacy ID-3 gate only in the new common-fleet profile.
  f09SensorAuthorized = F22UniversalCapability::validIdentity(
      VesselIdentity::ready(), VesselIdentity::nodeId());
#endif
#if THESIS_STUDY_SF_UNIVERSAL_OFFLINE == 1
  // SF7: explicitly selected shared-wiring profile, same NVS policy for all IDs.
  f09SensorAuthorized = F22UniversalCapability::validIdentity(
      VesselIdentity::ready(), VesselIdentity::nodeId());
#endif
#if THESIS_F22R3A_STANDARD_FLEET_PREVIEW == 1
  // Override the legacy ID3-only rule in the separate BUILD-ONLY preview.
  // Operator MUST confirm physical SDA21/SCL48 standard wiring before flashing.
  f09SensorAuthorized = V003F09SharedSensorBus::authorizedStandardPreview(
      VesselIdentity::ready(), VesselIdentity::nodeId(),
      THESIS_F22R3A_V003_WIRING_EXPLICIT_OPT_IN == 1);
#endif
  Serial.printf("[F09-I2C] id=%lu authorized=%u SDA=21 SCL=48 OUTPUTS=OFF\n",
                (unsigned long)VesselIdentity::nodeId(), unsigned(f09SensorAuthorized));
  if (f09SensorAuthorized) {
    setupSensors();
  } else {
    weatherInputsValid = false;
    weatherState = WEATHER_CALIBRATING;
#if THESIS_STUDY_SF_UNIVERSAL_OFFLINE == 1
    Serial.println("[SF7] SENSOR ACCESS BLOCKED: invalid/unprovisioned identity");
#elif THESIS_F22R3A_STANDARD_FLEET_PREVIEW == 1
    Serial.println("[F22R3A-PREVIEW] SENSOR ACCESS BLOCKED: invalid/unprovisioned identity");
#else
    Serial.println("[F09-I2C] SENSOR ACCESS BLOCKED: requires Vessel-003");
#endif
  }
#else
  setupSensors();
#endif
  setupGPS();
#if THESIS_V003_F09_SHARED_SENSOR_BUS != 1
  VesselIdentity::load();
#endif
#if THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER == 1
  // Observe the existing sensor-init flags ONLY. No additional I2C transaction.
  // The active build's wiring is a compile-time claim, NOT reviewed hardware evidence.
  F22UniversalCapability::Evidence f22Observed{};
  f22Observed.vesselId=VesselIdentity::nodeId();
  f22Observed.provisionedIdentity=VesselIdentity::ready();
#if THESIS_V003_F09_SHARED_SENSOR_BUS == 1
  f22Observed.sensorWiring=F22UniversalCapability::SensorWiring::Shared2148;
#else
  f22Observed.sensorWiring=F22UniversalCapability::SensorWiring::LegacySplit;
#endif
  f22Observed.bme680Responded=bmeAvailable;
  f22Observed.dht20Responded=dhtAvailable;
  // Do not manufacture proof of board/pin approval or SD-card readiness.
  // Their Evidence defaults deliberately keep sensors, SD and GPIO unauthorized.
  const auto f22Decision=F22UniversalCapability::evaluate(f22Observed);
  Serial.printf("[F22R2-PASSIVE] id=%lu provisioned=%u bme_seen=%u dht_seen=%u "
                "board_review=UNKNOWN sensor_eligible=%u weather_complete=%u "
                "sd_eligible=%u gpio=%u auto_enroll=%u NO_NEW_IO\n",
                (unsigned long)f22Observed.vesselId,
                unsigned(f22Decision.identityAccepted),
                unsigned(f22Observed.bme680Responded),unsigned(f22Observed.dht20Responded),
                unsigned(f22Decision.sensorBusEligible),
                unsigned(f22Decision.weatherInputSetComplete),
                unsigned(f22Decision.sdMountEligible),
                unsigned(f22Decision.physicalOutputsEnabled),
                unsigned(f22Decision.autoEnrolled));
#endif
#if THESIS_F22R3B_STORAGE_POLICY_PREVIEW == 1
  // Deliberately supply ONLY validated identity. Every SD/media/review/consent
  // field is false, even if a physical card is inserted. This preview never
  // powers, mounts, reads or writes the card; it does not call ThesisSDLogger.
  F22FleetStoragePolicy::Evidence f22r3bEvidence{};
  f22r3bEvidence.vesselId = VesselIdentity::nodeId();
  f22r3bEvidence.provisionedIdentity = VesselIdentity::ready();
  const auto f22r3bDecision = F22FleetStoragePolicy::evaluate(f22r3bEvidence);
  Serial.printf("[F22R3B-STORAGE-PREVIEW] id=%lu identity=%u mount=%u "
                "sandbox_write=%u research=%u legacy_V002_read=%u "
                "format=%u auto_enroll=%u NO_SD_IO\n",
                (unsigned long)f22r3bEvidence.vesselId,
                unsigned(f22r3bDecision.identityAccepted),
                unsigned(f22r3bDecision.mountEligible),
                unsigned(f22r3bDecision.newSessionWriteEligible),
                unsigned(f22r3bDecision.researchJournalEligible),
                unsigned(f22r3bDecision.legacyV002HistoryReadEligible),
                unsigned(f22r3bDecision.formatOrEraseAllowed),
                unsigned(f22r3bDecision.autoEnroll));
#endif
#line 913
  setupLoRa();
  setupEmergencyIdentity();
  startReceive();
#if THESIS_DTN_F25A_BUILD_ONLY == 1
  // No legacy SD initialization, including power, pins, SPI or mount.
  Serial.println("[F25A] LEGACY_SD_INIT_BYPASSED; NO_SESSION_AUTHORITY");
#else
#if THESIS_SD_LOGGER == 1
#if THESIS_V003_F15_MACRO_SD_SANDBOX == 1
#if THESIS_V003_F15_SD_PILOT_ARMED == 1
  if (f09SensorAuthorized && VesselIdentity::ready() &&
      VesselIdentity::nodeId() == 3U) {
    const bool f15SdReady = ThesisSDLogger::begin(pmu, VesselIdentity::nodeId());
    Serial.printf("[F15-SD] V003_AUTHORIZED; sandbox_mount_ok=%u\n",
                  unsigned(f15SdReady));
  }
#if THESIS_F22_FLEET_OPERATIONAL == 1
  else if (f09SensorAuthorized && VesselIdentity::ready() &&
           F22UniversalCapability::validIdentity(true,VesselIdentity::nodeId())) {
    const bool f15SdReady = ThesisSDLogger::begin(pmu, VesselIdentity::nodeId());
    Serial.printf("[F15-SD] FLEET_AUTHORIZED; sandbox_mount_ok=%u\n",
                  unsigned(f15SdReady));
  }
#endif
  else {
    Serial.println("[F15-SD] BLOCKED: Vessel-003 sensor/identity authorization required");
  }
#else
  Serial.println("[F15-SD] BUILD_ONLY_NO_SD_MOUNT");
#endif
#else
  ThesisSDLogger::begin(pmu, VesselIdentity::nodeId());
#endif
#endif
#endif

#if THESIS_F07_WEATHER_LOG == 1 && THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1
  // One REAL reading at boot for immediate OLED fault detection.
  // Log the same physical sample, then continue at 10-minute intervals.
  processWeather();
  if (!ThesisSDLogger::queueWeatherSample(weatherMeasurement))
    Serial.println("[WX-LOG] BOOT_SAMPLE_QUEUE_FAILED (not recorded)");
#endif

#if THESIS_STUDY_SF_BUILD_ONLY == 1
  // Original build-only profile remains denied. The new bench profile is V003-only.
  bool sfStarted=false;
  if(thesisStudySfConfigured() && VesselIdentity::ready() &&
#if THESIS_STUDY_SF_OFFLINE_V003 == 1 || THESIS_STUDY_SF_SYNC_V003 == 1
     VesselIdentity::nodeId()==3U &&
#elif THESIS_STUDY_SF_UNIVERSAL_OFFLINE == 1
     f09SensorAuthorized &&
#endif
     pmu.setBLDO1Voltage(3300)) {
    pmu.enableBLDO1();delay(200);
    if(pmu.isEnableBLDO1()) {pinMode(34,OUTPUT);digitalWrite(34,HIGH);sfStarted=thesisStudySfStart(VesselIdentity::nodeId());}
  }
#if THESIS_STUDY_SF_OFFLINE_V003 == 1
  Serial.printf("[STUDY-SF] started=%u; V003 OFFLINE STORAGE BENCH; WIFI=DISABLED\n",unsigned(sfStarted));
#elif THESIS_STUDY_SF_SYNC_V003 == 1
  Serial.printf("[STUDY-SF] started=%u; V003 LOCAL HTTPS SYNC BENCH\n",unsigned(sfStarted));
#elif THESIS_STUDY_SF_CLOUD == 1
  Serial.printf("[STUDY-SF] started=%u; UNIVERSAL CLOUD CANDIDATE node=%lu; private config controls Wi-Fi\n",unsigned(sfStarted),(unsigned long)VesselIdentity::nodeId());
#elif THESIS_STUDY_SF_UNIVERSAL_OFFLINE == 1
  Serial.printf("[STUDY-SF] started=%u; UNIVERSAL OFFLINE node=%lu; WIFI=DISABLED\n",unsigned(sfStarted),(unsigned long)VesselIdentity::nodeId());
#else
  Serial.printf("[STUDY-SF] started=%u; build-only; no hardware grant\n",unsigned(sfStarted));
#endif
  Serial.println("[STUDY-SF] RECORD_MODE=SOS_ARMED; VOLATILE=1; ROUTINE=OFF");
#endif

  // Each boot receives a different first telemetry slot. Subsequent intervals
  // vary independently; no change to SOS timing or packet serialization.
  randomSeed(esp_random());
  telemetryMac.begin(VesselIdentity::nodeId(), millis(), esp_random());
  Serial.printf("[MAC-INIT] v=3G4E3A id=%lu mode=AIRTIME_ONLY "
                "firstInMs=%lu maxKnownPeers=10 cadMode=%u\n",
    (unsigned long)VesselIdentity::nodeId(),
    (unsigned long)(uint32_t)(telemetryMac.nextDueMs() - millis()), (unsigned)MAC_CAD_MODE);

  lastWeatherTime = millis();
#if THESIS_V003_F12_INDICATOR_PREVIEW == 1
#if THESIS_F22_FLEET_OPERATIONAL == 1 || THESIS_STUDY_SF_UNIVERSAL_OFFLINE == 1
  f12IndicatorPreview.begin(f09SensorAuthorized &&
      F22UniversalCapability::validIdentity(VesselIdentity::ready(),VesselIdentity::nodeId()));
#else
  f12IndicatorPreview.begin(f09SensorAuthorized &&
                            VesselIdentity::ready() && VesselIdentity::nodeId() == 3U);
#endif
  Serial.printf("[F12-PREVIEW] id=%lu authorized=%u GPIO_WRITES=0 "
                "micro=EXPERIMENTAL; weather/SOS routing VIRTUAL_ONLY\n",
                (unsigned long)VesselIdentity::nodeId(),
                unsigned(f12IndicatorPreview.authorized()));
#endif
#if THESIS_V003_F13_SUPERVISED_DRIVER == 1
  // F13 is a separate output owner. Normal F12 and PhysicalIndicators remain
  // software-only; physical outputs need a dedicated explicit LIVE profile.
  f13RailEnableBitCached = pmu.isEnableBLDO2();
  f13LastRailPollMs = millis();
#if THESIS_F22_FLEET_OPERATIONAL == 1 || THESIS_STUDY_SF_UNIVERSAL_OFFLINE == 1
  f13Indicators.begin(f09SensorAuthorized &&
      F22UniversalCapability::validIdentity(VesselIdentity::ready(),VesselIdentity::nodeId()),
      f13RailEnableBitCached);
#else
  f13Indicators.begin(f09SensorAuthorized && VesselIdentity::ready() &&
                      VesselIdentity::nodeId() == 3U, f13RailEnableBitCached);
#endif
  Serial.printf("[F13-DRIVER] id=%lu authorized=%u BLDO2_ENABLE_BIT=%u "
                "GPIO_OUTPUTS=%s micro=VIRTUAL_ONLY; BENCH_ONLY\n",
                (unsigned long)VesselIdentity::nodeId(),
                unsigned(f13Indicators.authorized()), unsigned(f13RailEnableBitCached),
                V003F13Indicators::kPhysicalOutputsEnabled ? "LIVE" : "DISABLED");
#endif
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  f11InitialBatchPending = true;
  f11MacroSampleForMicro = false;
  Serial.println("[F11-MACRO] five physical readings per 10min; gap=2000ms; tendency=100min; OUTPUTS=OFF");
#endif
#if THESIS_V003_F10_MICRO_NOWCAST == 1
  lastF10MicroTime = lastWeatherTime;
  f10InitialSamplePending = true;
  Serial.println("[F10-MICRO] first read at boot; then 60s; 10min window; EXPERIMENTAL risk outlook; OUTPUTS=OFF");
#endif
  lastOLEDUpdate = millis();
  Serial.printf("[UNIVERSAL] %s (ID %lu): firmware initialized; relay forwarding controlled by build; multi-vessel validation PENDING\n",
                VesselIdentity::label(), (unsigned long)VesselIdentity::nodeId());
#if THESIS_LAB_RELAY_TX == 1
  Serial.println("[LAB-RELAY] BUILD=ENABLED; BENCH ONLY; VESSEL-002 GATE");
#else
  Serial.println("[LAB-RELAY] BUILD=DISABLED");
#endif
#if THESIS_V003_F17_INTEGRATED_PREFLIGHT == 1
  Serial.println("[F17-INTEGRATED] F10_FORECAST=EXPERIMENTAL_VIRTUAL F11_MACRO=REAL F12=VIRTUAL F13_GPIO=DISABLED F16_SD_OLED=ENABLED RELAY=OFF");
#endif
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
  Serial.println("[F18-WEATHER] MACRO=EMPIRICAL_100MIN MICRO=EXPERIMENTAL_10_30MIN OLED=LONG_HOLD_PAGE1 SD=V003_SANDBOX GPIO=OFF");
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  Serial.println("[F19-UI] MODE=OPERATIONAL OP_PAGES=4 DEV_PAGES=" OLED_PAGE_TOTAL " HOME_MENU=HOLD SOS=PRIORITY GPIO=OFF");
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  Serial.println("[F19R3] DEFAULT=SOS_ARMED DEV7=SOS_ONLY DEV8=SD_RECORDING RESEARCH=CONTINUOUS_ONLY HISTORY=READ_ONLY");
#endif
// F23C Phase 1 begin
// F23C Phase 1 end
// F23D begin
// F23D end
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
// F23D physical begin
#if THESIS_F23D_PHYSICAL_CANDIDATE != 1
// F23D physical end
  const uint32_t f23dExpectedRole=THESIS_F23D_EVENT_QUERY?1U:(THESIS_F23D_RX_OBSERVER?3U:2U);
  Serial.printf("[F23D-READINESS] local=%lu expected=%lu role_ok=%u permission=%u OFFLINE_ONLY=1\n",
      (unsigned long)VesselIdentity::nodeId(),(unsigned long)f23dExpectedRole,
      unsigned(VesselIdentity::ready() && VesselIdentity::nodeId()==f23dExpectedRole),
      unsigned(F23DBench::kPhysicalPermissionInstalled));
// F23D physical begin
#else
  Serial.println("[F23D-PHYSICAL] CANDIDATE_ONLY=1 PLATFORM_EVIDENCE=UNAVAILABLE STARTUP=DISARMED LEGACY_RELAY=OFF");
#endif
// F23D physical end
#endif
// F23D readiness end
  Serial.println("[TEST] Type TEST ALL, TEST NORMAL, TEST FRONT, TEST SQUALL, TEST BOTH, TEST CALIB, TEST Z, or TEST HELP");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
#if THESIS_SD_LOGGER == 1 && THESIS_DTN_F25A_BUILD_ONLY != 1
#if THESIS_F06_SD_HYBRID == 1
  // Assess authoritative local and received SOS state before SD poll.
  // An unresolved SOS keeps recording regardless of manual mode.
  ThesisSDLogger::hybridObserveSOSPresence(
      hasActiveLocalSOS() || getActiveSOSCount() > 0);
#endif
// F23D SD begin
#if !THESIS_F23D_SD_EVIDENCE
// F23D SD end
  ThesisSDLogger::poll();
#if THESIS_DTN_F24M_BUILD_ONLY == 1
  // Read-only preview diagnostics; never a durability or end-to-end ACK claim.
  static uint32_t f24mReportAt=0;
  if(uint32_t(millis()-f24mReportAt)>=30000UL){
    f24mReportAt=millis();
    Serial.printf("[F24M-PREVIEW] pending=%u full_drop=%lu owner=DENIED durable=NO\n",
      unsigned(ThesisSDLogger::f24mSosCapture.pending()),
      (unsigned long)ThesisSDLogger::f24mSosCapture.lost());
  }
#endif
// F23D SD begin
#endif
// F23D SD end
#endif
#if THESIS_F06_SD_SANDBOX == 1 && THESIS_DTN_F25A_BUILD_ONLY != 1
// F23D SD begin
#if !THESIS_F23D_SD_EVIDENCE
// F23D SD end
  ThesisSDLogger::pollHistory();
// F23D SD begin
#endif
// F23D SD end
#endif
  updateGPS();
#if THESIS_STUDY_SF_CLOUD == 1
  const bool shoreDevAllowed=!f19Display.operational() && !hasActiveLocalSOS() && getActiveSOSCount()==0;
  studyShore.observe(gpsHasFix && gps.location.isValid() && gps.location.age()<GPSService::kFreshFixMs,
                     latitude,longitude,shoreDevAllowed);
  thesisStudySfSetShoreAllowed(studyShore.allowed());
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  pollDisplayBatteryStatus(); // 15-second cached PMU reading, no OLED I2C reads
#endif
#if THESIS_STUDY_SF_BUILD_ONLY == 1
  static uint32_t studyLastGps=0;
  if(uint32_t(millis()-studyLastGps)>=30000UL){studyLastGps=millis();char utc[21]{};
    int battery=-1;
#if THESIS_V003_F19_DUAL_DISPLAY == 1
    if(displayBatteryPmuReady && lastDisplayBatteryPoll!=0U && uint32_t(studyLastGps-lastDisplayBatteryPoll)<=15000UL && cachedDisplayBatteryPercent>=0 && cachedDisplayBatteryPercent<=100)
      battery=cachedDisplayBatteryPercent;
#endif
    (void)thesisStudySfGps(studyLastGps,studyUtc(utc),gpsHasFix && gps.location.isValid() && gps.location.age()<=10000UL,latitude,longitude,battery);}
#endif
#if THESIS_V003_F19_DUAL_DISPLAY == 1 && THESIS_V003_F15_SD_PILOT_ARMED == 1
  capturePeriodicResearchSnapshot();
#endif
#if THESIS_FLEET_USB_ENROLLMENT == 1
  // USB-only: consumes FLEET lines without taking TEST/CANCEL SOS/SD commands.
  const bool fleetSerialConsumed = FleetUSBEnrollment::poll();
#endif
#if THESIS_SD_LOGGER == 1
  const bool sdCommandInProgress = pollSDExportCommand();
// F23D SD begin
#if !THESIS_F23D_SD_EVIDENCE
// F23D SD end
#if THESIS_DTN_F25A_BUILD_ONLY != 1
  ThesisSDLogger::pollExport();
#endif
// F23D SD begin
#endif
// F23D SD end
#if THESIS_FLEET_USB_ENROLLMENT == 1
  if (!sdCommandInProgress && !fleetSerialConsumed) processSerialTestCommands();
#else
  if (!sdCommandInProgress) processSerialTestCommands();
#endif
#else
#if THESIS_FLEET_USB_ENROLLMENT == 1
  if (!fleetSerialConsumed) processSerialTestCommands();
#else
  processSerialTestCommands();
#endif
#endif
  const auto action = SOSControls::pollAction(hasActiveLocalSOS());
  if (action == SOSControls::ButtonAction::StartSOS) {
    Serial.println("[SOS-BUTTON-A] Deliberate 1.5s SOS hold recognized");
    SOSControls::noteLocalSOS();
    transmitSOS();
  } else if (action == SOSControls::ButtonAction::CancelSOS) {
    Serial.println("[SOS-BUTTON-A] Deliberate 5s CANCEL hold recognized");
    cancelActiveSOS();
  }

  // CAD uses the same DIO1 as RX_DONE; consume its IRQ before RX polling.
  const TelemetryCadResult macCadCompleted = pollTelemetryCad();
  pollLoRaReceive();
  if (consumeNewReceivedSOS()) {
    oledPage = 6;  // New SOS overrides any page/selection.
#if THESIS_V003_F19_DUAL_DISPLAY == 1
    f21r42SOSBrowse.preempt(); // display attention only; radio record unchanged
    f19Display.dismissHomeMenuForSOS(); // a new alert replaces any old menu
    oledPeerMenuOpen=false;
#endif
    sosMenuOpen = false;
#if THESIS_F06_SD_HYBRID == 1
    sdStorageMenuOpen = false;
    sdHybridMenuOpen = false;
    sdOLEDSubview = 0;
#endif
    selectedSOSOrigin=0;selectedSOSEvent=0;
    lastOLEDUpdate = 0;
  }
  processSOSRetries();
  processCancelRetries();
#if THESIS_DTN_F25A_BUILD_ONLY == 1 && THESIS_STUDY_SF_BUILD_ONLY != 1
  thesisF25APoll(ThesisSDLogger::f24mSosCapture,thesisF25ARadioBusy());
#endif
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
  f23dPollEvidence(); // bounded nonblocking drain after operational controls
#endif
// F23D readiness end
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
  ThesisSDLogger::pollF23DEvidence(f23dSDRadioBusy());
#endif
// F23D SD end

#if THESIS_V003_F08_INDICATOR_BENCH == 0
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  if (f09SensorAuthorized) {
    const uint32_t batchNow = millis();
    const bool startDue = f11InitialBatchPending ||
        (!f11MacroBatch.active() && uint32_t(batchNow - lastWeatherTime) >= WEATHER_INTERVAL);
    if (startDue) {
      f11InitialBatchPending = false;
      lastWeatherTime = batchNow;
      f11MacroBatch.start(batchNow);
      Serial.printf("[F11-MACRO] START ms=%lu count=5 gapMs=2000\n",
                    (unsigned long)batchNow);
    }
    if (f11MacroBatch.due(millis())) {
      MicroWeatherSample physical{};
      const bool readOK = readMicroWeatherSample(physical);
      V003F11Macro::Sample sample{};
      if (readOK) {
        sample.temperatureC = physical.temperatureC;
        sample.humidityPercent = physical.humidityPercent;
        sample.stationPressureHpa = physical.stationPressureHpa;
      }
      const bool finished = f11MacroBatch.push(millis(), readOK, sample);
      Serial.printf("[F11-MACRO-READ] n=%u/5 valid=%u\n",
                    unsigned(f11MacroBatch.count()), unsigned(readOK));
      if (finished) {
        const bool batchOK = f11MacroBatch.valid();
        const auto median = f11MacroBatch.result();
        processWeatherFromF11Median(batchOK, median.temperatureC,
                                     median.humidityPercent, median.stationPressureHpa);
#if THESIS_STUDY_SF_BUILD_ONLY == 1
        studyMacroCapture();
#endif
        f11MacroSampleForMicro = true; // one fresh, accepted batch; no sixth read
        Serial.printf("[F11-MACRO] samples=5/5 valid=%u Tmed=%.2f RHmed=%.2f "
                      "Pmed=%.2f history=%u/%u dP100=%+.2f Z=%u state=%s\n",
                      unsigned(batchOK && weatherInputsValid),
                      median.temperatureC, median.humidityPercent,
                      median.stationPressureHpa,
                      unsigned(weatherMeasurement.historyCount), unsigned(WEATHER_HISTORY_SIZE),
                      pressureDelta, unsigned(zambrettiIndex), weatherName(weatherState));
#if THESIS_V003_F15_MACRO_SD_SANDBOX == 1
        // Precisely ONE queued record per completed 5-reading macro batch.
        // F10 may reuse this median; it MUST NOT trigger another SD record.
        queuePhysicalMacroMedianForStorage();
#endif
      }
    }
  }
#else
  if (millis() - lastWeatherTime >= WEATHER_INTERVAL) {
    lastWeatherTime = millis();
#if THESIS_V003_F09_SHARED_SENSOR_BUS == 1
    if (f09SensorAuthorized) {
      processWeather();
      Serial.printf("[F09-SAMPLE] dht=%u bme_pressure=%u inputs=%u history=%u weather=%s\n",
                    unsigned(dhtReadingValid), unsigned(bmePressureReadingValid),
                    unsigned(weatherInputsValid), unsigned(weatherMeasurement.historyCount),
                    weatherName(weatherState));
    }
#else
    processWeather();
#endif
#if THESIS_SD_LOGGER == 1 && THESIS_F07_WEATHER_LOG == 1
    // GNSS time is usable without a position fix, but only if both the
    // date and time were received recently. Otherwise JSON gets null.
    // Do not change the device clock or synthesize a calendar time.
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
    if (!ThesisSDLogger::queueWeatherSample(weatherMeasurement))
      Serial.println("[WX-LOG] QUEUE_FAILED (not recorded)");
#endif
  }
#endif  // THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
#endif  // THESIS_V003_F08_INDICATOR_BENCH == 0

#if THESIS_V003_F10_MICRO_NOWCAST == 1
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  // F11 owns the I2C bus during its nonblocking five-reading batch.
  // The completed median is the micro sample on each shared boundary.
  if (f09SensorAuthorized && !f11MacroBatch.active() &&
      (f11MacroSampleForMicro || f10InitialSamplePending ||
       uint32_t(millis() - lastF10MicroTime) >= V003F10Micro::kIntervalMs)) {
#else
  if (f09SensorAuthorized &&
      (f10InitialSamplePending ||
       uint32_t(millis() - lastF10MicroTime) >= V003F10Micro::kIntervalMs)) {
#endif
    f10InitialSamplePending = false;
    lastF10MicroTime = millis();
    MicroWeatherSample sample{};
    // The macro layer can complete a physical read at the same 10-minute
    // boundary. Reuse its accepted, fresh sample instead of issuing a second
    // back-to-back DHT20/BME680 transaction that can invalidate the micro FIFO.
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
    const bool reusedMacro = f11MacroSampleForMicro &&
        V003F10Micro::shouldReuseMacroSample(
            lastF10MicroTime, weatherSampleAttempted, weatherMeasurement.uptimeMs);
    f11MacroSampleForMicro = false;
#else
    const bool reusedMacro = V003F10Micro::shouldReuseMacroSample(
        lastF10MicroTime, weatherSampleAttempted, weatherMeasurement.uptimeMs);
#endif
    bool valid = false;
    if (reusedMacro) {
      valid = weatherMeasurement.inputsOK && weatherMeasurement.dhtOK &&
              weatherMeasurement.bmePressureOK && weatherMeasurement.tempSource == 1U;
      if (valid) {
        sample.temperatureC = weatherMeasurement.selectedTempC;
        sample.humidityPercent = weatherMeasurement.selectedRH;
        sample.stationPressureHpa = weatherMeasurement.stationPressureHpa;
      }
    } else {
      valid = readMicroWeatherSample(sample);
    }
    if (valid) {
      f10Forecaster.ingest(lastF10MicroTime, sample.temperatureC,
                           sample.humidityPercent, sample.stationPressureHpa);
    } else {
      f10Forecaster.invalidate();
    }
#if THESIS_STUDY_SF_BUILD_ONLY == 1
    char studyMicroUtc[21]{};
    (void)thesisStudySfWeather(lastF10MicroTime,studyUtc(studyMicroUtc),valid?sample.temperatureC:NAN,valid?sample.humidityPercent:NAN,valid?sample.stationPressureHpa:NAN,NAN,weatherState,V003F10Micro::stateName(f10Forecaster.state()),V003F10Micro::patternName(f10Forecaster.pattern()),valid);
#endif
    Serial.printf("[F10-MICRO] valid=%u count=%u/11 state=%s pattern=%s "
                  "dT10=%+.2fC dRH10=%+.2fpp dP10=%+.2fhPa dewGap=%.2fC "
                  "horizon=EXPERIMENTAL_10_30_MIN OUTPUTS=OFF source=%s\n",
                  unsigned(valid), unsigned(f10Forecaster.count()),
                  V003F10Micro::stateName(f10Forecaster.state()),
                  V003F10Micro::patternName(f10Forecaster.pattern()),
                  f10Forecaster.deltaTemperature(), f10Forecaster.deltaHumidity(),
                  f10Forecaster.deltaPressure(), f10Forecaster.dewPointGap(),
                  reusedMacro ? "MACRO_REUSED" : "DIRECT");
#if THESIS_V003_F18_WEATHER_INTERFACE == 1
#if THESIS_V003_F15_SD_PILOT_ARMED == 1
    // Micro data is an independent research product, NOT a physical alert.
    // Same bounded telemetry FIFO as F15: urgent SOS always dequeues first.
    const bool microQueued = ThesisSDLogger::queueMicroOutlook(
        lastF10MicroTime, valid, f10Forecaster.count(),
        V003F10Micro::stateName(f10Forecaster.state()),
        V003F10Micro::patternName(f10Forecaster.pattern()),
        f10Forecaster.deltaTemperature(), f10Forecaster.deltaHumidity(),
        f10Forecaster.deltaPressure(), f10Forecaster.dewPointGap(), reusedMacro);
    Serial.printf("[F18-MICRO-SD] queued=%u state=%s source=%s\n",
                  unsigned(microQueued), V003F10Micro::stateName(f10Forecaster.state()),
                  reusedMacro ? "MACRO_REUSED" : "DIRECT");
#else
    Serial.println("[F18-MICRO-SD] BUILD_ONLY_NO_SD_QUEUE");
#endif
#endif
  }
#endif

  // Live authoritative inputs only; OLED demo modes never reach physical outputs.
  PhysicalIndicators::update(millis(), weatherState, weatherInputsValid,
                             hasActiveLocalSOS(), getActiveSOSCount() > 0);
#if THESIS_V003_F12_INDICATOR_PREVIEW == 1
  // F12 is a separate, VIRTUAL policy instance. Production physical adapter
  // remains disabled. The OLED demo state never enters this decision path.
  const uint32_t f12Now = millis();
  const auto f12Result = f12IndicatorPreview.preview(
      f12Now, weatherState, weatherInputsValid, f10Forecaster.state(),
      hasActiveLocalSOS(), getActiveSOSCount() > 0);
  if (f12Result.authorized &&
      (f12Result.changed || uint32_t(f12Now - f12LastReportMs) >= 60000UL)) {
    f12LastReportMs = f12Now;
    Serial.printf("[F12-PREVIEW] source=%s mode=%u yellow=%u red=%u buzzer=%u "
                  "GPIO_WRITES=0 micro=EXPERIMENTAL\n",
                  V003F12Preview::name(f12Result.source), unsigned(f12Result.mode),
                  unsigned(f12Result.virtualOutputs.yellow),
                  unsigned(f12Result.virtualOutputs.red),
                  unsigned(f12Result.virtualOutputs.buzzer));
  }
#endif
#if THESIS_V003_F13_SUPERVISED_DRIVER == 1
  // Independent F13 production-weather policy; never uses OLED demos or
  // experimental F10 micro-watch as a physical warning source.
  const uint32_t f13Now = millis();
  // PMU enable-bit polling is limited to 1 Hz; sensor/radio loop stays free.
  // This bit is not a measurement of the external rail voltage/current.
  if (uint32_t(f13Now - f13LastRailPollMs) >= 1000U) {
    f13LastRailPollMs = f13Now;
    f13RailEnableBitCached = pmu.isEnableBLDO2();
  }
  uint8_t f13HardwareState = weatherState;
  bool f13HardwareValid = weatherInputsValid;
  f13Indicators.tick(f13Now, f13HardwareState, f13HardwareValid,
                     hasActiveLocalSOS(), getActiveSOSCount() > 0,
                     f13RailEnableBitCached);
  if (f13Indicators.authorized() &&
      uint32_t(f13Now - f13LastReportMs) >= 60000UL) {
    f13LastReportMs = f13Now;
    const auto physical = f13Indicators.outputs();
    Serial.printf("[F13-DRIVER] mode=%u yellow=%u red=%u buzzer=%u "
                  "GPIO_OUTPUTS=%s railLost=%u micro=VIRTUAL_ONLY\n",
                  unsigned(f13Indicators.mode()), unsigned(physical.yellow),
                  unsigned(physical.red), unsigned(physical.buzzer),
                  V003F13Indicators::kPhysicalOutputsEnabled ? "LIVE" : "DISABLED",
                  unsigned(f13Indicators.railLost()));
  }
#endif

  // PHASE 5.2.2C-3G-4E2: decentralized, nonblocking CAD experiment.
  // Mode 1 OBSERVE (default): busy CAD is logged but does not block telemetry.
  // Mode 2 ENFORCE (opt-in): finite random backoff, then fail-open TX.
#if MAC_CAD_MODE == 0
  if (telemetryMac.due(millis())) {
    const uint32_t macNowMs = millis();
    const uint32_t macPeers = getRecentMacPeerCount(
        macNowMs, DecentralizedMac::kRecentPeerHorizonMs);
    const uint32_t macIntervalMs = telemetryMac.scheduleNext(macNowMs, macPeers);
    Serial.printf("[MAC-TX] id=%lu mode=OFF peers=%lu baseMs=%lu nextInMs=%lu\n",
      (unsigned long)VesselIdentity::nodeId(), (unsigned long)macPeers,
      (unsigned long)telemetryMac.baseMs(), (unsigned long)macIntervalMs);
    transmitPacket();
  }
#else
  static MacContention::Gate macGate;
  if (!macGate.pending() && telemetryMac.due(millis())) {
    macGate.begin(millis());
  }
  if (macGate.pending()) {
    const uint32_t macNowMs = millis();
    TelemetryCadResult outcome = macCadCompleted;
    if (outcome == TelemetryCadResult::None &&
        !telemetryCadRunning() && macGate.ready(macNowMs)) {
      outcome = startTelemetryCad();
    }
    if (outcome != TelemetryCadResult::None &&
        outcome != TelemetryCadResult::Scanning) {
      MacContention::Sense sense = MacContention::Sense::Error;
      const char *outcomeName = "ERROR";
      switch (outcome) {
        case TelemetryCadResult::Free: sense=MacContention::Sense::Free; outcomeName="FREE"; break;
        case TelemetryCadResult::Busy: sense=MacContention::Sense::Busy; outcomeName="BUSY"; break;
        case TelemetryCadResult::RxActive: sense=MacContention::Sense::RxActive; outcomeName="RX_ACTIVE"; break;
        case TelemetryCadResult::Timeout: sense=MacContention::Sense::Timeout; outcomeName="TIMEOUT"; break;
        case TelemetryCadResult::Interrupted: sense=MacContention::Sense::Interrupted; outcomeName="CONTROL_TX"; break;
        default: break;
      }
      const uint32_t beforeMs = millis();
      const auto decision = macGate.report(beforeMs, sense,
                                           MAC_CAD_MODE, esp_random());
      Serial.printf("[MAC-CAD] id=%lu mode=%s result=%s action=%s "
                    "elapsedMs=%lu retries=%u\n",
        (unsigned long)VesselIdentity::nodeId(),
        MAC_CAD_MODE == 1 ? "OBSERVE" : "ENFORCE", outcomeName,
        decision == MacContention::Decision::Wait ? "BACKOFF" :
        decision == MacContention::Decision::ForcedSend ? "FORCE_SEND" : "SEND",
        (unsigned long)macGate.elapsed(beforeMs), (unsigned)macGate.retries());
      if (decision != MacContention::Decision::Wait) {
        const uint32_t nowTx = millis();
        const uint32_t peers = getRecentMacPeerCount(
            nowTx, DecentralizedMac::kRecentPeerHorizonMs);
        const uint32_t nextMs = telemetryMac.scheduleNext(nowTx, peers);
        macGate.finish(); // reset before the blocking radio transmission
        Serial.printf("[MAC-TX] id=%lu mode=%s peers=%lu nodes=%lu baseMs=%lu nextInMs=%lu\n",
          (unsigned long)VesselIdentity::nodeId(),
          MAC_CAD_MODE == 1 ? "OBSERVE" : "ENFORCE",
          (unsigned long)peers, (unsigned long)(peers + 1UL),
          (unsigned long)telemetryMac.baseMs(), (unsigned long)nextMs);
        transmitPacket(); // original routine TX and RX restoration unchanged
      }
    }
  }
#endif

  if (millis() - lastGPSPrint >= GPS_STATUS_INTERVAL) {
    lastGPSPrint = millis();
    printGPSStatus();
  }

  // Uptime begins at MCU boot and is not dependent on GPS or SD storage.
  static unsigned long lastUptimePrint = 0;
  if (millis() - lastUptimePrint >= UPTIME_PRINT_INTERVAL) {
    lastUptimePrint = millis();
    char uptimeText[40];
    formatUptime(uptimeText, sizeof(uptimeText));
    Serial.printf("[UPTIME-A] %s | Weather=%s | TX_OK=%lu | TX_FAIL=%lu | RX_TELEMETRY=%lu\n",
                  uptimeText, weatherName(weatherState),
                  static_cast<unsigned long>(txCount),
                  static_cast<unsigned long>(txFailures),
                  static_cast<unsigned long>(getTotalPeerTelemetryReceived()));
  }

  handleOLEDButton();

  if (millis() - lastOLEDUpdate >= OLED_UPDATE_INTERVAL) {
    lastOLEDUpdate = millis();
    updateOLED();
  }
  delay(5);
}

#include "hardware/initialization.inc"

#include "hardware/gnss.inc"

#include "display/navigation.inc"

#include "display/rendering.inc"

