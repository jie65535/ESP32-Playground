#pragma once

#include "games/Game2048Engine.h"

#include <cstdint>

namespace pgos {

struct Game2048AnalogDecision {
    bool triggered = false;
    Game2048Direction direction = Game2048Direction::Left;
};

class Game2048InputPolicy final {
public:
    static constexpr int16_t THRESHOLD = 180;
    static constexpr int16_t DOMINANCE_MARGIN = 48;
    static constexpr uint32_t INITIAL_REPEAT_MS = 300;
    static constexpr uint32_t REPEAT_MS = 240;

    void reset();
    Game2048AnalogDecision sample(int16_t axisX, int16_t axisY,
                                  uint32_t nowMs, bool inputAllowed);

private:
    int8_t heldX_ = 0;
    int8_t heldY_ = 0;
    uint32_t nextRepeatMs_ = 0;

    void dominantDirection(int16_t axisX, int16_t axisY,
                           int8_t& x, int8_t& y) const;
    static Game2048Direction directionFor(int8_t x, int8_t y);
};

}  // namespace pgos
