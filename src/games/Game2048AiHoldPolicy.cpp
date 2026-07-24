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

uint32_t Game2048AiHoldPolicy::activeDurationMs(uint32_t nowMs) const {
    if (!active_) {
        return 0;
    }
    return static_cast<uint32_t>(nowMs - heldSinceMs_) - HOLD_MS;
}

uint32_t Game2048AiPacing::moveIntervalMs(uint32_t activeDurationMs) {
    const uint32_t progress = easedProgress(activeDurationMs);
    return START_INTERVAL_MS -
        ((START_INTERVAL_MS - MIN_INTERVAL_MS) * progress) / 1000U;
}

uint32_t Game2048AiPacing::animationMs(uint32_t activeDurationMs) {
    const uint32_t progress = easedProgress(activeDurationMs);
    return START_ANIMATION_MS -
        ((START_ANIMATION_MS - MIN_ANIMATION_MS) * progress) / 1000U;
}

uint32_t Game2048AiPacing::easedProgress(uint32_t activeDurationMs) {
    const uint32_t progress = activeDurationMs >= RAMP_MS
        ? 1000U : (activeDurationMs * 1000U) / RAMP_MS;
    return (progress * progress * (3000U - 2U * progress)) / 1000000U;
}

}  // namespace pgos
