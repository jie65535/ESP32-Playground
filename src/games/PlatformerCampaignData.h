#pragma once

#include <cstddef>
#include <cstdint>

namespace pgos {

constexpr uint16_t PLATFORMER_EMPTY_TILE = 0xFFFFU;
constexpr uint8_t PLATFORMER_CAMPAIGN_LEVEL_COUNT = 32;

enum class PlatformerMapLayer : uint8_t {
    Background,
    Underground,
    Foreground,
    AboveForeground,
    Collectibles,
    Enemies,
    Count,
};

enum class PlatformerLevelType : uint8_t {
    None,
    Overworld,
    Underground,
    Underwater,
    Castle,
    StartUnderground,
};

enum class PlatformerBackgroundColor : uint8_t {
    Black,
    Blue,
};

enum class PlatformerDirection : uint8_t {
    None,
    Up,
    Down,
    Left,
    Right,
};

enum class PlatformerMotionType : uint8_t {
    None,
    OneDirectionRepeated,
    OneDirectionContinuous,
    BackAndForth,
    Gravity,
};

enum class PlatformerRotationDirection : uint8_t {
    None,
    Clockwise,
    CounterClockwise,
};

struct PlatformerPoint {
    int16_t x = 0;
    int16_t y = 0;
};

struct PlatformerDataSpan {
    uint16_t offset = 0;
    uint8_t count = 0;
};

// Each row is independently RLE encoded. A record is run:u8 followed by a
// little-endian tile ID:u16; 0xffff represents an empty source cell.
struct PlatformerTileLayer {
    const uint16_t* rowOffsets = nullptr;
    const uint8_t* runs = nullptr;
    uint16_t runDataSize = 0;
};

struct PlatformerWarpData {
    PlatformerPoint pipe;
    PlatformerPoint destination;
    PlatformerPoint camera;
    PlatformerDirection enterDirection = PlatformerDirection::None;
    PlatformerDirection exitDirection = PlatformerDirection::None;
    bool freezeCamera = false;
    PlatformerBackgroundColor background = PlatformerBackgroundColor::Black;
    PlatformerLevelType levelType = PlatformerLevelType::None;
    uint8_t destinationWorld = 0;
    uint8_t destinationStage = 0;
};

struct PlatformerMovingPlatformData {
    PlatformerPoint position;
    PlatformerMotionType motion = PlatformerMotionType::None;
    PlatformerDirection direction = PlatformerDirection::None;
    int16_t minimum = 0;
    int16_t maximum = 0;
    bool halfTileShift = false;
};

struct PlatformerPulleyData {
    PlatformerPoint left;
    PlatformerPoint right;
    int16_t pulleyY = 0;
};

struct PlatformerFireBarData {
    PlatformerPoint position;
    int16_t startingAngle = 0;
    PlatformerRotationDirection direction =
        PlatformerRotationDirection::None;
    uint8_t length = 0;
};

struct PlatformerVineData {
    PlatformerPoint block;
    PlatformerPoint destination;
    PlatformerPoint camera;
    int16_t resetBelowY = 0;
    PlatformerPoint resetDestination;
    int16_t cameraMaximum = 0;
    PlatformerBackgroundColor background = PlatformerBackgroundColor::Black;
    PlatformerLevelType levelType = PlatformerLevelType::None;
};

struct PlatformerTeleportData {
    int16_t triggerX = 0;
    int16_t destinationX = 0;
};

struct PlatformerFloatingTextData {
    PlatformerPoint position;
    const char* text = nullptr;
};

struct PlatformerCampaignLevel {
    uint8_t world = 0;
    uint8_t stage = 0;
    uint16_t width = 0;
    uint8_t height = 0;
    PlatformerPoint playerStart;
    PlatformerPoint cameraStart;
    uint16_t cameraMaximum = 0;
    uint8_t nextWorld = 0;
    uint8_t nextStage = 0;
    PlatformerLevelType levelType = PlatformerLevelType::None;
    PlatformerBackgroundColor background = PlatformerBackgroundColor::Black;
    PlatformerTileLayer layers[static_cast<size_t>(PlatformerMapLayer::Count)];
    PlatformerDataSpan warps;
    PlatformerDataSpan movingPlatforms;
    PlatformerDataSpan pulleys;
    PlatformerDataSpan fireBars;
    PlatformerDataSpan vines;
    PlatformerDataSpan teleports;
    PlatformerDataSpan floatingTexts;
};

extern const PlatformerCampaignLevel
    PLATFORMER_CAMPAIGN_LEVELS[PLATFORMER_CAMPAIGN_LEVEL_COUNT];
extern const PlatformerWarpData PLATFORMER_CAMPAIGN_WARPS[];
extern const PlatformerMovingPlatformData PLATFORMER_CAMPAIGN_MOVING_PLATFORMS[];
extern const PlatformerPulleyData PLATFORMER_CAMPAIGN_PULLEYS[];
extern const PlatformerFireBarData PLATFORMER_CAMPAIGN_FIRE_BARS[];
extern const PlatformerVineData PLATFORMER_CAMPAIGN_VINES[];
extern const PlatformerTeleportData PLATFORMER_CAMPAIGN_TELEPORTS[];
extern const PlatformerFloatingTextData PLATFORMER_CAMPAIGN_FLOATING_TEXTS[];

const PlatformerCampaignLevel* platformerCampaignLevel(uint8_t world,
                                                        uint8_t stage);
uint16_t platformerCampaignTileAt(const PlatformerCampaignLevel& level,
                                  PlatformerMapLayer layer, uint16_t x,
                                  uint8_t y);

}  // namespace pgos
