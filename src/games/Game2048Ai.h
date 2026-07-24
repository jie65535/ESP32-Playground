#pragma once

#include "games/Game2048Engine.h"

#include <cstdint>

namespace pgos {

struct Game2048AiDecision {
    bool valid = false;
    Game2048Direction direction = Game2048Direction::Left;
    float score = 0.0F;
    uint32_t evaluatedNodes = 0;
    uint8_t evaluatedDirections = 0;
};

class Game2048Ai final {
public:
    static constexpr uint32_t NODE_BUDGET = 12000;
    static constexpr uint8_t SEARCH_DEPTH = 3;

    Game2048AiDecision chooseMove(const uint32_t* cells);

private:
    struct Board {
        uint32_t cells[Game2048Engine::CELL_COUNT] = {};
    };

    uint32_t evaluatedNodes_ = 0;
    uint32_t directionNodes_ = 0;

    static constexpr uint32_t DIRECTION_NODE_BUDGET = NODE_BUDGET / 4U;

    bool consumeNode();

    float scoreMaxNode(const Board& board, uint8_t depth,
                       float cumulativeProbability);
    float scoreChanceNode(const Board& board, uint8_t depth,
                          float cumulativeProbability);
    static float heuristic(const Board& board);
    static bool executeMove(const Board& board, Game2048Direction direction,
                            Board& result);
    static uint8_t rankOf(uint32_t value);
    static uint8_t lineIndex(Game2048Direction direction, uint8_t line,
                             uint8_t offset);
};

}  // namespace pgos
