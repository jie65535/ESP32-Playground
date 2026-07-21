#include "games/PlatformerEngine.h"
#include "games/PlatformerTileAssets.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

using pgos::PlatformerEffectKind;
using pgos::PlatformerDeathReason;
using pgos::PlatformerEnemyMotion;
using pgos::PlatformerEngine;
using pgos::PlatformerEvent;
using pgos::PlatformerEventType;
using pgos::PlatformerInput;
using pgos::PlatformerPhase;
using pgos::PlatformerPlayerPower;
using pgos::PlatformerPowerupKind;

namespace {

void stepFor(PlatformerEngine& engine, uint32_t milliseconds,
             const PlatformerInput& input = {}) {
    for (uint32_t elapsed = 0; elapsed < milliseconds; elapsed += 8U) {
        engine.step(0.008F, input);
    }
}

bool receivedEvent(PlatformerEngine& engine, PlatformerEventType type) {
    PlatformerEvent event;
    bool received = false;
    while (engine.pollEvent(event)) {
        received |= event.type == type;
    }
    return received;
}

uint8_t activeProjectileCount(const PlatformerEngine& engine) {
    uint8_t count = 0;
    for (uint8_t index = 0; index < engine.snapshot().projectileCount;
         ++index) {
        count += engine.projectile(index).active ? 1U : 0U;
    }
    return count;
}

void testGroundingAndJump() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    const auto landed = engine.snapshot();
    assert(landed.phase == PlatformerPhase::Running);
    assert(landed.grounded);
    assert(std::fabs(landed.playerY - 185.0F) < 0.1F);

    engine.step(0.008F, PlatformerInput{0.0F, true, true});
    const auto airborne = engine.snapshot();
    assert(!airborne.grounded);
    assert(airborne.playerVy < 0.0F);
    assert(airborne.playerY < landed.playerY);
    assert(receivedEvent(engine, PlatformerEventType::Jumped));
}

void testStandingRemainsGrounded() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    for (uint16_t frame = 0; frame < 240U; ++frame) {
        engine.step(0.008F, PlatformerInput{});
        const auto standing = engine.snapshot();
        assert(standing.grounded);
        assert(std::fabs(standing.playerVy) < 0.01F);
        assert(std::fabs(standing.playerY - 185.0F) < 0.1F);
    }
}

void testWalkingOffLedgeStartsFalling() {
    PlatformerEngine engine;
    engine.start();
    engine.debugSetPlayer(1105.0F, 185.0F, 0.0F, 0.0F, true);
    engine.step(0.008F, PlatformerInput{});
    const auto falling = engine.snapshot();
    assert(!falling.grounded);
    assert(falling.playerVy > 0.0F);
}

float measureJumpApex(bool holdJump) {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    float minimumY = engine.snapshot().playerY;
    for (uint32_t frame = 0; frame < 120; ++frame) {
        PlatformerInput input;
        input.jumpPressed = frame == 0;
        input.jumpHeld = holdJump && frame < 45;
        engine.step(0.008F, input);
        minimumY = std::min(minimumY, engine.snapshot().playerY);
    }
    return minimumY;
}

void testVariableJumpHeight() {
    const float tapApex = measureJumpApex(false);
    const float holdApex = measureJumpApex(true);
    assert(holdApex + 10.0F < tapApex);
    assert(tapApex <= 153.0F);
    assert(holdApex <= 120.5F);
}

void testWalkRunTurnAndCamera() {
    PlatformerEngine walk;
    walk.start();
    stepFor(walk, 240);
    stepFor(walk, 700, PlatformerInput{1.0F, false, false, false, false, false});
    const float walkSpeed = walk.snapshot().playerVx;
    assert(walkSpeed > 100.0F && walkSpeed < 150.0F);

    PlatformerEngine run;
    run.start();
    stepFor(run, 240);
    stepFor(run, 700, PlatformerInput{1.0F, false, false, false, false, true});
    const auto running = run.snapshot();
    assert(running.playerVx > walkSpeed + 30.0F);
    assert(running.cameraX > 0.0F);
    stepFor(run, 120, PlatformerInput{-1.0F, false, false, false, false, true});
    assert(run.snapshot().playerSkidding || run.snapshot().playerVx < 0.0F);

    run.debugSetPlayer(1000.0F, 185.0F, 0.0F, 0.0F, true);
    const float forwardCamera = run.snapshot().cameraX;
    run.debugSetPlayer(500.0F, 185.0F, 0.0F, 0.0F, true);
    const auto returned = run.snapshot();
    assert(returned.cameraX < forwardCamera - 400.0F);
    assert(returned.playerX - returned.cameraX >= 79.9F);
}

void testQuestionBoxWinsAdjacentOverlap() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugSetPlayer(343.0F, 185.0F, 0.0F, 0.0F, true);
    for (uint16_t frame = 0; frame < 100U && !engine.box(32).opened;
         ++frame) {
        engine.step(0.008F,
                    PlatformerInput{0.0F, frame == 0U, true});
    }
    assert(engine.box(32).opened);
    assert(!engine.box(1).opened);
}

void testPauseFreezesSimulation() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.togglePause();
    assert(engine.phase() == PlatformerPhase::Paused);
    const auto before = engine.snapshot();
    stepFor(engine, 1000, PlatformerInput{1.0F, true, true});
    const auto after = engine.snapshot();
    assert(std::fabs(before.playerX - after.playerX) < 0.01F);
    assert(std::fabs(before.playerY - after.playerY) < 0.01F);
    assert(before.timeRemaining == after.timeRemaining);
    engine.togglePause();
    assert(engine.phase() == PlatformerPhase::Running);
}

