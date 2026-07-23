#include "games/MinesweeperRecords.h"

#include <algorithm>
#include <limits>

namespace pgos {

void MinesweeperRecords::reset() { data_ = {}; }

void MinesweeperRecords::importData(const MinesweeperRecordsData& data) {
    data_ = data;
    for (auto& levelStats : data_.levels) {
        normalizeLevel(levelStats);
    }
    if (data_.customWins > data_.customPlayed) {
        data_.customWins = data_.customPlayed;
    }
}

const MinesweeperRecordsData& MinesweeperRecords::data() const {
    return data_;
}

const MinesweeperLevelStats& MinesweeperRecords::level(
    MinesweeperDifficulty difficulty) const {
    const uint8_t index = static_cast<uint8_t>(difficulty);
    return data_.levels[index < STANDARD_LEVEL_COUNT ? index : 0];
}

MinesweeperRecordUpdate MinesweeperRecords::record(
    MinesweeperDifficulty difficulty, bool won, uint32_t elapsedMs) {
    MinesweeperRecordUpdate update{};
    update.recorded = true;
    if (!isStandard(difficulty)) {
        data_.customPlayed = saturatingIncrement(data_.customPlayed);
        if (won) {
            data_.customWins = saturatingIncrement(data_.customWins);
        }
        return update;
    }

    MinesweeperLevelStats& stats =
        data_.levels[static_cast<uint8_t>(difficulty)];
    stats.played = saturatingIncrement(stats.played);
    if (!won) {
        stats.currentStreak = 0;
        return update;
    }

    stats.wins = saturatingIncrement(stats.wins);
    stats.currentStreak = saturatingIncrement(stats.currentStreak);
    stats.bestStreak = std::max(stats.bestStreak, stats.currentStreak);

    elapsedMs = std::max<uint32_t>(1U, elapsedMs);
    uint8_t insertion = TIME_COUNT;
    for (uint8_t index = 0; index < TIME_COUNT; ++index) {
        if (stats.bestTimesMs[index] == 0 ||
            elapsedMs < stats.bestTimesMs[index]) {
            insertion = index;
            break;
        }
    }
    if (insertion == TIME_COUNT) {
        return update;
    }
    for (uint8_t index = TIME_COUNT - 1; index > insertion; --index) {
        stats.bestTimesMs[index] = stats.bestTimesMs[index - 1U];
    }
    stats.bestTimesMs[insertion] = elapsedMs;
    update.rank = insertion;
    update.newBest = insertion == 0;
    return update;
}

bool MinesweeperRecords::isStandard(MinesweeperDifficulty difficulty) {
    return static_cast<uint8_t>(difficulty) < STANDARD_LEVEL_COUNT;
}

uint32_t MinesweeperRecords::saturatingIncrement(uint32_t value) {
    return value == std::numeric_limits<uint32_t>::max() ? value : value + 1U;
}

uint16_t MinesweeperRecords::saturatingIncrement(uint16_t value) {
    return value == std::numeric_limits<uint16_t>::max() ? value : value + 1U;
}

void MinesweeperRecords::normalizeLevel(MinesweeperLevelStats& level) {
    if (level.wins > level.played) {
        level.wins = level.played;
    }
    if (level.currentStreak > level.wins) {
        level.currentStreak = static_cast<uint16_t>(
            std::min<uint32_t>(level.wins,
                               std::numeric_limits<uint16_t>::max()));
    }
    level.bestStreak = std::max(level.bestStreak, level.currentStreak);

    uint32_t sorted[TIME_COUNT] = {};
    uint8_t count = 0;
    for (uint8_t index = 0; index < TIME_COUNT; ++index) {
        if (level.bestTimesMs[index] != 0) {
            sorted[count++] = level.bestTimesMs[index];
        }
    }
    std::sort(sorted, sorted + count);
    for (uint8_t index = 0; index < TIME_COUNT; ++index) {
        level.bestTimesMs[index] = index < count ? sorted[index] : 0;
    }
}

}  // namespace pgos
