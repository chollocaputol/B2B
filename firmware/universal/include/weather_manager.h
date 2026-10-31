#pragma once
#include <Arduino.h>
#include <stdint.h>

// Existing weather contract; same values as the 62-byte LoRa packet.
enum WeatherState : uint8_t {
  WEATHER_CALIBRATING = 0,
  WEATHER_NORMAL = 1,
  WEATHER_FRONT = 2,
  WEATHER_SQUALL = 3
};

#define WEATHER_HISTORY_SIZE 10

// Measurement pins, unchanged from tested Board A firmware.
#define BME_SDA_PIN 21
#define BME_SCL_PIN 48
#define DHT_SDA_PIN 38
#define DHT_SCL_PIN 39

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
extern float temperatureC;
extern float humidityPercent;
extern float pressureStationHpa;
extern float pressureMSL;
extern float pressureDelta;
extern uint8_t zambrettiIndex;
extern uint8_t sensorFusionState;
extern uint8_t weatherState;