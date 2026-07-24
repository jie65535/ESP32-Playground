#define PGOS_2048_TESTING 1

#include "games/Game2048Engine.h"

#include <cassert>
#include <cstdint>

using pgos::Game2048Direction;
using pgos::Game2048Engine;

namespace {

void testStartHasTwoTiles() {
    Game2048Engine engine;
    engine.reset(1);
    engine.start();
    assert(engine.populatedCount() == 2);
    assert(engine.score() == 0);
    assert(engine.tileAt(0) == 2 || engine.tileAt(0) == 4 ||
           engine.bestTile() == 2 || engine.bestTile() == 4);
}

void testLeftMergeAndSingleSpawn() {
    Game2048Engine engine;
    const uint32_t board[] = {2, 2, 4, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0};
    assert(engine.loadBoardForTesting(board));
    engine.setRandomStateForTesting(3);
    const auto result = engine.move(Game2048Direction::Left);
    assert(result.moved);
    assert(result.scoreDelta == 4);
    assert(engine.tile(0, 0) == 4);
    assert(engine.tile(1, 0) == 4);
    assert(engine.populatedCount() == 3);
    assert(result.spawnIndex < Game2048Engine::CELL_COUNT);
    assert(result.motionCount == 3);
}

void testDoubleMergeDoesNotChain() {
    Game2048Engine engine;
    const uint32_t board[] = {2, 2, 2, 2, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0};
    assert(engine.loadBoardForTesting(board));
    const auto result = engine.move(Game2048Direction::Left);
    assert(result.moved);
    assert(result.scoreDelta == 8);
    assert(engine.tile(0, 0) == 4);
    assert(engine.tile(1, 0) == 4);
}

void testDirectionAndNoOp() {
    Game2048Engine engine;
    const uint32_t board[] = {2, 0, 0, 0, 2, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0};
    assert(engine.loadBoardForTesting(board));
    const auto result = engine.move(Game2048Direction::Up);
    assert(result.moved && result.scoreDelta == 4);
    assert(engine.tile(0, 0) == 4);

    const uint32_t blocked[] = {2, 4, 2, 4, 4, 2, 4, 2,
                                2, 4, 2, 4, 4, 2, 4, 2};
    assert(engine.loadBoardForTesting(blocked));
    const auto noOp = engine.move(Game2048Direction::Left);
    assert(!noOp.moved && noOp.gameOver);
}

void testTargetAndScore() {
    Game2048Engine engine;
    const uint32_t board[] = {1024, 1024, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0};
    assert(engine.loadBoardForTesting(board));
    const auto result = engine.move(Game2048Direction::Left);
    assert(result.moved && result.reachedTarget);
    assert(engine.tile(0, 0) == 2048);
    assert(engine.hasReachedTarget());
    assert(engine.score() == 2048);
}

}  // namespace

int main() {
    testStartHasTwoTiles();
    testLeftMergeAndSingleSpawn();
    testDoubleMergeDoesNotChain();
    testDirectionAndNoOp();
    testTargetAndScore();
    return 0;
}
