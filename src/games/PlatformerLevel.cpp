#include "games/PlatformerLevel.h"

namespace pgos {

namespace {

// The reference project scales its 16px art by 2.679 for an 800x600 window.
// The device keeps that source art at 1x, so these are the original level
// coordinates divided by the same factor and rounded to device pixels.
constexpr PlatformerSolidRect ORIGINAL_SOLIDS[] = {
    // Ground runs (the gaps are the original pits).
    {0, 201, 1102, 22},
    {1138, 201, 237, 22},
    {1426, 201, 1021, 22},
    {2481, 201, 859, 22},
    // Pipes.
    {449, 169, 31, 31},
    {609, 153, 31, 52},
    {736, 137, 31, 63},
    {913, 137, 31, 63},
    {2609, 169, 31, 31},
    {2865, 169, 31, 31},
    // Stair and castle blocks.
    {2144, 185, 15, 16},
    {2161, 169, 15, 16},
    {2177, 153, 15, 16},
    {2193, 137, 15, 66},
    {2240, 137, 15, 66},
    {2256, 152, 15, 15},
    {2272, 169, 15, 15},
    {2288, 185, 15, 15},
    {2368, 185, 15, 15},
    {2384, 169, 15, 15},
    {2401, 153, 15, 15},
    {2417, 137, 15, 15},
    {2433, 137, 15, 66},
    {2480, 137, 15, 66},
    {2496, 152, 15, 15},
    {2511, 169, 15, 15},
    {2527, 185, 15, 15},
    {2897, 185, 15, 15},
    {2913, 169, 15, 15},
    {2928, 153, 15, 15},
    {2944, 137, 15, 15},
    {2960, 121, 15, 15},
    {2976, 105, 15, 15},
    {2993, 88, 15, 15},
    {3009, 72, 15, 15},
    {3025, 72, 15, 134},
    {3168, 185, 15, 15},
};

constexpr PlatformerObjectSpawn ORIGINAL_OBJECTS[] = {
    // Breakable bricks.
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 320, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 352, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 384, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1231, 136},
    // Keep the 43px source brick run contiguous after 2.679x conversion;
    // 1264 would round one pixel past the adjacent 1247 question box.
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1263, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1280, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1296, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1312, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1328, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1344, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1360, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1376, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1392, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1456, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1472, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1488, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::MultiCoin, 1504, 136, 6},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1600, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::Star, 1616, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1888, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1936, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1952, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 1968, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 2049, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 2081, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 2097, 72},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 2065, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 2081, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 2688, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 2704, 136},
    {PlatformerObjectType::CoinBrick, PlatformerBoxReward::None, 2736, 136},
    // Coin boxes and power-up boxes.
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Coin, 256, 136},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Mushroom, 336, 136},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Coin, 368, 136},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Coin, 352, 72},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Mushroom, 1247, 136},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Coin, 1504, 72},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Coin, 1696, 136},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Coin, 1744, 136},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Mushroom, 1744, 72},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Coin, 1792, 136},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Coin, 2065, 72},
    {PlatformerObjectType::CoinBox, PlatformerBoxReward::Coin, 2720, 136},
    // The source level reveals this invisible 1UP box only when Mario jumps
    // through its checkpoint from below.
    {PlatformerObjectType::HiddenBox, PlatformerBoxReward::OneUp, 1023, 134},
};

// The original game activates enemy groups at checkpoints. Their initial
// positions below are the corresponding viewport-right positions after the
// same 2.679x-to-1x conversion; keeping them dormant off-screen gives the
// same first-screen composition without hard-coding enemies onto pipes.
constexpr PlatformerEnemySpawn ORIGINAL_ENEMIES[] = {
    {PlatformerEnemyType::Goomba, 390.0F, 185.0F, 190.0F},
    {PlatformerEnemyType::Goomba, 722.0F, 185.0F, 523.0F},
    {PlatformerEnemyType::Goomba, 850.0F, 185.0F, 650.0F},
    {PlatformerEnemyType::Goomba, 872.0F, 185.0F, 650.0F},
    {PlatformerEnemyType::Goomba, 1350.0F, 56.0F, 1150.0F},
    {PlatformerEnemyType::Goomba, 1372.0F, 56.0F, 1150.0F},
    {PlatformerEnemyType::Goomba, 1599.0F, 185.0F, 1400.0F},
    {PlatformerEnemyType::Goomba, 1621.0F, 185.0F, 1400.0F},
    {PlatformerEnemyType::Koopa, 1749.0F, 177.0F, 1549.0F},
    {PlatformerEnemyType::Goomba, 1868.0F, 185.0F, 1669.0F},
    {PlatformerEnemyType::Goomba, 1890.0F, 185.0F, 1669.0F},
    {PlatformerEnemyType::Goomba, 2049.0F, 185.0F, 1848.0F},
    {PlatformerEnemyType::Goomba, 2071.0F, 185.0F, 1848.0F},
    {PlatformerEnemyType::Goomba, 2105.0F, 185.0F, 1904.0F},
    {PlatformerEnemyType::Goomba, 2127.0F, 185.0F, 1904.0F},
    {PlatformerEnemyType::Goomba, 2737.0F, 185.0F, 2538.0F},
    {PlatformerEnemyType::Goomba, 2759.0F, 185.0F, 2538.0F},
};

}  // namespace

const PlatformerLevelDefinition PLATFORMER_LEVEL_1_1 = {
    3392,
    14,
    ORIGINAL_SOLIDS,
    static_cast<uint8_t>(sizeof(ORIGINAL_SOLIDS) / sizeof(ORIGINAL_SOLIDS[0])),
    ORIGINAL_OBJECTS,
    static_cast<uint8_t>(sizeof(ORIGINAL_OBJECTS) / sizeof(ORIGINAL_OBJECTS[0])),
    ORIGINAL_ENEMIES,
    static_cast<uint8_t>(sizeof(ORIGINAL_ENEMIES) / sizeof(ORIGINAL_ENEMIES[0])),
    3175,
    3275,
    201,
    36,
    169,
    184,
};

}  // namespace pgos
