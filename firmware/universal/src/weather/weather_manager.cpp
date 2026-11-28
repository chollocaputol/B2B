// F23D SD begin
#include "../../../shared/relay/f23d_sd_capture.h"
// F23D SD end
// F23D physical begin
#include "../../../shared/relay/f23d_physical_contract.h"
// F23D physical end
// F23D readiness begin
#include "../../../shared/relay/f23d_readiness_contract.h"
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
extern bool f23dHandleDiagnosticCommand(const char* line);
#endif
// F23D readiness end
// F23D begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_CANDIDATE == 1
// F23D end
#include "weather_manager.h"
#include "lora_manager.h"
#include <Wire.h>
#include <math.h>
#include <string.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <DHT20.h>
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
#include "macro_weather.h"
#endif

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
// Distinguish not-yet-sampled from a failed physical sensor read on OLED.
bool weatherSampleAttempted = false;
WeatherMeasurement weatherMeasurement{};


// -------------------- Weather State -----------------------
float temperatureC = NAN;
float humidityPercent = NAN;
float pressureStationHpa = NAN; // direct BME680 reading (station pressure)
float pressureMSL = NAN;        // altitude-normalized pressure used by weather logic
float pressureDelta = NAN;
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
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
static uint32_t f11PressureHistoryTimeMs[WEATHER_HISTORY_SIZE] = {};
static uint32_t f11PreviousAcceptedMs = 0;
static bool f11MedianArmed = false, f11MedianValid = false;
static float f11MedianT = NAN, f11MedianRH = NAN, f11MedianP = NAN;
#endif

// Copy only results of the just-completed physical cycle.  Forecast history
// remains exclusively owned by the existing weather algorithm.
static void completeWeatherMeasurement() {
  weatherMeasurement.dhtOK = dhtReadingValid;
  weatherMeasurement.bmePressureOK = bmePressureReadingValid;
  weatherMeasurement.inputsOK = weatherInputsValid;
  weatherMeasurement.tempSource = tempHumidityReadingValid
      ? (tempHumidityFromDHT ? 1 : 2) : 0;
  weatherMeasurement.selectedTempC = tempHumidityReadingValid ? temperatureC : NAN;
  weatherMeasurement.selectedRH = tempHumidityReadingValid ? humidityPercent : NAN;
  weatherMeasurement.stationPressureHpa = bmePressureReadingValid ? pressureStationHpa : NAN;
  weatherMeasurement.pressureMSLHpa = weatherInputsValid ? pressureMSL : NAN;
  weatherMeasurement.pressureDeltaHpa = (weatherInputsValid && weatherState != WEATHER_CALIBRATING) ? pressureDelta : NAN;
  weatherMeasurement.tempDeltaC = weatherInputsValid ? temperatureChangeC : NAN;
  weatherMeasurement.humidityDeltaPoints = weatherInputsValid ? humidityChangePoints : NAN;
  weatherMeasurement.historyCount = historyCount;
  weatherMeasurement.weatherState = weatherState;
  weatherMeasurement.zambretti = zambrettiIndex;
}
// Sensor recovery safeguards. Values are development defaults for a near-sea-level
// installation. They are NOT storm thresholds and must be configured for deployment.
const float PRESSURE_MIN_HPA = 870.0f;
const float PRESSURE_MAX_HPA = 1085.0f;
const float MAX_SINGLE_PRESSURE_STEP_HPA = 8.0f;
float lastAcceptedStationPressure = NAN;
// A rejected plausible jump is a candidate only, never a valid history sample.
float pendingStationPressure = NAN;
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
  // DHT20: isolated F09 profile shares SDA21/SCL48 with BME680.
  // Other builds retain the original SDA38/SCL39 selection.
  // ----------------------------------------------------------
  switchI2C(DHT_SDA_PIN, DHT_SCL_PIN);
#if THESIS_V003_F09_SHARED_SENSOR_BUS == 1
  Serial.println("[F09-I2C] sensor bus SDA=21 SCL=48; BME680=0x76/0x77 DHT20=0x38");
