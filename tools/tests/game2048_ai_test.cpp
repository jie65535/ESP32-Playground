#define PGOS_2048_TESTING 1

#include "games/Game2048Ai.h"
#include "games/Game2048Engine.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>

using pgos::Game2048Ai;
using pgos::Game2048Direction;
using pgos::Game2048Engine;

namespace {

void testTerminalBoardHasNoMove() {
    constexpr uint32_t BOARD[] = {
        2, 4, 2, 4, 4, 2, 4, 2,
        2, 4, 2, 4, 4, 2, 4, 2,
    };
    Game2048Ai ai;
    const auto decision = ai.chooseMove(BOARD);
    assert(!decision.valid);
    assert(decision.evaluatedDirections == 0);
    assert(decision.evaluatedNodes == 0);
}

void testAllLegalDirectionsReceiveTheirOwnBudget() {
    constexpr uint32_t BOARD[] = {
        2, 0, 4, 0, 0, 8, 0, 16,
        32, 0, 64, 0, 0, 128, 0, 256,
    };
    Game2048Ai ai;
    const auto decision = ai.chooseMove(BOARD);
    assert(decision.valid);
    assert(decision.evaluatedDirections == 4);
    assert(decision.evaluatedNodes > 0);
    assert(decision.evaluatedNodes <= Game2048Ai::NODE_BUDGET);

    Game2048Engine engine;
    assert(engine.loadBoardForTesting(BOARD));
    assert(engine.move(decision.direction).moved);
}

void testFixedBoardIsDeterministic() {
    constexpr uint32_t BOARD[] = {
        2, 4, 8, 16, 0, 8, 16, 32,
        0, 0, 32, 64, 0, 0, 0, 128,
    };
    Game2048Ai firstAi;
    Game2048Ai secondAi;
    const auto first = firstAi.chooseMove(BOARD);
    const auto second = secondAi.chooseMove(BOARD);
    assert(first.valid && second.valid);
    assert(first.direction == second.direction);
    assert(first.evaluatedNodes == second.evaluatedNodes);
    assert(std::fabs(first.score - second.score) < 0.001F);
}

void testFixedSeedAutoplaySmoke() {
    Game2048Ai ai;
    Game2048Engine engine;
    engine.reset(0xA12048U);
    engine.start();

    uint32_t decisions = 0;
    uint64_t totalUs = 0;
    uint64_t maxUs = 0;
    for (; decisions < 320 && !engine.isGameOver(); ++decisions) {
        const auto started = std::chrono::steady_clock::now();
        const auto decision = ai.chooseMove(engine.board());
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count();
        totalUs += static_cast<uint64_t>(elapsed);
        if (static_cast<uint64_t>(elapsed) > maxUs) {
            maxUs = static_cast<uint64_t>(elapsed);
        }
        assert(decision.valid);
        assert(decision.evaluatedNodes <= Game2048Ai::NODE_BUDGET);
        assert(engine.move(decision.direction).moved);
    }
    assert(decisions >= 100);
    assert(engine.bestTile() >= 128);
    std::printf("2048 AI smoke: moves=%u best=%u avg=%llu us max=%llu us\n",
                decisions, engine.bestTile(),
                static_cast<unsigned long long>(totalUs / decisions),
                static_cast<unsigned long long>(maxUs));
}

}  // namespace

int main() {
    testTerminalBoardHasNoMove();
    testAllLegalDirectionsReceiveTheirOwnBudget();
    testFixedBoardIsDeterministic();
    testFixedSeedAutoplaySmoke();
    return 0;
}
