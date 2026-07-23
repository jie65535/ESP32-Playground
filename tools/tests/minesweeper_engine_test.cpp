#define PGOS_MINESWEEPER_TESTING 1

#include "games/MinesweeperEngine.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using pgos::MinesweeperActionResult;
using pgos::MinesweeperCell;
using pgos::MinesweeperConfig;
using pgos::MinesweeperDeduction;
using pgos::MinesweeperEngine;
using pgos::MinesweeperPhase;
using pgos::MinesweeperSolver;
using pgos::MinesweeperSolverStats;
using pgos::MinesweeperVisibleCell;
using pgos::MinesweeperVisibility;

namespace {

void finishGeneration(MinesweeperEngine& engine, uint32_t limit = 50000) {
    for (uint32_t attempt = 0; attempt < limit; ++attempt) {
        if (engine.generateStep()) {
            return;
        }
    }
    assert(false && "no certified no-guess board generated");
}

void verifyBoard(const MinesweeperEngine& engine, uint8_t firstX,
                 uint8_t firstY) {
    const MinesweeperConfig config = engine.config();
    uint16_t mines = 0;
    for (uint8_t y = 0; y < config.height; ++y) {
        for (uint8_t x = 0; x < config.width; ++x) {
            const MinesweeperCell cell = engine.cell(x, y);
            mines += cell.mine;
            if (cell.mine) {
                continue;
            }
            uint8_t expected = 0;
            for (int8_t dy = -1; dy <= 1; ++dy) {
                for (int8_t dx = -1; dx <= 1; ++dx) {
                    const int16_t nx = static_cast<int16_t>(x) + dx;
                    const int16_t ny = static_cast<int16_t>(y) + dy;
                    if ((dx != 0 || dy != 0) && nx >= 0 && ny >= 0 &&
                        nx < config.width && ny < config.height) {
                        expected += engine.cell(static_cast<uint8_t>(nx),
                                                static_cast<uint8_t>(ny)).mine;
                    }
                }
            }
            assert(cell.adjacentMines == expected);
        }
    }
    assert(mines == config.mines);
    for (int8_t dy = -1; dy <= 1; ++dy) {
        for (int8_t dx = -1; dx <= 1; ++dx) {
            const int16_t x = static_cast<int16_t>(firstX) + dx;
            const int16_t y = static_cast<int16_t>(firstY) + dy;
            if (x >= 0 && y >= 0 && x < config.width && y < config.height) {
                assert(!engine.cell(static_cast<uint8_t>(x),
                                    static_cast<uint8_t>(y)).mine);
            }
        }
    }
    MinesweeperSolver solver;
    MinesweeperSolverStats stats{};
    const uint16_t first = static_cast<uint16_t>(firstY) * config.width + firstX;
    assert(solver.isSolvable(engine.contentForTesting(), config.width,
                             config.height, config.mines, first, &stats));
    assert(stats.revealedSafe ==
           static_cast<uint16_t>(config.width * config.height - config.mines));
}

void testConfigValidation() {
    assert(MinesweeperEngine::validConfig(MinesweeperEngine::BEGINNER));
    assert(MinesweeperEngine::validConfig(MinesweeperEngine::INTERMEDIATE));
    assert(MinesweeperEngine::validConfig(MinesweeperEngine::EXPERT));
    assert(!MinesweeperEngine::validConfig({4, 9, 10}));
    assert(!MinesweeperEngine::validConfig({31, 16, 10}));
    assert(!MinesweeperEngine::validConfig({9, 9, 0}));
    assert(!MinesweeperEngine::validConfig({5, 5, 17}));
}

void testStandardBoardsGenerateAndAreDeterministic() {
    const MinesweeperConfig configs[] = {
        MinesweeperEngine::BEGINNER,
        MinesweeperEngine::INTERMEDIATE,
        MinesweeperEngine::EXPERT,
    };
    for (uint8_t level = 0; level < 3; ++level) {
        MinesweeperEngine first;
        MinesweeperEngine second;
        assert(first.reset(configs[level], 0x12340000U + level));
        assert(second.reset(configs[level], 0x12340000U + level));
        const uint8_t x = configs[level].width / 2;
        const uint8_t y = configs[level].height / 2;
        assert(first.beginGeneration(x, y));
        assert(second.beginGeneration(x, y));
        finishGeneration(first);
        finishGeneration(second);
        assert(first.generationAttempts() == second.generationAttempts());
        verifyBoard(first, x, y);
        for (uint8_t row = 0; row < configs[level].height; ++row) {
            for (uint8_t column = 0; column < configs[level].width; ++column) {
                const MinesweeperCell a = first.cell(column, row);
                const MinesweeperCell b = second.cell(column, row);
                assert(a.mine == b.mine);
                assert(a.adjacentMines == b.adjacentMines);
            }
        }
    }
}

void testSafeZoneAtBoardEdges() {
    const uint8_t starts[][2] = {
        {0, 0}, {29, 0}, {0, 15}, {29, 15}, {14, 8},
    };
    for (uint8_t index = 0; index < 5; ++index) {
        MinesweeperEngine engine;
        assert(engine.reset(MinesweeperEngine::EXPERT,
                            0xED6E0000U + index));
        assert(engine.beginGeneration(starts[index][0], starts[index][1]));
        finishGeneration(engine);
        verifyBoard(engine, starts[index][0], starts[index][1]);
    }
}

void testSupportedCustomDensityGenerates() {
    const MinesweeperConfig configs[] = {
        {8, 8, 15}, {8, 16, 30}, {30, 8, 57}, {30, 16, 115},
    };
    for (uint8_t configIndex = 0; configIndex < 4; ++configIndex) {
        for (uint8_t seed = 0; seed < 3; ++seed) {
            MinesweeperEngine engine;
            assert(engine.reset(configs[configIndex],
                                0xC0570000U + configIndex * 16U + seed));
            assert(engine.beginGeneration(configs[configIndex].width / 2,
                                          configs[configIndex].height / 2));
            finishGeneration(engine);
            verifyBoard(engine, configs[configIndex].width / 2,
                        configs[configIndex].height / 2);
        }
    }
}

void testFlagAndRevealRules() {
    MinesweeperEngine engine;
    const MinesweeperConfig config{5, 5, 1};
    const uint16_t mines[] = {24};
    assert(engine.loadBoardForTesting(config, mines, 1));
    MinesweeperActionResult result = engine.toggleFlag(4, 4);
    assert(result.accepted && result.flagChanged && engine.flagCount() == 1);
    assert(!engine.reveal(4, 4).accepted);
    result = engine.toggleFlag(4, 4);
    assert(result.accepted && engine.flagCount() == 0);
    result = engine.reveal(0, 0);
    assert(result.accepted && result.won && !result.hitMine);
    assert(engine.phase() == MinesweeperPhase::Won);
    assert(engine.revealedSafeCount() == 24);
}

void testCorrectChordWins() {
    MinesweeperEngine engine;
    const MinesweeperConfig config{5, 5, 1};
    const uint16_t mines[] = {6};
    assert(engine.loadBoardForTesting(config, mines, 1));
    assert(engine.reveal(0, 0).accepted);
    assert(engine.toggleFlag(1, 1).accepted);
    MinesweeperActionResult result = engine.reveal(0, 0);
    assert(result.accepted && !result.hitMine && result.revealed == 2);
    assert(engine.phase() == MinesweeperPhase::Playing);
    result = engine.reveal(4, 4);
    assert(result.accepted && result.won);
    assert(engine.phase() == MinesweeperPhase::Won);
}

void testWrongChordDetonatesMine() {
    MinesweeperEngine engine;
    const MinesweeperConfig config{5, 5, 1};
    const uint16_t mines[] = {6};
    assert(engine.loadBoardForTesting(config, mines, 1));
    assert(engine.reveal(0, 0).accepted);
    assert(engine.toggleFlag(1, 0).accepted);
    const MinesweeperActionResult result = engine.reveal(0, 0);
    assert(result.accepted && result.hitMine);
    assert(engine.phase() == MinesweeperPhase::Lost);
    assert(engine.cell(1, 1).detonated);
}

void testSolverRejectsAmbiguousPosition() {
    // First cell sees one mine among three covered neighbors; no logical move
    // is forced, so a fair-board generator must reject it.
    const uint8_t board[] = {1, 1, 1, 0x80U};
    MinesweeperSolver solver;
    assert(!solver.isSolvable(board, 2, 2, 1, 0));
}

void testVisibleDeductionLocalRules() {
    MinesweeperDeduction deduction;
    const MinesweeperConfig mineConfig{2, 1, 1};
    const MinesweeperVisibleCell mineCells[] = {
        {MinesweeperVisibility::Revealed, 1},
        {MinesweeperVisibility::Covered, 0},
    };
    assert(deduction.analyze(mineConfig, mineCells));
    assert(deduction.knownMine(1));

    const MinesweeperConfig safeConfig{3, 1, 1};
    const MinesweeperVisibleCell safeCells[] = {
        {MinesweeperVisibility::Revealed, 0},
        {MinesweeperVisibility::Covered, 0},
        {MinesweeperVisibility::Covered, 0},
    };
    assert(deduction.analyze(safeConfig, safeCells));
    assert(deduction.knownSafe(1));
}

void testVisibleDeductionSubsetRules() {
    MinesweeperDeduction deduction;
    const MinesweeperVisibleCell safeDifference[] = {
        {MinesweeperVisibility::Revealed, 1},
        {MinesweeperVisibility::Revealed, 1},
        {MinesweeperVisibility::Revealed, 1},
        {MinesweeperVisibility::Covered, 0},
        {MinesweeperVisibility::Covered, 0},
        {MinesweeperVisibility::Covered, 0},
    };
    assert(deduction.analyze({3, 2, 1}, safeDifference));
    assert(deduction.knownSafe(5));

    const MinesweeperVisibleCell mineDifference[] = {
        {MinesweeperVisibility::Revealed, 1},
        {MinesweeperVisibility::Revealed, 2},
        {MinesweeperVisibility::Revealed, 1},
        {MinesweeperVisibility::Covered, 0},
        {MinesweeperVisibility::Covered, 0},
        {MinesweeperVisibility::Covered, 0},
    };
    assert(deduction.analyze({3, 2, 2}, mineDifference));
    assert(deduction.knownMine(5));
}

void testVisibleDeductionRejectsGuessingAndContradictoryFlags() {
    MinesweeperDeduction deduction;
    const MinesweeperVisibleCell ambiguous[] = {
        {MinesweeperVisibility::Revealed, 1},
        {MinesweeperVisibility::Covered, 0},
        {MinesweeperVisibility::Covered, 0},
        {MinesweeperVisibility::Covered, 0},
    };
    assert(deduction.analyze({2, 2, 1}, ambiguous));
    assert(!deduction.knownMine(1));
    assert(!deduction.knownSafe(1));
    assert(!deduction.knownMine(2));
    assert(!deduction.knownSafe(2));
    assert(!deduction.knownMine(3));
    assert(!deduction.knownSafe(3));

    const MinesweeperVisibleCell contradictory[] = {
        {MinesweeperVisibility::Revealed, 0},
        {MinesweeperVisibility::Flagged, 0},
    };
    assert(!deduction.analyze({2, 1, 1}, contradictory));
}

void testVisibleAssistActionsUseOnlyCurrentProof() {
    MinesweeperEngine engine;
    const uint16_t mines[] = {6, 24};
    assert(engine.loadBoardForTesting({5, 5, 2}, mines, 2));
    assert(engine.reveal(0, 0).accepted);
    assert(engine.reveal(1, 0).accepted);
    assert(engine.reveal(0, 1).accepted);
    const MinesweeperActionResult result = engine.revealKnownSafe();
    assert(result.accepted && result.revealed >= 2 && !result.hitMine);
    assert(engine.cell(2, 0).visibility == MinesweeperVisibility::Revealed);
    assert(engine.cell(2, 1).visibility == MinesweeperVisibility::Revealed);

    MinesweeperEngine ambiguous;
    const uint16_t singleMine[] = {6};
    assert(ambiguous.loadBoardForTesting({5, 5, 1}, singleMine, 1));
    assert(ambiguous.reveal(0, 0).accepted);
    const MinesweeperActionResult noMove = ambiguous.flagKnownMines();
    assert(!noMove.accepted && noMove.flagged == 0);

    MinesweeperEngine wrongFlag;
    assert(wrongFlag.loadBoardForTesting({5, 5, 2}, mines, 2));
    assert(wrongFlag.reveal(0, 0).accepted);
    assert(wrongFlag.toggleFlag(1, 0).accepted);
    const MinesweeperActionResult noUnsafeOpen = wrongFlag.revealKnownSafe();
    assert(!noUnsafeOpen.accepted && !noUnsafeOpen.hitMine);
}

void testVisibleAssistFindsLocalMinesAndSafeCells() {
    MinesweeperEngine engine;
    const uint16_t mines[] = {6, 24};
    assert(engine.loadBoardForTesting({5, 5, 2}, mines, 2));
    assert(engine.reveal(0, 0).accepted);
    assert(engine.reveal(1, 0).accepted);
    assert(engine.reveal(0, 1).accepted);

    const MinesweeperActionResult flags = engine.flagKnownMines();
    assert(flags.accepted && flags.flagged == 1);
    assert(engine.cell(1, 1).visibility == MinesweeperVisibility::Flagged);

    const MinesweeperActionResult safe = engine.revealKnownSafe();
    assert(safe.accepted && safe.revealed >= 2 && !safe.hitMine);
    assert(engine.cell(2, 0).visibility == MinesweeperVisibility::Revealed);
    assert(engine.cell(2, 1).visibility == MinesweeperVisibility::Revealed);
}

void benchmarkGeneration() {
    const MinesweeperConfig configs[] = {
        MinesweeperEngine::BEGINNER,
        MinesweeperEngine::INTERMEDIATE,
        MinesweeperEngine::EXPERT,
    };
    const char* names[] = {"beginner", "intermediate", "expert"};
    for (uint8_t level = 0; level < 3; ++level) {
        uint64_t totalAttempts = 0;
        uint32_t worstAttempts = 0;
        const auto started = std::chrono::steady_clock::now();
        for (uint32_t seed = 1; seed <= 12; ++seed) {
            MinesweeperEngine engine;
            assert(engine.reset(configs[level], 0xB0000000U + seed));
            assert(engine.beginGeneration(configs[level].width / 2,
                                          configs[level].height / 2));
            finishGeneration(engine);
            totalAttempts += engine.generationAttempts();
            if (engine.generationAttempts() > worstAttempts) {
                worstAttempts = engine.generationAttempts();
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        std::printf("%s: avg_attempts=%.1f worst=%u elapsed_ms=%lld\n",
                    names[level], static_cast<double>(totalAttempts) / 12.0,
                    static_cast<unsigned>(worstAttempts),
                    static_cast<long long>(elapsed));
    }
}

}  // namespace

int main() {
    testConfigValidation();
    testStandardBoardsGenerateAndAreDeterministic();
    testSafeZoneAtBoardEdges();
    testSupportedCustomDensityGenerates();
    testFlagAndRevealRules();
    testCorrectChordWins();
    testWrongChordDetonatesMine();
    testSolverRejectsAmbiguousPosition();
    testVisibleDeductionLocalRules();
    testVisibleDeductionSubsetRules();
    testVisibleDeductionRejectsGuessingAndContradictoryFlags();
    testVisibleAssistActionsUseOnlyCurrentProof();
    testVisibleAssistFindsLocalMinesAndSafeCells();
    benchmarkGeneration();
    return 0;
}
