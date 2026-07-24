#include "games/Game2048Ai.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace pgos {

namespace {

constexpr float PROBABILITY_CUTOFF = 0.0008F;
constexpr float EMPTY_WEIGHT = 270.0F;
constexpr float MERGE_WEIGHT = 700.0F;
constexpr float MONOTONICITY_WEIGHT = 47.0F;
constexpr float SUM_WEIGHT = 11.0F;
constexpr float SMOOTHNESS_WEIGHT = 16.0F;
constexpr float CORNER_WEIGHT = 95.0F;
constexpr float EDGE_WEIGHT = 18.0F;
constexpr float LOST_SCORE = -1000000.0F;

uint32_t power3(uint8_t value) {
    return static_cast<uint32_t>(value) * value * value;
}

uint32_t power4(uint8_t value) {
    const uint32_t squared = static_cast<uint32_t>(value) * value;
    return squared * squared;
}

}  // namespace

Game2048AiDecision Game2048Ai::chooseMove(const uint32_t* cells) {
    Game2048AiDecision decision{};
    if (cells == nullptr) {
        return decision;
    }
    Board board{};
    std::memcpy(board.cells, cells, sizeof(board.cells));
    evaluatedNodes_ = 0;
    float bestScore = -std::numeric_limits<float>::infinity();
    constexpr Game2048Direction DIRECTIONS[] = {
        Game2048Direction::Up, Game2048Direction::Left,
        Game2048Direction::Right, Game2048Direction::Down,
    };
    for (Game2048Direction direction : DIRECTIONS) {
        Board moved{};
        if (!executeMove(board, direction, moved)) {
            continue;
        }
        ++decision.evaluatedDirections;
        directionNodes_ = 0;
        const float score = scoreChanceNode(moved, SEARCH_DEPTH, 1.0F);
        if (!decision.valid || score > bestScore) {
            decision.valid = true;
            decision.direction = direction;
            decision.score = score;
            bestScore = score;
        }
    }
    decision.evaluatedNodes = evaluatedNodes_;
    return decision;
}

bool Game2048Ai::consumeNode() {
    if (directionNodes_ >= DIRECTION_NODE_BUDGET) {
        return false;
    }
    ++directionNodes_;
    ++evaluatedNodes_;
    return true;
}

float Game2048Ai::scoreMaxNode(const Board& board, uint8_t depth,
                               float cumulativeProbability) {
    if (!consumeNode() || depth == 0 ||
        cumulativeProbability < PROBABILITY_CUTOFF) {
        return heuristic(board);
    }
    float best = -std::numeric_limits<float>::infinity();
    bool moved = false;
    constexpr Game2048Direction DIRECTIONS[] = {
        Game2048Direction::Up, Game2048Direction::Left,
        Game2048Direction::Right, Game2048Direction::Down,
    };
    for (Game2048Direction direction : DIRECTIONS) {
        Board next{};
        if (!executeMove(board, direction, next)) {
            continue;
        }
        moved = true;
        best = std::max(best, scoreChanceNode(
            next, depth, cumulativeProbability));
    }
    return moved ? best : LOST_SCORE;
}

float Game2048Ai::scoreChanceNode(const Board& board, uint8_t depth,
                                  float cumulativeProbability) {
    if (!consumeNode() || depth == 0 ||
        cumulativeProbability < PROBABILITY_CUTOFF) {
        return heuristic(board);
    }
    uint8_t empty[Game2048Engine::CELL_COUNT] = {};
    uint8_t emptyCount = 0;
    for (uint8_t index = 0; index < Game2048Engine::CELL_COUNT; ++index) {
        if (board.cells[index] == 0) {
            empty[emptyCount++] = index;
        }
    }
    if (emptyCount == 0) {
        return scoreMaxNode(board, static_cast<uint8_t>(depth - 1U),
                            cumulativeProbability);
    }

    float expected = 0.0F;
    const float locationProbability = 1.0F / emptyCount;
    for (uint8_t i = 0; i < emptyCount; ++i) {
        Board withTwo = board;
        withTwo.cells[empty[i]] = 2;
        Board withFour = board;
        withFour.cells[empty[i]] = 4;
        expected += locationProbability *
            (0.9F * scoreMaxNode(
                withTwo, static_cast<uint8_t>(depth - 1U),
                cumulativeProbability * locationProbability * 0.9F) +
             0.1F * scoreMaxNode(
                withFour, static_cast<uint8_t>(depth - 1U),
                cumulativeProbability * locationProbability * 0.1F));
    }
    return expected;
}

