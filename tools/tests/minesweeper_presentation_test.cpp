#include "games/MinesweeperPresentation.h"

#include <cassert>
#include <cstdint>

using pgos::MinesweeperConfig;
using pgos::MinesweeperEngine;

namespace {

void verifyLayout(const MinesweeperConfig& config) {
    const auto layout = pgos::minesweeperBoardLayout(config);
    assert(layout.cellSize >= 10 && layout.cellSize <= 18);
    assert(layout.width == config.width * layout.cellSize);
    assert(layout.height == config.height * layout.cellSize);
    assert(layout.x >= 2 && layout.x + layout.width <= 318);
    assert(layout.y >= 38 && layout.y + layout.height <= 214);
}

void testStandardLayouts() {
    const auto beginner =
        pgos::minesweeperBoardLayout(MinesweeperEngine::BEGINNER);
    const auto intermediate =
        pgos::minesweeperBoardLayout(MinesweeperEngine::INTERMEDIATE);
    const auto expert =
        pgos::minesweeperBoardLayout(MinesweeperEngine::EXPERT);
    assert(beginner.cellSize == 18 && beginner.width == 162);
    assert(intermediate.cellSize == 11 && intermediate.height == 176);
    assert(expert.cellSize == 10 && expert.width == 300);
}

void testAllSupportedCustomLayoutsFit() {
    for (uint8_t height = 8; height <= 16; ++height) {
        for (uint8_t width = 8; width <= 30; ++width) {
            verifyLayout({width, height, 1});
        }
    }
}

void testRevealWaveIsBoundedAndRadial() {
    assert(pgos::minesweeperRevealDelay(4, 4, 4, 4) == 0);
    assert(pgos::minesweeperRevealDelay(5, 4, 4, 4) == 14);
    assert(pgos::minesweeperRevealDelay(5, 5, 4, 4) == 28);
    assert(pgos::minesweeperRevealDelay(29, 15, 0, 0) == 210);
}

void testDiscreteInputCannotBecomeAnImmediateHeldRepeat() {
    assert(!pgos::minesweeperHeldDirectionShouldRepeat(1000, 1001));
    assert(!pgos::minesweeperHeldDirectionShouldRepeat(1040, 1000));
    assert(pgos::minesweeperHeldDirectionShouldRepeat(1041, 1000));
}

void testAnalogAxesAreIndependentAndSupportDiagonals() {
    auto vector = pgos::minesweeperAnalogNavigationVector(220, -240, 135);
    assert(vector.x == 220 && vector.y == -240);
    vector = pgos::minesweeperAnalogNavigationVector(-300, 280, 135);
    assert(vector.x == -300 && vector.y == 280);
    vector = pgos::minesweeperAnalogNavigationVector(80, -134, 135);
    assert(vector.x == 0 && vector.y == 0);
}

void testAnalogAxesPreserveDirectionRatioAndClampOutliers() {
    const auto vector =
        pgos::minesweeperAnalogNavigationVector(448, -192, 135);
    assert(vector.x == 448 && vector.y == -192);
    const auto clamped =
        pgos::minesweeperAnalogNavigationVector(3000, -3000, 135);
    assert(clamped.x == pgos::MINESWEEPER_NAVIGATION_FULL_SCALE);
    assert(clamped.y == -pgos::MINESWEEPER_NAVIGATION_FULL_SCALE);
}

void testAnalogIntegrationFollowsStickAngle() {
    int32_t remainderX = 0;
    int32_t remainderY = 0;
    int32_t stepsX = 0;
    int32_t stepsY = 0;
    for (uint8_t sample = 0; sample < 10; ++sample) {
        stepsX += pgos::minesweeperIntegratedNavigationSteps(
            448, 50, 14, remainderX);
        stepsY += pgos::minesweeperIntegratedNavigationSteps(
            -192, 50, 14, remainderY);
    }
    assert(stepsX == 6);
    assert(stepsY == 2);
    assert(remainderX > 0 && remainderY > 0);
}

}  // namespace

int main() {
    testStandardLayouts();
    testAllSupportedCustomLayoutsFit();
    testRevealWaveIsBoundedAndRadial();
    testDiscreteInputCannotBecomeAnImmediateHeldRepeat();
    testAnalogAxesAreIndependentAndSupportDiagonals();
    testAnalogAxesPreserveDirectionRatioAndClampOutliers();
    testAnalogIntegrationFollowsStickAngle();
    return 0;
}