void testReferenceLevelData() {
    PlatformerEngine engine;
    engine.start();
    assert(engine.debugTileSolid(0, 12));
    assert(engine.debugTileSolid(60, 12));
    assert(engine.snapshot().totalBoxes == 44U);
    assert(engine.box(0).type == pgos::PlatformerObjectType::CoinBrick);
    assert(engine.box(4).x == 1263);
    assert(engine.box(9).x == 1344);
    assert(engine.box(16).reward == pgos::PlatformerBoxReward::MultiCoin);
    assert(engine.box(16).remainingUses == 6U);
    assert(engine.box(18).reward == pgos::PlatformerBoxReward::Star);
    assert(engine.box(31).type == pgos::PlatformerObjectType::CoinBox);
    assert(engine.box(43).type == pgos::PlatformerObjectType::HiddenBox);
    assert(!engine.box(43).visible);
    struct ExpectedRewardBox {
        uint8_t index;
        int16_t x;
        int16_t y;
        pgos::PlatformerObjectType type;
        pgos::PlatformerBoxReward reward;
        uint8_t uses;
    };
    constexpr ExpectedRewardBox EXPECTED_REWARD_BOXES[] = {
        {16, 1504, 136, pgos::PlatformerObjectType::CoinBrick,
         pgos::PlatformerBoxReward::MultiCoin, 6},
        {18, 1616, 136, pgos::PlatformerObjectType::CoinBrick,
         pgos::PlatformerBoxReward::Star, 1},
        {31, 256, 136, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Coin, 1},
        {32, 336, 136, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Mushroom, 1},
        {33, 368, 136, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Coin, 1},
        {34, 352, 72, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Coin, 1},
        {35, 1247, 136, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Mushroom, 1},
        {36, 1504, 72, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Coin, 1},
        {37, 1696, 136, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Coin, 1},
        {38, 1744, 136, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Coin, 1},
        {39, 1744, 72, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Mushroom, 1},
        {40, 1792, 136, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Coin, 1},
        {41, 2065, 72, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Coin, 1},
        {42, 2720, 136, pgos::PlatformerObjectType::CoinBox,
         pgos::PlatformerBoxReward::Coin, 1},
        {43, 1023, 134, pgos::PlatformerObjectType::HiddenBox,
         pgos::PlatformerBoxReward::OneUp, 1},
    };
    for (const auto& expected : EXPECTED_REWARD_BOXES) {
        const auto box = engine.box(expected.index);
        assert(box.x == expected.x);
        assert(box.y == expected.y);
        assert(box.type == expected.type);
        assert(box.reward == expected.reward);
        assert(box.remainingUses == expected.uses);
    }
    assert(std::fabs(engine.goalX() - 3175.0F) < 0.1F);
    assert(std::fabs(engine.castleX() - 3275.0F) < 0.1F);
    assert(pgos::PLATFORMER_LEVEL_1_1.flagTopY == 36U);
    assert(pgos::PLATFORMER_LEVEL_1_1.flagSlideY == 169U);
    assert(pgos::PLATFORMER_LEVEL_1_1.flagPoleBottomY == 184U);
    assert(engine.snapshot().enemyCount == 17U);
    assert(!engine.enemy(0).active);
    assert(engine.enemy(8).type == pgos::PlatformerEnemyType::Koopa);
    assert(std::fabs(engine.enemy(8).height - 24.0F) < 0.1F);
}

void testCheckpointEnemySpawnClearsPipeEdge() {
    PlatformerEngine engine;
    engine.start();
    engine.debugSetPlayer(190.0F, 185.0F, 0.0F, 0.0F, true);
    engine.step(0.008F, PlatformerInput{});
    engine.debugSetPlayer(523.0F, 185.0F, 0.0F, 0.0F, true);
    engine.step(0.008F, PlatformerInput{});

    const auto enemy = engine.enemy(1);
    assert(enemy.active);
    // The second checkpoint is immediately before the 63px pipe at x=736;
    // the spawned Goomba must be outside the pipe, never embedded in it.
    assert(enemy.x + enemy.width <= 736.01F || enemy.x >= 767.0F);
}

void testMapPanoramaMode() {
    PlatformerEngine engine;
    engine.startMapTest();
    const auto start = engine.snapshot();
    assert(start.mapTestMode);
    assert(std::fabs(start.playerX - 41.0F) < 0.1F);
    assert(start.enemyCount == 17U);
    for (uint8_t index = 0; index < start.enemyCount; ++index) {
        assert(engine.enemy(index).active);
    }

    stepFor(engine, 1000,
            PlatformerInput{1.0F, true, true, false, true, true});
    const auto middle = engine.snapshot();
    assert(middle.mapTestMode);
    assert(std::fabs(middle.playerX - start.playerX) < 0.1F);
    assert(std::fabs(middle.cameraX) < 0.1F);
    for (uint8_t step = 0; step < 12U; ++step) {
        engine.advanceMapTest();
    }
    const auto end = engine.snapshot();
    assert(end.mapTestMode);
    assert(end.cameraX > 3000.0F);
    assert(std::fabs(end.playerX - start.playerX) < 0.1F);
}

void testCoinBoxAndMultiCoinBrick() {
    PlatformerEngine engine;
    engine.start();
    engine.debugHitBox(31);
    assert(engine.box(31).opened);
    assert(engine.snapshot().coinsCollected == 1U);
    assert(engine.snapshot().score == 200U);
    assert(engine.effect(0).kind == PlatformerEffectKind::Score ||
           engine.effect(0).kind == PlatformerEffectKind::RisingCoin);

    for (uint8_t use = 0; use < 6U; ++use) {
        engine.debugHitBox(16);
    }
    assert(engine.box(16).opened);
    assert(engine.box(16).remainingUses == 0U);
    assert(engine.snapshot().coinsCollected == 7U);
    assert(engine.snapshot().score == 1400U);
    assert(receivedEvent(engine, PlatformerEventType::CoinBoxHit));
}

