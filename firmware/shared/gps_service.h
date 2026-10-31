#pragma once
// Phase 5.2.2B-2: shared L76K GPS service. No change to fix threshold,
// hardware pins, stored coordinates, or existing telemetry consumers.
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "tbeam_pins.h"

namespace GPSService {
static constexpr uint32_t kFreshFixMs = 3000UL;

inline void begin(HardwareSerial& port, const char* tag) {
  pinMode(GPS_EN_PIN, OUTPUT);
  digitalWrite(GPS_EN_PIN, HIGH);
  pinMode(GPS_PPS_PIN, INPUT);
  port.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.printf("[GPS-%s] L76K enabled\n", tag);
}

inline bool poll(HardwareSerial& port, TinyGPSPlus& parser,
                 double& latitude, double& longitude) {
  while (port.available()) parser.encode(port.read());
  const bool fresh = parser.location.isValid() &&
                     parser.location.age() < kFreshFixMs;
  if (fresh) {
    latitude = parser.location.lat();
    longitude = parser.location.lng();
  }
  return fresh;
}

inline void printStatus(const char* tag, const TinyGPSPlus& parser,
                        bool fix, double latitude, double longitude) {
  Serial.println();
  Serial.printf("[GPS-%s]\n", tag);
  Serial.print("Fix: "); Serial.println(fix ? "YES" : "NO");
  Serial.print("Satellites: ");
  if (parser.satellites.isValid()) Serial.println(parser.satellites.value());
  else Serial.println("?");
  Serial.print("HDOP: ");
  if (parser.hdop.isValid()) Serial.println(parser.hdop.hdop(), 1);
  else Serial.println("?");
  if (fix) {
    Serial.print("Lat: "); Serial.println(latitude, 7);
    Serial.print("Lon: "); Serial.println(longitude, 7);
  }
}
} // namespace GPSService