#endif

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
#if THESIS_V003_F10_MICRO_NOWCAST == 1
bool readMicroWeatherSample(MicroWeatherSample &sample) {
  sample = MicroWeatherSample{};
  if (!dhtAvailable || !bmeAvailable) return false;
  // Both sensors already share GPIO21/48 in this opt-in F09 configuration.
  switchI2C(BME_SDA_PIN, BME_SCL_PIN);
  bool dhtOK = dht.isConnected() && dht.read() == DHT20_OK;
  const float t = dhtOK ? dht.getTemperature() : NAN;
  const float h = dhtOK ? dht.getHumidity() : NAN;
  bool bmeOK = bme.performReading();
  const float p = bmeOK ? (bme.pressure / 100.0f) : NAN;
  switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.setClock(400000);
  // No source substitution: DHT20 is T/RH, BME680 is pressure.
  if (!dhtOK || !bmeOK || !isfinite(t) || t < -40.0f || t > 85.0f ||
      !isfinite(h) || h <= 0.0f || h > 100.0f ||
      !isfinite(p) || p < PRESSURE_MIN_HPA || p > PRESSURE_MAX_HPA)
    return false;
  sample.temperatureC = t;
  sample.humidityPercent = h;
  sample.stationPressureHpa = p;
  return true;
}
#endif
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
void processWeatherFromF11Median(bool valid, float t, float rh, float pStation) {
  f11MedianArmed = true;
  f11MedianValid = valid;
  f11MedianT = valid ? t : NAN;
  f11MedianRH = valid ? rh : NAN;
  f11MedianP = valid ? pStation : NAN;
  processWeather();
}
#endif
void processWeather() {
  weatherMeasurement = WeatherMeasurement{};
  weatherMeasurement.uptimeMs = millis();
  weatherSampleAttempted = true;
  // Confirmation must come from the immediately following physical cycle.
  const float previousPendingPressure = pendingStationPressure;
  pendingStationPressure = NAN;
  float t = NAN;
  float h = NAN;
  float pStation = NAN;
  float bmeT = NAN;
  float bmeH = NAN;

  bool gotDHT = false;
  bool gotBMEPressure = false;
  bool gotBMETempHumidity = false;
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  // Only the F11 development build may inject a fresh five-reading median.
  // This bypasses I2C *only* for the completed batch, never on other profiles.
  const bool f11UseMedian = f11MedianArmed;
  f11MedianArmed = false;
  if (f11UseMedian && f11MedianValid) {
    t = f11MedianT; h = f11MedianRH;
    gotDHT = isfinite(t) && isfinite(h) && t >= -40.0f && t <= 85.0f &&
             h > 0.0f && h <= 100.0f;
    if (gotDHT) { weatherMeasurement.dhtTempC = t; weatherMeasurement.dhtRH = h; }
  }
#else
  const bool f11UseMedian = false;
#endif
  float candidateBMEPressure = NAN;
  bool bmeReadSucceeded = false;

  // ----------------------------------------------------------
  // Read DHT20 temperature and humidity independently
  // ----------------------------------------------------------
  if (!f11UseMedian && dhtAvailable) {
    switchI2C(DHT_SDA_PIN, DHT_SCL_PIN);
    // Verify the actual DHT20 responds before trusting library readback.
    if (!dht.isConnected()) {
       Serial.println("[DHT20] No I2C response; selected T/RH invalid (no fallback)");
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
        weatherMeasurement.dhtTempC = dhtT;
        weatherMeasurement.dhtRH = dhtH;
      } else {
        Serial.println("[DHT20] Invalid measurement");
      }
    } else {
      Serial.printf("[DHT20] Read failed, status=%d\n", dhtStatus);
    }
    }
  }

  // ----------------------------------------------------------
  // BME680 physical pressure and optional T/RH diagnostics. F11 median is
  // trusted only as a *fresh batch input*, never as an accepted forecast value:
  // the original 8 hPa pressure discontinuity filter remains authoritative.
  // ----------------------------------------------------------
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  if (f11UseMedian && f11MedianValid) {
    candidateBMEPressure = f11MedianP;
    bmeReadSucceeded = true;
    weatherMeasurement.bmeRawPressureHpa = candidateBMEPressure;
  }
