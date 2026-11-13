#pragma once
#include <stdint.h>
#include "v003_f10_micro_nowcast.h"
#include "weather_indicator_policy.h"

// F12: software-only routing preview for real macro weather, experimental
// F10 micro risk, and authoritative local/received SOS. NO GPIO writes.
// Live actuation requires an independently reviewed later profile.
#ifndef THESIS_V003_F12_INDICATOR_PREVIEW
#define THESIS_V003_F12_INDICATOR_PREVIEW 0
#endif
#ifndef THESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN
#define THESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN 0
#endif
#if (THESIS_V003_F12_INDICATOR_PREVIEW != 0 && THESIS_V003_F12_INDICATOR_PREVIEW != 1) || \
    (THESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN != 0 && THESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN != 1)
#error "Invalid F12 indicator preview flags"
#endif
#if THESIS_V003_F12_INDICATOR_PREVIEW == 1 && \
    (THESIS_V003_F12_INDICATOR_PREVIEW_OPT_IN != 1 || \
     THESIS_V003_F09_SHARED_SENSOR_BUS != 1 || \
     THESIS_V003_F10_MICRO_NOWCAST != 1 || \
     THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1 || WEATHER_FAST_TEST != 0)
#error "F12 preview requires opt-in, F09/F10/F11 and real weather timing"
#endif

namespace V003F12Preview {
// Sources describe decisions, NOT hardware actuation or validated forecasting.
enum class Source : uint8_t {
  UNAUTHORIZED, OFF, MICRO_EXPERIMENTAL_WATCH, MACRO_FRONT, MACRO_SQUALL, SOS
};
inline const char* name(Source source) {
  switch (source) {
    case Source::UNAUTHORIZED: return "UNAUTHORIZED";
    case Source::OFF: return "OFF";
    case Source::MICRO_EXPERIMENTAL_WATCH: return "MICRO_EXPERIMENTAL_WATCH";
    case Source::MACRO_FRONT: return "MACRO_FRONT";
    case Source::MACRO_SQUALL: return "MACRO_SQUALL";
    case Source::SOS: return "SOS";
  }
  return "UNKNOWN";
}
struct Result {
  Source source = Source::UNAUTHORIZED;
  WeatherIndicators::Mode mode = WeatherIndicators::Mode::Off;
  WeatherIndicators::Outputs virtualOutputs{};
  bool changed = false;
  bool authorized = false;
};
class Controller {
 public:
  // Caller must supply verified persistent VesselIdentity::nodeId() == 3.
  void begin(bool authorizedVessel003) {
    authorized_ = authorizedVessel003;
    observed_ = false;
    lastSource_ = Source::UNAUTHORIZED;
    policy_ = WeatherIndicators::Policy{};
  }
  Result preview(uint32_t now, uint8_t macroWeather, bool macroInputsValid,
                 V003F10Micro::State microState,
                 bool localSOS, bool receivedSOS) {
    Result r{};
    r.authorized = authorized_;
    if (!authorized_) return r;

    // Never let experimental micro logic override an authoritative SOS,
    // or a validated macro warning. One candidate does not constitute a watch.
    uint8_t selected = WEATHER_CALIBRATING;
    bool selectedValid = false;
    if (localSOS || receivedSOS) {
      r.source = Source::SOS;
    } else if (macroInputsValid && macroWeather == WEATHER_SQUALL) {
      r.source = Source::MACRO_SQUALL;
      selected = WEATHER_SQUALL;
      selectedValid = true;
    } else if (macroInputsValid && macroWeather == WEATHER_FRONT) {
      r.source = Source::MACRO_FRONT;
      selected = WEATHER_FRONT;
      selectedValid = true;
    } else if (microState == V003F10Micro::State::RISK_WATCH) {
      r.source = Source::MICRO_EXPERIMENTAL_WATCH;
      selected = WEATHER_FRONT; // VIRTUAL yellow cadence only, never red/buzzer
      selectedValid = true;
    } else {
      r.source = Source::OFF;
    }
    r.virtualOutputs = policy_.update(
        now, selected, selectedValid, localSOS, receivedSOS);
    r.mode = policy_.mode();
    r.changed = !observed_ || r.source != lastSource_;
    lastSource_ = r.source;
    observed_ = true;
    return r;
  }
  bool authorized() const { return authorized_; }
 private:
  bool authorized_ = false;
  bool observed_ = false;
  Source lastSource_ = Source::UNAUTHORIZED;
  WeatherIndicators::Policy policy_{};
};
} // namespace V003F12Preview