void testBrickBreakAndSpecialContents() {
    PlatformerEngine engine;
    engine.start();
    engine.debugSetPlayerPower(PlatformerPlayerPower::Big);
    engine.debugHitBox(0);
    assert(engine.box(0).opened);
    assert(!engine.box(0).visible);
    bool hasPiece = false;
    for (uint8_t index = 0; index < engine.snapshot().effectCount; ++index) {
        hasPiece |= engine.effect(index).active &&
                    engine.effect(index).kind == PlatformerEffectKind::BrickPiece;
    }
    assert(hasPiece);

    engine.debugHitBox(18);
    assert(engine.box(18).opened);
    assert(engine.powerup(0).kind == PlatformerPowerupKind::Star);
    assert(engine.powerup(0).active);

    const uint8_t lives = engine.snapshot().lives;
    engine.debugHitBox(43);
    assert(engine.box(43).visible && engine.box(43).opened);
    uint8_t oneUpIndex = 0;
    while (oneUpIndex < engine.snapshot().powerupCount &&
           engine.powerup(oneUpIndex).kind != PlatformerPowerupKind::OneUp) {
        ++oneUpIndex;
    }
    assert(oneUpIndex < engine.snapshot().powerupCount);
    const auto oneUp = engine.powerup(oneUpIndex);
    engine.debugSetPlayer(oneUp.x, oneUp.y, 0.0F, 0.0F, false);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.snapshot().lives == lives + 1U);
    assert(receivedEvent(engine, PlatformerEventType::OneUp));
}

void testGrowthFireAndCrouch() {
    static_assert(PlatformerEngine::CROUCH_PLAYER_HEIGHT ==
                  PlatformerEngine::PLAYER_HEIGHT);
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    const auto small = engine.snapshot();
    engine.debugSpawnPowerup(PlatformerPowerupKind::Mushroom,
                             small.playerX, small.playerY);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.snapshot().playerBig);
    assert(engine.snapshot().playerPower == PlatformerPlayerPower::Big);
    stepFor(engine, 1000);

    const float standingY = engine.snapshot().playerY;
    stepFor(engine, 40, PlatformerInput{0.0F, false, false, true});
    const auto crouching = engine.snapshot();
    assert(crouching.playerCrouching);
    assert(std::fabs(crouching.playerY -
                     (PlatformerEngine::BIG_PLAYER_HEIGHT -
                      PlatformerEngine::CROUCH_PLAYER_HEIGHT) -
                     standingY) < 0.1F);
    stepFor(engine, 40);
    assert(!engine.snapshot().playerCrouching);
    assert(std::fabs(engine.snapshot().playerY - standingY) < 0.1F);

    engine.debugHitBox(32);
    assert(engine.powerup(0).kind == PlatformerPowerupKind::FireFlower);
    const auto flower = engine.powerup(0);
    engine.debugSetPlayer(flower.x, flower.y, 0.0F, 0.0F, false);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.snapshot().playerFire);
}

void testFireFlowerRestsAndHiddenBlockBecomesFloor() {
    PlatformerEngine flower;
    assert(flower.startCampaign(1, 1));
    flower.debugSpawnPowerup(PlatformerPowerupKind::FireFlower, 100.0F,
                             160.0F);
    const float flowerStartX = flower.powerup(0).x;
    stepFor(flower, 1200);
    assert(flower.powerup(0).active);
    assert(flower.powerup(0).state == pgos::PlatformerPowerupState::Resting);
    assert(std::fabs(flower.powerup(0).x - flowerStartX) < 0.01F);
    assert(std::fabs(flower.powerup(0).vx) < 0.01F);

    PlatformerEngine hidden;
    assert(hidden.startCampaign(1, 1));
    constexpr float BLOCK_X = 64.0F * 16.0F;
    constexpr float BLOCK_Y = 8.0F * 16.0F;
    assert(!hidden.debugTileSolid(64, 8));
    hidden.debugSetPlayer(BLOCK_X, BLOCK_Y + 17.0F, 0.0F, -240.0F,
                          false);
    hidden.step(0.016F, PlatformerInput{});
    assert(hidden.debugTileSolid(64, 8));

    hidden.debugSetPlayer(BLOCK_X, BLOCK_Y - 40.0F, 0.0F, 180.0F, false);
    stepFor(hidden, 320);
    const auto landed = hidden.snapshot();
    assert(landed.grounded);
    assert(std::fabs(landed.playerY - (BLOCK_Y - 16.0F)) < 0.1F);
}

void testDamageDeathAndLifeRestart() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    const auto grounded = engine.snapshot();
    engine.debugSetPlayerPower(PlatformerPlayerPower::Big);
    engine.debugActivateEnemy(0, grounded.playerX, 177.0F,
                              PlatformerEnemyMotion::Walking);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::Running);
    assert(engine.snapshot().playerPower == PlatformerPlayerPower::Small);
    assert(!engine.snapshot().playerDamageBlinking);
    assert(receivedEvent(engine, PlatformerEventType::PlayerHurt));

    const float hitX = engine.snapshot().playerX;
    stepFor(engine, 840,
            PlatformerInput{1.0F, false, false, false, false, true});
    assert(engine.snapshot().playerX > hitX + 0.1F);
    assert(engine.snapshot().playerDamageBlinking);
    engine.debugActivateEnemy(1, engine.snapshot().playerX, 185.0F,
                              PlatformerEnemyMotion::Walking);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::Running);

    engine.debugSetPlayer(1000.0F, 185.0F, 0.0F, 0.0F, true);
    stepFor(engine, 2500);
    const auto vulnerable = engine.snapshot();
    assert(!vulnerable.playerDamageBlinking);
    engine.debugActivateEnemy(1, vulnerable.playerX, 185.0F,
                              PlatformerEnemyMotion::Walking);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::Dying);
    assert(engine.snapshot().deathReason == PlatformerDeathReason::Enemy);
    assert(receivedEvent(engine, PlatformerEventType::PlayerDied));
    stepFor(engine, 2250);
    assert(engine.phase() == PlatformerPhase::Running);
    assert(engine.snapshot().lives == 2U);
    assert(engine.snapshot().timeRemaining == 400U);
}

