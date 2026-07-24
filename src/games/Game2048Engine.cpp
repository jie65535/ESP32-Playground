#include "games/Game2048Engine.h"

#include <algorithm>
#include <cstring>

namespace pgos {

namespace {

struct LineCell {
    uint32_t value = 0;
    uint8_t index = 0;
};

uint8_t lineIndex(Game2048Direction direction, uint8_t line, uint8_t offset) {
    switch (direction) {
        case Game2048Direction::Left:
            return static_cast<uint8_t>(line * 4U + offset);
        case Game2048Direction::Right:
            return static_cast<uint8_t>(line * 4U + (3U - offset));
        case Game2048Direction::Up:
            return static_cast<uint8_t>(offset * 4U + line);
        case Game2048Direction::Down:
            return static_cast<uint8_t>((3U - offset) * 4U + line);
    }
    return 0;
}

uint8_t targetIndex(Game2048Direction direction, uint8_t line, uint8_t offset) {
    return lineIndex(direction, line, offset);
}

}  // namespace

void Game2048Engine::reset(uint32_t seed) {
    std::fill_n(board_, CELL_COUNT, 0U);
    score_ = 0;
    bestTile_ = 0;
    reachedTarget_ = false;
    started_ = false;
    randomState_ = seed == 0 ? 0x2048C0DEU : seed;
}

void Game2048Engine::start() {
    std::fill_n(board_, CELL_COUNT, 0U);
    score_ = 0;
    bestTile_ = 0;
    reachedTarget_ = false;
    started_ = true;
    Game2048MoveResult ignored{};
    spawnOne(ignored);
    spawnOne(ignored);
}

Game2048MoveResult Game2048Engine::move(Game2048Direction direction) {
    Game2048MoveResult result{};
    if (!started_ || isGameOver()) {
        result.gameOver = isGameOver();
        return result;
    }

    uint32_t nextBoard[CELL_COUNT] = {};
    for (uint8_t line = 0; line < SIZE; ++line) {
        LineCell values[SIZE] = {};
        uint8_t count = 0;
        for (uint8_t offset = 0; offset < SIZE; ++offset) {
            const uint8_t source = lineIndex(direction, line, offset);
            if (board_[source] != 0) {
                values[count++] = {board_[source], source};
            }
        }

        uint8_t output = 0;
        uint8_t input = 0;
        while (input < count) {
            const uint8_t destination = targetIndex(direction, line, output);
            if (input + 1U < count && values[input].value == values[input + 1U].value) {
                const uint32_t mergedValue = values[input].value * 2U;
                nextBoard[destination] = mergedValue;
                if (result.motionCount + 2U <= CELL_COUNT) {
                    result.motions[result.motionCount++] =
                        {values[input].index, destination, values[input].value, true};
                    result.motions[result.motionCount++] =
                        {values[input + 1U].index, destination, values[input + 1U].value, true};
                }
                result.scoreDelta += mergedValue;
                bestTile_ = std::max(bestTile_, mergedValue);
                if (mergedValue >= TARGET) {
                    result.reachedTarget = !reachedTarget_;
                    reachedTarget_ = true;
                }
                input = static_cast<uint8_t>(input + 2U);
            } else {
                nextBoard[destination] = values[input].value;
                if (result.motionCount < CELL_COUNT) {
                    result.motions[result.motionCount++] =
                        {values[input].index, destination, values[input].value, false};
                }
                ++input;
            }
            ++output;
        }
    }

    if (std::memcmp(board_, nextBoard, sizeof(board_)) == 0) {
        result.gameOver = !hasMoves();
        return result;
    }

    std::memcpy(board_, nextBoard, sizeof(board_));
    score_ += result.scoreDelta;
    result.moved = true;
    spawnOne(result);
    result.gameOver = !hasMoves();
    return result;
}

uint32_t Game2048Engine::tile(uint8_t x, uint8_t y) const {
    return x < SIZE && y < SIZE ? board_[indexOf(x, y)] : 0;
}

uint32_t Game2048Engine::tileAt(uint8_t index) const {
    return index < CELL_COUNT ? board_[index] : 0;
}

uint32_t Game2048Engine::score() const { return score_; }

uint32_t Game2048Engine::bestTile() const { return bestTile_; }

bool Game2048Engine::hasReachedTarget() const { return reachedTarget_; }

bool Game2048Engine::isGameOver() const {
    return started_ && !hasMoves();
}

uint8_t Game2048Engine::populatedCount() const {
    uint8_t count = 0;
    for (uint32_t value : board_) {
        if (value != 0) {
            ++count;
        }
    }
    return count;
}

const uint32_t* Game2048Engine::board() const { return board_; }

uint32_t Game2048Engine::nextRandom() {
    uint32_t value = randomState_;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    randomState_ = value == 0 ? 0x2048C0DEU : value;
    return randomState_;
}

bool Game2048Engine::spawnOne(Game2048MoveResult& result) {
    uint8_t empty[CELL_COUNT] = {};
    uint8_t count = 0;
    for (uint8_t index = 0; index < CELL_COUNT; ++index) {
        if (board_[index] == 0) {
            empty[count++] = index;
        }
    }
    if (count == 0) {
        return false;
    }
    const uint8_t index = empty[nextRandom() % count];
    const uint32_t value = (nextRandom() % 10U) == 0U ? 4U : 2U;
    board_[index] = value;
    bestTile_ = std::max(bestTile_, value);
    result.spawnIndex = index;
    result.spawnValue = value;
    return true;
}

bool Game2048Engine::hasMoves() const {
    for (uint8_t y = 0; y < SIZE; ++y) {
        for (uint8_t x = 0; x < SIZE; ++x) {
            const uint32_t value = tile(x, y);
            if (value == 0) {
                return true;
            }
            if ((x + 1U < SIZE && value == tile(x + 1U, y)) ||
                (y + 1U < SIZE && value == tile(x, y + 1U))) {
                return true;
            }
        }
    }
    return false;
}

#if defined(PGOS_2048_TESTING)
bool Game2048Engine::loadBoardForTesting(const uint32_t* values,
                                         uint32_t score) {
    if (values == nullptr) {
        return false;
    }
    std::memcpy(board_, values, sizeof(board_));
    score_ = score;
    bestTile_ = 0;
    for (uint32_t value : board_) {
        bestTile_ = std::max(bestTile_, value);
    }
    reachedTarget_ = bestTile_ >= TARGET;
    started_ = true;
    return true;
}

void Game2048Engine::setRandomStateForTesting(uint32_t state) {
    randomState_ = state == 0 ? 0x2048C0DEU : state;
}
#endif

}  // namespace pgos
