#pragma once

#include <Arduino.h>

class TetrisScoreService {
public:
    static constexpr uint8_t SCORE_COUNT = 5;

    void begin(Stream& output);
    uint16_t bestScore() const;
    uint16_t scoreAt(uint8_t index) const;
    void recordScore(uint16_t score);

private:
    static constexpr const char* NAMESPACE = "pgos_tetris";
    static constexpr uint8_t SCHEMA_VERSION = 1;
    uint16_t scores_[SCORE_COUNT] = {};
};
