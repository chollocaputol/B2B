#include "weather_manager.h"
#include "lora_manager.h" // Phase 3 bench-only SOS command
#include <Wire.h>
#include <math.h>
#include <string.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <DHT20.h>

// Shared OLED bus restored after each weather reading cycle.
#define OLED_SDA_PIN 17
#define OLED_SCL_PIN 18

// Development parameter preserved from the tested source.
const float NODE_HEIGHT_M = 0.0f;

Adafruit_BME680 bme;
DHT20 dht;
bool bmeAvailable = false;
bool dhtAvailable = false;

// Current-cycle measurement validity. A failed sensor must not hide readings
// from the other sensor or make stale values look current.
bool dhtReadingValid = false;
bool bmePressureReadingValid = false;
bool tempHumidityReadingValid = false;
bool tempHumidityFromDHT = false;
bool weatherInputsValid = false;


// -------------------- Weather State -----------------------
float temperatureC = 0.0f;
float humidityPercent = 0.0f;
float pressureStationHpa = 0.0f; // direct BME680 reading (station pressure)
float pressureMSL = 0.0f;        // altitude-normalized pressure used by weather logic
float pressureDelta = 0.0f;
uint8_t zambrettiIndex = 0;
uint8_t sensorFusionState = WEATHER_CALIBRATING;
uint8_t weatherState = WEATHER_CALIBRATING;
float pressureHistory[WEATHER_HISTORY_SIZE] = {};
uint8_t historyCount = 0;
uint8_t historyIndex = 0;
float previousTemperatureC = NAN;
float previousHumidityPercent = NAN;
bool havePreviousWeatherSample = false;
bool previousTempHumidityFromDHT = false;
float temperatureChangeC = NAN;
float humidityChangePoints = NAN;
uint32_t validWeatherSamples = 0;
// Sensor recovery safeguards. Values are development defaults for a near-sea-level
// installation. They are NOT storm thresholds and must be configured for deployment.
const float PRESSURE_MIN_HPA = 870.0f;
const float PRESSURE_MAX_HPA = 1085.0f;
const float MAX_SINGLE_PRESSURE_STEP_HPA = 8.0f;
float lastAcceptedStationPressure = NAN;
uint8_t bmeConsecutiveFailures = 0;
uint8_t dhtConsecutiveFailures = 0;
// Recovered sensors require a fresh full history; never reuse stale calibration.
// Simulated test values never enter production history or transmitted telemetry.
struct WeatherDecision {
  uint8_t state;
  uint8_t fusion;
  uint8_t z;
  float dP;
  float dT;
  float dRH;
};
char testLine[48];
uint8_t testLineLength = 0;


// Local helper declarations (formerly declared in main.cpp).
void resetWeatherCalibration(const char *reason);
void addPressure(float p);
float pressureAverage();
float normalizePressure(float p, float t);
uint8_t calculateZambretti(float p, float trend);
uint8_t calculateFusion(float deltaT, float deltaH);
WeatherDecision evaluateWeather(float pressure, const float *history, uint8_t count,
                                float previousT, float previousRH,
                                float currentT, float currentRH, bool comparable);
void runWeatherAlgorithmTests(const char *requested);

