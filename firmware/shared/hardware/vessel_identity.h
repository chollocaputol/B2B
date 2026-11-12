#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Phase 5.2.2C-2C: read-only identity consumer. The standalone provisioner
// writes vessel-ident/nodeId and vessel-ident/label.
namespace VesselIdentity {
  inline uint32_t& storedId() { static uint32_t id=0; return id; }
  inline char* storedLabel() { static char name[12]={0}; return name; }
  inline bool& validFlag() { static bool valid=false; return valid; }
  inline uint32_t nodeId() { return validFlag() ? storedId() : 0; }
  inline const char* label() { return validFlag() ? storedLabel() : "UNPROVISION"; }
  inline bool ready() { return validFlag(); }
  inline bool load() {
    validFlag()=false;
    storedId()=0;
    storedLabel()[0]='\0';
    Preferences prefs;
    if (!prefs.begin("vessel-ident", true)) { Serial.println("[IDENTITY] NVS OPEN FAIL; TX BLOCKED"); return false; }
    uint32_t id=prefs.getUInt("nodeId", 0);
    String name=prefs.getString("label", "");
    prefs.end();
    char expected[12];
    snprintf(expected,sizeof(expected), "Vessel-%03lu", (unsigned long)id);
    if (id<1 || id>9999 || name!=expected || name.length()>11) {
      Serial.printf("[IDENTITY] INVALID/UNPROVISIONED id=%lu label=%s; TX BLOCKED\n", (unsigned long)id, name.c_str());
      return false;
    }
    storedId()=id;
    strncpy(storedLabel(), expected, 11);
    storedLabel()[11]='\0';
    validFlag()=true;
    Serial.printf("[IDENTITY] READY id=%lu label=%s\n", (unsigned long)id, storedLabel());
    return true;
  }
}
