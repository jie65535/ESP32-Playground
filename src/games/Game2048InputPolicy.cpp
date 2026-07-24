#include "games/Game2048InputPolicy.h"

#include <cstdlib>

namespace pgos {

void Game2048InputPolicy::reset() {
    heldX_ = 0;
    heldY_ = 0;
    nextRepeatMs_ = 0;
}

Game2048AnalogDecision Game2048InputPolicy::sample(
    int16_t axisX, int16_t axisY, uint32_t nowMs, bool inputAllowed) {
    int8_t x = 0;
    int8_t y = 0;
    dominantDirection(axisX, axisY, x, y);

    if (x != heldX_ || y != heldY_) {
        heldX_ = x;
        heldY_ = y;
        if (x == 0 && y == 0) {
            nextRepeatMs_ = 0;
            return {};
        }
        nextRepeatMs_ = nowMs + INITIAL_REPEAT_MS;
        return {inputAllowed, directionFor(x, y)};
    }
    if ((x == 0 && y == 0) || !inputAllowed ||
        static_cast<int32_t>(nowMs - nextRepeatMs_) < 0) {
        return {};
    }
    nextRepeatMs_ = nowMs + REPEAT_MS;
    return {true, directionFor(x, y)};
}

void Game2048InputPolicy::dominantDirection(int16_t axisX, int16_t axisY,
                                             int8_t& x, int8_t& y) const {
    const int32_t absX = std::abs(static_cast<int32_t>(axisX));
    const int32_t absY = std::abs(static_cast<int32_t>(axisY));
    const bool preferX = absX >= THRESHOLD &&
        (absX >= absY + DOMINANCE_MARGIN ||
         (heldX_ != 0 && absY < absX + DOMINANCE_MARGIN));
    const bool preferY = absY >= THRESHOLD &&
        (absY >= absX + DOMINANCE_MARGIN ||
         (heldY_ != 0 && absX < absY + DOMINANCE_MARGIN));
    if (preferX || (!preferY && absX >= THRESHOLD && absX >= absY)) {
        x = axisX < 0 ? -1 : 1;
    } else if (preferY || absY >= THRESHOLD) {
        y = axisY < 0 ? -1 : 1;
    }
}

Game2048Direction Game2048InputPolicy::directionFor(int8_t x, int8_t y) {
    if (x < 0) {
        return Game2048Direction::Left;
    }
    if (x > 0) {
        return Game2048Direction::Right;
    }
    return y < 0 ? Game2048Direction::Up : Game2048Direction::Down;
}

}  // namespace pgos