void setupSensors() {
  Serial.println("[SENSOR] Starting sensor diagnostics");

  // ----------------------------------------------------------
  // BME680: SDA 21, SCL 48
  // ----------------------------------------------------------
  switchI2C(BME_SDA_PIN, BME_SCL_PIN);

  Serial.println("[I2C-BME] Scanning addresses...");

  bool bmeAddress76 = false;
  bool bmeAddress77 = false;

  Wire.beginTransmission(0x76);
  bmeAddress76 = (Wire.endTransmission() == 0);

  Wire.beginTransmission(0x77);
  bmeAddress77 = (Wire.endTransmission() == 0);

  Serial.printf("[I2C-BME] 0x76: %s\n",
                bmeAddress76 ? "FOUND" : "not found");
  Serial.printf("[I2C-BME] 0x77: %s\n",
                bmeAddress77 ? "FOUND" : "not found");

  if (bme.begin(0x76)) {
    bmeAvailable = true;
  } else if (bme.begin(0x77)) {
    bmeAvailable = true;
  }

  if (bmeAvailable) {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);

    Serial.println("[SENSOR] BME680 initialized");
  } else {
    Serial.println("[SENSOR] BME680 initialization FAILED");
  }

  // ----------------------------------------------------------
  // DHT20: SDA 38, SCL 39
  // ----------------------------------------------------------
  switchI2C(DHT_SDA_PIN, DHT_SCL_PIN);

  Serial.println("[I2C-DHT] Scanning address 0x38...");

  Wire.beginTransmission(0x38);
  bool dhtAddressFound = (Wire.endTransmission() == 0);

  Serial.printf("[I2C-DHT] 0x38: %s\n",
                dhtAddressFound ? "FOUND" : "not found");

  dht.begin();

  if (dhtAddressFound && dht.isConnected()) {
    dhtAvailable = true;
    Serial.println("[SENSOR] DHT20 initialized");
  } else {
    dhtAvailable = false;
    Serial.println("[SENSOR] DHT20 initialization FAILED");
  }

  // Restore OLED bus
  switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.setClock(400000);

  Serial.printf("[SENSOR] BME680 available: %s\n",
                bmeAvailable ? "YES" : "NO");
  Serial.printf("[SENSOR] DHT20 available: %s\n",
                dhtAvailable ? "YES" : "NO");
}

