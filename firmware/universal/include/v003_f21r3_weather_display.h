#pragma once
// F21R3: PURE OLED display arbitration, NOT a new meteorological forecast.
// Existing F10 micro and F11 macro algorithms, physical alerts, LoRa, SOS,
// SD logging, and sensor polling are untouched by this header.
#include <stdint.h>

namespace V003F21WeatherDisplay {
enum class Macro : uint8_t { Waiting, Calibrating, Steady, Front, Squall, SensorError, Stale };
enum class Micro : uint8_t { Warmup, NoPattern, Candidate, RiskWatch, Invalid };
enum class Outlook : uint8_t {
  Baseline, NoPattern, Developing, RiskWatch, FrontSignal, SquallSignal, Unavailable
};
enum class Horizon : uint8_t { None, Experimental10To30Min, Observed100MinTrend };

// Raw values are taken only from preexisting, independent weather subsystems.
// The 720000ms macro freshness is the existing F19 OP convention.
// The 120000ms micro freshness is a proposed OLED-only display guard.
struct Input {
  uint32_t nowMs = 0;
  bool macroAttempted = false;
  bool macroInputsValid = false;
  uint32_t macroLastStartMs = 0;
  uint8_t macroHistoryCount = 0;
  uint8_t macroState = 0;  // WEATHER_CALIBRATING=0 NORMAL=1 FRONT=2 SQUALL=3
  bool macroPressureFinite = false;
  uint32_t microLastMs = 0;
  uint8_t microHistoryCount = 0;
  uint8_t microState = 0;  // WARMUP=0 NO_PATTERN=1 CANDIDATE=2 RISK_WATCH=3 INVALID=4
};
struct View {
  Macro current = Macro::Waiting;
  Micro micro = Micro::Invalid;
  Outlook outlook = Outlook::Unavailable;
  Horizon horizon = Horizon::None;
  bool macroDataGap = false;
};

inline Macro classifyMacro(const Input& x) {
  if (!x.macroAttempted) return Macro::Waiting;
  if (!x.macroInputsValid) return Macro::SensorError;
  if (uint32_t(x.nowMs - x.macroLastStartMs) > 720000UL) return Macro::Stale;
  if (x.macroHistoryCount < 10U || x.macroState == 0U || !x.macroPressureFinite)
    return Macro::Calibrating;
  if (x.macroState == 1U) return Macro::Steady;
  if (x.macroState == 2U) return Macro::Front;
  if (x.macroState == 3U) return Macro::Squall;
  return Macro::SensorError; // Never reinterpret an unknown state as normal.
}
inline Micro classifyMicro(const Input& x) {
  if (x.microLastMs == 0U) return Micro::Warmup;
  if (uint32_t(x.nowMs - x.microLastMs) > 120000UL) return Micro::Invalid;
  if (x.microState == 4U) return Micro::Invalid;
  if (x.microHistoryCount < 11U || x.microState == 0U) return Micro::Warmup;
  if (x.microState == 1U) return Micro::NoPattern;
  if (x.microState == 2U) return Micro::Candidate;
  if (x.microState == 3U) return Micro::RiskWatch;
  return Micro::Invalid;
}
inline View select(const Input& x) {
  View v{};
  v.current = classifyMacro(x);
  v.micro = classifyMicro(x);
  v.macroDataGap = v.current == Macro::Stale || v.current == Macro::SensorError;
  // Never allow a reassuring micro NO_PATTERN to hide macro FRONT/SQUALL.
  // A candidate/watch can be displayed simultaneously with the macro warning,
  // but remains exploratory and is never used for SOS/GPIO/radio decisions.
  if (v.micro == Micro::RiskWatch || v.micro == Micro::Candidate) {
    v.outlook = v.micro == Micro::RiskWatch ? Outlook::RiskWatch : Outlook::Developing;
    v.horizon = Horizon::Experimental10To30Min;
  } else if (v.current == Macro::Front || v.current == Macro::Squall) {
    v.outlook = v.current == Macro::Front ? Outlook::FrontSignal : Outlook::SquallSignal;
    v.horizon = Horizon::Observed100MinTrend;
  } else if (v.micro == Micro::NoPattern) {
    v.outlook = Outlook::NoPattern;
    v.horizon = Horizon::Experimental10To30Min;
  } else if (v.micro == Micro::Warmup && !v.macroDataGap) {
    v.outlook = Outlook::Baseline;
  } else {
    v.outlook = Outlook::Unavailable;
  }
  return v;
}
inline const char* currentText(Macro v) {
  switch(v) {
    case Macro::Waiting: return "WAITING";
    case Macro::Calibrating: return "CALIB.";
    case Macro::Steady: return "STEADY";
    case Macro::Front: return "FRONT";
    case Macro::Squall: return "SQUALL";
    case Macro::SensorError: return "NO DATA";
    default: return "STALE";
  }
}
inline const char* outlookText(Outlook v) {
  switch(v) {
    case Outlook::Baseline: return "BASELINE";
    case Outlook::NoPattern: return "NO PATTERN";
    case Outlook::Developing: return "DEVELOPING";
    case Outlook::RiskWatch: return "RISK WATCH";
    case Outlook::FrontSignal: return "FRONT SIGNAL";
    case Outlook::SquallSignal: return "SQUALL SIGNAL";
    default: return "UNAVAILABLE";
  }
}
inline const char* horizonText(Horizon h) {
  switch(h) {
    case Horizon::Experimental10To30Min: return "10-30m EXP";
    case Horizon::Observed100MinTrend: return "100m TREND";
    default: return "TIME --";
  }
}
}