void testCrouchingDamagePreservesPlayerFeet() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugSetPlayerPower(PlatformerPlayerPower::Big);
    stepFor(engine, 40, PlatformerInput{0.0F, false, false, true});

    const auto crouching = engine.snapshot();
    assert(crouching.playerCrouching);
    const float footBefore =
        crouching.playerY + PlatformerEngine::CROUCH_PLAYER_HEIGHT;

    engine.debugActivateEnemy(0, crouching.playerX, crouching.playerY + 4.0F,
                              PlatformerEnemyMotion::Walking);
    engine.step(0.008F, PlatformerInput{0.0F, false, false, true});

    const auto hurt = engine.snapshot();
    assert(hurt.playerPower == PlatformerPlayerPower::Small);
    assert(!hurt.playerCrouching);
    assert(std::fabs(hurt.playerY + PlatformerEngine::PLAYER_HEIGHT -
                     footBefore) < 0.1F);
    assert(receivedEvent(engine, PlatformerEventType::PlayerHurt));
}

void testKoopaShellLifecycle() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugActivateEnemy(8, 90.0F, 177.0F,
                              PlatformerEnemyMotion::Walking);
    engine.debugSetPlayer(90.0F, 160.0F, 0.0F, 180.0F, false);
    stepFor(engine, 16);
    assert(engine.enemy(8).motion == PlatformerEnemyMotion::ShellIdle);
    assert(engine.snapshot().playerVy < 0.0F);
    const uint32_t scoreAfterStomp = engine.snapshot().score;

    engine.debugSetPlayer(78.0F, 185.0F, 0.0F, 0.0F, true);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.enemy(8).motion == PlatformerEnemyMotion::ShellSliding);
    assert(std::fabs(engine.enemy(8).vx) > 150.0F);
    assert(receivedEvent(engine, PlatformerEventType::ShellKicked));
    assert(engine.snapshot().score == scoreAfterStomp);

    for (uint8_t cycle = 0; cycle < 4U; ++cycle) {
        const auto shell = engine.enemy(8);
        engine.debugSetPlayer(shell.x, shell.y - 17.0F, 0.0F, 180.0F,
                              false);
        engine.step(0.016F, PlatformerInput{});
        assert(engine.snapshot().score == scoreAfterStomp);
    }

    PlatformerEngine buzzy;
    assert(buzzy.startCampaign(1, 1));
    buzzy.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::BuzzyBeetle, 100.0F, 192.0F, 87U);
    const float buzzyFoot = buzzy.enemy(0).y + buzzy.enemy(0).height;
    buzzy.debugSetPlayer(100.0F, 175.0F, 0.0F, 180.0F, false);
    buzzy.step(0.016F, PlatformerInput{});
    assert(buzzy.enemy(0).motion == PlatformerEnemyMotion::ShellIdle);
    assert(std::fabs(buzzy.enemy(0).y + buzzy.enemy(0).height - buzzyFoot) <
           0.1F);
}

void testSimultaneousEnemyStomp() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugActivateEnemy(0, 90.0F, 185.0F,
                              PlatformerEnemyMotion::Walking);
    engine.debugActivateEnemy(1, 104.0F, 185.0F,
                              PlatformerEnemyMotion::Walking);
    engine.debugSetPlayer(97.0F, 167.0F, 0.0F, 180.0F, false);
    stepFor(engine, 16);
    assert(engine.phase() == PlatformerPhase::Running);
    assert(engine.enemy(0).motion == PlatformerEnemyMotion::Squashed);
    assert(engine.enemy(1).motion == PlatformerEnemyMotion::Squashed);
    assert(engine.snapshot().playerVy < 0.0F);
}

void testSquashedEnemyStopsCollidingImmediately() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugActivateEnemy(0, 90.0F, 185.0F,
                              PlatformerEnemyMotion::Walking);
    engine.debugSetPlayer(90.0F, 167.0F, 0.0F, 180.0F, false);
    engine.step(0.016F, PlatformerInput{});
    assert(engine.enemy(0).motion == PlatformerEnemyMotion::Squashed);

    engine.debugSetPlayer(90.0F, 174.0F, 0.0F, 80.0F, false);
    engine.step(0.016F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::Running);
    assert(engine.snapshot().playerVy > 0.0F);
}

void testStompSuppressesSameStepSideDamage() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugActivateEnemy(0, 90.0F, 185.0F,
                              PlatformerEnemyMotion::Walking);
    engine.debugActivateEnemy(1, 104.0F, 174.0F,
                              PlatformerEnemyMotion::Walking);
    engine.debugSetPlayer(97.0F, 167.0F, 0.0F, 180.0F, false);
    engine.step(0.016F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::Running);
    assert(engine.enemy(0).motion == PlatformerEnemyMotion::Squashed);
    assert(engine.enemy(1).motion == PlatformerEnemyMotion::Walking);
    assert(engine.snapshot().playerVy < 0.0F);
}

