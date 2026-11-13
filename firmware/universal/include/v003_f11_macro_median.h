#pragma once
#include <stdint.h>
#include <math.h>

// F11 macro sampling is an opt-in Vessel-003 development profile only.
// Five valid *physical* T/RH/P readings are collected without delay() and
// reduced to one median every ten minutes. This is not a deployment forecast.
#ifndef THESIS_V003_F11_MACRO_FIVE_MEDIAN
#define THESIS_V003_F11_MACRO_FIVE_MEDIAN 0
#endif
#ifndef THESIS_V003_F11_MACRO_FIVE_MEDIAN_OPT_IN
#define THESIS_V003_F11_MACRO_FIVE_MEDIAN_OPT_IN 0
#endif
#if (THESIS_V003_F11_MACRO_FIVE_MEDIAN != 0 && THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1) || \
    (THESIS_V003_F11_MACRO_FIVE_MEDIAN_OPT_IN != 0 && THESIS_V003_F11_MACRO_FIVE_MEDIAN_OPT_IN != 1)
#error "Invalid F11 macro build flags"
#endif
#if THESIS_V003_F11_MACRO_FIVE_MEDIAN == 1 && \
    (THESIS_V003_F11_MACRO_FIVE_MEDIAN_OPT_IN != 1 || \
     THESIS_V003_F09_SHARED_SENSOR_BUS != 1 || \
     THESIS_V003_F10_MICRO_NOWCAST != 1 || \
     THESIS_V003_F10_MICRO_NOWCAST_OPT_IN != 1 || WEATHER_FAST_TEST != 0)
#error "F11 requires explicit opt-in, F09+F10, and real 10-minute macro timing"
#endif

namespace V003F11Macro {
constexpr uint8_t kReadings = 5;
constexpr uint32_t kReadSpacingMs = 2000UL;
constexpr uint32_t kMaxReadGapMs = 5000UL;
constexpr uint32_t kMinMacroStepMs = 540000UL; // 9 minutes
constexpr uint32_t kMaxMacroStepMs = 660000UL; // 11 minutes
constexpr uint32_t kMinTendencySpanMs = 5940000UL; // 99 minutes
constexpr uint32_t kMaxTendencySpanMs = 6060000UL; // 101 minutes

struct Sample { float temperatureC = NAN, humidityPercent = NAN, stationPressureHpa = NAN; };
inline bool plausible(const Sample& x) {
  return isfinite(x.temperatureC) && x.temperatureC >= -40.0f && x.temperatureC <= 85.0f &&
         isfinite(x.humidityPercent) && x.humidityPercent > 0.0f && x.humidityPercent <= 100.0f &&
         isfinite(x.stationPressureHpa) && x.stationPressureHpa >= 870.0f && x.stationPressureHpa <= 1085.0f;
}
inline float median5(const float (&v)[kReadings]) {
  float a[kReadings];
  for (uint8_t i = 0; i < kReadings; ++i) a[i] = v[i];
  for (uint8_t i = 1; i < kReadings; ++i) {
    float x = a[i]; uint8_t j = i;
    while (j && a[j - 1] > x) { a[j] = a[j - 1]; --j; }
    a[j] = x;
  }
  return a[2];
}
inline bool validStep(uint32_t newMs, uint32_t previousMs) {
  uint32_t span = uint32_t(newMs - previousMs);
  return span >= kMinMacroStepMs && span <= kMaxMacroStepMs;
}
inline bool validTendencySpan(uint32_t spanMs) {
  return spanMs >= kMinTendencySpanMs && spanMs <= kMaxTendencySpanMs;
}

class Batch {
 public:
  bool start(uint32_t now) {
    if (active_) return false;
    active_ = true; complete_ = false; bad_ = false; count_ = 0;
    startedMs_ = now; lastMs_ = now;
    result_ = Sample{};
    return true;
  }
  bool active() const { return active_; }
  bool complete() const { return complete_; }
  uint8_t count() const { return count_; }
  bool valid() const { return complete_ && !bad_; }
  uint32_t startedMs() const { return startedMs_; }
  Sample result() const { return valid() ? result_ : Sample{}; }
  bool due(uint32_t now) const {
    return active_ && (count_ == 0 || uint32_t(now - lastMs_) >= kReadSpacingMs);
  }
  // True only upon the fifth physical attempt. All five must be valid; no
  // substitution with cached or earlier-cycle sensor readings is permitted.
  bool push(uint32_t now, bool sensorOK, const Sample& sample) {
    if (!due(now)) return false;
    if (count_ && uint32_t(now - lastMs_) > kMaxReadGapMs) bad_ = true;
    if (!sensorOK || !plausible(sample)) bad_ = true;
    if (sensorOK && plausible(sample)) {
      t_[count_] = sample.temperatureC;
      rh_[count_] = sample.humidityPercent;
      p_[count_] = sample.stationPressureHpa;
    }
    lastMs_ = now;
    if (++count_ < kReadings) return false;
    active_ = false; complete_ = true;
    if (!bad_) {
      result_.temperatureC = median5(t_);
      result_.humidityPercent = median5(rh_);
      result_.stationPressureHpa = median5(p_);
    }
    return true;
  }
 private:
  float t_[kReadings] = {}, rh_[kReadings] = {}, p_[kReadings] = {};
  uint32_t startedMs_ = 0, lastMs_ = 0;
  uint8_t count_ = 0;
  bool active_ = false, complete_ = false, bad_ = false;
  Sample result_{};
};
} // namespace V003F11Macro
