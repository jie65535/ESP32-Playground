#pragma once

#include "games/MinesweeperRecords.h"

#include <Arduino.h>

class MinesweeperProfileService final {
public:
    bool begin(Stream& output);

    const pgos::MinesweeperRecords& records() const;
    pgos::MinesweeperRecordUpdate recordResult(
        pgos::MinesweeperDifficulty difficulty, bool won,
        uint32_t elapsedMs);

    pgos::MinesweeperDifficulty lastDifficulty() const;
    uint8_t customWidth() const;
    uint8_t customHeight() const;
    uint16_t customMines() const;
    void saveSetup(pgos::MinesweeperDifficulty difficulty,
                   uint8_t customWidth, uint8_t customHeight,
                   uint16_t customMines);

private:
    static constexpr uint8_t SCHEMA_VERSION = 1;

    Stream* log_ = nullptr;
    pgos::MinesweeperRecords records_{};
    pgos::MinesweeperDifficulty lastDifficulty_ =
        pgos::MinesweeperDifficulty::Beginner;
    uint8_t customWidth_ = 12;
    uint8_t customHeight_ = 12;
    uint16_t customMines_ = 24;

    bool save() const;
};