void testCampaignBlockSeamAdvancesPastBrokenTile() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    const auto* level = engine.levelRuntime().level();
    assert(level != nullptr);
    int16_t leftColumn = -1;
    int16_t blockRow = -1;
    for (uint8_t row = 0; row < level->height && leftColumn < 0; ++row) {
        for (uint16_t column = 0; column + 1U < level->width; ++column) {
            if (engine.levelRuntime().tile(column, row).kind ==
                    pgos::PlatformerRuntimeTileKind::Brick &&
                engine.levelRuntime().tile(column + 1U, row).kind ==
                    pgos::PlatformerRuntimeTileKind::Brick) {
                leftColumn = static_cast<int16_t>(column);
                blockRow = row;
                break;
            }
        }
    }
    assert(leftColumn >= 0 && blockRow >= 0);
    engine.debugSetPlayerPower(PlatformerPlayerPower::Big);
    const float seamX = (leftColumn + 1) * 16.0F - 8.0F;
    const float belowY = (blockRow + 1) * 16.0F + 1.0F;
    engine.debugSetPlayer(seamX, belowY, 0.0F, -200.0F, false);
    engine.step(0.016F, PlatformerInput{});
    assert(engine.levelRuntime().modificationCount() == 1U);
    engine.debugSetPlayer(seamX, belowY, 0.0F, -200.0F, false);
    engine.step(0.016F, PlatformerInput{});
    assert(engine.levelRuntime().modificationCount() == 2U);
}

void testCampaignFlagUsesMovingFlagAnchor() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    const auto before = engine.snapshot();
    assert(before.flagTileId < pgos::PLATFORMER_BLOCK_TILE_COUNT);
    assert(pgos::PLATFORMER_BLOCK_REFERENCE_IDS[before.flagTileId] == 152U);
    engine.debugBeginGoal();
    const auto climbing = engine.snapshot();
    assert(climbing.phase == PlatformerPhase::Flagpole);
    assert(std::fabs(climbing.playerX - climbing.flagX) < 0.1F);
}

void testFireballPoolReusesExpiredSlots() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugSetPlayerPower(PlatformerPlayerPower::Fire);
    for (uint8_t shot = 0; shot < 5U; ++shot) {
        engine.step(
            0.008F,
            PlatformerInput{0.0F, false, false, false, true, true});
        assert(activeProjectileCount(engine) == 1U);
        stepFor(engine, 3000);
        assert(activeProjectileCount(engine) == 0U);
    }
    assert(engine.snapshot().projectileCount == 1U);

    PlatformerEngine simultaneous;
    simultaneous.start();
    stepFor(simultaneous, 240);
    simultaneous.debugSetPlayerPower(PlatformerPlayerPower::Fire);
    simultaneous.step(
        0.008F,
        PlatformerInput{0.0F, false, false, false, true, true});
    stepFor(simultaneous, 240);
    simultaneous.step(
        0.008F,
        PlatformerInput{0.0F, false, false, false, true, true});
    assert(activeProjectileCount(simultaneous) ==
           PlatformerEngine::MAX_PROJECTILES);
    stepFor(simultaneous, 240);
    simultaneous.step(
        0.008F,
        PlatformerInput{0.0F, false, false, false, true, true});
    assert(activeProjectileCount(simultaneous) ==
           PlatformerEngine::MAX_PROJECTILES);
}

void testTimerAndCompleteGoalSequence() {
    PlatformerEngine timer;
    timer.start();
    timer.debugSetTimeRemaining(1);
    stepFor(timer, 408);
    assert(timer.phase() == PlatformerPhase::Dying);
    assert(timer.snapshot().deathReason == PlatformerDeathReason::Time);

    PlatformerEngine fall;
    fall.start();
    fall.debugSetPlayer(1110.0F, 241.0F, 0.0F, 120.0F, false);
    fall.step(0.008F, PlatformerInput{});
    assert(fall.phase() == PlatformerPhase::Dying);
    assert(fall.snapshot().deathReason == PlatformerDeathReason::Fall);

    PlatformerEngine goal;
    goal.start();
    goal.debugSetPlayer(3150.0F, 48.0F, 0.0F, 0.0F, false);
    goal.debugBeginGoal();
    assert(goal.phase() == PlatformerPhase::Flagpole);
    assert(goal.snapshot().score >= 2000U);
    stepFor(goal, 1810);
    assert(goal.phase() == PlatformerPhase::CastleWalk);
    stepFor(goal, 1700);
    assert(goal.phase() == PlatformerPhase::TimeBonus);
    goal.debugSetTimeRemaining(5);
    stepFor(goal, 520);
    assert(goal.phase() == PlatformerPhase::Won);
    assert(goal.snapshot().goalReached);
    assert(receivedEvent(goal, PlatformerEventType::CourseClear));
}

void testCampaignRuntimeAndLevelAdvance() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    auto start = engine.snapshot();
    assert(start.campaignMode);
    assert(start.world == 1U && start.stage == 1U);
    assert(std::fabs(start.cameraY) < 0.1F);
    assert(start.totalBoxes == 0U);
    stepFor(engine, 240);
    const auto grounded = engine.snapshot();
    assert(grounded.grounded);
    assert(std::fabs(grounded.playerX - 32.0F) < 0.1F);
    assert(std::fabs(grounded.playerY - 192.0F) < 0.1F);
    assert(engine.debugTileSolid(0, 13));
    assert(!engine.debugTileSolid(69, 13));
    assert(grounded.enemyCount >= 1U);
    assert(engine.enemy(0).active);
    assert(engine.enemy(0).type == pgos::PlatformerEnemyType::Goomba);
    assert(engine.enemy(0).sourceTileId == 70U);

    engine.debugSetPlayer(256.0F, 192.0F, 0.0F, 0.0F, true);
    for (uint16_t frame = 0; frame < 120U &&
                             engine.levelRuntime().modificationCount() == 0U;
         ++frame) {
        engine.step(0.008F,
                    PlatformerInput{0.0F, frame == 0U, true});
    }
    assert(engine.levelRuntime().modificationCount() == 1U);
    assert(engine.snapshot().coinsCollected == 1U);

    engine.debugBeginGoal();
    stepFor(engine, 1810);
    assert(engine.phase() == PlatformerPhase::CastleWalk);
    stepFor(engine, 1700);
    assert(engine.phase() == PlatformerPhase::TimeBonus);
    engine.debugSetTimeRemaining(1);
    stepFor(engine, 520);
    assert(engine.phase() == PlatformerPhase::Won);
    assert(engine.advanceCampaign());
    const auto next = engine.snapshot();
    assert(next.phase == PlatformerPhase::Running);
    assert(next.world == 1U && next.stage == 2U);
    assert(next.campaignMode);
}

