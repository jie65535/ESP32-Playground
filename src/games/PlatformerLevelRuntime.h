#pragma once

#include "games/PlatformerCampaignData.h"

#include <cstdint>

namespace pgos {

enum class PlatformerRuntimeTileKind : uint8_t {
    Empty,
    Solid,
    Coin,
    Question,
    Brick,
    Hidden,
    FlagPole,
    Flag,
    Axe,
    Trampoline,
    MovingPlatform,
    CloudPlatform,
};

enum class PlatformerRuntimeReward : uint8_t {
    None,
    Coin,
    Mushroom,
    Star,
    OneUp,
    Vine,
};

enum class PlatformerTileModificationState : uint8_t {
    Used,
    Broken,
};

struct PlatformerRuntimeTile {
    PlatformerRuntimeTileKind kind = PlatformerRuntimeTileKind::Empty;
    PlatformerRuntimeReward reward = PlatformerRuntimeReward::None;
    PlatformerMapLayer layer = PlatformerMapLayer::Foreground;
    uint16_t sourceId = PLATFORMER_EMPTY_TILE;
};

struct PlatformerTileModification {
    uint16_t column = 0;
    uint8_t row = 0;
    PlatformerTileModificationState state =
        PlatformerTileModificationState::Used;
};

struct PlatformerBlockHitResult {
    PlatformerRuntimeTileKind kind = PlatformerRuntimeTileKind::Empty;
    PlatformerRuntimeReward reward = PlatformerRuntimeReward::None;
    bool accepted = false;
    bool changed = false;
    bool broken = false;
};

class PlatformerLevelRuntime final {
public:
    static constexpr uint8_t TILE_SIZE = 16;
    static constexpr uint8_t MAX_MODIFICATIONS = 64;

    bool load(uint8_t world, uint8_t stage);
    void resetChanges();

    const PlatformerCampaignLevel* level() const;
    uint8_t world() const;
    uint8_t stage() const;
    uint16_t widthPixels() const;
    uint16_t heightPixels() const;
    PlatformerLevelType activeLevelType() const;
    PlatformerBackgroundColor activeBackground() const;
    void setSection(PlatformerLevelType levelType,
                    PlatformerBackgroundColor background);

    PlatformerRuntimeTile tile(uint16_t column, uint8_t row) const;
    bool isSolid(uint16_t column, uint8_t row) const;
    bool rectHitsSolid(float x, float y, float width, float height) const;
    PlatformerBlockHitResult hitBlock(uint16_t column, uint8_t row,
                                      bool canBreakBrick);
    bool collectCoin(uint16_t column, uint8_t row);
    bool removeTile(uint16_t column, uint8_t row);

    int16_t goalColumn() const;
    int16_t axeColumn() const;
    uint8_t modificationCount() const;
    const PlatformerTileModification* modification(uint8_t index) const;
    const PlatformerTileModification* modificationAt(uint16_t column,
                                                       uint8_t row) const;

private:
    const PlatformerCampaignLevel* level_ = nullptr;
    PlatformerTileModification modifications_[MAX_MODIFICATIONS] = {};
    uint8_t modificationCount_ = 0;
    PlatformerLevelType activeLevelType_ = PlatformerLevelType::None;
    PlatformerBackgroundColor activeBackground_ =
        PlatformerBackgroundColor::Black;

    PlatformerRuntimeTile tileFromLayer(PlatformerMapLayer layer,
                                        uint16_t column, uint8_t row) const;
    PlatformerRuntimeReward rewardAt(uint16_t column, uint8_t row) const;
    bool addModification(uint16_t column, uint8_t row,
                         PlatformerTileModificationState state);
    int16_t findReferenceColumn(uint16_t reference) const;
};

}  // namespace pgos
