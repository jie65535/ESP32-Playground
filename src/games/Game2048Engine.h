#pragma once

#include <cstddef>
#include <cstdint>

namespace pgos {

enum class Game2048Direction : uint8_t { Up, Down, Left, Right };

struct Game2048Motion {
    uint8_t from = 0;
    uint8_t to = 0;
    uint32_t value = 0;
    bool merged = false;
};

struct Game2048MoveResult {
    bool moved = false;
    bool reachedTarget = false;
    bool gameOver = false;
    uint8_t motionCount = 0;
    uint8_t spawnIndex = 0xFF;
    uint32_t spawnValue = 0;
    uint32_t scoreDelta = 0;
    Game2048Motion motions[16] = {};
};

class Game2048Engine final {
public:
    static constexpr uint8_t SIZE = 4;
    static constexpr uint8_t CELL_COUNT = SIZE * SIZE;
    static constexpr uint32_t TARGET = 2048;

    void reset(uint32_t seed);
    void start();
    Game2048MoveResult move(Game2048Direction direction);

    uint32_t tile(uint8_t x, uint8_t y) const;
    uint32_t tileAt(uint8_t index) const;
    uint32_t score() const;
    uint32_t bestTile() const;
    bool hasReachedTarget() const;
    bool isGameOver() const;
    uint8_t populatedCount() const;
    const uint32_t* board() const;

#if defined(PGOS_2048_TESTING)
    bool loadBoardForTesting(const uint32_t* values, uint32_t score = 0);
    void setRandomStateForTesting(uint32_t state);
#endif

private:
    uint32_t board_[CELL_COUNT] = {};
    uint32_t score_ = 0;
    uint32_t bestTile_ = 0;
    uint32_t randomState_ = 0x2048C0DEU;
    bool reachedTarget_ = false;
    bool started_ = false;

    uint32_t nextRandom();
    bool spawnOne(Game2048MoveResult& result);
    bool hasMoves() const;
    static uint8_t indexOf(uint8_t x, uint8_t y) {
        return static_cast<uint8_t>(y * SIZE + x);
    }
};

}  // namespace pgos