// ============================================================
// HARDWARE WEATHER READINGS
// ============================================================
void processWeather() {
  float t = NAN;
  float h = NAN;
  float pStation = NAN;
  float bmeT = NAN;
  float bmeH = NAN;

  bool gotDHT = false;
  bool gotBMEPressure = false;
  bool gotBMETempHumidity = false;

  // ----------------------------------------------------------
  // Read DHT20 temperature and humidity independently
  // ----------------------------------------------------------
  if (dhtAvailable) {
    switchI2C(DHT_SDA_PIN, DHT_SCL_PIN);
    // Verify the actual DHT20 responds before trusting library readback.
    if (!dht.isConnected()) {
      Serial.println("[DHT20] No I2C response; backup BME680 T/RH if available");
    } else {
    int dhtStatus = dht.read();

    if (dhtStatus == DHT20_OK) {
      float dhtT = dht.getTemperature();
      float dhtH = dht.getHumidity();

      if (isfinite(dhtT) && isfinite(dhtH) &&
          dhtT >= -40.0f && dhtT <= 85.0f &&
          dhtH >= 0.0f && dhtH <= 100.0f) {
        t = dhtT;
        h = dhtH;
        gotDHT = true;
      } else {
        Serial.println("[DHT20] Invalid measurement");
      }
    } else {
      Serial.printf("[DHT20] Read failed, status=%d\n", dhtStatus);
    }
    }
  }

  // ----------------------------------------------------------
  // Read BME680 pressure and keep its T/RH available as backup
  // ----------------------------------------------------------
  if (bmeAvailable) {
    switchI2C(BME_SDA_PIN, BME_SCL_PIN);

    if (bme.performReading()) {
      bmeT = bme.temperature;
      bmeH = bme.humidity;
      float bmeP = bme.pressure / 100.0f;

      bool plausible = isfinite(bmeP) && bmeP >= PRESSURE_MIN_HPA &&
                       bmeP <= PRESSURE_MAX_HPA;
      bool jump = plausible && isfinite(lastAcceptedStationPressure) &&
                  fabsf(bmeP - lastAcceptedStationPressure) > MAX_SINGLE_PRESSURE_STEP_HPA;
      if (plausible && !jump) {
        pStation = bmeP;
        gotBMEPressure = true;
      } else {
        Serial.printf("[BME680] PRESSURE REJECTED: %.2f hPa (%s). Not used for history/forecast.\n",
                      bmeP, jump ? "abrupt jump" : "outside configured range");
      }

      if (isfinite(bmeT) && isfinite(bmeH) &&
          bmeT >= -40.0f && bmeT <= 85.0f &&
          bmeH >= 0.0f && bmeH <= 100.0f) {
        gotBMETempHumidity = true;
      }
    } else {
      Serial.println("[BME680] Reading failed");
    }
  }

  // Sensor recovery is explicit and separate from the OLED reading flags.
  if (gotDHT) {
    if (dhtConsecutiveFailures) Serial.println("[DHT20] Recovered; T/RH source may switch");
    dhtConsecutiveFailures = 0;
  } else if (dhtConsecutiveFailures < 255) {
    ++dhtConsecutiveFailures;
  }
  if (gotBMEPressure) {
    if (bmeConsecutiveFailures) Serial.println("[BME680] Recovered; fresh calibration required");
    bmeConsecutiveFailures = 0;
    lastAcceptedStationPressure = pStation;
  } else if (bmeConsecutiveFailures < 255) {
    ++bmeConsecutiveFailures;
  }

  // Prefer DHT20 for T/RH; fall back to BME680 if DHT20 failed.
  if (!gotDHT && gotBMETempHumidity) {
    t = bmeT;
    h = bmeH;
  }

  // Store current-cycle status independently of the overall weather result.
  dhtReadingValid = gotDHT;
  bmePressureReadingValid = gotBMEPressure;
  tempHumidityReadingValid = gotDHT || gotBMETempHumidity;
  tempHumidityFromDHT = gotDHT;
  weatherInputsValid = tempHumidityReadingValid && bmePressureReadingValid;

  // Publish each valid measurement even if the other sensor failed.
  if (tempHumidityReadingValid) {
    temperatureC = t;
    humidityPercent = h;
  }
  if (bmePressureReadingValid) {
    pressureStationHpa = pStation;
    // If T/RH is unavailable, keep the raw station pressure for display.
    // Forecast calculations remain disabled until all inputs are valid.
    pressureMSL = tempHumidityReadingValid
                    ? normalizePressure(pStation, temperatureC)
                    : pStation;
  }

  // Restore OLED I2C configuration before any display update.
  switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.setClock(400000);

  // ----------------------------------------------------------
  // A missing sensor disables weather calculations only. It must not clear
  // otherwise valid readings or make the working sensor look broken.
  // ----------------------------------------------------------
  if (!weatherInputsValid) {
    pressureDelta = 0.0f;
    zambrettiIndex = 0;
    temperatureChangeC = NAN;
    humidityChangePoints = NAN;
    resetWeatherCalibration("missing/rejected weather input");
    sensorFusionState = WEATHER_CALIBRATING;
    weatherState = WEATHER_CALIBRATING;

    Serial.print("[WEATHER-A] T=");
    if (tempHumidityReadingValid) {
      Serial.printf("%.1fC RH=%.1f%%", temperatureC, humidityPercent);
    } else {
      Serial.print("-- RH=--");
    }
    Serial.print(" P=");
    if (bmePressureReadingValid) {
      Serial.printf("%.1fhPa", pressureStationHpa);
    } else {
      Serial.print("-- (BME680 FAIL)");
    }
    Serial.printf(" DHT20=%s BME680=%s State=%s; weather calculations skipped\n",
                  dhtReadingValid ? "OK" : "FAIL",
                  bmePressureReadingValid ? "OK" : "FAIL",
                  weatherName(weatherState));
    return;
  }

  // ----------------------------------------------------------
  // Both T/RH and pressure are valid: run manuscript decision hierarchy.
  // Compare only consecutive valid samples from the SAME T/RH source;
  // switching DHT20 to BME680 can cause an artificial temperature jump.
  // ----------------------------------------------------------
  pressureMSL = normalizePressure(pStation, temperatureC);
  bool comparable = havePreviousWeatherSample &&
                    previousTempHumidityFromDHT == tempHumidityFromDHT;
  // Evaluate the EXACT same function used by the synthetic test harness.
  WeatherDecision decision = evaluateWeather(
    pressureMSL, pressureHistory, historyCount,
    previousTemperatureC, previousHumidityPercent,
    temperatureC, humidityPercent, comparable);
  pressureDelta = decision.dP;
  temperatureChangeC = decision.dT;
  humidityChangePoints = decision.dRH;
  zambrettiIndex = decision.z;
  sensorFusionState = decision.fusion;
  weatherState = decision.state;

  addPressure(pressureMSL);
  validWeatherSamples++;
  previousTemperatureC = temperatureC;
  previousHumidityPercent = humidityPercent;
  previousTempHumidityFromDHT = tempHumidityFromDHT;
  havePreviousWeatherSample = true;

  Serial.printf(
    "[WEATHER-A] T=%.1fC RH=%.1f%% Psta=%.2f Pmsl=%.2f "
    "dP=%+.2f Z=%u hist=%u/%u DHT=%s BME=%s State=%s\n",
    temperatureC, humidityPercent, pressureStationHpa, pressureMSL,
    pressureDelta, zambrettiIndex, historyCount, WEATHER_HISTORY_SIZE,
    gotDHT ? "OK" : "FAIL (BME T/RH fallback)",
    gotBMEPressure ? "OK" : "FAIL", weatherName(weatherState)
  );
  if (comparable) {
    Serial.printf("[FUSION-A] dT=%+.2fC dRH=%+.2f percentage points; squall=%s\n",
                  temperatureChangeC, humidityChangePoints,
                  weatherState == WEATHER_SQUALL ? "YES" : "NO");
  } else {
    Serial.println("[FUSION-A] Baseline unavailable or source changed; awaiting next comparable sample");
  }
}

