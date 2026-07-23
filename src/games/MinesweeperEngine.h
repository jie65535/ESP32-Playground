#pragma once

#include <cstddef>
#include <cstdint>

namespace pgos {

struct MinesweeperConfig {
    uint8_t width = 9;
    uint8_t height = 9;
    uint16_t mines = 10;
};

enum class MinesweeperPhase : uint8_t {
    Ready,
    Generating,
    Playing,
    Won,
    Lost,
};

enum class MinesweeperVisibility : uint8_t {
    Covered,
    Flagged,
    Revealed,
};

struct MinesweeperCell {
    MinesweeperVisibility visibility = MinesweeperVisibility::Covered;
    uint8_t adjacentMines = 0;
    bool mine = false;
    bool detonated = false;
};

struct MinesweeperActionResult {
    bool accepted = false;
    bool flagChanged = false;
    bool hitMine = false;
    bool won = false;
    uint16_t revealed = 0;
    uint16_t flagged = 0;
};

struct MinesweeperVisibleCell {
    MinesweeperVisibility visibility = MinesweeperVisibility::Covered;
    uint8_t adjacentMines = 0;
};

// A deliberately content-blind deduction pass. It receives only what the
// player can see, so assist actions cannot inspect the hidden mine bitmap.
class MinesweeperDeduction final {
public:
    static constexpr uint16_t MAX_CELLS = 30U * 16U;

    bool analyze(const MinesweeperConfig& config,
                 const MinesweeperVisibleCell* cells);
    bool knownMine(uint16_t index) const;
    bool knownSafe(uint16_t index) const;

private:
    enum State : uint8_t {
        Unknown,
        Safe,
        Mine,
    };

    struct Constraint {
        uint16_t cells[8] = {};
        uint8_t count = 0;
        uint8_t mines = 0;
    };

    MinesweeperConfig config_{};
    uint16_t cellCount_ = 0;
    uint8_t state_[MAX_CELLS] = {};
    Constraint constraints_[MAX_CELLS] = {};
    uint16_t constraintCount_ = 0;

    bool inBounds(int16_t x, int16_t y) const;
    bool mark(uint16_t index, State state);
    bool applyGlobal(bool& progress);
    bool applyLocal(const MinesweeperVisibleCell* cells, bool& progress);
    bool buildConstraints(const MinesweeperVisibleCell* cells);
    bool applySubsets(bool& progress);
    static bool isSubset(const Constraint& subset,
                         const Constraint& superset);
};

struct MinesweeperSolverStats {
    uint16_t revealedSafe = 0;
    uint16_t markedMines = 0;
    uint16_t deductionPasses = 0;
    uint16_t subsetDeductions = 0;
};

class MinesweeperSolver final {
public:
    static constexpr uint16_t MAX_CELLS = 30U * 16U;

    bool isSolvable(const uint8_t* content, uint8_t width, uint8_t height,
                    uint16_t mineCount, uint16_t firstIndex,
                    MinesweeperSolverStats* stats = nullptr);

private:
    enum Knowledge : uint8_t {
        Unknown,
        Revealed,
        MarkedMine,
    };

    struct Constraint {
        uint16_t cells[8] = {};
        uint8_t count = 0;
        uint8_t mines = 0;
    };

    Knowledge knowledge_[MAX_CELLS] = {};
    uint16_t queue_[MAX_CELLS] = {};
    Constraint constraints_[MAX_CELLS] = {};
    uint16_t cellCount_ = 0;
    uint16_t revealedSafe_ = 0;
    uint16_t markedMines_ = 0;
    uint16_t constraintCount_ = 0;

    bool revealSafe(const uint8_t* content, uint8_t width, uint8_t height,
                    uint16_t index);
    bool markMine(uint16_t index);
    bool applyLocalDeductions(const uint8_t* content, uint8_t width,
                              uint8_t height, bool& progress);
    bool buildConstraints(const uint8_t* content, uint8_t width,
                          uint8_t height);
    bool applySubsetDeductions(const uint8_t* content, uint8_t width,
                               uint8_t height, bool& progress,
                               uint16_t& deductionCount);
    static bool isSubset(const Constraint& subset,
                         const Constraint& superset);
};

class MinesweeperEngine final {
public:
    static constexpr uint8_t MAX_WIDTH = 30;
    static constexpr uint8_t MAX_HEIGHT = 16;
    static constexpr uint16_t MAX_CELLS = MAX_WIDTH * MAX_HEIGHT;

    static constexpr MinesweeperConfig BEGINNER{9, 9, 10};
    static constexpr MinesweeperConfig INTERMEDIATE{16, 16, 40};
    static constexpr MinesweeperConfig EXPERT{30, 16, 99};

    bool reset(const MinesweeperConfig& config, uint32_t seed);
    bool beginGeneration(uint8_t firstX, uint8_t firstY);
    bool generateStep(uint8_t candidateBudget = 1);

    MinesweeperActionResult reveal(uint8_t x, uint8_t y);
    MinesweeperActionResult toggleFlag(uint8_t x, uint8_t y);
    MinesweeperActionResult flagKnownMines();
    MinesweeperActionResult revealKnownSafe();

    MinesweeperPhase phase() const;
    const MinesweeperConfig& config() const;
    MinesweeperCell cell(uint8_t x, uint8_t y) const;
    uint16_t flagCount() const;
    uint16_t revealedSafeCount() const;
    uint32_t seed() const;
    uint32_t generationAttempts() const;
    const MinesweeperSolverStats& solverStats() const;

    static bool validConfig(const MinesweeperConfig& config);

#if defined(PGOS_MINESWEEPER_TESTING)
    bool loadBoardForTesting(const MinesweeperConfig& config,
                             const uint16_t* mineIndices,
                             uint16_t mineCount);
    const uint8_t* contentForTesting() const;
#endif

private:
    static constexpr uint8_t CONTENT_MINE = 0x80U;
    static constexpr uint8_t CONTENT_ADJACENT_MASK = 0x0FU;

    MinesweeperConfig config_{};
    MinesweeperPhase phase_ = MinesweeperPhase::Ready;
    uint8_t content_[MAX_CELLS] = {};
    MinesweeperVisibility visibility_[MAX_CELLS] = {};
    uint16_t candidatePool_[MAX_CELLS] = {};
    uint16_t floodQueue_[MAX_CELLS] = {};
    MinesweeperSolver solver_{};
    MinesweeperDeduction deduction_{};
    MinesweeperSolverStats solverStats_{};
    uint32_t seed_ = 0;
    uint32_t randomState_ = 0;
    uint32_t generationAttempts_ = 0;
    uint16_t revealedSafeCount_ = 0;
    uint16_t flagCount_ = 0;
    uint16_t firstIndex_ = 0;
    int16_t detonatedIndex_ = -1;

    uint16_t cellCount() const;
    uint16_t indexOf(uint8_t x, uint8_t y) const;
    bool inBounds(int16_t x, int16_t y) const;
    bool createCandidate();
    void computeAdjacentCounts();
    uint16_t revealSafeRegion(uint16_t startIndex);
    MinesweeperActionResult chord(uint16_t index);
    bool analyzeVisibleState();
    void finishIfWon(MinesweeperActionResult& result);
    uint32_t nextRandom();
    static uint32_t mixSeed(uint32_t value);
};

}  // namespace pgos
