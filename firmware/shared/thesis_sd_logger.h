#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <XPowersLib.h>

// 0: original Vessel-002-only experimental profile; 1: identity-aware F11 profile.
#ifndef THESIS_SD_FLEET
#define THESIS_SD_FLEET 0
#endif
#if THESIS_SD_FLEET != 0 && THESIS_SD_FLEET != 1
#error "THESIS_SD_FLEET must be 0 or 1"
#endif

// F06 bench-only mode: create new per-boot SD journals in an isolated directory.
#ifndef THESIS_F06_SD_SANDBOX
#define THESIS_F06_SD_SANDBOX 0
#endif
#if THESIS_F06_SD_SANDBOX != 0 && THESIS_F06_SD_SANDBOX != 1
#error "THESIS_F06_SD_SANDBOX must be 0 or 1"
#endif
#if THESIS_F06_SD_SANDBOX == 1
#if THESIS_SD_FLEET != 1 || THESIS_F05_RECOVERY != 1
#error "F06 SD sandbox requires both fleet SD logger and F05 recovery"
#endif
#if THESIS_LAB_RELAY_TX == 1
#error "F06 SD sandbox cannot enable unauthenticated lab relay TX"
#endif
#endif

namespace ThesisSDLogger {

static SPIClass sdSPI(HSPI);
static bool ready = false;
static uint32_t lastHeartbeat = 0;
static uint32_t vesselId = 0;

static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000UL;

// Runtime paths are derived from validated NVS identity. For ID 2,
// they remain byte-for-byte identical to the existing SD journal paths.
static char DIRECTORY[64] = "/THESIS/VESSEL_002";
static char DIAGNOSTICS[64] = "/THESIS/VESSEL_002/diagnostics.jsonl";
inline bool appendRecord(const char* path, const char* record);
static char TELEMETRY[64] = "/THESIS/VESSEL_002/telemetry.jsonl";
static char SOS_EVENTS[64] = "/THESIS/VESSEL_002/sos_events.jsonl";

inline bool isAuthorizedVessel(uint32_t id) {
#if THESIS_SD_FLEET == 1
    return id >= 1 && id <= 3;
#else
    return id == 2;
#endif
}

// All SD operations run only in main loop, never in radio RX/TX handlers.
// Two bounded FIFOs allow SOS events to take precedence over telemetry.
static constexpr uint8_t EVENT_CAPACITY = 24;
struct EventRecord { char line[320]; };
struct EventQueue {
    EventRecord entries[EVENT_CAPACITY];
    uint8_t read = 0, write = 0, count = 0;
};
static EventQueue urgentQueue;
static EventQueue telemetryQueue;
static uint32_t bootToken = 0;
static uint32_t eventCounter = 0;
static uint32_t lostUrgent = 0, lostTelemetry = 0;
static uint32_t lastDropNotice = 0;

inline bool queueEvent(bool urgent, const char* kind, uint32_t origin,
                       uint32_t seq, int32_t result, int16_t rssi = 0,
                       int16_t snrX10 = 0, int32_t latE7 = 0,
                       int32_t lonE7 = 0, uint8_t flags = 0,
                       uint8_t hops = 0, uint8_t attempt = 0) {
    if (!ready) return false;
    EventQueue &q = urgent ? urgentQueue : telemetryQueue;
    if (q.count >= EVENT_CAPACITY) {
        if (urgent) ++lostUrgent; else ++lostTelemetry;
        return false;
    }
    char &first = q.entries[q.write].line[0];
    const int len = snprintf(&first, sizeof(q.entries[q.write].line),
        "{\"kind\":\"%s\",\"vessel_id\":%lu,\"boot_token\":%lu,"
        "\"event_index\":%lu,\"uptime_ms\":%lu,\"origin_id\":%lu,"
        "\"seq\":%lu,\"result\":%ld,\"rssi\":%d,\"snr_x10\":%d,"
        "\"lat_e7\":%ld,\"lon_e7\":%ld,\"flags\":%u,"
        "\"hops\":%u,\"attempt\":%u}",
        kind, (unsigned long)vesselId, (unsigned long)bootToken,
        (unsigned long)(++eventCounter), (unsigned long)millis(),
        (unsigned long)origin, (unsigned long)seq, (long)result,
        (int)rssi, (int)snrX10, (long)latE7, (long)lonE7,
        (unsigned)flags, (unsigned)hops, (unsigned)attempt);
    if (len < 0 || (size_t)len >= sizeof(q.entries[q.write].line)) {
        if (urgent) ++lostUrgent; else ++lostTelemetry;
        return false;
    }
    q.write = (q.write + 1) % EVENT_CAPACITY;
    ++q.count;
    return true;
}

inline void pollEvents() {
    if (!ready) return;
    EventQueue &q = urgentQueue.count ? urgentQueue : telemetryQueue;
    if (q.count) {
        const char* path = urgentQueue.count ? SOS_EVENTS : TELEMETRY;
        // One append+flush per loop iteration: no SPI writes in radio callbacks.
        if (!appendRecord(path, q.entries[q.read].line)) {
            Serial.println("[SD-EVENT] WRITE_FAILED; LOGGER_DISABLED");
            ready = false;
            return;
        }
        q.read = (q.read + 1) % EVENT_CAPACITY;
        --q.count;
    }
    const uint32_t now = millis();
    if ((lostUrgent || lostTelemetry) &&
        uint32_t(now - lastDropNotice) >= 5000UL) {
        lastDropNotice = now;
        Serial.printf("[SD-EVENT] DROPPED urgent=%lu telemetry=%lu\n",
          (unsigned long)lostUrgent, (unsigned long)lostTelemetry);
    }
}


inline bool appendRecord(const char* path, const char* record) {
    if (!ready) return false;

    File file = SD.open(path, FILE_APPEND);

    if (!file) {
        Serial.println("[SD-LOG] OPEN_FAIL");
        return false;
    }

    const size_t expected = strlen(record) + 1;
    const size_t written = file.print(record) + file.print('\n');

    file.flush();
    file.close();

    if (written != expected) {
        Serial.println("[SD-LOG] WRITE_FAIL");
        return false;
    }

    return true;
}

inline bool begin(XPowersAXP2101& pmu, uint32_t id) {
    Serial.println("[SD-LOG] START");

    vesselId = id;

    // Never initialize storage using an unprovisioned/unknown ID.
    if (!isAuthorizedVessel(vesselId)) {
        Serial.println("[SD-LOG] WRONG_VESSEL_ID");
        return false;
    }

    // Separate journals prevent a moved SD card from mixing vessels.
    snprintf(DIRECTORY, sizeof(DIRECTORY), "/THESIS/VESSEL_%03lu",
             (unsigned long)vesselId);
    snprintf(DIAGNOSTICS, sizeof(DIAGNOSTICS), "%s/diagnostics.jsonl", DIRECTORY);
    snprintf(TELEMETRY, sizeof(TELEMETRY), "%s/telemetry.jsonl", DIRECTORY);
    snprintf(SOS_EVENTS, sizeof(SOS_EVENTS), "%s/sos_events.jsonl", DIRECTORY);

    if (!pmu.setBLDO1Voltage(3300)) {
        Serial.println("[SD-LOG] REGULATOR_CONFIG_FAIL");
        return false;
    }

    pmu.enableBLDO1();
    delay(200);

    if (!pmu.isEnableBLDO1()) {
        Serial.println("[SD-LOG] POWER_FAIL");
        return false;
    }

    pinMode(34, OUTPUT);
    digitalWrite(34, HIGH);

    pinMode(47, OUTPUT);
    digitalWrite(47, HIGH);

    sdSPI.begin(36, 37, 35, 47);

    // Last argument false: do not format on mount failure.
    if (!SD.begin(47, sdSPI, 1000000, "/sd", 5, false)) {
        Serial.println("[SD-LOG] MOUNT_FAIL");
        return false;
    }

    Serial.printf(
        "[SD-LOG] MOUNT_OK capacity_mib=%llu\n",
        (unsigned long long)(SD.cardSize() / 1048576ULL)
    );

    if (!SD.exists("/THESIS") && !SD.mkdir("/THESIS")) {
        Serial.println("[SD-LOG] ROOT_CREATE_FAIL");
        return false;
    }

#if THESIS_F06_SD_SANDBOX == 1
    // Do not touch /THESIS/VESSEL_XXX journals. Every boot creates its own
    // empty session directory. Collisions are skipped, never reopened.
    const char* f06Root = "/THESIS/F06TEST";
    if (!SD.exists(f06Root) && !SD.mkdir(f06Root)) {
        Serial.println("[SD-F06] TEST_ROOT_CREATE_FAIL; NO JOURNAL WRITES");
        return false;
    }
    char f06VesselRoot[40];
    snprintf(f06VesselRoot, sizeof(f06VesselRoot), "%s/V%03lu",
             f06Root, (unsigned long)vesselId);
    if (!SD.exists(f06VesselRoot) && !SD.mkdir(f06VesselRoot)) {
        Serial.println("[SD-F06] VESSEL_TEST_ROOT_CREATE_FAIL; NO JOURNAL WRITES");
        return false;
    }
    bool isolatedDirCreated = false;
    for (uint8_t tryNumber = 0; tryNumber < 16; ++tryNumber) {
        char candidate[sizeof(DIRECTORY)];
        const uint32_t token = esp_random() & 0x0FFFFFFFUL;
        const int length = snprintf(candidate, sizeof(candidate), "%s/S%07lX",
                                    f06VesselRoot, (unsigned long)token);
        if (length <= 0 || (size_t)length >= sizeof(candidate)) {
            Serial.println("[SD-F06] SESSION_PATH_TOO_LONG; NO JOURNAL WRITES");
            return false;
        }
        if (SD.exists(candidate)) continue;
        if (!SD.mkdir(candidate)) {
            Serial.println("[SD-F06] SESSION_DIR_CREATE_FAIL; NO JOURNAL WRITES");
            return false;
        }
        strcpy(DIRECTORY, candidate);
        isolatedDirCreated = true;
        break;
    }
    if (!isolatedDirCreated) {
        Serial.println("[SD-F06] NO_UNIQUE_SESSION; NO JOURNAL WRITES");
        return false;
    }
    snprintf(DIAGNOSTICS, sizeof(DIAGNOSTICS), "%s/diagnostics.jsonl", DIRECTORY);
    snprintf(TELEMETRY, sizeof(TELEMETRY), "%s/telemetry.jsonl", DIRECTORY);
    snprintf(SOS_EVENTS, sizeof(SOS_EVENTS), "%s/sos_events.jsonl", DIRECTORY);
    Serial.printf("[SD-F06] SANDBOX_READY path=%s legacy_journals=UNTOUCHED\n", DIRECTORY);
#endif

    if (!SD.exists(DIRECTORY) && !SD.mkdir(DIRECTORY)) {
        Serial.println("[SD-LOG] VESSEL_DIR_CREATE_FAIL");
        return false;
    }

    File dir = SD.open(DIRECTORY, FILE_READ);

    if (!dir || !dir.isDirectory()) {
        Serial.println("[SD-LOG] VESSEL_DIR_INVALID");
        if (dir) dir.close();
        return false;
    }

    dir.close();

    // Inspect the existing thesis journal before adding a new record.
    // Read at most 64 KiB to keep startup work bounded.
    uint64_t bytesBefore = 0;
    uint32_t priorLines = 0;
    bool lastLineComplete = false;

    if (SD.exists(DIAGNOSTICS)) {
        File previous = SD.open(DIAGNOSTICS, FILE_READ);

        if (!previous) {
            Serial.println("[SD-VERIFY] PRIOR_OPEN_FAIL");
            return false;
        }

        bytesBefore = previous.size();

        if (bytesBefore <= 65536ULL) {
            char current[192] = {};
            char last[192] = {};
            size_t used = 0;

            while (previous.available()) {
                int ch = previous.read();

                if (ch < 0) break;

                if (ch == '\n') {
                    current[used] = '\0';
                    strncpy(last, current, sizeof(last) - 1);
                    last[sizeof(last) - 1] = '\0';
                    used = 0;
                    priorLines++;
                    lastLineComplete = true;
                } else if (ch != '\r') {
                    if (used < sizeof(current) - 1) {
                        current[used++] = (char)ch;
                    }
                    lastLineComplete = false;
                }
            }

            Serial.printf(
                "[SD-VERIFY] PRIOR_LINES=%lu complete=%u\n",
                (unsigned long)priorLines,
                (unsigned)lastLineComplete
            );

            if (priorLines > 0) {
                Serial.printf("[SD-VERIFY] LAST_RECORD=%s\n", last);
            }
        } else {
            Serial.println("[SD-VERIFY] SCAN_SKIPPED_OVER_64K");
        }

        previous.close();
    }

    Serial.printf(
        "[SD-VERIFY] BEFORE_BYTES=%llu\n",
        (unsigned long long)bytesBefore
    );

    bootToken = esp_random();
    eventCounter = 0;
    urgentQueue = EventQueue{};
    telemetryQueue = EventQueue{};
    lostUrgent = lostTelemetry = 0;
    lastDropNotice = millis();
    ready = true;

    char record[160];

    snprintf(
        record,
        sizeof(record),
        "{\"kind\":\"boot\",\"vessel_id\":%lu,\"uptime_ms\":%lu}",
        (unsigned long)vesselId,
        (unsigned long)millis()
    );

    if (!appendRecord(DIAGNOSTICS, record)) {
        ready = false;
        Serial.println("[SD-LOG] BOOT_WRITE_FAIL");
        return false;
    }

    File verification = SD.open(DIAGNOSTICS, FILE_READ);

    if (!verification) {
        ready = false;
        Serial.println("[SD-VERIFY] READBACK_OPEN_FAIL");
        return false;
    }

    uint64_t bytesAfter = verification.size();
    verification.close();

    if (bytesAfter <= bytesBefore) {
        ready = false;
        Serial.println("[SD-VERIFY] FILE_NOT_GROWING");
        return false;
    }

    Serial.printf(
        "[SD-VERIFY] AFTER_BYTES=%llu DELTA=%llu\n",
        (unsigned long long)bytesAfter,
        (unsigned long long)(bytesAfter - bytesBefore)
    );

    lastHeartbeat = millis();

    Serial.println("[SD-LOG] BOOT_WRITE_OK");
    Serial.printf("[SD-LOG] READY path=%s\n", DIRECTORY);

    return true;
}


// USB-only readback: exact byte chunks as hexadecimal, never modifies SD contents.
// File sizes are snapshotted when requested. One chunk is emitted per loop.
static const char* exportPaths[] = { SOS_EVENTS, TELEMETRY, DIAGNOSTICS };
static const char* exportNames[] = { "sos_events.jsonl", "telemetry.jsonl", "diagnostics.jsonl" };
static bool exportActive = false;
static uint8_t exportIndex = 0;
static uint32_t exportOffset = 0;
static uint32_t exportLimit = 0;
static uint32_t exportChecksum = 2166136261UL; // FNV-1a for transfer check
static File exportFile;

inline void requestExport() {
    if (!ready || !isAuthorizedVessel(vesselId)) {
        Serial.println("[SDX] ERROR LOGGER_NOT_READY");
        return;
    }
    if (exportActive) {
        Serial.println("[SDX] ERROR BUSY");
        return;
    }
    if (urgentQueue.count || telemetryQueue.count) {
        Serial.println("[SDX] ERROR QUEUE_NOT_EMPTY_RETRY");
        return;
    }
    exportIndex = 0;
    exportOffset = 0;
    exportLimit = 0;
    exportChecksum = 2166136261UL;
    exportActive = true;
    Serial.println("[SDX] SESSION_START files=3 encoding=HEX checksum=FNV1A32");
}

inline void pollExport() {
    if (!exportActive) return;
    if (!ready) {
        if (exportFile) exportFile.close();
        exportActive = false;
        Serial.println("[SDX] ERROR LOGGER_DISABLED");
        return;
    }
    if (!exportFile) {
        if (exportIndex >= 3) {
            Serial.println("[SDX] SESSION_END");
            exportActive = false;
            return;
        }
        exportFile = SD.open(exportPaths[exportIndex], FILE_READ);
        if (!exportFile) {
            // A missing file is valid only when it does not exist.
            if (SD.exists(exportPaths[exportIndex])) {
                Serial.printf("[SDX] ERROR OPEN_FAILED index=%u\n", (unsigned)exportIndex);
                exportActive = false;
                return;
            }
            Serial.printf("[SDX] BEGIN %u %s 0\n", (unsigned)exportIndex, exportNames[exportIndex]);
            Serial.printf("[SDX] END %u 0 %08lX\n", (unsigned)exportIndex, 2166136261UL);
            ++exportIndex;
            return;
        }
        if (exportFile.size() > 1048576UL) {
            Serial.printf("[SDX] ERROR FILE_TOO_LARGE index=%u\n", (unsigned)exportIndex);
            exportFile.close();
            exportActive = false;
            return;
        }
        exportOffset = 0;
        exportLimit = (uint32_t)exportFile.size();
        exportChecksum = 2166136261UL;
        Serial.printf("[SDX] BEGIN %u %s %lu\n", (unsigned)exportIndex,
                      exportNames[exportIndex], (unsigned long)exportLimit);
        return;
    }
    if (exportOffset >= exportLimit) {
        Serial.printf("[SDX] END %u %lu %08lX\n", (unsigned)exportIndex,
                      (unsigned long)exportOffset, (unsigned long)exportChecksum);
        exportFile.close();
        ++exportIndex;
        return;
    }
    static const char hex[] = "0123456789ABCDEF";
    char encoded[129];
    const size_t wanted = (exportLimit - exportOffset > 64) ? 64 : (size_t)(exportLimit - exportOffset);
    size_t actual = 0;
    for (; actual < wanted; ++actual) {
        const int v = exportFile.read();
        if (v < 0) break;
        const uint8_t byte = (uint8_t)v;
        encoded[actual*2] = hex[byte >> 4];
        encoded[actual*2+1] = hex[byte & 15];
        exportChecksum = (exportChecksum ^ byte) * 16777619UL;
    }
    if (actual != wanted) {
        Serial.printf("[SDX] ERROR EARLY_EOF index=%u\n", (unsigned)exportIndex);
        exportFile.close();
        exportActive = false;
        return;
    }
    encoded[actual*2] = '\0';
    Serial.printf("[SDX] DATA %u %lu %s\n", (unsigned)exportIndex,
                  (unsigned long)exportOffset, encoded);
    exportOffset += (uint32_t)actual;
}

inline void poll() {
    if (!ready) return;
    pollEvents();
    if (!ready) return;

    // Read-only storage audit, once every 30 seconds.
    static uint32_t lastAuditMs = 0;
    if (uint32_t(millis() - lastAuditMs) >= 30000UL) {
        lastAuditMs = millis();

        uint64_t telemetryBytes = 0;
        uint64_t sosBytes = 0;

        File telemetryFile = SD.open(TELEMETRY, FILE_READ);
        if (telemetryFile) {
            telemetryBytes = telemetryFile.size();
            telemetryFile.close();
        }

        File sosFile = SD.open(SOS_EVENTS, FILE_READ);
        if (sosFile) {
            sosBytes = sosFile.size();
            sosFile.close();
        }

        Serial.printf(
            "[SD-AUDIT] telemetry_bytes=%llu sos_bytes=%llu queued_telemetry=%u queued_sos=%u dropped_telemetry=%lu dropped_sos=%lu events=%lu\n",
            (unsigned long long)telemetryBytes,
            (unsigned long long)sosBytes,
            (unsigned)telemetryQueue.count,
            (unsigned)urgentQueue.count,
            (unsigned long)lostTelemetry,
            (unsigned long)lostUrgent,
            (unsigned long)eventCounter
        );
    }

    uint32_t now = millis();

    if (uint32_t(now - lastHeartbeat) < HEARTBEAT_INTERVAL_MS) {
        return;
    }

    lastHeartbeat = now;

    char record[160];

    snprintf(
        record,
        sizeof(record),
        "{\"kind\":\"heartbeat\",\"vessel_id\":%lu,\"uptime_ms\":%lu}",
        (unsigned long)vesselId,
        (unsigned long)now
    );

    if (appendRecord(DIAGNOSTICS, record)) {
        Serial.println("[SD-LOG] HEARTBEAT_WRITE_OK");
    } else {
        ready = false;
        Serial.println("[SD-LOG] DISABLED_AFTER_WRITE_FAIL");
    }
}

#if THESIS_F06_SD_SANDBOX == 1
// F06 historical inspection is COM13 USB-only, allowlisted and read-only.
// No caller-supplied path or filename is ever sent to SD.open().
// Exactly two selectors are allowed: LEGACY or S + 7 uppercase hex digits.
// These routines NEVER use FILE_APPEND, FILE_WRITE, mkdir, remove or rename.
static constexpr uint32_t HISTORY_MAX_FILE_BYTES = 1048576UL;
static constexpr size_t HISTORY_READ_CHUNK = 64;
static const char* const historyNames[3] = {
    "sos_events.jsonl", "telemetry.jsonl", "diagnostics.jsonl"
};
static bool historyActive = false;
static uint8_t historyIndex = 0;
static uint32_t historyOffset = 0;
static uint32_t historySizes[3] = {};
static uint32_t historyChecksum = 2166136261UL;
static char historyRoot[72] = {};
static File historyFile;

inline bool historyReadActive() { return historyActive; }

inline bool validHistorySession(const char* id) {
    if (!id || strlen(id) != 8 || id[0] != 'S') return false;
    for (size_t i = 1; i < 8; ++i) {
        const char c = id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

inline bool historyPath(char* output, size_t capacity, uint8_t index) {
    if (index >= 3) return false;
    const int n = snprintf(output, capacity, "%s/%s", historyRoot, historyNames[index]);
    return n > 0 && static_cast<size_t>(n) < capacity;
}

inline void historyStop(const char* reason) {
    if (historyFile) historyFile.close();
    historyActive = false;
    Serial.printf("[SDH] ERROR %s\n", reason);
}

inline void listHistory() {
    if (!ready || vesselId != 2 || historyActive || exportActive) {
        Serial.println("[SDH] ERROR LOGGER_NOT_READY_OR_BUSY"); return;
    }
    char root[48];
    snprintf(root, sizeof(root), "/THESIS/F06TEST/V%03lu", (unsigned long)vesselId);
    File directory = SD.open(root, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) directory.close();
        Serial.println("[SDH] ERROR SESSION_DIRECTORY_MISSING"); return;
    }
    Serial.println("[SDH] LIST_BEGIN scope=V002 sessions_only=1");
    Serial.println("[SDH] TARGET LEGACY");
    uint8_t shown = 0;
    bool truncated = false;
    while (File entry = directory.openNextFile()) {
        const char* name = entry.name();
        const char* slash = strrchr(name, '/');
        if (slash) name = slash + 1;
        if (entry.isDirectory() && validHistorySession(name)) {
            if (shown < 48) {
                Serial.printf("[SDH] TARGET %s\n", name);
                ++shown;
            } else {
                truncated = true;
                entry.close();
                break;
            }
        }
        entry.close();
    }
    directory.close();
    Serial.printf("[SDH] LIST_END shown=%u truncated=%u\n",
                  (unsigned)shown, (unsigned)truncated);
}

inline void requestHistory(const char* selector) {
    if (!ready || vesselId != 2) {
        Serial.println("[SDH] ERROR LOGGER_NOT_READY_OR_WRONG_VESSEL"); return;
    }
    if (historyActive || exportActive) {
        Serial.println("[SDH] ERROR EXPORT_BUSY"); return;
    }
    if (strcmp(selector, "LEGACY") == 0) {
        snprintf(historyRoot, sizeof(historyRoot), "/THESIS/VESSEL_%03lu",
                 (unsigned long)vesselId);
    } else if (validHistorySession(selector)) {
        snprintf(historyRoot, sizeof(historyRoot),
                 "/THESIS/F06TEST/V%03lu/%s", (unsigned long)vesselId, selector);
    } else {
        Serial.println("[SDH] ERROR SELECTOR_REJECTED"); return;
    }
    File root = SD.open(historyRoot, FILE_READ);
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        Serial.println("[SDH] ERROR TARGET_DIRECTORY_MISSING"); return;
    }
    root.close();

    // Preflight all files before sending any data. The only SD.opens are READ.
    for (uint8_t i = 0; i < 3; ++i) {
        char path[96];
        if (!historyPath(path, sizeof(path), i)) {
            Serial.println("[SDH] ERROR PATH_TOO_LONG"); return;
        }
        File file = SD.open(path, FILE_READ);
        if (!file) {
            if (SD.exists(path)) {
                Serial.println("[SDH] ERROR FILE_OPEN_FAILED"); return;
            }
            historySizes[i] = 0; // Missing SOS file is valid: zero bytes.
        } else {
            if (file.isDirectory() || file.size() > HISTORY_MAX_FILE_BYTES) {
                file.close();
                Serial.println("[SDH] ERROR FILE_INVALID_OR_OVER_1MIB"); return;
            }
            historySizes[i] = static_cast<uint32_t>(file.size());
            file.close();
        }
    }
    historyIndex = 0;
    historyOffset = 0;
    historyChecksum = 2166136261UL;
    historyActive = true;
    Serial.printf("[SDH] SESSION_START target=%s files=3\n", selector);
}

inline void pollHistory() {
    if (!historyActive) return;
    if (!ready || vesselId != 2) {
        historyStop("LOGGER_NOT_READY"); return;
    }
    if (historyIndex == 3) {
        Serial.println("[SDH] SESSION_END");
        historyActive = false;
        return;
    }
    const uint8_t i = historyIndex;
    if (!historyFile && historyOffset == 0) {
        Serial.printf("[SDH] BEGIN %u %s %lu\n", (unsigned)i,
                      historyNames[i], (unsigned long)historySizes[i]);
        if (historySizes[i] == 0) {
            Serial.printf("[SDH] END %u 0 %08lX\n", (unsigned)i,
                          (unsigned long)2166136261UL);
            ++historyIndex;
            return;
        }
        char path[96];
        if (!historyPath(path, sizeof(path), i)) {
            historyStop("PATH_TOO_LONG"); return;
        }
        historyFile = SD.open(path, FILE_READ);
        if (!historyFile || historyFile.isDirectory() ||
            historyFile.size() != historySizes[i]) {
            historyStop("FILE_CHANGED_OR_OPEN_FAILED"); return;
        }
        historyChecksum = 2166136261UL;
    }
    if (historyOffset == historySizes[i]) {
        Serial.printf("[SDH] END %u %lu %08lX\n", (unsigned)i,
                      (unsigned long)historyOffset, (unsigned long)historyChecksum);
        historyFile.close();
        historyOffset = 0;
        ++historyIndex;
        return;
    }
    const size_t remain = historySizes[i] - historyOffset;
    const size_t wanted = remain < HISTORY_READ_CHUNK ? remain : HISTORY_READ_CHUNK;
    static const char digits[] = "0123456789ABCDEF";
    char hex[HISTORY_READ_CHUNK * 2 + 1];
    for (size_t j = 0; j < wanted; ++j) {
        const int value = historyFile.read();
        if (value < 0) { historyStop("EARLY_EOF"); return; }
        const uint8_t byte = static_cast<uint8_t>(value);
        hex[j * 2] = digits[byte >> 4];
        hex[j * 2 + 1] = digits[byte & 15];
        historyChecksum = (historyChecksum ^ byte) * 16777619UL;
    }
    hex[wanted * 2] = '\0';
    Serial.printf("[SDH] DATA %u %lu %s\n", (unsigned)i,
                  (unsigned long)historyOffset, hex);
    historyOffset += static_cast<uint32_t>(wanted);
}
#endif // THESIS_F06_SD_SANDBOX
}
