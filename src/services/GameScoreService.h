#pragma once

#include <Arduino.h>

class GameScoreService {
public:
    static constexpr uint8_t SCORE_COUNT = 5;

    GameScoreService(const char* nvsNamespace, uint8_t schemaVersion,
                     const char* logTag);

    bool begin(Stream& output);
    uint16_t bestScore() const;
    uint16_t scoreAt(uint8_t index) const;
    bool recordScore(uint16_t score);

private:
    const char* nvsNamespace_;
    const char* logTag_;
    uint8_t schemaVersion_;
    uint16_t scores_[SCORE_COUNT] = {};

    bool save() const;
};