void testReferenceTileRoundnessAllowsTightOpenings() {
    PlatformerEngine shaft;
    assert(shaft.startCampaign(1, 2));
    stepFor(shaft, 4500);
    assert(shaft.phase() == PlatformerPhase::Running);
    shaft.debugSetPlayerPower(PlatformerPlayerPower::Big);
    const auto* shaftLevel = shaft.levelRuntime().level();
    assert(shaftLevel != nullptr);
    int shaftColumn = -1;
    int shaftRow = -1;
    for (int row = 2; row < shaftLevel->height - 1 && shaftColumn < 0;
         ++row) {
        for (int column = 1; column < shaftLevel->width - 1; ++column) {
            if (!shaft.levelRuntime().isSolid(column, row) &&
                shaft.levelRuntime().isSolid(column - 1, row) &&
                shaft.levelRuntime().isSolid(column + 1, row) &&
                !shaft.levelRuntime().rectHitsSolid(
                    column * 16.0F, (row - 2) * 16.0F, 16.0F, 32.0F)) {
                shaftColumn = column;
                shaftRow = row;
                break;
            }
        }
    }
    assert(shaftColumn >= 0 && shaftRow >= 0);
    const float shaftStartY = (shaftRow - 2) * 16.0F;
    shaft.debugSetPlayer(shaftColumn * 16.0F + 0.75F, shaftStartY,
                         0.0F, 60.0F, false);
    shaft.step(0.016F, PlatformerInput{});
    assert(shaft.snapshot().playerY > shaftStartY + 0.1F);

    PlatformerEngine tunnel;
    assert(tunnel.startCampaign(1, 2));
    stepFor(tunnel, 4500);
    assert(tunnel.phase() == PlatformerPhase::Running);
    tunnel.debugSetPlayerPower(PlatformerPlayerPower::Big);
    const auto* tunnelLevel = tunnel.levelRuntime().level();
    assert(tunnelLevel != nullptr);
    int tunnelColumn = -1;
    int tunnelRow = -1;
    for (int row = 1; row < tunnelLevel->height - 2 && tunnelColumn < 0;
         ++row) {
        for (int column = 0; column < tunnelLevel->width - 1; ++column) {
            if (tunnel.levelRuntime().isSolid(column, row - 1) &&
                tunnel.levelRuntime().isSolid(column + 1, row - 1) &&
                !tunnel.levelRuntime().isSolid(column, row) &&
                !tunnel.levelRuntime().isSolid(column + 1, row) &&
                !tunnel.levelRuntime().isSolid(column, row + 1) &&
                !tunnel.levelRuntime().isSolid(column + 1, row + 1) &&
                tunnel.levelRuntime().isSolid(column, row + 2) &&
                tunnel.levelRuntime().isSolid(column + 1, row + 2)) {
                tunnelColumn = column;
                tunnelRow = row;
                break;
            }
        }
    }
    assert(tunnelColumn >= 0 && tunnelRow >= 0);
    const float tunnelStartX = tunnelColumn * 16.0F;
    tunnel.debugSetPlayer(tunnelStartX, tunnelRow * 16.0F + 0.75F,
                          0.0F, 0.0F, true);
    tunnel.step(0.016F,
                PlatformerInput{1.0F, false, false, false, false, true});
    assert(tunnel.snapshot().playerX > tunnelStartX + 0.001F);
}

void testCampaignPipeTransitions() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSetPlayer(57.0F * 16.0F, 128.0F, 0.0F, 0.0F, true);
    engine.step(0.008F,
                PlatformerInput{0.0F, false, false, true});
    assert(engine.phase() == PlatformerPhase::Warping);
    assert(receivedEvent(engine, PlatformerEventType::WarpStarted));
    stepFor(engine, 440);
    auto underground = engine.snapshot();
    assert(underground.phase == PlatformerPhase::Running);
    assert(std::fabs(underground.playerX - 32.0F) < 0.1F);
    assert(std::fabs(underground.playerY - 320.0F) < 0.1F);
    assert(std::fabs(underground.cameraY - 288.0F) < 0.1F);
    assert(engine.levelRuntime().activeBackground() ==
           pgos::PlatformerBackgroundColor::Black);
    assert(engine.levelRuntime().activeLevelType() ==
           pgos::PlatformerLevelType::Underground);
    assert(receivedEvent(engine, PlatformerEventType::WarpCompleted));

    stepFor(engine, 456);
    engine.debugSetPlayer(192.0F, 456.0F, 0.0F, 0.0F, true);
    engine.step(0.008F,
                PlatformerInput{1.0F, false, false, false});
    assert(engine.phase() == PlatformerPhase::Warping);
    stepFor(engine, 880);
    const auto overworld = engine.snapshot();
    assert(overworld.phase == PlatformerPhase::Running);
    assert(std::fabs(overworld.cameraY) < 0.1F);
    assert(overworld.cameraX >= 159.0F * 16.0F - 0.1F);
    assert(engine.levelRuntime().activeBackground() ==
           pgos::PlatformerBackgroundColor::Blue);
    assert(engine.levelRuntime().activeLevelType() ==
           pgos::PlatformerLevelType::Overworld);
}

