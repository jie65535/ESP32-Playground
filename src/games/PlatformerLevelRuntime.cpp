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
        case PlatformerRuntimeTileKind::CloudPlatform:
            return true;
        case PlatformerRuntimeTileKind::Empty:
        case PlatformerRuntimeTileKind::Coin:
        case PlatformerRuntimeTileKind::Hidden:
        case PlatformerRuntimeTileKind::FlagPole:
        case PlatformerRuntimeTileKind::Flag:
        case PlatformerRuntimeTileKind::Axe:
        case PlatformerRuntimeTileKind::MovingPlatform:
            return false;
    }
    return false;
}

}  // namespace

bool PlatformerLevelRuntime::load(uint8_t world, uint8_t stage) {
    level_ = platformerCampaignLevel(world, stage);
    resetChanges();
    return level_ != nullptr;
}

void PlatformerLevelRuntime::resetChanges() {
    modificationCount_ = 0;
    for (auto& change : modifications_) {
        change = PlatformerTileModification{};
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
