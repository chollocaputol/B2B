#pragma once
// USB serial-only first enrollment for identical V003-standard fleet devices.
// No network enrollment, no NVS erase, no automatic identity replacement.
#include <Arduino.h>
#include <Preferences.h>
#include <esp_mac.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "vessel_identity.h"

namespace FleetUSBEnrollment {
inline bool efuseMac(char out[13]) {
  uint8_t mac[6] = {};
  if (esp_efuse_mac_get_default(mac) != ESP_OK) return false;
  snprintf(out,13,"%02X%02X%02X%02X%02X%02X",
           unsigned(mac[0]),unsigned(mac[1]),unsigned(mac[2]),
           unsigned(mac[3]),unsigned(mac[4]),unsigned(mac[5]));
  return true;
}
inline void status() {
  char mac[13] = {};
  if (!efuseMac(mac)) { Serial.println("[FLEET] ERROR MAC_UNAVAILABLE"); return; }
  bool existing = false;
  Preferences p;
  if (p.begin("vessel-ident", true)) {
    existing = p.isKey("nodeId") || p.isKey("label");
    p.end();
  } else {
    Serial.println("[FLEET] ERROR NVS_READ_FAILED"); return;
  }
  const char* state = VesselIdentity::ready() ? "READY" : (existing ? "INVALID" : "BLANK");
  Serial.printf("[FLEET] STATUS MAC=%s STATE=%s ID=%lu LABEL=%s\n",
                mac,state,(unsigned long)VesselIdentity::nodeId(),VesselIdentity::label());
}
inline void enroll(const char* request) {
  unsigned long rawId = 0;
  char suppliedMac[13] = {};
  char extra = 0;
  // Canonical wire format: FLEET ENROLL <1..9999> <12 uppercase MAC hex>.
  if (sscanf(request,"FLEET ENROLL %lu %12s %c",&rawId,suppliedMac,&extra)!=2 ||
      rawId<1UL || rawId>9999UL || strlen(suppliedMac)!=12U) {
    Serial.println("[FLEET] ERROR ENROLL_FORMAT"); return;
  }
  char actualMac[13] = {};
  if (!efuseMac(actualMac) || strcmp(actualMac,suppliedMac)!=0) {
    Serial.println("[FLEET] ERROR MAC_MISMATCH"); return;
  }
  if (VesselIdentity::ready()) {
    Serial.println("[FLEET] ERROR ALREADY_PROVISIONED"); return;
  }
  Preferences p;
  if (!p.begin("vessel-ident", true)) {
    Serial.println("[FLEET] ERROR NVS_READ_FAILED"); return;
  }
  const bool hasOldIdentity = p.isKey("nodeId") || p.isKey("label");
  p.end();
  if (hasOldIdentity) {
    Serial.println("[FLEET] ERROR EXISTING_KEYS_REFUSE_OVERWRITE"); return;
  }
  char name[12];
  snprintf(name,sizeof(name),"Vessel-%03lu",rawId);
  if (!p.begin("vessel-ident",false)) {
    Serial.println("[FLEET] ERROR NVS_WRITE_OPEN_FAILED"); return;
  }
  // Check again immediately before writing. Never clear an existing namespace.
  if (p.isKey("nodeId") || p.isKey("label")) {
    p.end(); Serial.println("[FLEET] ERROR EXISTING_KEYS_REFUSE_OVERWRITE"); return;
  }
  const bool idWritten = p.putUInt("nodeId",uint32_t(rawId))==sizeof(uint32_t);
  const bool nameWritten = idWritten && p.putString("label",name)==strlen(name);
  p.end();
  if (!idWritten || !nameWritten || !VesselIdentity::load() ||
      VesselIdentity::nodeId()!=uint32_t(rawId)) {
    // Partial write must be repaired manually, never retried automatically.
    Serial.println("[FLEET] ERROR WRITE_VERIFY_FAILED_MANUAL_RECOVERY"); return;
  }
  Serial.printf("[FLEET] ENROLLED MAC=%s ID=%lu LABEL=%s REBOOT_REQUIRED=1\n",
                actualMac,rawId,name);
}
inline void process(const char* command) {
  if (strcmp(command,"FLEET STATUS")==0) { status(); return; }
  if (strncmp(command,"FLEET ENROLL ",13)==0) { enroll(command); return; }
  Serial.println("[FLEET] ERROR UNKNOWN_COMMAND");
}
// Return true when consuming an in-progress FLEET line. Never consume TEST,
// CANCEL SOS or SD commands belonging to the existing serial handlers.
inline bool poll() {
  static char buffer[72] = {};
  static size_t length = 0;
  if (!Serial.available()) return length!=0;
  if (length==0 && Serial.peek()!='F') return false;
  while (Serial.available()) {
    const int value = Serial.read();
    if (value=='\n' || value=='\r') {
      if (length) { buffer[length]='\0'; process(buffer); }
      length=0; return true;
    }
    if (value<0 || length>=sizeof(buffer)-1) {
      length=0; Serial.println("[FLEET] ERROR COMMAND_TOO_LONG"); return true;
    }
    buffer[length++]=char(value);
  }
  return true;
}
} // namespace FleetUSBEnrollment
