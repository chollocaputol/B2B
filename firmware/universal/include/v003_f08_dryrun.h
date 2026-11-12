#pragma once
#include <stdint.h>
#include <string.h>

// Vessel-003 F08 bench: serial request decoder ONLY. This file performs no
// pin configuration, GPIO writes, buzzer activation or policy changes.
namespace V003F08DryRun {
enum class Request : uint8_t {
  NotF08, Help, Off, Yellow, Red, Buzzer, All, Invalid
};
struct Channels {
  bool yellow;
  bool red;
  bool buzzer;
};

inline Request parse(const char *line) {
  if (!line || strncmp(line, "F08 TEST ", 9) != 0) return Request::NotF08;
  const char *arg = line + 9;
  if (strcmp(arg, "HELP") == 0) return Request::Help;
  if (strcmp(arg, "OFF") == 0) return Request::Off;
  if (strcmp(arg, "YELLOW") == 0) return Request::Yellow;
  if (strcmp(arg, "RED") == 0) return Request::Red;
  if (strcmp(arg, "BUZZER") == 0) return Request::Buzzer;
  if (strcmp(arg, "ALL") == 0) return Request::All;
  return Request::Invalid;
}

inline Channels requestedChannels(Request request) {
  return {
      request == Request::Yellow || request == Request::All,
      request == Request::Red || request == Request::All,
      request == Request::Buzzer || request == Request::All};
}

inline const char *requestName(Request request) {
  switch (request) {
    case Request::Off: return "OFF";
    case Request::Yellow: return "YELLOW";
    case Request::Red: return "RED";
    case Request::Buzzer: return "BUZZER";
    case Request::All: return "ALL";
    case Request::Help: return "HELP";
    case Request::Invalid: return "INVALID";
    case Request::NotF08: return "NOT_F08";
  }
  return "INVALID";
}
}  // namespace V003F08DryRun
