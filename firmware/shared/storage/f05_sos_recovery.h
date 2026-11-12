#pragma once
#include <stdint.h>
#include <stddef.h>

// F05 LAB PROFILE ONLY: journaled crash-state checkpoint (not authentication).
// This header deliberately uses no Arduino dependencies so it is host-testable.
namespace F05Recovery {
static constexpr uint32_t kMagic = 0x46303553UL; // F05S
static constexpr uint32_t kVersion = 1UL;
static constexpr uint32_t kCancelNone = 0UL;
static constexpr uint32_t kCancelPending = 1UL;
static constexpr uint32_t kCancelUnconfirmed = 2UL;
static constexpr uint32_t kMaxCancelAttempts = 4UL;

struct State {
    uint32_t magic;
    uint32_t version;
    uint32_t node;
    uint32_t generation;
    uint32_t activeId;
    uint32_t cancelId;
    uint32_t cancelAttempts;
    uint32_t cancelStatus;
    uint32_t check;
};
static_assert(sizeof(State) == 36, "F05 NVS snapshot layout changed");

inline uint32_t checksum(const State& s) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&s);
    uint32_t value = 2166136261UL;
    for (size_t i = 0; i < offsetof(State, check); ++i)
        value = (value ^ data[i]) * 16777619UL; // FNV-1a, error check only
    return value;
}
inline bool valid(const State& s, uint32_t node, uint32_t nextId) {
    if (s.magic != kMagic || s.version != kVersion ||
        s.node != node || node < 1 || node > 3 ||
        s.generation == 0 || s.check != checksum(s)) return false;
    if ((s.activeId && s.activeId >= nextId) ||
        (s.cancelId && s.cancelId >= nextId)) return false;
    if (s.activeId && s.activeId == s.cancelId) return false;
    if (s.cancelAttempts > kMaxCancelAttempts) return false;
    if (s.cancelStatus == kCancelNone)
        return s.cancelId == 0 && s.cancelAttempts == 0;
    if (s.cancelStatus == kCancelPending)
        return s.cancelId != 0 && s.cancelAttempts <= kMaxCancelAttempts;
    if (s.cancelStatus == kCancelUnconfirmed)
        return s.cancelId != 0 && s.cancelAttempts == kMaxCancelAttempts;
    return false;
}
inline bool newer(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) > 0; // safe across generation rollover
}
inline State make(uint32_t node, uint32_t generation, uint32_t activeId,
                  uint32_t cancelId, uint32_t attempts, uint32_t status) {
    State s{};
    s.magic = kMagic;
    s.version = kVersion;
    s.node = node;
    s.generation = generation;
    s.activeId = activeId;
    s.cancelId = cancelId;
    s.cancelAttempts = attempts;
    s.cancelStatus = status;
    s.check = checksum(s);
    return s;
}
} // namespace F05Recovery