void testCastleTeleportLoops() {
    PlatformerEngine engine;
    assert(engine.startCampaign(8, 4));
    engine.debugSetPlayer(1598.8F, 96.0F, 200.0F, 0.0F, false);
    engine.step(0.008F,
                PlatformerInput{1.0F, false, false, false, false, true});
    const auto firstLoop = engine.snapshot();
    assert(firstLoop.playerX >= 32.0F * 16.0F);
    assert(firstLoop.playerX < 33.0F * 16.0F);

    engine.debugSetPlayer(2638.8F, 96.0F, 200.0F, 0.0F, false);
    engine.step(0.008F,
                PlatformerInput{1.0F, false, false, false, false, true});
    const auto secondLoop = engine.snapshot();
    assert(secondLoop.playerX >= 100.0F * 16.0F);
    assert(secondLoop.playerX < 101.0F * 16.0F);
}

void testUnderwaterMovement() {
    PlatformerEngine engine;
    assert(engine.startCampaign(2, 2));
    assert(engine.levelRuntime().activeLevelType() ==
           pgos::PlatformerLevelType::StartUnderground);
    engine.debugSetPlayer(144.0F, 192.0F, 0.0F, 0.0F, true);
    engine.step(0.008F,
                PlatformerInput{1.0F, false, false, false});
    assert(engine.phase() == PlatformerPhase::Warping);
    stepFor(engine, 440);
    assert(engine.levelRuntime().activeLevelType() ==
           pgos::PlatformerLevelType::Underwater);
    engine.debugSetPlayer(64.0F, 96.0F, 0.0F, 40.0F, false);
    engine.step(0.008F, PlatformerInput{0.0F, true, true});
    const auto swimming = engine.snapshot();
    assert(swimming.playerVy < -100.0F);
    assert(!swimming.grounded);
    stepFor(engine, 80);
    assert(engine.snapshot().playerVy < 0.0F);

    PlatformerEngine normal;
    assert(normal.startCampaign(1, 1));
    normal.debugSetPlayer(64.0F, 96.0F, 0.0F, 40.0F, false);
    normal.step(0.008F, PlatformerInput{});
    assert(normal.snapshot().playerVy > swimming.playerVy + 100.0F);
}

void testCampaignMovingPlatformsAndPulleys() {
    PlatformerEngine engine;
    assert(engine.startCampaign(3, 3));
    const auto start = engine.snapshot();
    assert(start.movingPlatformCount == 11U);
    assert(!engine.debugTileSolid(30, 4));

    const auto platform = engine.movingPlatform(0);
    assert(platform.active);
    assert(platform.widthTiles == 3U);
    assert(platform.motion == pgos::PlatformerMotionType::BackAndForth);
    engine.debugSetPlayer(platform.x + 8.0F,
                          platform.y - PlatformerEngine::PLAYER_HEIGHT,
                          0.0F, 0.0F, true);
    engine.step(0.008F, PlatformerInput{});
    const auto movedPlatform = engine.movingPlatform(0);
    const auto movedPlayer = engine.snapshot();
    assert(movedPlatform.x < platform.x);
    assert(std::fabs((movedPlayer.playerY + PlatformerEngine::PLAYER_HEIGHT) -
                     movedPlatform.y) < 0.1F);
    assert(movedPlayer.playerX < platform.x + 8.0F);

    const auto pulley = engine.movingPlatform(7);
    assert(pulley.pulley);
    assert(pulley.pairIndex == 8);
}

void testCampaignFireBars() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 4));
    const auto start = engine.snapshot();
    assert(start.fireBarCount == 7U);
    const auto initial = engine.fireBar(0);
    assert(initial.active && initial.length == 6U);
    engine.step(0.008F, PlatformerInput{});
    const auto rotated = engine.fireBar(0);
    assert(rotated.angleDegrees > initial.angleDegrees);

    constexpr float PI = 3.14159265358979323846F;
    const float radians = rotated.angleDegrees * PI / 180.0F;
    engine.debugSetPlayerPower(PlatformerPlayerPower::Big);
    engine.debugSetPlayer(rotated.x + std::cos(radians) * 16.0F + 5.0F,
                          rotated.y - std::sin(radians) * 16.0F + 5.0F,
                          0.0F, 0.0F, false);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.snapshot().playerPower == PlatformerPlayerPower::Small);
    assert(receivedEvent(engine, PlatformerEventType::PlayerHurt));
}

void testStartUndergroundIntro() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 2));
    const float startingX = engine.snapshot().playerX;
    stepFor(engine, 1200);
    assert(engine.snapshot().playerX > startingX + 30.0F);
    stepFor(engine, 3200);
    assert(engine.phase() == PlatformerPhase::Running);
    assert(engine.levelRuntime().activeLevelType() ==
           pgos::PlatformerLevelType::Underground);
    assert(engine.snapshot().cameraY > 250.0F);
}

void testCampaignTrampoline() {
    PlatformerEngine engine;
    assert(engine.startCampaign(2, 1));
    const auto* level = engine.levelRuntime().level();
    assert(level != nullptr);
    int16_t trampolineX = -1;
    int16_t trampolineY = -1;
    for (uint8_t row = 0; row < level->height; ++row) {
        for (uint16_t column = 0; column < level->width; ++column) {
            if (engine.levelRuntime().tile(column, row).kind ==
                pgos::PlatformerRuntimeTileKind::Trampoline) {
                trampolineX = static_cast<int16_t>(column * 16);
                trampolineY = static_cast<int16_t>(row * 16);
                break;
            }
        }
        if (trampolineX >= 0) {
            break;
        }
    }
    assert(trampolineX >= 0 && trampolineY >= 0);
    engine.debugSetPlayer(static_cast<float>(trampolineX),
                          static_cast<float>(trampolineY - 18), 0.0F, 200.0F,
                          false);
    engine.step(0.016F, PlatformerInput{});
    assert(engine.snapshot().playerVy < -250.0F);
}

