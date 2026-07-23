#include "games/MinesweeperEngine.h"

#include <algorithm>
#include <cstring>

namespace pgos {

namespace {

constexpr bool hasMine(uint8_t content) {
    return (content & 0x80U) != 0;
}

constexpr uint8_t adjacentCount(uint8_t content) {
    return content & 0x0FU;
}

}  // namespace

bool MinesweeperDeduction::inBounds(int16_t x, int16_t y) const {
    return x >= 0 && y >= 0 && x < config_.width && y < config_.height;
}

bool MinesweeperDeduction::mark(uint16_t index, State state) {
    if (index >= cellCount_) {
        return false;
    }
    if (state_[index] == Unknown) {
        state_[index] = state;
        return true;
    }
    return state_[index] == state;
}

bool MinesweeperDeduction::applyGlobal(bool& progress) {
    uint16_t unknownCount = 0;
    uint16_t mineCount = 0;
    for (uint16_t index = 0; index < cellCount_; ++index) {
        if (state_[index] == Unknown) {
            ++unknownCount;
        } else if (state_[index] == Mine) {
            ++mineCount;
        }
    }
    if (mineCount > config_.mines ||
        config_.mines - mineCount > unknownCount) {
        return false;
    }
    const uint16_t remaining =
        static_cast<uint16_t>(config_.mines - mineCount);
    if (remaining != 0 && remaining != unknownCount) {
        return true;
    }
    const State target = remaining == 0 ? Safe : Mine;
    for (uint16_t index = 0; index < cellCount_; ++index) {
        if (state_[index] != Unknown) {
            continue;
        }
        if (!mark(index, target)) {
            return false;
        }
        progress = true;
    }
    return true;
}

bool MinesweeperDeduction::applyLocal(const MinesweeperVisibleCell* cells,
                                      bool& progress) {
    for (uint16_t index = 0; index < cellCount_; ++index) {
        if (cells[index].visibility != MinesweeperVisibility::Revealed) {
            continue;
        }
        const int16_t cx = static_cast<int16_t>(index % config_.width);
        const int16_t cy = static_cast<int16_t>(index / config_.width);
        uint16_t unknown[8] = {};
        uint8_t unknownCount = 0;
        uint8_t knownMines = 0;
        for (int16_t dy = -1; dy <= 1; ++dy) {
            for (int16_t dx = -1; dx <= 1; ++dx) {
                if ((dx == 0 && dy == 0) || !inBounds(cx + dx, cy + dy)) {
                    continue;
                }
                const uint16_t neighbor = static_cast<uint16_t>(
                    (cy + dy) * config_.width + (cx + dx));
                if (state_[neighbor] == Mine) {
                    ++knownMines;
                } else if (state_[neighbor] == Unknown) {
                    unknown[unknownCount++] = neighbor;
                }
            }
        }
        const uint8_t number = cells[index].adjacentMines;
        if (knownMines > number ||
            static_cast<uint8_t>(number - knownMines) > unknownCount) {
            return false;
        }
        const uint8_t needed = static_cast<uint8_t>(number - knownMines);
        if (needed != 0 && needed != unknownCount) {
            continue;
        }
        const State target = needed == 0 ? Safe : Mine;
        for (uint8_t i = 0; i < unknownCount; ++i) {
            const bool newlyKnown = state_[unknown[i]] == Unknown;
            if (!mark(unknown[i], target)) {
                return false;
            }
            progress = progress || newlyKnown;
        }
    }
    return true;
}

bool MinesweeperDeduction::buildConstraints(
    const MinesweeperVisibleCell* cells) {
    constraintCount_ = 0;
    for (uint16_t index = 0; index < cellCount_; ++index) {
        if (cells[index].visibility != MinesweeperVisibility::Revealed) {
            continue;
        }
        const int16_t cx = static_cast<int16_t>(index % config_.width);
        const int16_t cy = static_cast<int16_t>(index / config_.width);
        Constraint constraint{};
        for (int16_t dy = -1; dy <= 1; ++dy) {
            for (int16_t dx = -1; dx <= 1; ++dx) {
                if ((dx == 0 && dy == 0) || !inBounds(cx + dx, cy + dy)) {
                    continue;
                }
                const uint16_t neighbor = static_cast<uint16_t>(
                    (cy + dy) * config_.width + (cx + dx));
                if (state_[neighbor] == Mine) {
                    ++constraint.mines;
                } else if (state_[neighbor] == Unknown &&
                           constraint.count < 8) {
                    constraint.cells[constraint.count++] = neighbor;
                }
            }
        }
        if (constraint.mines > cells[index].adjacentMines ||
            cells[index].adjacentMines - constraint.mines >
                constraint.count) {
            return false;
        }
        if (constraint.count == 0 || constraintCount_ >= MAX_CELLS) {
            continue;
        }
        constraint.mines = static_cast<uint8_t>(
            cells[index].adjacentMines - constraint.mines);
        constraints_[constraintCount_++] = constraint;
    }
    return true;
}

bool MinesweeperDeduction::isSubset(const Constraint& subset,
                                    const Constraint& superset) {
    for (uint8_t i = 0; i < subset.count; ++i) {
        bool found = false;
        for (uint8_t j = 0; j < superset.count; ++j) {
            if (subset.cells[i] == superset.cells[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool MinesweeperDeduction::applySubsets(bool& progress) {
    for (uint16_t first = 0; first < constraintCount_; ++first) {
        for (uint16_t second = 0; second < constraintCount_; ++second) {
            if (first == second ||
                constraints_[first].count >= constraints_[second].count ||
                !isSubset(constraints_[first], constraints_[second])) {
                continue;
            }
            uint16_t difference[8] = {};
            uint8_t differenceCount = 0;
            for (uint8_t j = 0; j < constraints_[second].count; ++j) {
                bool present = false;
                for (uint8_t i = 0; i < constraints_[first].count; ++i) {
                    if (constraints_[second].cells[j] ==
                        constraints_[first].cells[i]) {
                        present = true;
                        break;
                    }
                }
                if (!present && differenceCount < 8) {
                    difference[differenceCount++] =
                        constraints_[second].cells[j];
                }
            }
            const int16_t differenceMines =
                static_cast<int16_t>(constraints_[second].mines) -
                constraints_[first].mines;
            if (differenceMines < 0 || differenceMines > differenceCount) {
                return false;
            }
            if (differenceMines != 0 &&
                differenceMines != differenceCount) {
                continue;
            }
            const State target = differenceMines == 0 ? Safe : Mine;
            for (uint8_t i = 0; i < differenceCount; ++i) {
                const bool newlyKnown = state_[difference[i]] == Unknown;
                if (!mark(difference[i], target)) {
                    return false;
                }
                progress = progress || newlyKnown;
            }
            if (progress) {
                return true;
            }
        }
    }
    return true;
}

bool MinesweeperDeduction::analyze(const MinesweeperConfig& config,
                                   const MinesweeperVisibleCell* cells) {
    if (cells == nullptr || config.width == 0 || config.height == 0 ||
        config.width > 30 || config.height > 16) {
        return false;
    }
    config_ = config;
    cellCount_ = static_cast<uint16_t>(config.width) * config.height;
    if (cellCount_ > MAX_CELLS || config.mines > cellCount_) {
        return false;
    }
    std::fill_n(state_, cellCount_, Unknown);
    constraintCount_ = 0;
    for (uint16_t index = 0; index < cellCount_; ++index) {
        if (cells[index].visibility == MinesweeperVisibility::Revealed) {
            state_[index] = Safe;
        }
    }

    for (uint16_t pass = 0; pass < MAX_CELLS; ++pass) {
        bool progress = false;
        if (!applyGlobal(progress) || !applyLocal(cells, progress)) {
            return false;
        }
        if (progress) {
            continue;
        }
        if (!buildConstraints(cells) || !applySubsets(progress)) {
            return false;
        }
        if (!progress) {
            break;
        }
    }
    return true;
}

bool MinesweeperDeduction::knownMine(uint16_t index) const {
    return index < cellCount_ && state_[index] == Mine;
}

bool MinesweeperDeduction::knownSafe(uint16_t index) const {
    return index < cellCount_ && state_[index] == Safe;
}

bool MinesweeperSolver::isSolvable(const uint8_t* content, uint8_t width,
                                   uint8_t height, uint16_t mineCount,
                                   uint16_t firstIndex,
                                   MinesweeperSolverStats* stats) {
    if (content == nullptr || width == 0 || height == 0 ||
        width > 30 || height > 16) {
        return false;
    }
    cellCount_ = static_cast<uint16_t>(width) * height;
    if (firstIndex >= cellCount_ || mineCount >= cellCount_ ||
        hasMine(content[firstIndex])) {
        return false;
    }

    std::fill_n(knowledge_, cellCount_, Unknown);
    revealedSafe_ = 0;
    markedMines_ = 0;
    constraintCount_ = 0;
    MinesweeperSolverStats localStats{};

    if (!revealSafe(content, width, height, firstIndex)) {
        return false;
    }

    const uint16_t safeTarget = static_cast<uint16_t>(cellCount_ - mineCount);
    while (revealedSafe_ < safeTarget) {
        bool progress = false;
        ++localStats.deductionPasses;

        if (markedMines_ > mineCount ||
            revealedSafe_ + markedMines_ > cellCount_) {
            return false;
        }
        const uint16_t unknownCount = static_cast<uint16_t>(
            cellCount_ - revealedSafe_ - markedMines_);
        const uint16_t remainingMines = static_cast<uint16_t>(
            mineCount - markedMines_);
        if (remainingMines == 0) {
            for (uint16_t index = 0; index < cellCount_; ++index) {
                if (knowledge_[index] == Unknown &&
                    !revealSafe(content, width, height, index)) {
                    return false;
                }
            }
            progress = true;
        } else if (remainingMines == unknownCount) {
            for (uint16_t index = 0; index < cellCount_; ++index) {
                if (knowledge_[index] == Unknown) {
                    progress = markMine(index) || progress;
                }
            }
        }

        if (!applyLocalDeductions(content, width, height, progress)) {
            return false;
        }
        if (!buildConstraints(content, width, height) ||
            !applySubsetDeductions(content, width, height, progress,
                                   localStats.subsetDeductions)) {
            return false;
        }
        if (!progress) {
            break;
        }
        if (localStats.deductionPasses >= MAX_CELLS) {
            return false;
        }
    }

    localStats.revealedSafe = revealedSafe_;
    localStats.markedMines = markedMines_;
    if (stats != nullptr) {
        *stats = localStats;
    }
    if (revealedSafe_ != safeTarget) {
        return false;
    }
    // Truth is used only to validate the completed proof, never to choose a
    // deduction. This catches implementation mistakes without certifying a
    // board through hidden information.
    for (uint16_t index = 0; index < cellCount_; ++index) {
        if ((knowledge_[index] == Revealed && hasMine(content[index])) ||
            (knowledge_[index] == MarkedMine && !hasMine(content[index]))) {
            return false;
        }
    }
    return true;
}

bool MinesweeperSolver::revealSafe(const uint8_t* content, uint8_t width,
                                   uint8_t height, uint16_t index) {
    if (index >= cellCount_ || hasMine(content[index])) {
        return false;
    }
    if (knowledge_[index] == Revealed) {
        return true;
    }
    if (knowledge_[index] == MarkedMine) {
        return false;
    }

    uint16_t head = 0;
    uint16_t tail = 0;
    knowledge_[index] = Revealed;
    queue_[tail++] = index;
    ++revealedSafe_;

    while (head < tail) {
        const uint16_t current = queue_[head++];
        if (adjacentCount(content[current]) != 0) {
            continue;
        }
        const int16_t cx = static_cast<int16_t>(current % width);
        const int16_t cy = static_cast<int16_t>(current / width);
        for (int16_t dy = -1; dy <= 1; ++dy) {
            for (int16_t dx = -1; dx <= 1; ++dx) {
                const int16_t nx = cx + dx;
                const int16_t ny = cy + dy;
                if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 ||
                    nx >= width || ny >= height) {
                    continue;
                }
                const uint16_t neighbor =
                    static_cast<uint16_t>(ny * width + nx);
                if (knowledge_[neighbor] == Unknown &&
                    !hasMine(content[neighbor])) {
                    knowledge_[neighbor] = Revealed;
                    queue_[tail++] = neighbor;
                    ++revealedSafe_;
                }
            }
        }
    }
    return true;
}

bool MinesweeperSolver::markMine(uint16_t index) {
    if (index >= cellCount_ || knowledge_[index] != Unknown) {
        return false;
    }
    knowledge_[index] = MarkedMine;
    ++markedMines_;
    return true;
}

bool MinesweeperSolver::applyLocalDeductions(const uint8_t* content,
                                             uint8_t width, uint8_t height,
                                             bool& progress) {
    for (uint16_t index = 0; index < cellCount_; ++index) {
        if (knowledge_[index] != Revealed) {
            continue;
        }
        const uint8_t number = adjacentCount(content[index]);
        if (number == 0) {
            continue;
        }
        const int16_t cx = static_cast<int16_t>(index % width);
        const int16_t cy = static_cast<int16_t>(index / width);
        uint16_t unknown[8] = {};
        uint8_t unknownCount = 0;
        uint8_t knownMines = 0;
        for (int16_t dy = -1; dy <= 1; ++dy) {
            for (int16_t dx = -1; dx <= 1; ++dx) {
                const int16_t nx = cx + dx;
                const int16_t ny = cy + dy;
                if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 ||
                    nx >= width || ny >= height) {
                    continue;
                }
                const uint16_t neighbor =
                    static_cast<uint16_t>(ny * width + nx);
                if (knowledge_[neighbor] == MarkedMine) {
                    ++knownMines;
                } else if (knowledge_[neighbor] == Unknown) {
                    unknown[unknownCount++] = neighbor;
                }
            }
        }
        if (knownMines > number || number - knownMines > unknownCount) {
            return false;
        }
        const uint8_t needed = static_cast<uint8_t>(number - knownMines);
        if (needed == 0) {
            for (uint8_t i = 0; i < unknownCount; ++i) {
                if (!revealSafe(content, width, height, unknown[i])) {
                    return false;
                }
                progress = true;
            }
        } else if (needed == unknownCount) {
            for (uint8_t i = 0; i < unknownCount; ++i) {
                progress = markMine(unknown[i]) || progress;
            }
        }
    }
    return true;
}

bool MinesweeperSolver::buildConstraints(const uint8_t* content,
                                         uint8_t width, uint8_t height) {
    constraintCount_ = 0;
    for (uint16_t index = 0; index < cellCount_; ++index) {
        if (knowledge_[index] != Revealed ||
            adjacentCount(content[index]) == 0) {
            continue;
        }
        Constraint constraint{};
        uint8_t knownMines = 0;
        const int16_t cx = static_cast<int16_t>(index % width);
        const int16_t cy = static_cast<int16_t>(index / width);
        for (int16_t dy = -1; dy <= 1; ++dy) {
            for (int16_t dx = -1; dx <= 1; ++dx) {
                const int16_t nx = cx + dx;
                const int16_t ny = cy + dy;
                if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 ||
                    nx >= width || ny >= height) {
                    continue;
                }
                const uint16_t neighbor =
                    static_cast<uint16_t>(ny * width + nx);
                if (knowledge_[neighbor] == MarkedMine) {
                    ++knownMines;
                } else if (knowledge_[neighbor] == Unknown) {
                    constraint.cells[constraint.count++] = neighbor;
                }
            }
        }
        const uint8_t number = adjacentCount(content[index]);
        if (knownMines > number || number - knownMines > constraint.count) {
            return false;
        }
        if (constraint.count == 0) {
            continue;
        }
        constraint.mines = static_cast<uint8_t>(number - knownMines);
        constraints_[constraintCount_++] = constraint;
    }
    return true;
}

bool MinesweeperSolver::applySubsetDeductions(
    const uint8_t* content, uint8_t width, uint8_t height, bool& progress,
    uint16_t& deductionCount) {
    for (uint16_t first = 0; first < constraintCount_; ++first) {
        for (uint16_t second = 0; second < constraintCount_; ++second) {
            if (first == second ||
                constraints_[first].count >= constraints_[second].count ||
                !isSubset(constraints_[first], constraints_[second])) {
                continue;
            }
            const Constraint& subset = constraints_[first];
            const Constraint& superset = constraints_[second];
            if (subset.mines > superset.mines) {
                return false;
            }
            const uint8_t differenceMines =
                static_cast<uint8_t>(superset.mines - subset.mines);
            const uint8_t differenceCount =
                static_cast<uint8_t>(superset.count - subset.count);
            if (differenceMines != 0 &&
                differenceMines != differenceCount) {
                continue;
            }
            bool changed = false;
            for (uint8_t i = 0; i < superset.count; ++i) {
                bool inSubset = false;
                for (uint8_t j = 0; j < subset.count; ++j) {
                    inSubset |= superset.cells[i] == subset.cells[j];
                }
                if (inSubset || knowledge_[superset.cells[i]] != Unknown) {
                    continue;
                }
                if (differenceMines == 0) {
                    if (!revealSafe(content, width, height,
                                    superset.cells[i])) {
                        return false;
                    }
                } else {
                    markMine(superset.cells[i]);
                }
                progress = true;
                changed = true;
                ++deductionCount;
            }
            // The constraint list is now stale. Rebuild it before making the
            // next subset comparison.
            if (changed) {
                return true;
            }
        }
    }
    return true;
}

bool MinesweeperSolver::isSubset(const Constraint& subset,
                                 const Constraint& superset) {
    for (uint8_t i = 0; i < subset.count; ++i) {
        bool found = false;
        for (uint8_t j = 0; j < superset.count; ++j) {
            found |= subset.cells[i] == superset.cells[j];
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool MinesweeperEngine::reset(const MinesweeperConfig& config,
                              uint32_t seed) {
    if (!validConfig(config)) {
        return false;
    }
    config_ = config;
    phase_ = MinesweeperPhase::Ready;
    std::fill_n(content_, cellCount(), 0);
    std::fill_n(visibility_, cellCount(), MinesweeperVisibility::Covered);
    solverStats_ = {};
    seed_ = seed == 0 ? 0x4D494E45U : seed;
    randomState_ = mixSeed(seed_);
    generationAttempts_ = 0;
    revealedSafeCount_ = 0;
    flagCount_ = 0;
    firstIndex_ = 0;
    detonatedIndex_ = -1;
    return true;
}

bool MinesweeperEngine::beginGeneration(uint8_t firstX, uint8_t firstY) {
    if (phase_ != MinesweeperPhase::Ready || !inBounds(firstX, firstY)) {
        return false;
    }
    firstIndex_ = indexOf(firstX, firstY);
    if (visibility_[firstIndex_] == MinesweeperVisibility::Flagged) {
        return false;
    }
    randomState_ = mixSeed(seed_ ^ (static_cast<uint32_t>(firstIndex_) + 1U) *
                                      0x9E3779B9U);
    generationAttempts_ = 0;
    solverStats_ = {};
    phase_ = MinesweeperPhase::Generating;
    return true;
}

bool MinesweeperEngine::generateStep(uint8_t candidateBudget) {
    if (phase_ != MinesweeperPhase::Generating || candidateBudget == 0) {
        return phase_ == MinesweeperPhase::Playing ||
               phase_ == MinesweeperPhase::Won;
    }
    for (uint8_t candidate = 0; candidate < candidateBudget; ++candidate) {
        ++generationAttempts_;
        if (!createCandidate()) {
            continue;
        }
        MinesweeperSolverStats stats{};
        if (!solver_.isSolvable(content_, config_.width, config_.height,
                                config_.mines, firstIndex_, &stats)) {
            continue;
        }
        solverStats_ = stats;
        phase_ = MinesweeperPhase::Playing;
        MinesweeperActionResult result{};
        result.accepted = true;
        result.revealed = revealSafeRegion(firstIndex_);
        finishIfWon(result);
        return true;
    }
    return false;
}

MinesweeperActionResult MinesweeperEngine::reveal(uint8_t x, uint8_t y) {
    MinesweeperActionResult result{};
    if (phase_ != MinesweeperPhase::Playing || !inBounds(x, y)) {
        return result;
    }
    const uint16_t index = indexOf(x, y);
    if (visibility_[index] == MinesweeperVisibility::Flagged) {
        return result;
    }
    if (visibility_[index] == MinesweeperVisibility::Revealed) {
        return chord(index);
    }

    result.accepted = true;
    if (hasMine(content_[index])) {
        phase_ = MinesweeperPhase::Lost;
        detonatedIndex_ = static_cast<int16_t>(index);
        visibility_[index] = MinesweeperVisibility::Revealed;
        result.hitMine = true;
        return result;
    }
    result.revealed = revealSafeRegion(index);
    finishIfWon(result);
    return result;
}

MinesweeperActionResult MinesweeperEngine::toggleFlag(uint8_t x, uint8_t y) {
    MinesweeperActionResult result{};
    if ((phase_ != MinesweeperPhase::Ready &&
         phase_ != MinesweeperPhase::Playing) ||
        !inBounds(x, y)) {
        return result;
    }
    const uint16_t index = indexOf(x, y);
    if (visibility_[index] == MinesweeperVisibility::Revealed) {
        return result;
    }
    result.accepted = true;
    result.flagChanged = true;
    if (visibility_[index] == MinesweeperVisibility::Flagged) {
        visibility_[index] = MinesweeperVisibility::Covered;
        --flagCount_;
    } else {
        visibility_[index] = MinesweeperVisibility::Flagged;
        ++flagCount_;
    }
    return result;
}

MinesweeperActionResult MinesweeperEngine::flagKnownMines() {
    MinesweeperActionResult result{};
    if (phase_ != MinesweeperPhase::Playing || !analyzeVisibleState()) {
        return result;
    }

    for (uint16_t index = 0; index < cellCount(); ++index) {
        if (visibility_[index] != MinesweeperVisibility::Covered ||
            !deduction_.knownMine(index)) {
            continue;
        }
        visibility_[index] = MinesweeperVisibility::Flagged;
        ++flagCount_;
        ++result.flagged;
    }
    result.accepted = result.flagged != 0;
    result.flagChanged = result.accepted;
    return result;
}

MinesweeperActionResult MinesweeperEngine::revealKnownSafe() {
    MinesweeperActionResult result{};
    if (phase_ != MinesweeperPhase::Playing || !analyzeVisibleState()) {
        return result;
    }

    uint16_t candidateCount = 0;
    for (uint16_t index = 0; index < cellCount(); ++index) {
        if (visibility_[index] == MinesweeperVisibility::Covered &&
            deduction_.knownSafe(index)) {
            candidatePool_[candidateCount++] = index;
        }
    }
    if (candidateCount == 0) {
        return result;
    }

    result.accepted = true;
    for (uint16_t candidate = 0; candidate < candidateCount; ++candidate) {
        const uint16_t index = candidatePool_[candidate];
        if (visibility_[index] != MinesweeperVisibility::Covered) {
            continue;
        }
        if (hasMine(content_[index])) {
            visibility_[index] = MinesweeperVisibility::Revealed;
            detonatedIndex_ = static_cast<int16_t>(index);
            phase_ = MinesweeperPhase::Lost;
            result.hitMine = true;
            break;
        }
        result.revealed = static_cast<uint16_t>(
            result.revealed + revealSafeRegion(index));
    }
    if (!result.hitMine) {
        finishIfWon(result);
    }
    return result;
}

MinesweeperPhase MinesweeperEngine::phase() const { return phase_; }

const MinesweeperConfig& MinesweeperEngine::config() const { return config_; }

MinesweeperCell MinesweeperEngine::cell(uint8_t x, uint8_t y) const {
    if (!inBounds(x, y)) {
        return {};
    }
    const uint16_t index = indexOf(x, y);
    return {visibility_[index], adjacentCount(content_[index]),
            hasMine(content_[index]),
            detonatedIndex_ == static_cast<int16_t>(index)};
}

uint16_t MinesweeperEngine::flagCount() const { return flagCount_; }

uint16_t MinesweeperEngine::revealedSafeCount() const {
    return revealedSafeCount_;
}

uint32_t MinesweeperEngine::seed() const { return seed_; }

uint32_t MinesweeperEngine::generationAttempts() const {
    return generationAttempts_;
}

const MinesweeperSolverStats& MinesweeperEngine::solverStats() const {
    return solverStats_;
}

bool MinesweeperEngine::validConfig(const MinesweeperConfig& config) {
    if (config.width < 5 || config.height < 5 ||
        config.width > MAX_WIDTH || config.height > MAX_HEIGHT) {
        return false;
    }
    const uint16_t cells = static_cast<uint16_t>(config.width) * config.height;
    return config.mines > 0 && config.mines <= cells - 9U;
}

#if defined(PGOS_MINESWEEPER_TESTING)
bool MinesweeperEngine::loadBoardForTesting(const MinesweeperConfig& config,
                                            const uint16_t* mineIndices,
                                            uint16_t mineCount) {
    if (!validConfig(config) || mineIndices == nullptr ||
        mineCount != config.mines) {
        return false;
    }
    if (!reset(config, 1)) {
        return false;
    }
    for (uint16_t i = 0; i < mineCount; ++i) {
        if (mineIndices[i] >= cellCount() ||
            hasMine(content_[mineIndices[i]])) {
            return false;
        }
        content_[mineIndices[i]] = CONTENT_MINE;
    }
    computeAdjacentCounts();
    phase_ = MinesweeperPhase::Playing;
    return true;
}

const uint8_t* MinesweeperEngine::contentForTesting() const {
    return content_;
}
#endif

uint16_t MinesweeperEngine::cellCount() const {
    return static_cast<uint16_t>(config_.width) * config_.height;
}

uint16_t MinesweeperEngine::indexOf(uint8_t x, uint8_t y) const {
    return static_cast<uint16_t>(y) * config_.width + x;
}

bool MinesweeperEngine::inBounds(int16_t x, int16_t y) const {
    return x >= 0 && y >= 0 && x < config_.width && y < config_.height;
}

bool MinesweeperEngine::analyzeVisibleState() {
    MinesweeperVisibleCell visible[MAX_CELLS] = {};
    for (uint16_t index = 0; index < cellCount(); ++index) {
        visible[index].visibility = visibility_[index];
        if (visibility_[index] == MinesweeperVisibility::Revealed) {
            visible[index].adjacentMines = adjacentCount(content_[index]);
        }
    }
    return deduction_.analyze(config_, visible);
}

bool MinesweeperEngine::createCandidate() {
    std::fill_n(content_, cellCount(), 0);
    const int16_t firstX = static_cast<int16_t>(firstIndex_ % config_.width);
    const int16_t firstY = static_cast<int16_t>(firstIndex_ / config_.width);
    uint16_t eligibleCount = 0;
    for (uint16_t index = 0; index < cellCount(); ++index) {
        const int16_t x = static_cast<int16_t>(index % config_.width);
        const int16_t y = static_cast<int16_t>(index / config_.width);
        if (x >= firstX - 1 && x <= firstX + 1 &&
            y >= firstY - 1 && y <= firstY + 1) {
            continue;
        }
        candidatePool_[eligibleCount++] = index;
    }
    if (config_.mines > eligibleCount) {
        return false;
    }
    for (uint16_t i = 0; i < config_.mines; ++i) {
        const uint16_t pick = static_cast<uint16_t>(
            i + nextRandom() % (eligibleCount - i));
        std::swap(candidatePool_[i], candidatePool_[pick]);
        content_[candidatePool_[i]] = CONTENT_MINE;
    }
    computeAdjacentCounts();
    return true;
}

void MinesweeperEngine::computeAdjacentCounts() {
    for (uint16_t index = 0; index < cellCount(); ++index) {
        if (hasMine(content_[index])) {
            continue;
        }
        const int16_t cx = static_cast<int16_t>(index % config_.width);
        const int16_t cy = static_cast<int16_t>(index / config_.width);
        uint8_t count = 0;
        for (int16_t dy = -1; dy <= 1; ++dy) {
            for (int16_t dx = -1; dx <= 1; ++dx) {
                if ((dx != 0 || dy != 0) && inBounds(cx + dx, cy + dy)) {
                    count += hasMine(content_[indexOf(
                        static_cast<uint8_t>(cx + dx),
                        static_cast<uint8_t>(cy + dy))]);
                }
            }
        }
        content_[index] = count & CONTENT_ADJACENT_MASK;
    }
}

uint16_t MinesweeperEngine::revealSafeRegion(uint16_t startIndex) {
    if (startIndex >= cellCount() || hasMine(content_[startIndex]) ||
        visibility_[startIndex] == MinesweeperVisibility::Flagged) {
        return 0;
    }
    if (visibility_[startIndex] == MinesweeperVisibility::Revealed) {
        return 0;
    }
    uint16_t head = 0;
    uint16_t tail = 0;
    uint16_t revealed = 0;
    visibility_[startIndex] = MinesweeperVisibility::Revealed;
    floodQueue_[tail++] = startIndex;
    ++revealed;
    ++revealedSafeCount_;

    while (head < tail) {
        const uint16_t current = floodQueue_[head++];
        if (adjacentCount(content_[current]) != 0) {
            continue;
        }
        const int16_t cx = static_cast<int16_t>(current % config_.width);
        const int16_t cy = static_cast<int16_t>(current / config_.width);
        for (int16_t dy = -1; dy <= 1; ++dy) {
            for (int16_t dx = -1; dx <= 1; ++dx) {
                if ((dx == 0 && dy == 0) || !inBounds(cx + dx, cy + dy)) {
                    continue;
                }
                const uint16_t neighbor = indexOf(
                    static_cast<uint8_t>(cx + dx),
                    static_cast<uint8_t>(cy + dy));
                if (visibility_[neighbor] == MinesweeperVisibility::Covered &&
                    !hasMine(content_[neighbor])) {
                    visibility_[neighbor] = MinesweeperVisibility::Revealed;
                    floodQueue_[tail++] = neighbor;
                    ++revealed;
                    ++revealedSafeCount_;
                }
            }
        }
    }
    return revealed;
}

MinesweeperActionResult MinesweeperEngine::chord(uint16_t index) {
    MinesweeperActionResult result{};
    const uint8_t number = adjacentCount(content_[index]);
    if (number == 0) {
        return result;
    }
    const int16_t cx = static_cast<int16_t>(index % config_.width);
    const int16_t cy = static_cast<int16_t>(index / config_.width);
    uint8_t adjacentFlags = 0;
    for (int16_t dy = -1; dy <= 1; ++dy) {
        for (int16_t dx = -1; dx <= 1; ++dx) {
            if ((dx == 0 && dy == 0) || !inBounds(cx + dx, cy + dy)) {
                continue;
            }
            adjacentFlags += visibility_[indexOf(
                static_cast<uint8_t>(cx + dx),
                static_cast<uint8_t>(cy + dy))] ==
                MinesweeperVisibility::Flagged;
        }
    }
    if (adjacentFlags != number) {
        return result;
    }
    result.accepted = true;
    for (int16_t dy = -1; dy <= 1; ++dy) {
        for (int16_t dx = -1; dx <= 1; ++dx) {
            if ((dx == 0 && dy == 0) || !inBounds(cx + dx, cy + dy)) {
                continue;
            }
            const uint16_t neighbor = indexOf(
                static_cast<uint8_t>(cx + dx),
                static_cast<uint8_t>(cy + dy));
            if (visibility_[neighbor] != MinesweeperVisibility::Covered) {
                continue;
            }
            if (hasMine(content_[neighbor])) {
                if (!result.hitMine) {
                    detonatedIndex_ = static_cast<int16_t>(neighbor);
                }
                visibility_[neighbor] = MinesweeperVisibility::Revealed;
                result.hitMine = true;
            } else {
                result.revealed = static_cast<uint16_t>(
                    result.revealed + revealSafeRegion(neighbor));
            }
        }
    }
    if (result.hitMine) {
        phase_ = MinesweeperPhase::Lost;
    } else {
        finishIfWon(result);
    }
    return result;
}

void MinesweeperEngine::finishIfWon(MinesweeperActionResult& result) {
    if (revealedSafeCount_ == cellCount() - config_.mines) {
        phase_ = MinesweeperPhase::Won;
        result.won = true;
    }
}

uint32_t MinesweeperEngine::nextRandom() {
    uint32_t value = randomState_;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    randomState_ = value == 0 ? 0x4D494E45U : value;
    return randomState_;
}

uint32_t MinesweeperEngine::mixSeed(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352DU;
    value ^= value >> 15;
    value *= 0x846CA68BU;
    value ^= value >> 16;
    return value == 0 ? 0x4D494E45U : value;
}

}  // namespace pgos
