#pragma once

#include <Arduino.h>
#include <Preferences.h>

class SnakeScoreService {
public:
    static constexpr uint8_t SCORE_COUNT = 5;

    bool begin(Stream& log);

    uint16_t bestScore() const;
    uint16_t scoreAt(uint8_t index) const;
    bool recordScore(uint16_t score);

private:
    static constexpr uint8_t SETTINGS_SCHEMA = 2;
    static constexpr uint16_t DEFAULT_BEST_SCORE = 0;

    Preferences preferences_;
    Stream* log_ = nullptr;
    bool preferencesReady_ = false;
    uint16_t bestScore_ = DEFAULT_BEST_SCORE;
    uint16_t scores_[SCORE_COUNT] = {};
};
