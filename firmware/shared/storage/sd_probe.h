#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <XPowersLib.h>

namespace ThesisSD {

static SPIClass sdSPI(HSPI);

inline bool probe(XPowersAXP2101& pmu) {
    Serial.println("[SD-READONLY] Starting inspection");

    pmu.setBLDO1Voltage(3300);
    pmu.enableBLDO1();
    delay(200);

    if (!pmu.isEnableBLDO1()) {
        Serial.println("[SD-READONLY] POWER_FAIL");
        return false;
    }

    pinMode(34, OUTPUT);
    digitalWrite(34, HIGH);

    pinMode(47, OUTPUT);
    digitalWrite(47, HIGH);

    sdSPI.begin(36, 37, 35, 47);

    // Never format a card if mounting fails.
    if (!SD.begin(47, sdSPI, 1000000, "/sd", 5, false)) {
        Serial.println("[SD-READONLY] MOUNT_FAIL");
        return false;
    }

    Serial.println("[SD-READONLY] MOUNT_OK");

    Serial.printf(
        "[SD-READONLY] Capacity: %llu MB\n",
        (unsigned long long)(SD.cardSize() / 1048576ULL)
    );

    File root = SD.open("/", FILE_READ);

    if (!root || !root.isDirectory()) {
        Serial.println("[SD-READONLY] ROOT_OPEN_FAIL");
        return false;
    }

    Serial.println("[SD-READONLY] ROOT DIRECTORY:");

    unsigned int count = 0;

    File entry = root.openNextFile();

    while (entry && count < 100) {
        Serial.printf(
            "[SD-FILE] %s | %s | %llu bytes\n",
            entry.name(),
            entry.isDirectory() ? "DIR" : "FILE",
            (unsigned long long)entry.size()
        );

        entry.close();
        count++;

        entry = root.openNextFile();
    }

    if (entry) {
        entry.close();
        Serial.println("[SD-READONLY] LIST_TRUNCATED");
    }

    root.close();

    Serial.printf(
        "[SD-READONLY] LIST_COMPLETE entries=%u\n",
        count
    );

    Serial.println("[SD-READONLY] NO FILE WRITES REQUESTED");

    return true;
}

}