#pragma once

#include <cstdint>

namespace pgos {

enum class MinesweeperDifficulty : uint8_t {
    Beginner,
    Intermediate,
    Expert,
    Custom,
};

struct MinesweeperLevelStats {
    uint32_t bestTimesMs[5] = {};
    uint32_t played = 0;
    uint32_t wins = 0;
    uint16_t currentStreak = 0;
    uint16_t bestStreak = 0;
};

struct MinesweeperRecordsData {
    MinesweeperLevelStats levels[3] = {};
    uint32_t customPlayed = 0;
    uint32_t customWins = 0;
};

struct MinesweeperRecordUpdate {
    bool recorded = false;
    bool newBest = false;
    uint8_t rank = 0xFF;
};

class MinesweeperRecords final {
public:
    static constexpr uint8_t STANDARD_LEVEL_COUNT = 3;
    static constexpr uint8_t TIME_COUNT = 5;

    void reset();
    void importData(const MinesweeperRecordsData& data);
    const MinesweeperRecordsData& data() const;
    const MinesweeperLevelStats& level(MinesweeperDifficulty difficulty) const;
    MinesweeperRecordUpdate record(MinesweeperDifficulty difficulty, bool won,
                                   uint32_t elapsedMs);

    static bool isStandard(MinesweeperDifficulty difficulty);

private:
    MinesweeperRecordsData data_{};

    static uint32_t saturatingIncrement(uint32_t value);
    static uint16_t saturatingIncrement(uint16_t value);
    static void normalizeLevel(MinesweeperLevelStats& level);
};

}  // namespace pgos
