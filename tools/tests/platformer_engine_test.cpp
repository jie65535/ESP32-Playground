#include "games/PlatformerEngine.h"

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
    assert(engine.snapshot().playerCrouching);
    assert(engine.snapshot().playerY > standingY + 9.0F);
    stepFor(engine, 40);
    assert(!engine.snapshot().playerCrouching);

    engine.debugHitBox(32);
    assert(engine.powerup(0).kind == PlatformerPowerupKind::FireFlower);
    const auto flower = engine.powerup(0);
    engine.debugSetPlayer(flower.x, flower.y, 0.0F, 0.0F, false);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.snapshot().playerFire);
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
    assert(receivedEvent(engine, PlatformerEventType::PlayerHurt));

    stepFor(engine, 1200);
    const auto vulnerable = engine.snapshot();
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

    engine.debugSetPlayer(78.0F, 185.0F, 0.0F, 0.0F, true);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.enemy(8).motion == PlatformerEnemyMotion::ShellSliding);
    assert(std::fabs(engine.enemy(8).vx) > 150.0F);
    assert(receivedEvent(engine, PlatformerEventType::ShellKicked));
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
    testDamageDeathAndLifeRestart();
    testKoopaShellLifecycle();
    testSimultaneousEnemyStomp();
    testFireballPoolReusesExpiredSlots();
    testTimerAndCompleteGoalSequence();
    return 0;
}
