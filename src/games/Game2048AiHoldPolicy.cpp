#include "games/Game2048AiHoldPolicy.h"

namespace pgos {

void Game2048AiHoldPolicy::reset(bool buttonHeld) {
    previousHeld_ = buttonHeld;
    waitingForRelease_ = buttonHeld;
    active_ = false;
    heldSinceMs_ = 0;
}

bool Game2048AiHoldPolicy::sample(bool buttonHeld, uint32_t nowMs,
                                  bool eligible) {
    if (!eligible) {
        reset(buttonHeld);
        return false;
    }

    if (waitingForRelease_) {
        if (!buttonHeld) {
            reset(false);
        }
        return false;
    }

    if (!buttonHeld) {
        previousHeld_ = false;
        active_ = false;
        heldSinceMs_ = 0;
        return false;
    }

    if (!previousHeld_) {
        previousHeld_ = true;
        heldSinceMs_ = nowMs;
        active_ = false;
        return false;
    }

    if (!active_ && static_cast<uint32_t>(nowMs - heldSinceMs_) >= HOLD_MS) {
        active_ = true;
    }
    return active_;
}

}  // namespace pgos
