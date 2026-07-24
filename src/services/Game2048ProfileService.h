#pragma once

#include <Arduino.h>

class Game2048ProfileService final {
public:
    bool begin(Stream& output);

    uint32_t bestScore() const;
    uint32_t bestTile() const;
    uint32_t gamesPlayed() const;
    uint32_t wins() const;

    void observeBest(uint32_t score, uint32_t tile);
    void recordGame(uint32_t score, uint32_t tile, bool won);
    void save();

private:
    static constexpr uint8_t SCHEMA_VERSION = 1;
    static constexpr uint32_t MAX_SCORE = 0xFFFFFFFFUL;

    Stream* log_ = nullptr;
    uint32_t bestScore_ = 0;
    uint32_t bestTile_ = 0;
    uint32_t gamesPlayed_ = 0;
    uint32_t wins_ = 0;
};
