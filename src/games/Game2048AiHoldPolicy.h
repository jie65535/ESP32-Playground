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

private:
    bool previousHeld_ = false;
    bool waitingForRelease_ = false;
    bool active_ = false;
    uint32_t heldSinceMs_ = 0;
};

}  // namespace pgos
