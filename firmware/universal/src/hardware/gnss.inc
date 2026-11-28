// ============================================================
// GPS
// ============================================================
#if THESIS_V003_F19_DUAL_DISPLAY == 1
static GnssPhtClock::Tracker displayGnssLocalClock;
#endif
void setupGPS() {
  GPSService::begin(GPSSerial, "A");
}

void updateGPS() {
  gpsHasFix = GPSService::poll(GPSSerial, gps, latitude, longitude);
  if (gpsHasFix) haveLastKnownPosition = true;
#if THESIS_V003_F19_DUAL_DISPLAY == 1
  // A stationary startup UTC 00:00 is not reliable synchronized GNSS time.
  GnssPhtClock::Utc observed{};
  observed.year=static_cast<uint16_t>(gps.date.year());
  observed.month=static_cast<uint8_t>(gps.date.month());
  observed.day=static_cast<uint8_t>(gps.date.day());
  observed.hour=static_cast<uint8_t>(gps.time.hour());
  observed.minute=static_cast<uint8_t>(gps.time.minute());
  observed.second=static_cast<uint8_t>(gps.time.second());
  observed.fresh=gps.date.isValid() && gps.time.isValid() &&
    gps.date.age()<=GnssPhtClock::kMaxFreshMs &&
    gps.time.age()<=GnssPhtClock::kMaxFreshMs;
  displayGnssLocalClock.observe(observed,millis());
#endif
}

void printGPSStatus() {
  GPSService::printStatus("A", gps, gpsHasFix, latitude, longitude);
}