void resetWeatherCalibration(const char *reason) {
  if (historyCount || havePreviousWeatherSample)
    Serial.printf("[WEATHER] Calibration reset: %s\n", reason);
  historyCount = 0;
  historyIndex = 0;
  havePreviousWeatherSample = false;
  previousTemperatureC = NAN;
  previousHumidityPercent = NAN;
}

WeatherDecision evaluateWeather(float pressure, const float *history, uint8_t count,
                                float previousT, float previousRH,
                                float currentT, float currentRH, bool comparable) {
  WeatherDecision out = {WEATHER_CALIBRATING, WEATHER_CALIBRATING, 0,
                         0.0f, NAN, NAN};
  float sum = 0.0f;
  for (uint8_t i = 0; i < count; ++i) sum += history[i];
  if (count) out.dP = pressure - sum / count;
  if (comparable) {
    out.dT = currentT - previousT;
    out.dRH = currentRH - previousRH;
  }
  if (count < WEATHER_HISTORY_SIZE) return out;
  out.fusion = comparable ? calculateFusion(out.dT, out.dRH) : WEATHER_NORMAL;
  out.z = calculateZambretti(pressure, out.dP);
  if (out.fusion == WEATHER_SQUALL) out.state = WEATHER_SQUALL;
  else if (out.dP < -1.6f) out.state = WEATHER_FRONT;
  else out.state = WEATHER_NORMAL;
  return out;
}

struct WeatherTestCase {
  const char *name;
  float prevT, prevRH, curT, curRH, curP, historicalP;
  uint8_t historyLength;
  uint8_t expected;
  int expectedZ; // -1 means not asserted
};

void runWeatherAlgorithmTests(const char *requested) {
  // Development-only synthetic samples; no writes to real telemetry state.
  const WeatherTestCase cases[] = {
    {"NORMAL", 32.1f, 70.2f, 32.0f, 70.5f, 1010.3f, 1010.4f, 10, WEATHER_NORMAL, 13},
    {"FRONT", 32.1f, 70.2f, 31.8f, 71.0f, 1008.0f, 1010.3f, 10, WEATHER_FRONT, 6},
    {"SQUALL", 32.1f, 70.2f, 29.6f, 76.2f, 1010.0f, 1010.3f, 10, WEATHER_SQUALL, 13},
    {"BOTH", 32.1f, 70.2f, 29.6f, 76.2f, 1008.0f, 1010.3f, 10, WEATHER_SQUALL, 6},
    {"CALIB", 32.1f, 70.2f, 29.6f, 76.2f, 1008.0f, 1010.3f, 9, WEATHER_CALIBRATING, 0},
    {"Z", 32.1f, 70.2f, 32.0f, 70.5f, 1010.3f, 1010.4f, 10, WEATHER_NORMAL, 13}
  };
  int total = 0, passed = 0;
  Serial.println("\n========== SYNTHETIC WEATHER ALGORITHM TEST ==========");
  for (const auto &c : cases) {
    if (strcmp(requested, "ALL") != 0 && strcmp(requested, c.name) != 0) continue;
    float syntheticHistory[WEATHER_HISTORY_SIZE] = {};
    for (uint8_t i = 0; i < c.historyLength; ++i) syntheticHistory[i] = c.historicalP;
    WeatherDecision d = evaluateWeather(c.curP, syntheticHistory, c.historyLength,
                                        c.prevT, c.prevRH, c.curT, c.curRH, true);
    bool ok = (d.state == c.expected) && (c.expectedZ < 0 || d.z == c.expectedZ);
    ++total;
    if (ok) ++passed;
    Serial.printf("[TEST %s] T %.1f->%.1f dT=%+.2f | RH %.1f->%.1f dRH=%+.2f | P=%.2f dP=%+.2f hist=%u/10 Z=%u\n",
                  c.name, c.prevT, c.curT, d.dT, c.prevRH, c.curRH, d.dRH,
                  c.curP, d.dP, c.historyLength, d.z);
    Serial.printf("[TEST %s] expected=%s actual=%s Z_expected=%d result=%s\n",
                  c.name, weatherName(c.expected), weatherName(d.state), c.expectedZ,
                  ok ? "PASS" : "FAIL");
  }
  if (total) Serial.printf("[TEST] TOTAL: %d/%d PASS (synthetic only; REAL sensors/LoRa unchanged)\n", passed, total);
  else Serial.println("[TEST] Unknown test. Type TEST HELP");
  Serial.println("======================================================\n");
}

void processSerialTestCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (!testLineLength) continue;
      testLine[testLineLength] = '\0';
      for (uint8_t i = 0; i < testLineLength; ++i)
        if (testLine[i] >= 'a' && testLine[i] <= 'z') testLine[i] -= ('a' - 'A');
      if (strcmp(testLine, "SOS") == 0) {
        transmitSOS();
      } else if (strcmp(testLine, "TEST HELP") == 0) {
        Serial.println("[TEST] Commands: TEST ALL | NORMAL | FRONT | SQUALL | BOTH | CALIB | Z");
        Serial.println("[TEST] Synthetic tests do not overwrite live weather or LoRa payloads");
      } else if (strncmp(testLine, "TEST ", 5) == 0) {
        runWeatherAlgorithmTests(testLine + 5);
      } else {
        Serial.println("[TEST] Unknown command; type TEST HELP");
      }
      testLineLength = 0;
    } else if (testLineLength < sizeof(testLine) - 1) {
      testLine[testLineLength++] = c;
    } else {
      testLineLength = 0;
      Serial.println("[TEST] Command too long; discarded");
    }
  }
}

float normalizePressure(float p, float t) {
  float term = 1.0f - ((0.0065f * NODE_HEIGHT_M) / (t + 273.15f));
  return p * pow(term, -5.257f);
}

void addPressure(float p) {
  pressureHistory[historyIndex] = p;
  historyIndex = (historyIndex + 1) % WEATHER_HISTORY_SIZE;
  if (historyCount < WEATHER_HISTORY_SIZE) historyCount++;
}

float pressureAverage() {
  if (historyCount == 0) return 0.0f;
  float sum = 0.0f;
  for (uint8_t i = 0; i < historyCount; i++) sum += pressureHistory[i];
  return sum / historyCount;
}

uint8_t calculateFusion(float deltaT, float deltaH) {
  // Manuscript thresholds compare consecutive sample changes, not absolute T/RH.
  return (deltaT <= -2.0f && deltaH >= 5.0f)
           ? WEATHER_SQUALL : WEATHER_NORMAL;
}

uint8_t calculateZambretti(float p, float trend) {
  float z;
  if (trend < -0.5f) z = 127.0f - (0.12f * p);
  else if (trend > 0.5f) z = 185.0f - (0.16f * p);
  else z = 144.0f - (0.13f * p);
  if (z < 0) z = 0;
  if (z > 255) z = 255;
  uint8_t result = static_cast<uint8_t>(round(z));
  if (p > 900.0f && p < 1100.0f && (result < 1 || result > 32)) {
    Serial.printf("[Z-CHECK] Unexpected Z=%u Pmsl=%.2f trend=%+.2f\n", result, p, trend);
  }
  return result;
}

const char *weatherName(uint8_t state) {
  switch (state) {
    case WEATHER_CALIBRATING: return "CALIBRATING";
    case WEATHER_NORMAL: return "NORMAL";
    case WEATHER_FRONT: return "APPROACHING FRONT";
    case WEATHER_SQUALL: return "IMMEDIATE SQUALL";
    default: return "UNKNOWN";
  }
}

const char *shortWeatherName(uint8_t state) {
  switch (state) {
    case WEATHER_CALIBRATING: return "CALIB";
    case WEATHER_NORMAL: return "NORMAL";
    case WEATHER_FRONT: return "FRONT";
    case WEATHER_SQUALL: return "SQUALL";
    default: return "UNKNOWN";
  }
}

// ============================================================