void testCastleBridgeSequence() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 4));
    engine.debugSetPlayer(engine.goalX(), 48.0F, 0.0F, 0.0F, false);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::CastleBridge);
    stepFor(engine, 400);
    assert(engine.levelRuntime().modificationCount() > 0U);
    stepFor(engine, 4000);
    assert(engine.phase() == PlatformerPhase::TimeBonus ||
           engine.phase() == PlatformerPhase::Won);
}

void testCampaignVineRoute() {
    PlatformerEngine engine;
    assert(engine.startCampaign(5, 2));
    constexpr float blockX = 85.0F * 16.0F;
    constexpr float blockY = 20.0F * 16.0F;
    engine.debugSetPlayer(blockX, blockY + 17.0F, 0.0F, -200.0F, false);
    engine.step(0.016F, PlatformerInput{});
    assert(engine.snapshot().vineActive);
    stepFor(engine, 900);
    const auto grown = engine.vine();
    assert(grown.grownPixels >= 48.0F);

    engine.debugSetPlayer(grown.x, grown.baseY - grown.grownPixels + 8.0F,
                          0.0F, 0.0F, false);
    engine.step(0.008F, PlatformerInput{0.0F, true, true});
    assert(engine.phase() == PlatformerPhase::VineClimb);
    stepFor(engine, 920);
    assert(engine.phase() == PlatformerPhase::Running);
    assert(engine.snapshot().cameraX >= 81.0F * 16.0F - 0.1F);

    engine.debugSetPlayer(engine.snapshot().playerX, 13.0F * 16.0F + 2.0F,
                          0.0F, 0.0F, false);
    engine.step(0.008F, PlatformerInput{});
    assert(!engine.snapshot().vineActive);
    assert(engine.snapshot().playerX >= 130.0F * 16.0F - 0.1F);
    assert(engine.snapshot().cameraY >= 15.0F * 16.0F - 0.1F);
}

void testCampaignEnemyStateMachines() {
    PlatformerEngine wallTurn;
    assert(wallTurn.startCampaign(1, 1));
    wallTurn.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Koopa, 480.0F, 184.0F, 38U);
    wallTurn.step(0.016F, PlatformerInput{});
    assert(wallTurn.enemy(0).vx > 0.0F);
    assert(!wallTurn.enemy(0).facingLeft);

    PlatformerEngine paratroopa;
    assert(paratroopa.startCampaign(1, 1));
    paratroopa.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::KoopaParatroopa, 100.0F, 160.0F, 40U);
    paratroopa.debugSetPlayer(100.0F, 145.0F, 0.0F, 180.0F, false);
    paratroopa.step(0.016F, PlatformerInput{});
    assert(paratroopa.enemy(0).type == pgos::PlatformerEnemyType::Koopa);
    assert(paratroopa.enemy(0).motion == PlatformerEnemyMotion::Walking);
    paratroopa.debugSetPlayer(100.0F, paratroopa.enemy(0).y - 15.0F,
                              0.0F, 180.0F, false);
    paratroopa.step(0.016F, PlatformerInput{});
    assert(paratroopa.enemy(0).motion == PlatformerEnemyMotion::ShellIdle);

    PlatformerEngine piranha;
    assert(piranha.startCampaign(1, 1));
    piranha.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::PiranhaPlant, 220.0F, 128.0F, 44U);
    const float piranhaStartX = piranha.enemy(0).x;
    const float piranhaStartY = piranha.enemy(0).y;
    stepFor(piranha, 3300);
    assert(std::fabs(piranha.enemy(0).x - piranhaStartX) < 0.01F);
    assert(std::fabs(piranha.enemy(0).vx) < 0.01F);
    assert(piranha.enemy(0).y > piranhaStartY + 8.0F);

    PlatformerEngine hammer;
    assert(hammer.startCampaign(1, 1));
    hammer.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::HammerBro, 220.0F, 160.0F, 56U);
    stepFor(hammer, 2050);
    assert(hammer.snapshot().enemyHazardCount >= 1U);
    assert(hammer.enemyHazard(0).active);
    assert(hammer.enemyHazard(0).kind ==
           pgos::PlatformerEnemyHazardKind::Hammer);
}

}  // namespace

int main() {
    testGroundingAndJump();
    testStandingRemainsGrounded();
    testWalkingOffLedgeStartsFalling();
    testVariableJumpHeight();
    testWalkRunTurnAndCamera();
    testQuestionBoxWinsAdjacentOverlap();
    testPauseFreezesSimulation();
    testReferenceLevelData();
    testCheckpointEnemySpawnClearsPipeEdge();
    testMapPanoramaMode();
    testCoinBoxAndMultiCoinBrick();
    testBrickBreakAndSpecialContents();
    testGrowthFireAndCrouch();
    testFireFlowerRestsAndHiddenBlockBecomesFloor();
    testDamageDeathAndLifeRestart();
    testCrouchingDamagePreservesPlayerFeet();
    testKoopaShellLifecycle();
    testSimultaneousEnemyStomp();
    testSquashedEnemyStopsCollidingImmediately();
    testStompSuppressesSameStepSideDamage();
    testCampaignBlockSeamAdvancesPastBrokenTile();
    testCampaignFlagUsesMovingFlagAnchor();
    testFireballPoolReusesExpiredSlots();
    testTimerAndCompleteGoalSequence();
    testCampaignRuntimeAndLevelAdvance();
    testReferenceTileRoundnessAllowsTightOpenings();
    testCampaignPipeTransitions();
    testCastleTeleportLoops();
    testUnderwaterMovement();
    testCampaignMovingPlatformsAndPulleys();
    testCampaignFireBars();
    testStartUndergroundIntro();
    testCampaignTrampoline();
    testCastleBridgeSequence();
    testCampaignVineRoute();
    testCampaignEnemyStateMachines();
    return 0;
}
