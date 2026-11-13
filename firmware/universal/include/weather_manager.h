#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <math.h>

// Existing weather contract; same values as the 62-byte LoRa packet.
enum WeatherState : uint8_t {
  WEATHER_CALIBRATING = 0,
  WEATHER_NORMAL = 1,
  WEATHER_FRONT = 2,
  WEATHER_SQUALL = 3
};

#define WEATHER_HISTORY_SIZE 10

// Default pins preserve existing Vessel-001/002 and F08 bench behavior.
// Only the opt-in Vessel-003 F09 sensor profile shares BME680 I2C pins.
#include "v003_f09_shared_sensor_bus.h"
#define BME_SDA_PIN 21
#define BME_SCL_PIN 48
#if THESIS_V003_F09_SHARED_SENSOR_BUS == 1
#define DHT_SDA_PIN BME_SDA_PIN
#define DHT_SCL_PIN BME_SCL_PIN
#else
#define DHT_SDA_PIN 38
#define DHT_SCL_PIN 39
#endif

// The I2C switch remains owned by main.cpp and shared with the OLED.
void switchI2C(uint8_t sda, uint8_t scl);
void setupSensors();
void processWeather();
void processSerialTestCommands();
const char *weatherName(uint8_t state);
const char *shortWeatherName(uint8_t state);

// Current weather snapshot, read by OLED/LoRa code in main.cpp.
extern bool dhtReadingValid;
extern bool bmePressureReadingValid;
extern bool tempHumidityReadingValid;
extern bool tempHumidityFromDHT;
extern bool weatherInputsValid;
extern bool weatherSampleAttempted;
extern bool bmeAvailable;
extern bool dhtAvailable;
extern float temperatureC;
extern float humidityPercent;
extern float pressureStationHpa;
extern float pressureMSL;
extern float pressureDelta;
extern uint8_t zambrettiIndex;
extern uint8_t sensorFusionState;
extern uint8_t weatherState;

// F07: one real sensor cycle. Invalid or rejected fields are NaN in RAM and
// serialized as JSON null; never turn a failed measurement into zero.
struct WeatherMeasurement {
  uint32_t uptimeMs = 0;
  char gpsUtcIso8601[21] = {}; // YYYY-MM-DDTHH:MM:SSZ; empty if unavailable
  bool gpsUtcValid = false;
  float dhtTempC = NAN;
  float dhtRH = NAN;
  float bmeTempC = NAN;
  float bmeRH = NAN;
  float bmeRawPressureHpa = NAN;
  float stationPressureHpa = NAN;
  float selectedTempC = NAN;
  float selectedRH = NAN;
  float pressureMSLHpa = NAN;
  float pressureDeltaHpa = NAN;
  float tempDeltaC = NAN;
  float humidityDeltaPoints = NAN;
  uint8_t historyCount = 0;
  uint8_t weatherState = WEATHER_CALIBRATING;
  uint8_t zambretti = 0;
  uint8_t tempSource = 0; // 0=none, 1=DHT20, 2=BME680 fallback
  bool dhtOK = false;
  bool bmeTHOK = false;
  bool bmePressureOK = false;
  bool inputsOK = false;
};
extern WeatherMeasurement weatherMeasurement;