#endif
  if (!f11UseMedian && bmeAvailable) {
    switchI2C(BME_SDA_PIN, BME_SCL_PIN);
    if (bme.performReading()) {
      bmeT = bme.temperature;
      bmeH = bme.humidity;
      candidateBMEPressure = bme.pressure / 100.0f;
      bmeReadSucceeded = true;
      weatherMeasurement.bmeRawPressureHpa = candidateBMEPressure;
      if (isfinite(bmeT) && isfinite(bmeH) &&
          bmeT >= -40.0f && bmeT <= 85.0f &&
          bmeH >= 0.0f && bmeH <= 100.0f) {
        gotBMETempHumidity = true;
        weatherMeasurement.bmeTempC = bmeT;
        weatherMeasurement.bmeRH = bmeH;
        weatherMeasurement.bmeTHOK = true;
      }
    } else {
      Serial.println("[BME680] Reading failed");
    }
  }
  if (bmeReadSucceeded) {
    const float bmeP = candidateBMEPressure;
    const bool plausible = isfinite(bmeP) && bmeP >= PRESSURE_MIN_HPA &&
                           bmeP <= PRESSURE_MAX_HPA;
    const bool jump = plausible && isfinite(lastAcceptedStationPressure) &&
                      fabsf(bmeP - lastAcceptedStationPressure) > MAX_SINGLE_PRESSURE_STEP_HPA;
    const bool recoveredShift = jump && isfinite(previousPendingPressure) &&
        fabsf(bmeP - previousPendingPressure) <= MAX_SINGLE_PRESSURE_STEP_HPA;
    if (plausible && (!jump || recoveredShift)) {
      if (recoveredShift) {
        resetWeatherCalibration("confirmed pressure discontinuity");
        Serial.println("[BME680] Plausible pressure shift confirmed; fresh calibration required");
      }
      pStation = bmeP;
      gotBMEPressure = true;
    } else {
      if (jump) pendingStationPressure = bmeP;
      Serial.printf("[BME680] PRESSURE REJECTED: %.2f hPa (%s). Not used for history/forecast.\n",
                    bmeP, jump ? "abrupt jump" : "outside configured range");
    }
  }

  // Sensor recovery is explicit and separate from the OLED reading flags.
  if (gotDHT) {
    if (dhtConsecutiveFailures) Serial.println("[DHT20] Recovered; DHT20 T/RH valid again");
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

  // Strict source policy: DHT20 is the sole source of selected T/RH.
  // Keep independently measured BME680 T/RH in weatherMeasurement for
  // diagnostics/SD, but never substitute them for a failed DHT20.

  // Store current-cycle status independently of the overall weather result.
  dhtReadingValid = gotDHT;
  bmePressureReadingValid = gotBMEPressure;
  tempHumidityReadingValid = gotDHT;
  tempHumidityFromDHT = gotDHT;
  weatherInputsValid = tempHumidityReadingValid && bmePressureReadingValid;

  // Invalidate failed sensor values immediately; never show stale values.
  temperatureC = tempHumidityReadingValid ? t : NAN;
  humidityPercent = tempHumidityReadingValid ? h : NAN;
  pressureStationHpa = bmePressureReadingValid ? pStation : NAN;
  // Sea-level normalization requires both live physical inputs.
  pressureMSL = weatherInputsValid
                  ? normalizePressure(pStation, temperatureC) : NAN;

  // Restore OLED I2C configuration before any display update.
  switchI2C(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.setClock(400000);

  // ----------------------------------------------------------
  // A missing sensor disables weather calculations only. It must not clear
  // otherwise valid readings or make the working sensor look broken.
  // ----------------------------------------------------------
  if (!weatherInputsValid) {
    pressureDelta = NAN;
    zambrettiIndex = 0;
    temperatureChangeC = NAN;
    humidityChangePoints = NAN;
    resetWeatherCalibration("missing/rejected weather input");
    sensorFusionState = WEATHER_CALIBRATING;
    weatherState = WEATHER_CALIBRATING;
    completeWeatherMeasurement();

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
  // DHT20 T/RH and BME680 pressure are both valid: run decision hierarchy.
  // Only independently measured physical values enter weather history.
  // ----------------------------------------------------------
  pressureMSL = normalizePressure(pStation, temperatureC);
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  // Do not pass off discontinuous or out-of-phase samples as a 100-min trend.
  if (historyCount && !V003F11Macro::validStep(millis(), f11PreviousAcceptedMs)) {
    resetWeatherCalibration("F11 macro interval gap");
  }
  if (historyCount == WEATHER_HISTORY_SIZE &&
      !V003F11Macro::validTendencySpan(uint32_t(millis() - f11PressureHistoryTimeMs[historyIndex]))) {
    resetWeatherCalibration("F11 history not exactly 100 minutes");
  }
#endif
  bool comparable = havePreviousWeatherSample &&
                    previousTempHumidityFromDHT == tempHumidityFromDHT;
  // Evaluate the EXACT same function used by the synthetic test harness.
  WeatherDecision decision = evaluateWeather(
    pressureMSL, pressureHistory, historyCount,
    previousTemperatureC, previousHumidityPercent,
    temperatureC, humidityPercent, comparable);
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  if (historyCount < WEATHER_HISTORY_SIZE) {
    // Never label an average-over-short-history as a 100-minute tendency.
    decision.dP = NAN;
  } else {
    // Next insertion overwrites the *oldest* of ten previous median points.
    // At the eleventh point that oldest median is 100 minutes old.
    decision.dP = pressureMSL - pressureHistory[historyIndex];
    decision.z = calculateZambretti(pressureMSL, decision.dP);
    if (decision.fusion == WEATHER_SQUALL) decision.state = WEATHER_SQUALL;
    else if (decision.dP < -1.6f) decision.state = WEATHER_FRONT;
    else decision.state = WEATHER_NORMAL;
  }
#endif
  pressureDelta = decision.dP;
  temperatureChangeC = decision.dT;
  humidityChangePoints = decision.dRH;
  zambrettiIndex = decision.z;
  sensorFusionState = decision.fusion;
  weatherState = decision.state;

#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1
  f11PressureHistoryTimeMs[historyIndex] = millis();
  f11PreviousAcceptedMs = millis();
#endif
  addPressure(pressureMSL);
  validWeatherSamples++;
  previousTemperatureC = temperatureC;
  previousHumidityPercent = humidityPercent;
  previousTempHumidityFromDHT = tempHumidityFromDHT;
  havePreviousWeatherSample = true;
  completeWeatherMeasurement();

  Serial.printf(
    "[WEATHER-A] T=%.1fC RH=%.1f%% Psta=%.2f Pmsl=%.2f "
    "dP=%+.2f Z=%u hist=%u/%u DHT=%s BME=%s State=%s\n",
    temperatureC, humidityPercent, pressureStationHpa, pressureMSL,
    pressureDelta, zambrettiIndex, historyCount, WEATHER_HISTORY_SIZE,
    gotDHT ? "OK" : "FAIL",
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
// F23D readiness begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_READINESS_ACTIVE
#if THESIS_F23D_READINESS_ACTIVE || THESIS_F23D_PHYSICAL_CANDIDATE == 1
// F23D physical end
// F23D SD begin
#if THESIS_F23D_SD_EVIDENCE
      if(thesisF23dSDCommand(testLine)){testLineLength=0;continue;}
#endif
// F23D SD end
      if(f23dHandleDiagnosticCommand(testLine)){testLineLength=0;continue;}
#endif
// F23D readiness end
// F23D begin
// F23D physical begin
// F23D physical restore: #if THESIS_F23D_CANDIDATE == 1
// F23D end
      if (strcmp(testLine, "CANCEL SOS") == 0) {
        cancelActiveSOS();
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