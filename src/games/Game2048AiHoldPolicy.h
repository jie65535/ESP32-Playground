#pragma once

#include <cstdint>

namespace pgos {

// Keeps the hidden autoplay gesture independent from the game/app and from
// the BLE service.  A press must begin while the policy is armed; a button
// already held when the app opens is intentionally ignored until release.
class Game2048AiHoldPolicy final {
public:
    static constexpr uint32_t HOLD_MS = 900;

    void reset(bool buttonHeld = false);
    bool sample(bool buttonHeld, uint32_t nowMs, bool eligible);
    bool active() const { return active_; }
    uint32_t activeDurationMs(uint32_t nowMs) const;

private:
    bool previousHeld_ = false;
    bool waitingForRelease_ = false;
    bool active_ = false;
    uint32_t heldSinceMs_ = 0;
};

class Game2048AiPacing final {
public:
    static constexpr uint32_t RAMP_MS = 12000;
    static constexpr uint32_t START_INTERVAL_MS = 240;
    static constexpr uint32_t MIN_INTERVAL_MS = 95;
    static constexpr uint32_t START_ANIMATION_MS = 145;
    static constexpr uint32_t MIN_ANIMATION_MS = 80;

    static uint32_t moveIntervalMs(uint32_t activeDurationMs);
    static uint32_t animationMs(uint32_t activeDurationMs);

private:
    static uint32_t easedProgress(uint32_t activeDurationMs);
};

}  // namespace pgos
