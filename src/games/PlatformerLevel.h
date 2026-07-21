#pragma once

#include <cstdint>

namespace pgos {

// Collision geometry from the original level is expressed in the source
// project's pixel coordinate system after its 2.679x art scale is removed.
// Static scenery is precomposed in the reference background; only collision
// rectangles and interactive objects live in this data model.
struct PlatformerSolidRect {
    int16_t x = 0;
    int16_t y = 0;
    int16_t width = 0;
    int16_t height = 0;
};

enum class PlatformerEnemyType : uint8_t {
    Goomba,
    Koopa,
    KoopaParatroopa,
    BuzzyBeetle,
    PiranhaPlant,
    Blooper,
    CheepCheep,
    Lakitu,
    Spiny,
    HammerBro,
    BulletBill,
    LavaBubble,
    Bowser,
};

struct PlatformerEnemySpawn {
    PlatformerEnemyType type = PlatformerEnemyType::Goomba;
    float x = 0.0F;
    float y = 0.0F;
    float leftLimit = 0.0F;
};

enum class PlatformerObjectType : uint8_t {
    CoinBox,
    CoinBrick,
    HiddenBox,
};

enum class PlatformerBoxReward : uint8_t {
    None,
    Coin,
    Mushroom,
    MultiCoin,
    Star,
    OneUp,
};

struct PlatformerObjectSpawn {
    PlatformerObjectType type = PlatformerObjectType::CoinBox;
    PlatformerBoxReward reward = PlatformerBoxReward::None;
    int16_t x = 0;
    int16_t y = 0;
    uint8_t uses = 1;
};

struct PlatformerLevelDefinition {
    uint16_t width = 0;
    uint8_t height = 0;
    const PlatformerSolidRect* solids = nullptr;
    uint8_t solidCount = 0;
    const PlatformerObjectSpawn* objects = nullptr;
    uint8_t objectCount = 0;
    const PlatformerEnemySpawn* enemies = nullptr;
    uint8_t enemyCount = 0;
    uint16_t goalX = 0;
    uint16_t castleX = 0;
    uint8_t groundY = 0;
    uint8_t flagTopY = 0;
    uint8_t flagSlideY = 0;
    uint8_t flagPoleBottomY = 0;
};

extern const PlatformerLevelDefinition PLATFORMER_LEVEL_1_1;

}  // namespace pgos