float Game2048Ai::heuristic(const Board& board) {
    uint8_t ranks[Game2048Engine::CELL_COUNT] = {};
    uint8_t emptyCount = 0;
    uint8_t maxRank = 0;
    uint8_t maxIndex = 0;
    for (uint8_t index = 0; index < Game2048Engine::CELL_COUNT; ++index) {
        ranks[index] = rankOf(board.cells[index]);
        if (ranks[index] == 0) {
            ++emptyCount;
        } else if (ranks[index] > maxRank) {
            maxRank = ranks[index];
            maxIndex = index;
        }
    }

    float score = emptyCount * EMPTY_WEIGHT;
    for (uint8_t axis = 0; axis < 2; ++axis) {
        for (uint8_t line = 0; line < 4; ++line) {
            uint8_t values[4] = {};
            for (uint8_t offset = 0; offset < 4; ++offset) {
                const uint8_t index = axis == 0
                    ? static_cast<uint8_t>(line * 4U + offset)
                    : static_cast<uint8_t>(offset * 4U + line);
                values[offset] = ranks[index];
                score -= SUM_WEIGHT * power3(values[offset]);
            }
            uint8_t previous = 0;
            for (uint8_t offset = 0; offset < 4; ++offset) {
                if (values[offset] == 0) {
                    continue;
                }
                if (previous == values[offset]) {
                    score += MERGE_WEIGHT;
                }
                previous = values[offset];
            }
            float increasing = 0.0F;
            float decreasing = 0.0F;
            for (uint8_t offset = 1; offset < 4; ++offset) {
                const uint32_t before = power4(values[offset - 1U]);
                const uint32_t after = power4(values[offset]);
                if (before > after) {
                    decreasing += static_cast<float>(before - after);
                } else {
                    increasing += static_cast<float>(after - before);
                }
                score -= SMOOTHNESS_WEIGHT *
                    static_cast<float>(values[offset - 1U] > values[offset]
                        ? values[offset - 1U] - values[offset]
                        : values[offset] - values[offset - 1U]);
            }
            score -= MONOTONICITY_WEIGHT *
                std::min(increasing, decreasing);
        }
    }
    const uint8_t x = maxIndex % 4U;
    const uint8_t y = maxIndex / 4U;
    const bool corner = (x == 0 || x == 3) && (y == 0 || y == 3);
    const bool edge = x == 0 || x == 3 || y == 0 || y == 3;
    if (corner) {
        score += CORNER_WEIGHT * power4(maxRank);
    } else if (edge) {
        score += EDGE_WEIGHT * power4(maxRank);
    }
    return score;
}

bool Game2048Ai::executeMove(const Board& board,
                             Game2048Direction direction, Board& result) {
    result = {};
    for (uint8_t line = 0; line < 4; ++line) {
        uint32_t values[4] = {};
        uint8_t count = 0;
        for (uint8_t offset = 0; offset < 4; ++offset) {
            const uint32_t value = board.cells[
                lineIndex(direction, line, offset)];
            if (value != 0) {
                values[count++] = value;
            }
        }
        uint8_t input = 0;
        uint8_t output = 0;
        while (input < count) {
            const uint8_t destination = lineIndex(direction, line, output++);
            if (input + 1U < count && values[input] == values[input + 1U]) {
                result.cells[destination] = values[input] * 2U;
                input = static_cast<uint8_t>(input + 2U);
            } else {
                result.cells[destination] = values[input++];
            }
        }
    }
    return std::memcmp(board.cells, result.cells, sizeof(board.cells)) != 0;
}

uint8_t Game2048Ai::rankOf(uint32_t value) {
    uint8_t rank = 0;
    while (value > 1U && rank < 31U) {
        value >>= 1U;
        ++rank;
    }
    return rank;
}

uint8_t Game2048Ai::lineIndex(Game2048Direction direction, uint8_t line,
                              uint8_t offset) {
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

}  // namespace pgos
