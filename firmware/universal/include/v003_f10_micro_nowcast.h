#pragma once
#include <stdint.h>
#include <math.h>

// F10 experimental, Vessel-003-only, observation-driven near-term risk outlook.
// Not a validated storm predictor. Does not actuate LEDs/buzzer, send radio
// messages, or update macro/Zambretti history.
#ifndef THESIS_V003_F10_MICRO_NOWCAST
#define THESIS_V003_F10_MICRO_NOWCAST 0
#endif
#ifndef THESIS_V003_F10_MICRO_NOWCAST_OPT_IN
#define THESIS_V003_F10_MICRO_NOWCAST_OPT_IN 0
#endif
#if (THESIS_V003_F10_MICRO_NOWCAST != 0 && THESIS_V003_F10_MICRO_NOWCAST != 1) || \
    (THESIS_V003_F10_MICRO_NOWCAST_OPT_IN != 0 && THESIS_V003_F10_MICRO_NOWCAST_OPT_IN != 1)
#error "Invalid F10 nowcast build flag"
#endif
#if THESIS_V003_F10_MICRO_NOWCAST == 1 && \
    (THESIS_V003_F10_MICRO_NOWCAST_OPT_IN != 1 || THESIS_V003_F09_SHARED_SENSOR_BUS != 1)
#error "F10 requires explicit opt-in and shared F09 sensor bus"
#endif

namespace V003F10Micro {
constexpr uint32_t kIntervalMs = 60000UL;
constexpr uint32_t kMinStepMs = 45000UL;
constexpr uint32_t kMaxStepMs = 90000UL;
constexpr uint32_t kMinWindowMs = 540000UL;
constexpr uint32_t kMaxWindowMs = 660000UL;
constexpr uint8_t kWindowCount = 11; // 11 one-minute points span ten minutes

// If the ten-minute macro reader has just sampled the same sensors, reuse
// its fresh, validated measurement. This avoids immediately repeating the
// DHT20/BME680 I2C transaction at the shared sampling boundary.
constexpr uint32_t kMacroReuseMaxAgeMs = 5000UL;
inline bool shouldReuseMacroSample(uint32_t now, bool attempted, uint32_t macroMs) {
  return attempted && uint32_t(now - macroMs) <= kMacroReuseMaxAgeMs;
}

// Initial engineering hypotheses, NOT validated meteorological thresholds.
constexpr float kMinPressureChangeHpa = 1.2f;
constexpr float kMaxTemperatureChangeC = -1.0f;
constexpr float kMinHumidityChangePoints = 6.0f;
constexpr float kMaxSingleMinutePressureStepHpa = 4.0f;

enum class State : uint8_t { WARMUP, NO_PATTERN, CANDIDATE, RISK_WATCH, INVALID };
enum class Pattern : uint8_t { NONE, PRESSURE_RISE, PRESSURE_FALL };
inline const char* stateName(State s) {
  switch (s) {
    case State::WARMUP: return "WARMUP";
    case State::NO_PATTERN: return "NO_PATTERN";
    case State::CANDIDATE: return "CANDIDATE";
    case State::RISK_WATCH: return "EXPERIMENTAL_RISK_WATCH";
    default: return "INVALID";
  }
}
inline const char* patternName(Pattern p) {
  switch (p) {
    case Pattern::PRESSURE_RISE: return "RISE_COOL_MOIST";
    case Pattern::PRESSURE_FALL: return "FALL_COOL_MOIST";
    default: return "NONE";
  }
}
inline bool plausible(float t, float rh, float p) {
  return isfinite(t) && isfinite(rh) && isfinite(p) &&
      t >= -40.0f && t <= 85.0f && rh > 0.0f && rh <= 100.0f &&
      p >= 870.0f && p <= 1085.0f;
}
inline float dewPointDepression(float t, float rh) {
  if (!isfinite(t) || !isfinite(rh) || rh <= 0.0f || rh > 100.0f)
    return NAN;
  // Magnus approximation: contextual humidity signal, NOT a rain predictor.
  const float alpha = logf(rh / 100.0f) + 17.625f * t / (243.04f + t);
  const float td = 243.04f * alpha / (17.625f - alpha);
  return t - td;
}

class Forecaster {
 public:
  struct Reading { uint32_t ms; float t, rh, p; };

  void reset() {
    count_ = 0; next_ = 0; candidateCount_ = 0;
    state_ = State::WARMUP; pattern_ = Pattern::NONE;
    dT_ = dRH_ = dP_ = depression_ = NAN;
  }
  void invalidate() {
    reset();
    state_ = State::INVALID;
  }

  // Returns false if the sample is invalid/rejected or arrives too early.
  bool ingest(uint32_t now, float t, float rh, float stationP) {
    if (!plausible(t, rh, stationP)) {
      invalidate();
      return false;
    }
    if (count_ > 0) {
      const Reading& previous = readings_[(next_ + kWindowCount - 1U) % kWindowCount];
      const uint32_t elapsed = now - previous.ms; // unsigned millis rollover safe
      if (elapsed < kMinStepMs) return false;
      if (elapsed > kMaxStepMs) reset();
      else if (fabsf(stationP - previous.p) > kMaxSingleMinutePressureStepHpa) {
        invalidate(); // reject a single suspect pressure discontinuity
        return false;
      }
    }

    readings_[next_] = { now, t, rh, stationP };
    next_ = uint8_t((next_ + 1U) % kWindowCount);
    if (count_ < kWindowCount) ++count_;
    depression_ = dewPointDepression(t, rh);
    if (count_ < kWindowCount) {
      state_ = State::WARMUP;
      return true;
    }

    const Reading& oldest = readings_[next_];
    const uint32_t span = now - oldest.ms;
    if (span < kMinWindowMs || span > kMaxWindowMs) {
      reset(); // never compare values from an incomplete/misaligned window
      readings_[0] = { now, t, rh, stationP };
      next_ = 1; count_ = 1;
      depression_ = dewPointDepression(t, rh);
      return true;
    }
    dT_ = t - oldest.t;
    dRH_ = rh - oldest.rh;
    dP_ = stationP - oldest.p;
    pattern_ = Pattern::NONE;
    if (dT_ <= kMaxTemperatureChangeC && dRH_ >= kMinHumidityChangePoints) {
      if (dP_ >= kMinPressureChangeHpa) pattern_ = Pattern::PRESSURE_RISE;
      else if (dP_ <= -kMinPressureChangeHpa) pattern_ = Pattern::PRESSURE_FALL;
    }
    if (pattern_ == Pattern::NONE) {
      candidateCount_ = 0;
      state_ = State::NO_PATTERN;
    } else {
      if (candidateCount_ < 2) ++candidateCount_;
      state_ = candidateCount_ >= 2 ? State::RISK_WATCH : State::CANDIDATE;
    }
    return true;
  }

  State state() const { return state_; }
  Pattern pattern() const { return pattern_; }
  uint8_t count() const { return count_; }
  float deltaTemperature() const { return dT_; }
  float deltaHumidity() const { return dRH_; }
  float deltaPressure() const { return dP_; }
  float dewPointGap() const { return depression_; }

 private:
  Reading readings_[kWindowCount] = {};
  uint8_t count_ = 0, next_ = 0, candidateCount_ = 0;
  State state_ = State::WARMUP;
  Pattern pattern_ = Pattern::NONE;
  float dT_ = NAN, dRH_ = NAN, dP_ = NAN, depression_ = NAN;
};
} // namespace V003F10Micro
