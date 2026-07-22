#include "games/PlatformerLevelRuntime.h"

#include "games/PlatformerTileAssets.h"

#include <algorithm>
#include <cmath>

namespace pgos {
namespace {

PlatformerRuntimeTileKind classifyReference(uint16_t reference) {
    switch (reference) {
        case 101:
        case 149:
            return PlatformerRuntimeTileKind::FlagPole;
        case 144:
            return PlatformerRuntimeTileKind::Coin;
        case 152:
            return PlatformerRuntimeTileKind::Flag;
        case 192:
            return PlatformerRuntimeTileKind::Question;
        case 240:
            return PlatformerRuntimeTileKind::Axe;
        case 289:
        case 290:
            return PlatformerRuntimeTileKind::Brick;
        case 346:
            return PlatformerRuntimeTileKind::Trampoline;
        case 761:
        case 809:
            return PlatformerRuntimeTileKind::MovingPlatform;
        case 857:
            return PlatformerRuntimeTileKind::CloudPlatform;
        case 394:
        case 762:
        case 810:
        case 811:
        case 858:
        case 859:
            return PlatformerRuntimeTileKind::Empty;
        default:
            return PlatformerRuntimeTileKind::Solid;
    }
}

bool kindIsSolid(PlatformerRuntimeTileKind kind) {
    switch (kind) {
        case PlatformerRuntimeTileKind::Solid:
        case PlatformerRuntimeTileKind::Question:
        case PlatformerRuntimeTileKind::Brick:
        case PlatformerRuntimeTileKind::Trampoline:
            return true;
        case PlatformerRuntimeTileKind::Empty:
        case PlatformerRuntimeTileKind::Coin:
        case PlatformerRuntimeTileKind::Hidden:
        case PlatformerRuntimeTileKind::FlagPole:
        case PlatformerRuntimeTileKind::Flag:
        case PlatformerRuntimeTileKind::Axe:
        case PlatformerRuntimeTileKind::MovingPlatform:
        case PlatformerRuntimeTileKind::CloudPlatform:
            return false;
    }
    return false;
}

}  // namespace

bool PlatformerLevelRuntime::load(uint8_t world, uint8_t stage) {
    level_ = platformerCampaignLevel(world, stage);
    scanTrampolines();
    scanAnimatedTiles();
    resetChanges();
    return level_ != nullptr;
}

void PlatformerLevelRuntime::resetChanges() {
    modificationCount_ = 0;
    animationFrame_ = 0U;
    for (uint8_t index = 0; index < animatedTileCount_; ++index) {
        animatedTiles_[index].ageFrames = 0U;
    }
    for (auto& change : modifications_) {
        change = PlatformerTileModification{};
    }
    for (auto& bump : blockBumps_) {
        bump = PlatformerBlockBumpState{};
    }
    for (uint8_t index = 0; index < trampolineCount_; ++index) {
        trampolines_[index].sequenceIndex = 0U;
        trampolines_[index].visualState = 0U;
        trampolines_[index].activated = false;
    }
    if (level_ != nullptr) {
        activeLevelType_ = level_->levelType;
        activeBackground_ = level_->background;
    } else {
        activeLevelType_ = PlatformerLevelType::None;
        activeBackground_ = PlatformerBackgroundColor::Black;
    }
}

const PlatformerCampaignLevel* PlatformerLevelRuntime::level() const {
    return level_;
}

uint8_t PlatformerLevelRuntime::world() const {
    return level_ == nullptr ? 0U : level_->world;
}

uint8_t PlatformerLevelRuntime::stage() const {
    return level_ == nullptr ? 0U : level_->stage;
}

uint16_t PlatformerLevelRuntime::widthPixels() const {
    return level_ == nullptr
               ? 0U
               : static_cast<uint16_t>(level_->width * TILE_SIZE);
}

uint16_t PlatformerLevelRuntime::heightPixels() const {
    return level_ == nullptr
               ? 0U
               : static_cast<uint16_t>(level_->height * TILE_SIZE);
}

PlatformerLevelType PlatformerLevelRuntime::activeLevelType() const {
    return activeLevelType_;
}

PlatformerBackgroundColor PlatformerLevelRuntime::activeBackground() const {
    return activeBackground_;
}

void PlatformerLevelRuntime::setSection(
    PlatformerLevelType levelType, PlatformerBackgroundColor background) {
    activeLevelType_ = levelType;
    activeBackground_ = background;
}

PlatformerRuntimeReward PlatformerLevelRuntime::rewardAt(uint16_t column,
                                                          uint8_t row) const {
    if (level_ == nullptr) {
        return PlatformerRuntimeReward::None;
    }
    const uint16_t source = platformerCampaignTileAt(
        *level_, PlatformerMapLayer::Collectibles, column, row);
    if (source == PLATFORMER_EMPTY_TILE ||
        source >= PLATFORMER_BLOCK_TILE_COUNT) {
        return PlatformerRuntimeReward::None;
    }
    switch (PLATFORMER_BLOCK_REFERENCE_IDS[source]) {
        case 52:
            return PlatformerRuntimeReward::OneUp;
        case 96:
            return PlatformerRuntimeReward::Star;
        case 144:
            return PlatformerRuntimeReward::Coin;
        case 148:
            return PlatformerRuntimeReward::Vine;
        case 608:
            return PlatformerRuntimeReward::Mushroom;
        default:
            return PlatformerRuntimeReward::None;
    }
}

PlatformerRuntimeTile PlatformerLevelRuntime::tileFromLayer(
    PlatformerMapLayer layer, uint16_t column, uint8_t row) const {
    PlatformerRuntimeTile result;
    result.layer = layer;
    if (level_ == nullptr || column >= level_->width || row >= level_->height) {
        return result;
    }
    result.sourceId = platformerCampaignTileAt(*level_, layer, column, row);
    if (result.sourceId == PLATFORMER_EMPTY_TILE) {
        if (layer == PlatformerMapLayer::Foreground &&
            rewardAt(column, row) != PlatformerRuntimeReward::None) {
            result.kind = PlatformerRuntimeTileKind::Hidden;
            result.reward = rewardAt(column, row);
        }
        return result;
    }
    if (result.sourceId >= PLATFORMER_BLOCK_TILE_COUNT) {
        return result;
    }
    const uint16_t reference = PLATFORMER_BLOCK_REFERENCE_IDS[result.sourceId];
    result.kind = classifyReference(reference);
    result.reward = rewardAt(column, row);
    if (result.kind == PlatformerRuntimeTileKind::Question &&
        result.reward == PlatformerRuntimeReward::None) {
        result.reward = PlatformerRuntimeReward::Coin;
    }
    return result;
}

PlatformerRuntimeTile PlatformerLevelRuntime::tile(uint16_t column,
                                                    uint8_t row) const {
    PlatformerRuntimeTile foreground = tileFromLayer(
        PlatformerMapLayer::Foreground, column, row);
    if (foreground.kind != PlatformerRuntimeTileKind::Empty) {
        return foreground;
    }
    PlatformerRuntimeTile underground = tileFromLayer(
        PlatformerMapLayer::Underground, column, row);
    if (underground.kind != PlatformerRuntimeTileKind::Empty) {
        return underground;
    }
    return foreground;
}

const PlatformerTileModification* PlatformerLevelRuntime::modificationAt(
    uint16_t column, uint8_t row) const {
    for (uint8_t index = 0; index < modificationCount_; ++index) {
        if (modifications_[index].column == column &&
            modifications_[index].row == row) {
            return &modifications_[index];
        }
    }
    return nullptr;
}

bool PlatformerLevelRuntime::isSolid(uint16_t column, uint8_t row) const {
    const PlatformerTileModification* change = modificationAt(column, row);
    if (change != nullptr) {
        return change->state == PlatformerTileModificationState::Used;
    }
    for (uint8_t index = 0; index < trampolineCount_; ++index) {
        const PlatformerTrampolineRuntimeState& trampoline =
            trampolines_[index];
        if (trampoline.activated && trampoline.column == column &&
            trampoline.row == row) {
            return false;
        }
    }
    return kindIsSolid(tile(column, row).kind);
}

bool PlatformerLevelRuntime::rectHitsSolid(float x, float y, float width,
                                           float height) const {
    if (level_ == nullptr || width <= 0.0F || height <= 0.0F) {
        return false;
    }
    const int32_t firstColumn = static_cast<int32_t>(std::floor(x / TILE_SIZE));
    const int32_t lastColumn = static_cast<int32_t>(
        std::floor((x + width - 0.01F) / TILE_SIZE));
    const int32_t firstRow = static_cast<int32_t>(std::floor(y / TILE_SIZE));
    const int32_t lastRow = static_cast<int32_t>(
        std::floor((y + height - 0.01F) / TILE_SIZE));
    for (int32_t row = firstRow; row <= lastRow; ++row) {
        if (row < 0 || row >= level_->height) {
            continue;
        }
        for (int32_t column = firstColumn; column <= lastColumn; ++column) {
            if (column >= 0 && column < level_->width &&
                isSolid(static_cast<uint16_t>(column),
                        static_cast<uint8_t>(row))) {
                return true;
            }
        }
    }
    return false;
}

bool PlatformerLevelRuntime::addModification(
    uint16_t column, uint8_t row, PlatformerTileModificationState state) {
    for (uint8_t index = 0; index < modificationCount_; ++index) {
        if (modifications_[index].column == column &&
            modifications_[index].row == row) {
            modifications_[index].state = state;
            return true;
        }
    }
    if (modificationCount_ >= MAX_MODIFICATIONS) {
        return false;
    }
    modifications_[modificationCount_++] = {column, row, state};
    return true;
}

PlatformerBlockHitResult PlatformerLevelRuntime::hitBlock(
    uint16_t column, uint8_t row, bool canBreakBrick) {
    PlatformerBlockHitResult result;
    const PlatformerRuntimeTile source = tile(column, row);
    result.kind = source.kind;
    result.reward = source.reward;
    if (modificationAt(column, row) != nullptr) {
        return result;
    }
    if (source.kind == PlatformerRuntimeTileKind::Question ||
        source.kind == PlatformerRuntimeTileKind::Hidden ||
        (source.kind == PlatformerRuntimeTileKind::Brick &&
         source.reward != PlatformerRuntimeReward::None)) {
        result.accepted = true;
        result.changed = addModification(
            column, row, PlatformerTileModificationState::Used);
        return result;
    }
    if (source.kind == PlatformerRuntimeTileKind::Brick) {
        result.accepted = true;
        if (canBreakBrick) {
            result.changed = addModification(
                column, row, PlatformerTileModificationState::Broken);
            result.broken = result.changed;
        }
    }
    return result;
}

bool PlatformerLevelRuntime::collectCoin(uint16_t column, uint8_t row) {
    if (modificationAt(column, row) != nullptr ||
        tile(column, row).kind != PlatformerRuntimeTileKind::Coin) {
        return false;
    }
    return addModification(column, row, PlatformerTileModificationState::Broken);
}

bool PlatformerLevelRuntime::removeTile(uint16_t column, uint8_t row) {
    if (level_ == nullptr || column >= level_->width || row >= level_->height) {
        return false;
    }
    return addModification(column, row,
                           PlatformerTileModificationState::Broken);
}

void PlatformerLevelRuntime::setAnimationFrame(uint32_t frame) {
    animationFrame_ = frame;
    const uint8_t cycleFrame =
        frame == 0U
            ? 0U
            : static_cast<uint8_t>((frame - 1U) % 57U + 1U);
    for (uint8_t index = 0; index < animatedTileCount_; ++index) {
        animatedTiles_[index].ageFrames = cycleFrame;
    }
}

uint32_t PlatformerLevelRuntime::animationFrame() const {
    return animationFrame_;
}

void PlatformerLevelRuntime::updateAnimations(float cameraX, float cameraY,
                                               uint16_t viewportWidth,
                                               uint16_t viewportHeight) {
    ++animationFrame_;
    for (uint8_t index = 0; index < animatedTileCount_; ++index) {
        PlatformerAnimatedTileState& tile = animatedTiles_[index];
        const float x = static_cast<float>(tile.column * TILE_SIZE);
        const float y = static_cast<float>(tile.row * TILE_SIZE);
        const bool inCamera =
            x + TILE_SIZE >= cameraX && x <= cameraX + viewportWidth &&
            y + TILE_SIZE >= cameraY && y <= cameraY + viewportHeight;
        if (!inCamera) {
            continue;
        }
        tile.ageFrames = tile.ageFrames >= 57U
                             ? 1U
                             : static_cast<uint8_t>(tile.ageFrames + 1U);
    }
}

uint8_t PlatformerLevelRuntime::animatedTileCount() const {
    return animatedTileCount_;
}

bool PlatformerLevelRuntime::startBlockBump(uint16_t column, uint8_t row) {
    for (const auto& bump : blockBumps_) {
        if (bump.active && bump.column == column && bump.row == row) {
            return false;
        }
    }
    for (auto& bump : blockBumps_) {
        if (!bump.active) {
            bump.column = column;
            bump.row = row;
            bump.step = 0U;
            bump.offset = 0;
            bump.active = true;
            return true;
        }
    }
    return false;
}

void PlatformerLevelRuntime::updateBlockBumps() {
    // Reference cumulative positions after its eight 32px-tile deltas,
    // scaled to the 16px PGOS tiles and rounded to integer pixels.
    static constexpr int8_t OFFSETS[] = {-2, -3, -4, -5,
                                          -4, -3, -2, 0};
    for (auto& bump : blockBumps_) {
        if (!bump.active) {
            continue;
        }
        bump.offset = OFFSETS[bump.step];
        ++bump.step;
        if (bump.step >= sizeof(OFFSETS) / sizeof(OFFSETS[0])) {
            bump.active = false;
            bump.offset = 0;
        }
    }
}

int8_t PlatformerLevelRuntime::blockBumpOffset(uint16_t column,
                                                uint8_t row) const {
    for (const auto& bump : blockBumps_) {
        if (bump.active && bump.column == column && bump.row == row) {
            return bump.offset;
        }
    }
    return 0;
}

const PlatformerBlockBumpState* PlatformerLevelRuntime::blockBump(
    uint8_t index) const {
    if (index >= MAX_BLOCK_BUMPS || !blockBumps_[index].active) {
        return nullptr;
    }
    return &blockBumps_[index];
}

uint8_t PlatformerLevelRuntime::trampolineCount() const {
    return trampolineCount_;
}

const PlatformerTrampolineRuntimeState* PlatformerLevelRuntime::trampoline(
    uint8_t index) const {
    return index < trampolineCount_ ? &trampolines_[index] : nullptr;
}

bool PlatformerLevelRuntime::setTrampolineState(uint8_t index,
                                                uint8_t sequenceIndex,
                                                uint8_t visualState,
                                                bool activated) {
    if (index >= trampolineCount_ || visualState > 2U) {
        return false;
    }
    PlatformerTrampolineRuntimeState& trampoline = trampolines_[index];
    trampoline.sequenceIndex = sequenceIndex;
    trampoline.visualState = visualState;
    trampoline.activated = activated;
    return true;
}

uint16_t PlatformerLevelRuntime::displaySourceId(uint16_t column, uint8_t row,
                                                 uint16_t sourceId) const {
    if (sourceId >= PLATFORMER_BLOCK_TILE_COUNT) {
        return sourceId;
    }
    const uint16_t reference = PLATFORMER_BLOCK_REFERENCE_IDS[sourceId];
    for (uint8_t index = 0; index < trampolineCount_; ++index) {
        const PlatformerTrampolineRuntimeState& trampoline =
            trampolines_[index];
        const bool top = trampoline.column == column &&
                         trampoline.row == row && reference == 346U;
        const bool bottom = trampoline.column == column &&
                            static_cast<uint16_t>(trampoline.row) + 1U == row &&
                            reference == 394U;
        if (top || bottom) {
            return static_cast<uint16_t>(sourceId + trampoline.visualState);
        }
    }
    if (modificationAt(column, row) == nullptr &&
        (reference == 144U || reference == 192U || reference == 240U)) {
        uint8_t ageFrames = 0U;
        for (uint8_t index = 0; index < animatedTileCount_; ++index) {
            const PlatformerAnimatedTileState& tile = animatedTiles_[index];
            if (tile.column == column && tile.row == row &&
                tile.sourceId == sourceId) {
                ageFrames = tile.ageFrames;
                break;
            }
        }
        const uint8_t frame = platformerReferencePausedAnimationFrame(
            ageFrames, 8U, 4U, 25U);
        if (sourceId + frame < PLATFORMER_BLOCK_TILE_COUNT) {
            return static_cast<uint16_t>(sourceId + frame);
        }
    }
    return sourceId;
}

void PlatformerLevelRuntime::scanAnimatedTiles() {
    animatedTileCount_ = 0U;
    for (auto& tile : animatedTiles_) {
        tile = PlatformerAnimatedTileState{};
    }
    if (level_ == nullptr) {
        return;
    }
    for (uint8_t row = 0; row < level_->height; ++row) {
        for (uint16_t column = 0; column < level_->width; ++column) {
            for (uint8_t layerIndex = 0;
                 layerIndex < static_cast<uint8_t>(PlatformerMapLayer::Count);
                 ++layerIndex) {
                const uint16_t source = platformerCampaignTileAt(
                    *level_, static_cast<PlatformerMapLayer>(layerIndex),
                    column, row);
                if (source >= PLATFORMER_BLOCK_TILE_COUNT) {
                    continue;
                }
                const uint16_t reference =
                    PLATFORMER_BLOCK_REFERENCE_IDS[source];
                if (reference != 144U && reference != 192U &&
                    reference != 240U) {
                    continue;
                }
                bool duplicate = false;
                for (uint8_t index = 0; index < animatedTileCount_; ++index) {
                    const PlatformerAnimatedTileState& tile =
                        animatedTiles_[index];
                    if (tile.column == column && tile.row == row &&
                        tile.sourceId == source) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate && animatedTileCount_ < MAX_ANIMATED_TILES) {
                    PlatformerAnimatedTileState& tile =
                        animatedTiles_[animatedTileCount_++];
                    tile.column = column;
                    tile.row = row;
                    tile.sourceId = source;
                }
            }
        }
    }
}

void PlatformerLevelRuntime::scanTrampolines() {
    trampolineCount_ = 0U;
    for (auto& trampoline : trampolines_) {
        trampoline = PlatformerTrampolineRuntimeState{};
    }
    if (level_ == nullptr) {
        return;
    }
    for (uint8_t row = 0; row < level_->height; ++row) {
        for (uint16_t column = 0; column < level_->width; ++column) {
            for (PlatformerMapLayer layer : {PlatformerMapLayer::Foreground,
                                             PlatformerMapLayer::Underground}) {
                const uint16_t source = platformerCampaignTileAt(
                    *level_, layer, column, row);
                if (source >= PLATFORMER_BLOCK_TILE_COUNT ||
                    PLATFORMER_BLOCK_REFERENCE_IDS[source] != 346U) {
                    continue;
                }
                if (trampolineCount_ < MAX_TRAMPOLINES) {
                    PlatformerTrampolineRuntimeState& trampoline =
                        trampolines_[trampolineCount_++];
                    trampoline.column = column;
                    trampoline.row = row;
                    trampoline.sourceId = source;
                }
                break;
            }
        }
    }
}

int16_t PlatformerLevelRuntime::findReferenceColumn(uint16_t reference) const {
    if (level_ == nullptr) {
        return -1;
    }
    for (uint16_t column = 0; column < level_->width; ++column) {
        for (uint8_t row = 0; row < level_->height; ++row) {
            for (PlatformerMapLayer layer : {PlatformerMapLayer::Foreground,
                                             PlatformerMapLayer::Underground}) {
                const uint16_t source =
                    platformerCampaignTileAt(*level_, layer, column, row);
                if (source < PLATFORMER_BLOCK_TILE_COUNT &&
                    PLATFORMER_BLOCK_REFERENCE_IDS[source] == reference) {
                    return static_cast<int16_t>(column);
                }
            }
        }
    }
    return -1;
}

int16_t PlatformerLevelRuntime::goalColumn() const {
    int16_t result = findReferenceColumn(101);
    return result >= 0 ? result : findReferenceColumn(149);
}

int16_t PlatformerLevelRuntime::axeColumn() const {
    return findReferenceColumn(240);
}

uint8_t PlatformerLevelRuntime::modificationCount() const {
    return modificationCount_;
}

const PlatformerTileModification* PlatformerLevelRuntime::modification(
    uint8_t index) const {
    return index < modificationCount_ ? &modifications_[index] : nullptr;
}

}  // namespace pgos
