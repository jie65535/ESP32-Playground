#include "games/MinesweeperRecords.h"

#include <cassert>
#include <cstdint>

using pgos::MinesweeperDifficulty;
using pgos::MinesweeperRecords;
using pgos::MinesweeperRecordsData;

namespace {

void testAscendingTopFiveAndRanks() {
    MinesweeperRecords records;
    const uint32_t times[] = {50000, 30000, 40000, 20000, 60000, 10000};
    for (uint8_t index = 0; index < 6; ++index) {
        const auto update = records.record(MinesweeperDifficulty::Beginner,
                                           true, times[index]);
        assert(update.recorded);
        if (index == 0 || index == 1 || index == 3 || index == 5) {
            assert(update.newBest);
        }
    }
    const auto& stats = records.level(MinesweeperDifficulty::Beginner);
    const uint32_t expected[] = {10000, 20000, 30000, 40000, 50000};
    for (uint8_t index = 0; index < 5; ++index) {
        assert(stats.bestTimesMs[index] == expected[index]);
    }
    assert(stats.played == 6 && stats.wins == 6);
    assert(stats.currentStreak == 6 && stats.bestStreak == 6);
}

void testLossResetsCurrentStreakOnly() {
    MinesweeperRecords records;
    records.record(MinesweeperDifficulty::Expert, true, 100000);
    records.record(MinesweeperDifficulty::Expert, true, 90000);
    records.record(MinesweeperDifficulty::Expert, false, 0);
    const auto& stats = records.level(MinesweeperDifficulty::Expert);
    assert(stats.played == 3 && stats.wins == 2);
    assert(stats.currentStreak == 0 && stats.bestStreak == 2);
}

void testCustomTracksAggregateWithoutLeaderboard() {
    MinesweeperRecords records;
    records.record(MinesweeperDifficulty::Custom, true, 12000);
    records.record(MinesweeperDifficulty::Custom, false, 0);
    assert(records.data().customPlayed == 2);
    assert(records.data().customWins == 1);
    assert(records.level(MinesweeperDifficulty::Beginner).bestTimesMs[0] == 0);
}

void testImportedDataIsNormalized() {
    MinesweeperRecordsData data{};
    data.levels[0].played = 2;
    data.levels[0].wins = 9;
    data.levels[0].currentStreak = 8;
    data.levels[0].bestTimesMs[0] = 9000;
    data.levels[0].bestTimesMs[1] = 3000;
    data.levels[0].bestTimesMs[3] = 6000;
    data.customPlayed = 1;
    data.customWins = 4;
    MinesweeperRecords records;
    records.importData(data);
    const auto& stats = records.level(MinesweeperDifficulty::Beginner);
    assert(stats.wins == 2 && stats.currentStreak == 2);
    assert(stats.bestStreak == 2);
    assert(stats.bestTimesMs[0] == 3000);
    assert(stats.bestTimesMs[1] == 6000);
    assert(stats.bestTimesMs[2] == 9000);
    assert(stats.bestTimesMs[3] == 0);
    assert(records.data().customWins == 1);
}

}  // namespace

int main() {
    testAscendingTopFiveAndRanks();
    testLossResetsCurrentStreakOnly();
    testCustomTracksAggregateWithoutLeaderboard();
    testImportedDataIsNormalized();
    return 0;
}
