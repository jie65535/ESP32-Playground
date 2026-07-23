#pragma once

#include "games/MinesweeperEngine.h"

#include <algorithm>
#include <cstdint>

namespace pgos {

struct MinesweeperBoardLayout {
    int16_t cellSize = 10;
    int16_t x = 0;
    int16_t y = 38;
    int16_t width = 0;
    int16_t height = 0;
};

struct MinesweeperNavigationVector {
    // Keep the post-dead-zone axis amplitude.  Collapsing this to -1/0/+1
    // makes every diagonal stick position look like a fixed 45 degree move.
    // Bluepad32's centred gamepad axes are normalised to roughly +/-512 by
    // the game input layer, so callers can use the value as a rate weight.
    int16_t x = 0;
    int16_t y = 0;
};

constexpr int16_t MINESWEEPER_NAVIGATION_FULL_SCALE = 512;
constexpr int32_t MINESWEEPER_NAVIGATION_REMAINDER_DENOMINATOR =
    static_cast<int32_t>(MINESWEEPER_NAVIGATION_FULL_SCALE) * 1000;

inline MinesweeperBoardLayout minesweeperBoardLayout(
    const MinesweeperConfig& config) {
    MinesweeperBoardLayout layout{};
    const int16_t byWidth = static_cast<int16_t>(300 / config.width);
    const int16_t byHeight = static_cast<int16_t>(176 / config.height);
    layout.cellSize =
        std::clamp<int16_t>(std::min(byWidth, byHeight), 10, 18);
    layout.width = static_cast<int16_t>(config.width * layout.cellSize);
    layout.height = static_cast<int16_t>(config.height * layout.cellSize);
    layout.x = static_cast<int16_t>((320 - layout.width) / 2);
    layout.y = static_cast<int16_t>(38 + (176 - layout.height) / 2);
    return layout;
}

inline uint16_t minesweeperRevealDelay(uint8_t x, uint8_t y,
                                       uint8_t originX, uint8_t originY) {
    constexpr uint16_t STEP_MS = 14;
    constexpr uint16_t MAX_DELAY_MS = 210;
    const uint16_t distance = static_cast<uint16_t>(
        std::abs(static_cast<int16_t>(x) - originX) +
        std::abs(static_cast<int16_t>(y) - originY));
    return std::min<uint16_t>(MAX_DELAY_MS, distance * STEP_MS);
}

// onCommand() can sample millis() a few microseconds after the kernel's
// onTick() timestamp. Signed arithmetic preserves that ordering at wraparound
// and prevents one D-pad press from becoming a synthetic second move.
inline bool minesweeperHeldDirectionShouldRepeat(uint32_t nowMs,
                                                 uint32_t lastDiscreteMs) {
    return static_cast<int32_t>(nowMs - lastDiscreteMs) > 40;
}

inline MinesweeperNavigationVector minesweeperAnalogNavigationVector(
    int16_t axisX, int16_t axisY, int16_t threshold) {
    MinesweeperNavigationVector vector{};
    axisX = std::clamp(axisX,
                       static_cast<int16_t>(-MINESWEEPER_NAVIGATION_FULL_SCALE),
                       MINESWEEPER_NAVIGATION_FULL_SCALE);
    axisY = std::clamp(axisY,
                       static_cast<int16_t>(-MINESWEEPER_NAVIGATION_FULL_SCALE),
                       MINESWEEPER_NAVIGATION_FULL_SCALE);
    threshold = std::clamp<int16_t>(threshold, 0,
                                    MINESWEEPER_NAVIGATION_FULL_SCALE);
    if (axisX <= -threshold) {
        vector.x = axisX;
    } else if (axisX >= threshold) {
        vector.x = axisX;
    }
    if (axisY <= -threshold) {
        vector.y = axisY;
    } else if (axisY >= threshold) {
        vector.y = axisY;
    }
    return vector;
}

inline int32_t minesweeperIntegratedNavigationSteps(
    int16_t axis, uint32_t elapsedMs, uint16_t cellsPerSecond,
    int32_t& remainder) {
    const int32_t magnitude = std::abs(std::clamp<int32_t>(
        axis, -MINESWEEPER_NAVIGATION_FULL_SCALE,
        MINESWEEPER_NAVIGATION_FULL_SCALE));
    remainder += magnitude * static_cast<int32_t>(cellsPerSecond) *
                 static_cast<int32_t>(elapsedMs);
    const int32_t steps =
        remainder / MINESWEEPER_NAVIGATION_REMAINDER_DENOMINATOR;
    remainder %= MINESWEEPER_NAVIGATION_REMAINDER_DENOMINATOR;
    return steps;
}

}  // namespace pgos
