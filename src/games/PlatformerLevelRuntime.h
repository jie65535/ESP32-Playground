#pragma once

#include "games/PlatformerAnimation.h"
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

struct PlatformerBlockBumpState {
    uint16_t column = 0;
    uint8_t row = 0;
    uint8_t step = 0;
    int8_t offset = 0;
    bool active = false;
};

struct PlatformerTrampolineRuntimeState {
    uint16_t column = 0;
    uint8_t row = 0;
    uint16_t sourceId = PLATFORMER_EMPTY_TILE;
    uint8_t sequenceIndex = 0;
    uint8_t visualState = 0;
    bool activated = false;
};

struct PlatformerAnimatedTileState {
    uint16_t column = 0;
    uint16_t sourceId = PLATFORMER_EMPTY_TILE;
    uint8_t row = 0;
    uint8_t ageFrames = 0;
};

class PlatformerLevelRuntime final {
public:
    static constexpr uint8_t TILE_SIZE = 16;
    static constexpr uint8_t MAX_MODIFICATIONS = 64;
    static constexpr uint8_t MAX_BLOCK_BUMPS = 8;
    static constexpr uint8_t MAX_TRAMPOLINES = 8;
    static constexpr uint8_t MAX_ANIMATED_TILES = 128;

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
    void setAnimationFrame(uint32_t frame);
    uint32_t animationFrame() const;
    void updateAnimations(float cameraX, float cameraY,
                          uint16_t viewportWidth, uint16_t viewportHeight);
    uint8_t animatedTileCount() const;
    bool startBlockBump(uint16_t column, uint8_t row);
    void updateBlockBumps();
    int8_t blockBumpOffset(uint16_t column, uint8_t row) const;
    const PlatformerBlockBumpState* blockBump(uint8_t index) const;
    uint8_t trampolineCount() const;
    const PlatformerTrampolineRuntimeState* trampoline(uint8_t index) const;
    bool setTrampolineState(uint8_t index, uint8_t sequenceIndex,
                            uint8_t visualState, bool activated);
    uint16_t displaySourceId(uint16_t column, uint8_t row,
                             uint16_t sourceId) const;

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
    uint32_t animationFrame_ = 0;
    PlatformerBlockBumpState blockBumps_[MAX_BLOCK_BUMPS] = {};
    PlatformerTrampolineRuntimeState trampolines_[MAX_TRAMPOLINES] = {};
    uint8_t trampolineCount_ = 0;
    PlatformerAnimatedTileState animatedTiles_[MAX_ANIMATED_TILES] = {};
    uint8_t animatedTileCount_ = 0;
    PlatformerLevelType activeLevelType_ = PlatformerLevelType::None;
    PlatformerBackgroundColor activeBackground_ =
        PlatformerBackgroundColor::Black;

    PlatformerRuntimeTile tileFromLayer(PlatformerMapLayer layer,
                                        uint16_t column, uint8_t row) const;
    PlatformerRuntimeReward rewardAt(uint16_t column, uint8_t row) const;
    bool addModification(uint16_t column, uint8_t row,
                         PlatformerTileModificationState state);
    void scanTrampolines();
    void scanAnimatedTiles();
    int16_t findReferenceColumn(uint16_t reference) const;
};

}  // namespace pgos
