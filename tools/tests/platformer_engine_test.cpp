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

static_assert(pgos::platformerReferenceAnimationFrame(0U, 8U, 4U) == 0U);
static_assert(pgos::platformerReferenceAnimationFrame(1U, 8U, 4U) == 1U);
static_assert(pgos::platformerReferenceAnimationFrame(8U, 8U, 4U) == 1U);
static_assert(pgos::platformerReferenceAnimationFrame(9U, 8U, 4U) == 2U);
static_assert(
    pgos::platformerReferencePausedAnimationFrame(25U, 8U, 4U, 25U) == 0U);
static_assert(
    pgos::platformerReferencePausedAnimationFrame(57U, 8U, 4U, 25U) == 0U);
static_assert(
    pgos::platformerReferencePausedAnimationFrame(58U, 8U, 4U, 25U) == 1U);

void stepFor(PlatformerEngine& engine, uint32_t milliseconds,
             const PlatformerInput& input = {}) {
    const uint32_t frames =
        (milliseconds * 60U + 999U) / 1000U;
    for (uint32_t frame = 0; frame < frames; ++frame) {
        engine.step(1.0F / 60.0F, input);
    }
}

void stepReferenceFrames(PlatformerEngine& engine, uint16_t frames,
                         const PlatformerInput& input = {}) {
    for (uint16_t frame = 0; frame < frames; ++frame) {
        engine.step(1.0F / 60.0F, input);
    }
}

uint16_t finishWarp(PlatformerEngine& engine) {
    uint16_t frames = 0U;
    while (engine.phase() == PlatformerPhase::Warping && frames < 256U) {
        engine.step(1.0F / 60.0F, PlatformerInput{});
        ++frames;
    }
    assert(engine.phase() != PlatformerPhase::Warping);
    return frames;
}

bool receivedEvent(PlatformerEngine& engine, PlatformerEventType type) {
    PlatformerEvent event;
    bool received = false;
    while (engine.pollEvent(event)) {
        received |= event.type == type;
    }
    return received;
}

bool takeEvent(PlatformerEngine& engine, PlatformerEventType type,
               PlatformerEvent& matched) {
    PlatformerEvent event;
    bool received = false;
    while (engine.pollEvent(event)) {
        if (event.type == type) {
            matched = event;
            received = true;
        }
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

uint8_t activeEffectKindCount(const PlatformerEngine& engine,
                              PlatformerEffectKind kind) {
    uint8_t count = 0;
    for (uint8_t index = 0; index < engine.snapshot().effectCount; ++index) {
        const auto effect = engine.effect(index);
        count += effect.active && effect.kind == kind ? 1U : 0U;
    }
    return count;
}

uint8_t activeEnemyTypeCount(const PlatformerEngine& engine,
                             pgos::PlatformerEnemyType type) {
    uint8_t count = 0;
    for (uint8_t index = 0; index < engine.snapshot().enemyCount; ++index) {
        const auto enemy = engine.enemy(index);
        count += enemy.active && enemy.type == type ? 1U : 0U;
    }
    return count;
}

void moveCampaignEnemiesAway(PlatformerEngine& engine) {
    const uint8_t count = engine.snapshot().enemyCount;
    for (uint8_t index = 0U; index < count; ++index) {
        engine.debugActivateEnemy(index, -1000.0F - index * 32.0F,
                                  -1000.0F,
                                  PlatformerEnemyMotion::Walking);
    }
}

void becomeFireMarioThroughPowerup(PlatformerEngine& engine) {
    engine.step(1.0F / 60.0F, PlatformerInput{});
    moveCampaignEnemiesAway(engine);
    engine.debugSetPlayerPower(PlatformerPlayerPower::Big);
    const auto player = engine.snapshot();
    engine.debugSpawnPowerup(PlatformerPowerupKind::FireFlower,
                             player.playerX, player.playerY);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.snapshot().playerPowerTransition ==
           pgos::PlatformerPowerTransition::Fire);
    stepReferenceFrames(engine, 60U);
    const auto fire = engine.snapshot();
    assert(fire.playerPower == PlatformerPlayerPower::Fire);
    assert(fire.playerPowerTransition ==
           pgos::PlatformerPowerTransition::None);
    assert(!fire.playerCrouching);
}

uint8_t expectedLoadedCampaignEnemies(
    const pgos::PlatformerCampaignLevel& level) {
    uint8_t count = 0U;
    const float cameraX = static_cast<float>(level.cameraStart.x * 16);
    const float cameraY = static_cast<float>(level.cameraStart.y * 16);
    for (uint8_t row = 0U; row < level.height; ++row) {
        for (uint16_t column = 0U; column < level.width; ++column) {
            const uint16_t source = pgos::platformerCampaignTileAt(
                level, pgos::PlatformerMapLayer::Enemies, column, row);
            if (!pgos::platformerEnemySourceCreatesEntity(source)) {
                continue;
            }
            const uint16_t reference =
                pgos::PLATFORMER_ENEMY_REFERENCE_IDS[source];
            if (reference == 90U) {
                const float x = static_cast<float>(column * 16);
                const float y = static_cast<float>(row * 16);
                if (x + 16.0F < cameraX || x > cameraX + 320.0F ||
                    y + 16.0F < cameraY || y > cameraY + 240.0F) {
                    continue;
                }
            }
            ++count;
        }
    }
    return count;
}

struct ReferenceGroundMotion {
    float x = 0.0F;
    float velocity = 0.0F;
    float acceleration = 0.0F;

    void step(float axis, bool running, bool crouching = false) {
        constexpr float POSITION_SCALE = 0.5F;
        constexpr float FRICTION = 0.94F;
        constexpr float BASE_ACCELERATION = 0.24F;
        constexpr float WALK_MULTIPLIER = 0.7978723404255319148936F;
        constexpr float RUN_MULTIPLIER = 1.3297872340425531914F;
        constexpr float MAX_SPEED = 10.0F;

        x += velocity * POSITION_SCALE;
        velocity += acceleration;
        velocity *= FRICTION;
        velocity = std::max(-MAX_SPEED, std::min(MAX_SPEED, velocity));
        if (std::fabs(velocity) < BASE_ACCELERATION * 0.5F &&
            acceleration == 0.0F) {
            velocity = 0.0F;
        }
        acceleration = axis * BASE_ACCELERATION *
                       (running ? RUN_MULTIPLIER : WALK_MULTIPLIER);
        if (crouching) {
            acceleration = 0.0F;
            if (velocity > 1.5F) {
                velocity -= 0.5F;
            } else if (velocity < -1.5F) {
                velocity += 0.5F;
            }
        }
    }

    float deviceVelocity() const {
        // The reference moves in 32px tiles at 60Hz; PGOS uses 16px tiles.
        return velocity * 30.0F;
    }
};

struct ReferenceJumpMotion {
    float y = 185.0F;
    float velocity = 0.0F;
    float acceleration = 0.0F;
    bool grounded = true;

    void step(bool jumpPressed, bool jumpHeld, bool running = false,
              float horizontalVelocity = 0.0F) {
        constexpr float POSITION_SCALE = 0.5F;
        constexpr float GRAVITY = 0.575F;
        constexpr float MAX_FALL_SPEED = 7.5F;
        constexpr float FLOOR_Y = 201.0F;
        constexpr float PLAYER_HEIGHT = 16.0F;

        velocity += GRAVITY;
        y += velocity * POSITION_SCALE;
        velocity += acceleration;
        velocity = std::min(velocity, MAX_FALL_SPEED);
        if (y + PLAYER_HEIGHT >= FLOOR_Y) {
            y = FLOOR_Y - PLAYER_HEIGHT;
            velocity = 0.0F;
            acceleration = 0.0F;
            grounded = true;
        } else {
            grounded = false;
        }

        if (grounded) {
            if (jumpPressed) {
                velocity = -7.3F;
                grounded = false;
            }
        } else {
            acceleration = jumpHeld && velocity < -1.0F
                               ? (running && std::fabs(horizontalVelocity) > 3.5F
                                      ? -0.414F
                                      : -0.412F)
                               : 0.0F;
        }
    }

    float deviceVelocity() const {
        return velocity * 30.0F;
    }
};

struct ReferenceWaterMotion {
    float x = 64.0F;
    float y = 96.0F;
    float velocityX = 0.0F;
    float velocityY = 0.0F;
    float accelerationY = 0.0F;

    void step(float axis, bool jumpPressed) {
        constexpr float POSITION_SCALE = 0.5F;
        constexpr float GRAVITY = 0.575F;
        constexpr float PID_GAIN = 0.20F + 0.02F / 60.0F;

        velocityY += GRAVITY;
        x += velocityX * POSITION_SCALE;
        y += velocityY * POSITION_SCALE;
        velocityY += accelerationY;
        velocityY = std::min(velocityY, 7.5F);

        velocityX += (axis * 3.0F - velocityX) * PID_GAIN;
        accelerationY = -0.45480F;
        velocityY = std::min(velocityY, 2.0F);
        if (jumpPressed) {
            velocityY = -3.53F;
        }
    }

    float deviceVelocityX() const {
        return velocityX * 30.0F;
    }

    float deviceVelocityY() const {
        return velocityY * 30.0F;
    }
};

void testReferenceGroundMotionTrace() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);

    ReferenceGroundMotion reference;
    reference.x = engine.snapshot().playerX;
    float referenceCameraX = engine.snapshot().cameraX;
    constexpr float REFERENCE_DT = 1.0F / 60.0F;
    for (uint16_t frame = 0; frame < 180U; ++frame) {
        PlatformerInput input;
        if (frame < 60U) {
            input.moveAxis = 1.0F;
        } else if (frame < 120U) {
            input.moveAxis = 1.0F;
            input.actionHeld = true;
        } else if (frame >= 150U) {
            input.moveAxis = -1.0F;
            input.actionHeld = true;
        }

        reference.step(input.moveAxis, input.actionHeld);
        if (reference.x + PlatformerEngine::PLAYER_WIDTH * 0.5F >
                referenceCameraX + PlatformerEngine::VIEWPORT_WIDTH * 0.5F &&
            reference.velocity > 0.0F) {
            referenceCameraX += reference.deviceVelocity() * REFERENCE_DT;
        }
        engine.step(REFERENCE_DT, input);
        const auto actual = engine.snapshot();
        assert(std::fabs(actual.playerX - reference.x) <= 0.05F);
        assert(std::fabs(actual.playerVx - reference.deviceVelocity()) <=
               0.05F);
        assert(std::fabs(actual.cameraX - referenceCameraX) <= 0.05F);
    }
}

void testReferenceJumpMotionTrace() {
    for (const bool holdJump : {false, true}) {
        PlatformerEngine engine;
        engine.start();
        stepFor(engine, 240);

        ReferenceJumpMotion reference;
        for (uint16_t frame = 0; frame < 120U; ++frame) {
            PlatformerInput input;
            input.jumpPressed = frame == 0U;
            input.jumpHeld = holdJump && frame > 0U && frame < 90U;
            reference.step(input.jumpPressed, input.jumpHeld);
            engine.step(1.0F / 60.0F, input);

            const auto actual = engine.snapshot();
            assert(std::fabs(actual.playerY - reference.y) <= 0.05F);
            assert(std::fabs(actual.playerVy - reference.deviceVelocity()) <=
                   0.05F);
            assert(actual.grounded == reference.grounded);
        }
    }
}

void testReferenceHasNoCoyoteTimeOrJumpBuffer() {
    PlatformerEngine coyote;
    coyote.start();
    coyote.debugSetPlayer(1105.0F, 185.0F, 0.0F, 0.0F, true);
    stepReferenceFrames(coyote, 1);
    assert(!coyote.snapshot().grounded);
    coyote.step(1.0F / 60.0F,
                 PlatformerInput{0.0F, true, false});
    assert(coyote.snapshot().playerVy > 0.0F);
    assert(!receivedEvent(coyote, PlatformerEventType::Jumped));

    PlatformerEngine buffered;
    buffered.start();
    buffered.debugSetPlayer(100.0F, 180.0F, 0.0F, 150.0F, false);
    buffered.step(1.0F / 60.0F,
                  PlatformerInput{0.0F, true, false});
    stepReferenceFrames(buffered, 8);
    assert(buffered.snapshot().grounded);
    assert(std::fabs(buffered.snapshot().playerVy) < 0.05F);
    assert(!receivedEvent(buffered, PlatformerEventType::Jumped));
}

void testReferenceCrouchMotionAndHitbox() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugSetPlayerPower(PlatformerPlayerPower::Big);

    ReferenceGroundMotion reference;
    reference.x = engine.snapshot().playerX;
    for (uint16_t frame = 0; frame < 90U; ++frame) {
        const bool crouching = frame >= 60U;
        PlatformerInput input;
        input.moveAxis = 1.0F;
        input.actionHeld = true;
        input.crouchHeld = crouching;
        reference.step(input.moveAxis, input.actionHeld, crouching);
        engine.step(1.0F / 60.0F, input);
        assert(std::fabs(engine.snapshot().playerX - reference.x) < 0.05F);
        assert(std::fabs(engine.snapshot().playerVx -
                         reference.deviceVelocity()) < 0.05F);
    }
    assert(engine.snapshot().playerCrouching);

    PlatformerEngine airborne;
    airborne.start();
    airborne.debugSetPlayerPower(PlatformerPlayerPower::Big);
    airborne.debugSetPlayer(100.0F, 100.0F, 0.0F, 0.0F, false);
    const float standingBottom =
        airborne.snapshot().playerY + PlatformerEngine::BIG_PLAYER_HEIGHT;
    airborne.step(1.0F / 60.0F,
                  PlatformerInput{0.0F, false, false, true});
    const auto crouched = airborne.snapshot();
    assert(crouched.playerCrouching);
    assert(std::fabs(crouched.playerY +
                         PlatformerEngine::CROUCH_PLAYER_HEIGHT -
                     (standingBottom + 0.575F * 0.5F)) < 0.05F);
    airborne.step(1.0F / 60.0F, PlatformerInput{});
    assert(!airborne.snapshot().playerCrouching);

    PlatformerEngine crouchJump;
    crouchJump.start();
    stepFor(crouchJump, 240);
    crouchJump.debugSetPlayerPower(PlatformerPlayerPower::Big);
    crouchJump.step(1.0F / 60.0F,
                    PlatformerInput{0.0F, true, false, true});
    assert(crouchJump.snapshot().playerCrouching);
    assert(!crouchJump.snapshot().grounded);
    assert(std::fabs(crouchJump.snapshot().playerVy - (-7.3F * 30.0F)) <
           0.05F);
}

void testReferenceUnderwaterMotionTrace() {
    PlatformerEngine engine;
    assert(engine.startCampaign(2, 2));
    engine.debugSetPlayer(144.0F, 192.0F, 0.0F, 0.0F, true);
    engine.step(1.0F / 60.0F,
                PlatformerInput{1.0F, false, false, false});
    assert(engine.phase() == PlatformerPhase::Warping);
    finishWarp(engine);
    assert(engine.levelRuntime().activeLevelType() ==
           pgos::PlatformerLevelType::Underwater);
    engine.debugSetPlayer(64.0F, 96.0F, 0.0F, 0.0F, false);

    ReferenceWaterMotion reference;
    for (uint16_t frame = 0; frame < 60U; ++frame) {
        PlatformerInput input;
        input.moveAxis = frame < 45U ? 1.0F : 0.0F;
        input.jumpPressed = frame == 0U || frame == 20U;
        reference.step(input.moveAxis, input.jumpPressed);
        engine.step(1.0F / 60.0F, input);
        const auto actual = engine.snapshot();
        assert(std::fabs(actual.playerX - reference.x) < 0.05F);
        assert(std::fabs(actual.playerY - reference.y) < 0.05F);
        assert(std::fabs(actual.playerVx - reference.deviceVelocityX()) <
               0.05F);
        assert(std::fabs(actual.playerVy - reference.deviceVelocityY()) <
               0.05F);
    }
}

void testReferenceIndependentAnimationClocks() {
    PlatformerEngine player;
    player.start();
    stepFor(player, 240U);
    const PlatformerInput walk{1.0F, false, false, false};
    player.step(1.0F / 60.0F, walk);
    assert(player.snapshot().playerWalking);
    assert(player.snapshot().playerAnimationFrame == 1U);
    stepReferenceFrames(player, 7U, walk);
    assert(player.snapshot().playerAnimationFrame == 1U);
    stepReferenceFrames(player, 1U, walk);
    assert(player.snapshot().playerAnimationFrame == 2U);

    uint16_t coastFrames = 0U;
    while (player.snapshot().playerWalking && coastFrames < 180U) {
        player.step(1.0F / 60.0F, PlatformerInput{});
        ++coastFrames;
    }
    assert(coastFrames < 180U);
    assert(player.snapshot().playerAnimationFrame == 0U);
    player.step(1.0F / 60.0F, walk);
    assert(player.snapshot().playerWalking);
    assert(player.snapshot().playerAnimationFrame == 1U);
    player.togglePause();
    const uint8_t pausedPlayerFrame =
        player.snapshot().playerAnimationFrame;
    stepReferenceFrames(player, 30U);
    assert(player.snapshot().playerAnimationFrame == pausedPlayerFrame);

    PlatformerEngine enemy;
    assert(enemy.startCampaign(1, 1));
    enemy.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::PiranhaPlant, 100.0F, 128.0F, 44U);
    enemy.step(1.0F / 60.0F, PlatformerInput{});
    assert(enemy.enemy(0).animationFrame == 1U);
    enemy.debugSetCamera(1000.0F);
    stepReferenceFrames(enemy, 20U);
    assert(enemy.enemy(0).active);
    assert(enemy.enemy(0).animationFrame == 1U);
    enemy.debugSetCamera(0.0F);
    stepReferenceFrames(enemy, 14U);
    assert(enemy.enemy(0).animationFrame == 1U);
    stepReferenceFrames(enemy, 1U);
    assert(enemy.enemy(0).animationFrame == 0U);

    PlatformerEngine powerup;
    powerup.start();
    powerup.debugSpawnPowerup(PlatformerPowerupKind::Star,
                              500.0F, 100.0F);
    powerup.step(1.0F / 60.0F, PlatformerInput{});
    assert(powerup.powerup(0).active);
    assert(powerup.powerup(0).animationFrame == 0U);
    powerup.debugSetCamera(400.0F);
    powerup.step(1.0F / 60.0F, PlatformerInput{});
    assert(powerup.powerup(0).animationFrame == 1U);
}

void testReferenceBasicEnemyAndShellMotion() {
    PlatformerEngine enemy;
    enemy.start();
    stepFor(enemy, 240);
    enemy.debugActivateEnemy(0, 100.0F, 185.0F,
                             PlatformerEnemyMotion::Walking);
    enemy.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(enemy.enemy(0).x - 99.5F) < 0.05F);
    assert(std::fabs(enemy.enemy(0).vx - (-30.0F)) < 0.05F);

    PlatformerEngine shell;
    shell.start();
    stepFor(shell, 240);
    shell.debugActivateEnemy(8, 100.0F, 185.0F,
                             PlatformerEnemyMotion::ShellIdle);
    shell.debugSetPlayer(85.0F, 185.0F, 0.0F, 0.0F, true);
    shell.step(1.0F / 60.0F, PlatformerInput{});
    assert(shell.enemy(8).motion == PlatformerEnemyMotion::ShellSliding);
    assert(std::fabs(shell.enemy(8).vx - 180.0F) < 0.05F);
}

void testReferenceOrderedEnemyPairCollisions() {
    PlatformerEngine turn;
    assert(turn.startCampaign(1, 1));
    turn.debugSetPlayer(300.0F, 100.0F, 0.0F, 0.0F, false);
    const uint8_t leftIndex = turn.snapshot().enemyCount;
    turn.debugSpawnCampaignEnemy(pgos::PlatformerEnemyType::Goomba,
                                 100.0F, 100.0F, 70U);
    const uint8_t rightIndex = turn.snapshot().enemyCount;
    turn.debugSpawnCampaignEnemy(pgos::PlatformerEnemyType::Goomba,
                                 115.0F, 100.0F, 70U);
    turn.debugSetEnemyVelocity(leftIndex, 60.0F, 0.0F);
    turn.debugSetEnemyVelocity(rightIndex, -60.0F, 0.0F);

    stepReferenceFrames(turn, 1);
    assert(std::fabs(turn.enemy(leftIndex).vx - 60.0F) < 0.05F);
    assert(std::fabs(turn.enemy(rightIndex).vx - 30.0F) < 0.05F);
    stepReferenceFrames(turn, 1);
    assert(std::fabs(turn.enemy(leftIndex).vx - (-30.0F)) < 0.05F);
    assert(std::fabs(turn.enemy(rightIndex).vx - 30.0F) < 0.05F);

    PlatformerEngine spinyImmune;
    assert(spinyImmune.startCampaign(1, 1));
    spinyImmune.debugSetPlayer(300.0F, 100.0F, 0.0F, 0.0F, false);
    const uint8_t spinyIndex = spinyImmune.snapshot().enemyCount;
    spinyImmune.debugSpawnCampaignEnemy(pgos::PlatformerEnemyType::Spiny,
                                        108.0F, 100.0F, 500U);
    const uint8_t shellIndex = spinyImmune.snapshot().enemyCount;
    spinyImmune.debugSpawnCampaignEnemy(pgos::PlatformerEnemyType::Koopa,
                                        100.0F, 100.0F, 38U);
    spinyImmune.debugActivateEnemy(
        shellIndex, 100.0F, 100.0F,
        PlatformerEnemyMotion::ShellSliding);
    stepReferenceFrames(spinyImmune, 1);
    assert(spinyImmune.enemy(spinyIndex).active);
    assert(spinyImmune.enemy(spinyIndex).motion ==
           PlatformerEnemyMotion::Walking);
}

void testReferenceStompBounceVelocity() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugActivateEnemy(0, 90.0F, 185.0F,
                              PlatformerEnemyMotion::Walking);
    engine.debugSetPlayer(90.0F, 168.0F, 0.0F, 90.0F, false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.enemy(0).motion == PlatformerEnemyMotion::Squashed);
    assert(std::fabs(engine.snapshot().playerVy - (-3.5F * 30.0F)) <
           0.05F);
    assert(engine.snapshot().score == 100U);
    assert(activeEffectKindCount(engine, PlatformerEffectKind::Score) == 1U);
}

void testReferenceShellAndCrushableRules() {
    PlatformerEngine idleShell;
    idleShell.start();
    stepFor(idleShell, 240);
    idleShell.debugActivateEnemy(8, 90.0F, 185.0F,
                                 PlatformerEnemyMotion::ShellIdle);
    idleShell.debugSetPlayer(90.0F, 168.0F, 0.0F, 90.0F, false);
    idleShell.step(1.0F / 60.0F, PlatformerInput{});
    assert(idleShell.enemy(8).motion ==
           PlatformerEnemyMotion::ShellSliding);
    assert(std::fabs(idleShell.enemy(8).vx - 180.0F) < 0.05F);
    assert(idleShell.snapshot().playerVy > 0.0F);

    PlatformerEngine bullet;
    assert(bullet.startCampaign(1, 1));
    bullet.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::BulletBill, 100.0F, 185.0F, 90U);
    bullet.debugSetPlayer(100.0F, 184.0F, 0.0F, 90.0F, false);
    bullet.step(1.0F / 60.0F, PlatformerInput{});
    assert(bullet.phase() == PlatformerPhase::Running);
    assert(std::fabs(bullet.snapshot().playerVy - (-3.5F * 30.0F)) <
           0.05F);
    assert(bullet.snapshot().score == 100U);
}

void testReferencePowerupEmergenceAndMotion() {
    PlatformerEngine engine;
    engine.start();
    engine.debugSpawnPowerup(PlatformerPowerupKind::Mushroom, 100.0F,
                             120.0F);
    stepReferenceFrames(engine, 32);
    auto powerup = engine.powerup(0);
    assert(powerup.state == pgos::PlatformerPowerupState::Emerging);
    assert(std::fabs(powerup.y - 104.0F) < 0.05F);

    stepReferenceFrames(engine, 1);
    powerup = engine.powerup(0);
    assert(powerup.state == pgos::PlatformerPowerupState::Moving);
    assert(std::fabs(powerup.y - 103.5F) < 0.05F);
    assert(std::fabs(powerup.vx - 60.0F) < 0.05F);

    stepReferenceFrames(engine, 1);
    powerup = engine.powerup(0);
    assert(std::fabs(powerup.x - 101.0F) < 0.05F);
    assert(std::fabs(powerup.vy - (0.575F * 30.0F)) < 0.05F);

    PlatformerEngine campaign;
    assert(campaign.startCampaign(1, 1));
    campaign.debugSpawnPowerup(PlatformerPowerupKind::Mushroom,
                               50.0F, 192.0F);
    stepReferenceFrames(campaign, 90U);
    const auto landed = campaign.powerup(0);
    assert(landed.state == pgos::PlatformerPowerupState::Moving);
    assert(std::fabs(landed.y - 192.0F) < 0.05F);
    assert(std::fabs(landed.vy) < 0.05F);

    PlatformerEngine star;
    assert(star.startCampaign(1, 1));
    star.debugSpawnPowerup(PlatformerPowerupKind::Star,
                           100.0F, 192.0F);
    uint16_t bounceFrames = 0U;
    while (star.powerup(0).state !=
               pgos::PlatformerPowerupState::Bouncing &&
           bounceFrames < 120U) {
        star.step(1.0F / 60.0F, PlatformerInput{});
        ++bounceFrames;
    }
    assert(bounceFrames < 120U);
    assert(std::fabs(star.powerup(0).y - 192.0F) < 0.05F);
    assert(std::fabs(star.powerup(0).vy + 300.0F) < 0.05F);
}

void testReferencePiranhaFrameCycle() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::PiranhaPlant, 220.0F, 128.0F, 44U);

    stepReferenceFrames(engine, 179);
    assert(std::fabs(engine.enemy(0).y - 128.0F) < 0.05F);
    stepReferenceFrames(engine, 1);
    assert(std::fabs(engine.enemy(0).y - 128.0F) < 0.05F);
    assert(std::fabs(engine.enemy(0).vy - 30.0F) < 0.05F);

    stepReferenceFrames(engine, 64);
    assert(std::fabs(engine.enemy(0).y - 160.0F) < 0.05F);
    stepReferenceFrames(engine, 116);
    assert(std::fabs(engine.enemy(0).y - 160.0F) < 0.05F);
    assert(std::fabs(engine.enemy(0).vy - (-30.0F)) < 0.05F);
    stepReferenceFrames(engine, 64);
    assert(std::fabs(engine.enemy(0).y - 128.0F) < 0.05F);

    PlatformerEngine offscreen;
    assert(offscreen.startCampaign(1, 1));
    offscreen.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::PiranhaPlant, 400.0F, 128.0F, 44U);
    stepReferenceFrames(offscreen, 244);
    assert(std::fabs(offscreen.enemy(0).y - 160.0F) < 0.05F);
    assert(std::fabs(offscreen.enemy(0).vy) < 0.05F);
}

void testReferenceBlooperAttackCycle() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Blooper, 0.0F, 0.0F, 48U);
    engine.debugSetPlayer(100.0F, 185.0F, 0.0F, 0.0F, true);

    stepReferenceFrames(engine, 59);
    assert(std::fabs(engine.enemy(0).vx) < 0.05F);
    stepReferenceFrames(engine, 1);
    assert(std::fabs(engine.enemy(0).vx - 90.0F) < 0.05F);
    assert(std::fabs(engine.enemy(0).vy - 90.0F) < 0.05F);

    stepReferenceFrames(engine, 29);
    assert(std::fabs(engine.enemy(0).vx - 90.0F) < 0.05F);
    stepReferenceFrames(engine, 1);
    assert(std::fabs(engine.enemy(0).x - 39.0F) < 0.05F);
    assert(std::fabs(engine.enemy(0).vx) < 0.05F);
    assert(std::fabs(engine.enemy(0).vy) < 0.05F);
}

void testReferenceLakituControlAndSpinyDelay() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Lakitu, 220.0F, 80.0F, 50U);
    engine.debugSetPlayer(240.0F, 100.0F, 0.0F, 0.0F, false);

    stepReferenceFrames(engine, 1);
    assert(engine.enemy(0).facingLeft);
    assert(std::fabs(engine.enemy(0).x - 220.0F) < 0.05F);
    assert(std::fabs(engine.enemy(0).vx - (-180.0F)) < 0.05F);
    stepReferenceFrames(engine, 1);
    assert(std::fabs(engine.enemy(0).x - 217.0F) < 0.05F);

    stepReferenceFrames(engine, 178);
    const auto enteredCloud = engine.enemy(0);
    assert(enteredCloud.alternatePose);
    assert(std::fabs(enteredCloud.y - 96.0F) < 0.05F);
    assert(std::fabs(enteredCloud.height - 16.0F) < 0.05F);

    stepReferenceFrames(engine, 45);
    assert(activeEnemyTypeCount(engine, pgos::PlatformerEnemyType::Spiny) ==
           0U);
    stepReferenceFrames(engine, 1);
    assert(activeEnemyTypeCount(engine, pgos::PlatformerEnemyType::Spiny) ==
           1U);
    const auto emerged = engine.enemy(0);
    pgos::PlatformerEnemyState spiny;
    bool foundSpiny = false;
    for (uint8_t index = 0; index < engine.snapshot().enemyCount; ++index) {
        const auto candidate = engine.enemy(index);
        if (candidate.active &&
            candidate.type == pgos::PlatformerEnemyType::Spiny) {
            spiny = candidate;
            foundSpiny = true;
            break;
        }
    }
    assert(foundSpiny);
    assert(!emerged.alternatePose);
    assert(std::fabs(emerged.y - 80.0F) < 0.05F);
    assert(std::fabs(emerged.height - 32.0F) < 0.05F);
    assert(std::fabs(spiny.x - emerged.x) < 0.05F);
    assert(std::fabs(spiny.y - emerged.y) < 0.05F);
    assert(std::fabs(spiny.vx -
                     (emerged.facingLeft ? 75.0F : -75.0F)) < 0.05F);
    assert(std::fabs(spiny.vy) < 0.05F);
    assert(spiny.bornFrame == engine.snapshot().logicFrame);

    PlatformerEngine missedOffscreenCycle;
    assert(missedOffscreenCycle.startCampaign(1, 1));
    missedOffscreenCycle.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Lakitu, 500.0F, 80.0F, 50U);
    missedOffscreenCycle.debugSetPlayer(100.0F, 100.0F, 0.0F, 0.0F,
                                        false);
    stepReferenceFrames(missedOffscreenCycle, 180);
    assert(!missedOffscreenCycle.enemy(0).alternatePose);
    missedOffscreenCycle.debugSetCamera(400.0F);
    stepReferenceFrames(missedOffscreenCycle, 179);
    assert(!missedOffscreenCycle.enemy(0).alternatePose);
    stepReferenceFrames(missedOffscreenCycle, 1);
    assert(missedOffscreenCycle.enemy(0).alternatePose);

    PlatformerEngine sequenceContinuesOffscreen;
    assert(sequenceContinuesOffscreen.startCampaign(1, 1));
    sequenceContinuesOffscreen.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Lakitu, 220.0F, 80.0F, 50U);
    sequenceContinuesOffscreen.debugSetPlayer(240.0F, 100.0F, 0.0F, 0.0F,
                                              false);
    stepReferenceFrames(sequenceContinuesOffscreen, 180);
    assert(sequenceContinuesOffscreen.enemy(0).alternatePose);
    sequenceContinuesOffscreen.debugSetPlayer(500.0F, 185.0F, 0.0F, 0.0F,
                                              true);
    sequenceContinuesOffscreen.debugSetCamera(400.0F);
    stepReferenceFrames(sequenceContinuesOffscreen, 46);
    assert(!sequenceContinuesOffscreen.enemy(0).alternatePose);
    assert(activeEnemyTypeCount(sequenceContinuesOffscreen,
                                pgos::PlatformerEnemyType::Spiny) == 1U);
    stepReferenceFrames(sequenceContinuesOffscreen, 1);
    assert(activeEnemyTypeCount(sequenceContinuesOffscreen,
                                pgos::PlatformerEnemyType::Spiny) == 0U);

    PlatformerEngine crushedDuringSequence;
    assert(crushedDuringSequence.startCampaign(1, 1));
    crushedDuringSequence.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Lakitu, 220.0F, 80.0F, 50U);
    crushedDuringSequence.debugSetPlayer(240.0F, 100.0F, 0.0F, 0.0F,
                                         false);
    stepReferenceFrames(crushedDuringSequence, 180U);
    const auto cloudLakitu = crushedDuringSequence.enemy(0);
    crushedDuringSequence.debugSetPlayer(cloudLakitu.x, cloudLakitu.y - 12.0F,
                                         0.0F, 90.0F, false);
    stepReferenceFrames(crushedDuringSequence, 1U);
    assert(crushedDuringSequence.enemy(0).motion ==
           PlatformerEnemyMotion::FallingDefeated);
    stepReferenceFrames(crushedDuringSequence, 45U);
    assert(activeEnemyTypeCount(crushedDuringSequence,
                                pgos::PlatformerEnemyType::Spiny) == 1U);
}

void testReferenceParatroopaAndProjectileEnemyMotion() {
    PlatformerEngine paratroopa;
    assert(paratroopa.startCampaign(1, 1));
    paratroopa.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::KoopaParatroopa, 100.0F, 184.0F, 40U);
    paratroopa.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(paratroopa.enemy(0).vy - (-8.0F * 30.0F)) <
           0.05F);
    paratroopa.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(paratroopa.enemy(0).y - 180.2875F) < 0.05F);
    assert(std::fabs(paratroopa.enemy(0).vy - (-7.645F * 30.0F)) <
           0.05F);

    PlatformerEngine bullet;
    assert(bullet.startCampaign(1, 1));
    bullet.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::BulletBill, 200.0F, 100.0F, 90U);
    bullet.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(bullet.enemy(0).x - 198.5F) < 0.05F);
    assert(std::fabs(bullet.enemy(0).vx - (-90.0F)) < 0.05F);

    PlatformerEngine cheep;
    assert(cheep.startCampaign(2, 2));
    cheep.debugSetPlayer(144.0F, 192.0F, 0.0F, 0.0F, true);
    cheep.step(1.0F / 60.0F,
               PlatformerInput{1.0F, false, false, false});
    finishWarp(cheep);
    assert(cheep.levelRuntime().activeLevelType() ==
           pgos::PlatformerLevelType::Underwater);
    cheep.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::CheepCheep, 200.0F, 300.0F, 81U);
    const uint8_t index = static_cast<uint8_t>(cheep.snapshot().enemyCount - 1U);
    cheep.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(cheep.enemy(index).x - 199.5F) < 0.05F);
    assert(std::fabs(cheep.enemy(index).y - 300.0F) < 0.05F);
}

void testReferenceHammerBroReleaseDelay() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::HammerBro, 100.0F, 176.0F, 56U);
    engine.debugSetPlayer(200.0F, 100.0F, 0.0F, 0.0F, false);

    stepReferenceFrames(engine, 1);
    assert(engine.enemy(0).facingLeft);
    stepReferenceFrames(engine, 118);
    assert(engine.snapshot().enemyHazardCount == 0U);
    stepReferenceFrames(engine, 1);
    assert(engine.snapshot().enemyHazardCount == 0U);
    const auto holding = engine.enemy(0);
    assert(holding.heldHammer);
    assert(std::fabs(holding.heldHammerX -
                     (holding.x + holding.width * 0.5F - 8.0F)) < 0.05F);
    assert(std::fabs(holding.heldHammerY - 176.0F) < 0.05F);
    stepReferenceFrames(engine, 28);
    assert(engine.snapshot().enemyHazardCount == 0U);
    assert(engine.enemy(0).heldHammer);
    stepReferenceFrames(engine, 1);
    assert(engine.snapshot().enemyHazardCount == 1U);
    const auto released = engine.enemy(0);
    assert(!released.heldHammer);
    assert(engine.enemyHazard(0).kind ==
           pgos::PlatformerEnemyHazardKind::Hammer);
    const float expectedHammerX =
        released.facingLeft ? released.x + released.width
                            : released.x - 16.0F;
    assert(std::fabs(engine.enemyHazard(0).x - expectedHammerX) < 0.05F);
    assert(std::fabs(engine.enemyHazard(0).y -
                     (released.y - 16.0F)) < 0.05F);
    assert(std::fabs(engine.enemyHazard(0).vy - (-6.0F * 30.0F)) <
           0.05F);

    PlatformerEngine offscreenFacing;
    assert(offscreenFacing.startCampaign(1, 1));
    offscreenFacing.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::HammerBro, 100.0F, 176.0F, 56U);
    offscreenFacing.debugSetPlayer(300.0F, 100.0F, 0.0F, 0.0F, false);
    stepReferenceFrames(offscreenFacing, 120U);
    assert(offscreenFacing.enemy(0).facingLeft);
    offscreenFacing.debugSetPlayer(0.0F, 100.0F, 0.0F, 0.0F, false);
    offscreenFacing.debugSetCamera(400.0F);
    stepReferenceFrames(offscreenFacing, 29U);
    assert(offscreenFacing.snapshot().enemyHazardCount == 1U);
    assert(std::fabs(offscreenFacing.enemyHazard(0).vx - 90.0F) < 0.05F);
}

void testReferenceEnemyHazardsHaveNoAgeLimit() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSetPlayer(16.0F, 185.0F, 0.0F, 0.0F, true);
    engine.debugSpawnEnemyHazard(
        pgos::PlatformerEnemyHazardKind::BowserFire,
        160.0F, 96.0F, 0.0F, 0.0F);
    stepReferenceFrames(engine, 361U);
    assert(engine.enemyHazard(0).active);
    assert(engine.enemyHazard(0).ageMs > 6000U);

    PlatformerEngine impact;
    assert(impact.startCampaign(1, 1));
    impact.debugSetPlayer(100.0F, 100.0F, 0.0F, 0.0F, false);
    impact.debugSpawnEnemyHazard(
        pgos::PlatformerEnemyHazardKind::BowserFire,
        100.0F, 100.0F, 0.0F, 0.0F);
    impact.step(1.0F / 60.0F, PlatformerInput{});
    assert(impact.phase() == PlatformerPhase::Dying);
    assert(impact.enemyHazard(0).active);
}

void testReferenceScriptedPhasesKeepWorldRunning() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugBeginGoal();
    const float playerX = engine.snapshot().playerX;
    engine.debugSetCamera(playerX - 160.0F, 0.0F);
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Goomba, playerX - 96.0F, 192.0F, 70U);
    engine.debugSpawnPowerup(
        PlatformerPowerupKind::Mushroom, playerX - 64.0F, 160.0F);
    engine.debugSpawnEnemyHazard(
        pgos::PlatformerEnemyHazardKind::BowserFire,
        playerX, engine.snapshot().playerY, 0.0F, 0.0F);
    const float enemyX = engine.enemy(0).x;
    const float powerupY = engine.powerup(0).y;

    engine.step(1.0F / 60.0F, PlatformerInput{});

    assert(engine.phase() == PlatformerPhase::Flagpole);
    assert(engine.enemy(0).x < enemyX);
    assert(engine.powerup(0).y < powerupY);
    assert(engine.enemyHazard(0).active);
}

void testReferenceLavaBubbleCycle() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 4));
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::LavaBubble, 200.0F, 180.0F, 504U);

    stepReferenceFrames(engine, 359U);
    assert(std::fabs(engine.enemy(0).y - 180.0F) > 0.1F);
    stepReferenceFrames(engine, 1U);
    assert(std::fabs(engine.enemy(0).y - 180.0F) < 0.01F);
    assert(std::fabs(engine.enemy(0).vy + 300.0F) < 0.01F);
    stepReferenceFrames(engine, 1U);
    assert(std::fabs(engine.enemy(0).y - 175.2875F) < 0.01F);
    assert(std::fabs(engine.enemy(0).vy + 294.75F) < 0.01F);
}

void testReferenceBowserTimersAndDelayedAttack() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Bowser, 100.0F, 176.0F, 61U);
    engine.debugSetRandomState(2U);
    engine.debugSetPlayer(200.0F, 100.0F, 0.0F, 0.0F, false);

    stepReferenceFrames(engine, 119U);
    assert(engine.enemy(0).facingLeft);
    assert(std::fabs(engine.enemy(0).vx) < 0.01F);
    assert(engine.snapshot().enemyHazardCount == 0U);
    stepReferenceFrames(engine, 1U);
    assert(std::fabs(engine.enemy(0).vx - 30.0F) < 0.01F);
    assert(std::fabs(engine.enemy(0).vy + 150.0F) < 0.01F);
    assert(engine.snapshot().enemyHazardCount == 0U);

    stepReferenceFrames(engine, 1U);
    assert(std::fabs(engine.enemy(0).x - 100.5F) < 0.05F);
    assert(std::fabs(engine.enemy(0).vy + 143.25F) < 0.05F);
    stepReferenceFrames(engine, 118U);
    assert(engine.snapshot().enemyHazardCount > 0U);
    assert(engine.enemyHazard(0).kind ==
           pgos::PlatformerEnemyHazardKind::BowserFire);
    assert(receivedEvent(engine, PlatformerEventType::BowserFire));

    PlatformerEngine offscreenCallback;
    assert(offscreenCallback.startCampaign(1, 1));
    offscreenCallback.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Bowser, 100.0F, 176.0F, 61U);
    offscreenCallback.debugSetPlayer(200.0F, 100.0F,
                                     0.0F, 0.0F, false);
    stepReferenceFrames(offscreenCallback, 120U);
    assert(offscreenCallback.snapshot().enemyHazardCount == 0U);
    offscreenCallback.debugSetCamera(1000.0F);
    stepReferenceFrames(offscreenCallback, 2U);
    assert(offscreenCallback.snapshot().enemyHazardCount == 0U);
    stepReferenceFrames(offscreenCallback, 1U);
    assert(offscreenCallback.snapshot().enemyHazardCount == 1U);

    PlatformerEngine hammerBurst;
    assert(hammerBurst.startCampaign(1, 1));
    hammerBurst.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Bowser, 100.0F, 176.0F, 61U);
    hammerBurst.debugSetRandomState(1U);
    hammerBurst.debugSetPlayer(200.0F, 100.0F, 0.0F, 0.0F, false);
    stepReferenceFrames(hammerBurst, 160U);
    assert(hammerBurst.snapshot().enemyHazardCount == 9U);
    for (uint8_t index = 0U;
         index < hammerBurst.snapshot().enemyHazardCount; ++index) {
        assert(hammerBurst.enemyHazard(index).active);
        assert(hammerBurst.enemyHazard(index).kind ==
               pgos::PlatformerEnemyHazardKind::Hammer);
    }

    PlatformerEngine bridgeCallback;
    assert(bridgeCallback.startCampaign(1, 4));
    bridgeCallback.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Bowser, 100.0F, 176.0F, 61U);
    bridgeCallback.debugSetRandomState(2U);
    bridgeCallback.debugSetPlayer(200.0F, 100.0F, 0.0F, 0.0F, false);
    stepReferenceFrames(bridgeCallback, 120U);
    assert(bridgeCallback.snapshot().enemyHazardCount == 0U);
    bridgeCallback.debugSetPlayer(bridgeCallback.goalX(), 48.0F,
                                  0.0F, 0.0F, false);
    bridgeCallback.step(1.0F / 60.0F, PlatformerInput{});
    assert(bridgeCallback.phase() == PlatformerPhase::CastleBridge);
    uint16_t bridgeFrames = 0U;
    bool bowserLeftCamera = false;
    while (bridgeCallback.phase() == PlatformerPhase::CastleBridge &&
           bridgeCallback.snapshot().enemyHazardCount == 0U &&
           bridgeFrames < 240U) {
        bridgeCallback.step(1.0F / 60.0F, PlatformerInput{});
        bowserLeftCamera |= !bridgeCallback.enemy(0).active;
        ++bridgeFrames;
    }
    assert(bowserLeftCamera);
    assert(bridgeCallback.snapshot().enemyHazardCount == 1U);
    assert(bridgeCallback.enemyHazard(0).kind ==
           pgos::PlatformerEnemyHazardKind::BowserFire);
}

void testReferenceFlyingCheepCallbackRange() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::CheepCheep, 100.0F, 250.0F, 498U);

    stepReferenceFrames(engine, 150U);
    assert(std::fabs(engine.enemy(0).vx) < 0.01F);
    uint16_t callbackFrames = 0U;
    while (std::fabs(engine.enemy(0).vx) < 0.01F &&
           callbackFrames < 150U) {
        engine.step(1.0F / 60.0F, PlatformerInput{});
        ++callbackFrames;
    }
    assert(callbackFrames >= 30U && callbackFrames <= 150U);
    assert(engine.enemy(0).vx >= 60.0F && engine.enemy(0).vx <= 150.0F);
    assert(std::fabs(engine.enemy(0).y - 250.0F) < 0.01F);
    assert(std::fabs(engine.enemy(0).vy + 300.0F) < 0.01F);

    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(engine.enemy(0).y - 245.2875F) < 0.01F);
    assert(std::fabs(engine.enemy(0).vy + 296.376F) < 0.02F);
}

void testReferenceRisingCheepStompAtPlayerApex() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::CheepCheep, 40.0F, 190.0F, 498U);
    engine.debugSetEnemyVelocity(0U, 0.0F, -60.0F);
    engine.debugSetPlayer(40.0F, 184.0F, 0.0F, 0.0F, true);

    engine.step(1.0F / 60.0F, PlatformerInput{});

    assert(engine.enemy(0).motion ==
           PlatformerEnemyMotion::FallingDefeated);
    assert(std::fabs(engine.snapshot().playerVy + 105.0F) < 0.05F);
    assert(engine.snapshot().score == 100U);
}

void testReferenceFireballSpawnAndFirstPhysicsFrame() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugSetPlayerPower(PlatformerPlayerPower::Fire);
    const auto player = engine.snapshot();

    engine.step(1.0F / 60.0F,
                PlatformerInput{0.0F, false, false, false, true, false});
    auto fireball = engine.projectile(0);
    assert(fireball.active);
    assert(engine.snapshot().playerFireballPose);
    assert(std::fabs(fireball.x -
                     (player.playerX + PlatformerEngine::PLAYER_WIDTH)) <
           0.05F);
    assert(std::fabs(fireball.y - (player.playerY + 2.0F)) < 0.05F);
    assert(std::fabs(fireball.vx - 300.0F) < 0.05F);
    assert(std::fabs(fireball.vy - 150.0F) < 0.05F);

    engine.step(1.0F / 60.0F, PlatformerInput{});
    fireball = engine.projectile(0);
    assert(std::fabs(fireball.x -
                     (player.playerX + PlatformerEngine::PLAYER_WIDTH +
                      5.0F)) < 0.05F);
    assert(std::fabs(fireball.vy -
                     ((5.0F + 0.575F) * 30.0F)) < 0.05F);

    engine.step(1.0F / 60.0F,
                PlatformerInput{0.0F, false, false, false, true, false});
    assert(activeProjectileCount(engine) == 2U);
    stepReferenceFrames(engine, 5U);
    assert(engine.snapshot().playerFireballPose);
    stepReferenceFrames(engine, 1U);
    assert(!engine.snapshot().playerFireballPose);
}

void testReferenceEnemyHitboxes() {
    PlatformerEngine piranhaVisual;
    assert(piranhaVisual.startCampaign(1, 1));
    piranhaVisual.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::PiranhaPlant, 100.0F, 100.0F, 44U);
    piranhaVisual.debugSetPlayer(112.0F, 104.0F, 0.0F, -30.0F, false);
    piranhaVisual.step(1.0F / 60.0F, PlatformerInput{});
    assert(piranhaVisual.phase() == PlatformerPhase::Running);

    PlatformerEngine piranhaHitbox;
    assert(piranhaHitbox.startCampaign(1, 1));
    piranhaHitbox.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::PiranhaPlant, 100.0F, 100.0F, 44U);
    piranhaHitbox.debugSetPlayer(112.0F, 110.0F, 0.0F, -30.0F, false);
    piranhaHitbox.step(1.0F / 60.0F, PlatformerInput{});
    assert(piranhaHitbox.phase() == PlatformerPhase::Dying);

    PlatformerEngine blooperVisual;
    assert(blooperVisual.startCampaign(1, 1));
    blooperVisual.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Blooper, 100.0F, 100.0F, 48U);
    blooperVisual.debugSetPlayer(100.0F, 90.0F, 0.0F, -30.0F, false);
    blooperVisual.step(1.0F / 60.0F, PlatformerInput{});
    assert(blooperVisual.phase() == PlatformerPhase::Running);

    PlatformerEngine blooperHitbox;
    assert(blooperHitbox.startCampaign(1, 1));
    blooperHitbox.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Blooper, 100.0F, 100.0F, 48U);
    blooperHitbox.debugSetPlayer(100.0F, 94.0F, 0.0F, -30.0F, false);
    blooperHitbox.step(1.0F / 60.0F, PlatformerInput{});
    assert(blooperHitbox.phase() == PlatformerPhase::Dying);

    PlatformerEngine koopaVisual;
    assert(koopaVisual.startCampaign(1, 1));
    koopaVisual.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Koopa, 100.0F, 100.0F, 38U);
    koopaVisual.debugSetPlayer(100.0F, 90.0F, 0.0F, -30.0F, false);
    koopaVisual.step(1.0F / 60.0F, PlatformerInput{});
    assert(koopaVisual.phase() == PlatformerPhase::Running);

    PlatformerEngine koopaHitbox;
    assert(koopaHitbox.startCampaign(1, 1));
    koopaHitbox.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Koopa, 100.0F, 100.0F, 38U);
    koopaHitbox.debugSetPlayer(100.0F, 93.0F, 0.0F, -30.0F, false);
    koopaHitbox.step(1.0F / 60.0F, PlatformerInput{});
    assert(koopaHitbox.phase() == PlatformerPhase::Dying);
}

void testReferenceFireballDirectionalTileCollisions() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    const auto* level = engine.levelRuntime().level();
    assert(level != nullptr);

    int32_t floorColumn = -1;
    int32_t floorRow = -1;
    int32_t ceilingColumn = -1;
    int32_t ceilingRow = -1;
    for (uint8_t row = 1U; row + 1U < level->height; ++row) {
        for (uint16_t column = 1U; column + 1U < level->width; ++column) {
            if (!engine.debugTileSolid(column, row)) {
                continue;
            }
            if (floorColumn < 0 && !engine.debugTileSolid(column, row - 1U)) {
                floorColumn = column;
                floorRow = row;
            }
            if (ceilingColumn < 0 &&
                !engine.debugTileSolid(column, row + 1U)) {
                ceilingColumn = column;
                ceilingRow = row;
            }
        }
    }
    assert(floorColumn >= 0 && ceilingColumn >= 0);

    const float floorX = floorColumn * PlatformerEngine::TILE_SIZE + 4.0F;
    const float floorY = floorRow * PlatformerEngine::TILE_SIZE;
    engine.debugSetCamera(
        std::max(0.0F, floorX - PlatformerEngine::VIEWPORT_WIDTH * 0.5F));
    engine.debugSpawnProjectile(floorX, floorY - 9.0F, 0.0F, 30.0F);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    auto projectile = engine.projectile(0);
    assert(projectile.active && !projectile.exploding);
    assert(std::fabs(projectile.y - (floorY - 8.0F)) < 0.01F);
    assert(std::fabs(projectile.vy + 120.0F) < 0.01F);

    const float ceilingX =
        ceilingColumn * PlatformerEngine::TILE_SIZE + 4.0F;
    const float ceilingBottom =
        (ceilingRow + 1) * PlatformerEngine::TILE_SIZE;
    engine.debugSetCamera(std::max(
        0.0F, ceilingX - PlatformerEngine::VIEWPORT_WIDTH * 0.5F));
    engine.debugSpawnProjectile(ceilingX, ceilingBottom + 0.3F, 0.0F,
                                -30.0F);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    projectile = engine.projectile(1);
    assert(projectile.active && !projectile.exploding);
    assert(std::fabs(projectile.y - ceilingBottom) < 0.01F);
    assert(std::fabs(projectile.vy) < 0.01F);

    PlatformerEngine wall;
    wall.start();
    constexpr float PIPE_LEFT = 449.0F;
    wall.debugSetCamera(300.0F);
    wall.debugSpawnProjectile(PIPE_LEFT - 9.0F, 173.0F, 300.0F, 0.0F);
    wall.step(1.0F / 60.0F, PlatformerInput{});
    projectile = wall.projectile(0);
    assert(projectile.active);
    assert(projectile.exploding);
    assert(projectile.explosionFrames == 4U);
    assert(receivedEvent(wall, PlatformerEventType::FireballHit));
    stepReferenceFrames(wall, 3U);
    assert(wall.projectile(0).active);
    stepReferenceFrames(wall, 1U);
    assert(!wall.projectile(0).active);
}

void testReferenceFireballEnemyFilteringAndHitboxes() {
    PlatformerEngine piranhaVisual;
    assert(piranhaVisual.startCampaign(1, 1));
    piranhaVisual.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::PiranhaPlant, 100.0F, 100.0F, 44U);
    piranhaVisual.debugSpawnProjectile(100.0F, 100.0F, 0.0F, 0.0F);
    piranhaVisual.step(1.0F / 60.0F, PlatformerInput{});
    assert(piranhaVisual.enemy(0).active);
    assert(piranhaVisual.projectile(0).active);

    PlatformerEngine piranhaHitbox;
    assert(piranhaHitbox.startCampaign(1, 1));
    piranhaHitbox.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::PiranhaPlant, 100.0F, 100.0F, 44U);
    piranhaHitbox.debugSpawnProjectile(112.0F, 124.0F, 0.0F, 0.0F);
    piranhaHitbox.step(1.0F / 60.0F, PlatformerInput{});
    assert(piranhaHitbox.enemy(0).active);
    assert(piranhaHitbox.enemy(0).motion ==
           PlatformerEnemyMotion::Defeated);
    assert(!piranhaHitbox.enemy(0).verticalFlipped);
    assert(!piranhaHitbox.projectile(0).active);
    assert(piranhaHitbox.snapshot().score == 0U);
    assert(activeEffectKindCount(piranhaHitbox,
                                 PlatformerEffectKind::Score) == 1U);
    piranhaHitbox.step(1.0F / 60.0F, PlatformerInput{});
    assert(!piranhaHitbox.enemy(0).active);

    PlatformerEngine lava;
    assert(lava.startCampaign(1, 1));
    lava.debugSpawnCampaignEnemy(pgos::PlatformerEnemyType::LavaBubble,
                                 100.0F, 100.0F, 504U);
    lava.debugSpawnProjectile(100.0F, 100.0F, 0.0F, 0.0F);
    lava.step(1.0F / 60.0F, PlatformerInput{});
    assert(lava.enemy(0).active);
    assert(lava.projectile(0).active);

    PlatformerEngine bullet;
    assert(bullet.startCampaign(1, 1));
    bullet.debugSpawnCampaignEnemy(pgos::PlatformerEnemyType::BulletBill,
                                   100.0F, 100.0F, 90U);
    bullet.debugSpawnProjectile(100.0F, 117.0F, 0.0F, 0.0F);
    bullet.step(1.0F / 60.0F, PlatformerInput{});
    assert(bullet.enemy(0).active);
    assert(bullet.projectile(0).active);
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
    assert(std::fabs(airborne.playerVy - (-7.3F * 30.0F)) < 0.05F);
    assert(std::fabs(airborne.playerY - landed.playerY) < 0.05F);
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
        input.jumpHeld = holdJump && frame > 0 && frame < 90;
        engine.step(1.0F / 60.0F, input);
        minimumY = std::min(minimumY, engine.snapshot().playerY);
    }
    return minimumY;
}

float measureReferenceJumpApex(bool holdJump) {
    ReferenceJumpMotion reference;
    float minimumY = reference.y;
    for (uint32_t frame = 0; frame < 120; ++frame) {
        const bool jumpPressed = frame == 0;
        const bool jumpHeld = holdJump && frame > 0 && frame < 90;
        reference.step(jumpPressed, jumpHeld);
        minimumY = std::min(minimumY, reference.y);
    }
    return minimumY;
}

void testVariableJumpHeight() {
    const float tapApex = measureJumpApex(false);
    const float holdApex = measureJumpApex(true);
    assert(std::fabs(tapApex - measureReferenceJumpApex(false)) < 0.05F);
    assert(std::fabs(holdApex - measureReferenceJumpApex(true)) < 0.05F);
    assert(holdApex + 10.0F < tapApex);
}

void testWalkRunTurnAndCamera() {
    PlatformerEngine walk;
    walk.start();
    stepFor(walk, 240);
    stepReferenceFrames(
        walk, 120,
        PlatformerInput{1.0F, false, false, false, false, false});
    const float walkSpeed = walk.snapshot().playerVx;
    assert(std::fabs(walkSpeed - 90.0F) < 0.1F);

    PlatformerEngine run;
    run.start();
    stepFor(run, 240);
    stepReferenceFrames(
        run, 90,
        PlatformerInput{1.0F, false, false, false, false, true});
    const auto running = run.snapshot();
    assert(std::fabs(running.playerVx - 150.0F) < 1.0F);
    assert(running.cameraX > 0.0F);
    stepFor(run, 120, PlatformerInput{-1.0F, false, false, false, false, true});
    assert(run.snapshot().playerSkidding || run.snapshot().playerVx < 0.0F);
    const float cameraAfterTurn = run.snapshot().cameraX;
    stepReferenceFrames(
        run, 60,
        PlatformerInput{-1.0F, false, false, false, false, true});
    assert(run.snapshot().cameraX >= cameraAfterTurn);
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
    assert(receivedEvent(engine, PlatformerEventType::Paused));
    const auto before = engine.snapshot();
    stepFor(engine, 1000, PlatformerInput{1.0F, true, true});
    const auto after = engine.snapshot();
    assert(std::fabs(before.playerX - after.playerX) < 0.01F);
    assert(std::fabs(before.playerY - after.playerY) < 0.01F);
    assert(before.timeRemaining == after.timeRemaining);
    assert(before.logicFrame == after.logicFrame);
    engine.togglePause();
    assert(engine.phase() == PlatformerPhase::Running);
}

void testReferencePauseRunsOnlyQueuedCommands() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSpawnCampaignEnemy(pgos::PlatformerEnemyType::Lakitu,
                                   220.0F, 80.0F, 50U);
    engine.debugSetPlayer(240.0F, 100.0F, 0.0F, 0.0F, false);
    stepReferenceFrames(engine, 180U);
    const auto beforePause = engine.snapshot();
    assert(engine.enemy(0).alternatePose);
    engine.togglePause();
    stepReferenceFrames(engine, 45U);
    assert(engine.phase() == PlatformerPhase::Paused);
    assert(engine.snapshot().logicFrame == beforePause.logicFrame);
    assert(engine.snapshot().animationFrame == beforePause.animationFrame);
    assert(engine.snapshot().timeRemaining == beforePause.timeRemaining);
    assert(engine.enemy(0).alternatePose);
    assert(activeEnemyTypeCount(engine, pgos::PlatformerEnemyType::Spiny) ==
           0U);
    stepReferenceFrames(engine, 1U);
    assert(!engine.enemy(0).alternatePose);
    assert(activeEnemyTypeCount(engine, pgos::PlatformerEnemyType::Spiny) ==
           1U);
    assert(engine.snapshot().logicFrame == beforePause.logicFrame);
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

void testReferenceIgnoredEnemyLayerTilesDoNotSpawn() {
    PlatformerEngine engine;
    assert(engine.startCampaign(2, 1));
    engine.debugSetCamera(150.0F * 16.0F - 120.0F,
                          25.0F * 16.0F - 100.0F);
    engine.step(1.0F / 60.0F, PlatformerInput{});

    for (uint8_t index = 0; index < engine.snapshot().enemyCount; ++index) {
        assert(engine.enemy(index).sourceTileId != 75U);
    }
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
    assert(engine.snapshot().score == 100U);
    PlatformerEvent blockEvent;
    PlatformerEvent coinEvent;
    assert(engine.pollEvent(blockEvent));
    assert(engine.pollEvent(coinEvent));
    assert(blockEvent.type == PlatformerEventType::BlockHit);
    assert(coinEvent.type == PlatformerEventType::CoinCollected);
    assert(activeEffectKindCount(engine, PlatformerEffectKind::Score) == 1U);
    assert(activeEffectKindCount(engine,
                                 PlatformerEffectKind::RisingCoin) == 1U);
    uint8_t coinEffect = 0U;
    while (coinEffect < engine.snapshot().effectCount &&
           engine.effect(coinEffect).kind !=
               PlatformerEffectKind::RisingCoin) {
        ++coinEffect;
    }
    assert(coinEffect < engine.snapshot().effectCount);
    assert(std::fabs(engine.effect(coinEffect).y - engine.box(31).y) < 0.01F);
    assert(std::fabs(engine.effect(coinEffect).vy + 300.0F) < 0.01F);
    stepReferenceFrames(engine, 1U);
    assert(std::fabs(engine.effect(coinEffect).y -
                     (engine.box(31).y - 4.7125F)) < 0.01F);
    assert(std::fabs(engine.effect(coinEffect).vy + 273.75F) < 0.01F);
    stepReferenceFrames(engine, 17U);
    assert(engine.effect(coinEffect).active);
    stepReferenceFrames(engine, 1U);
    assert(!engine.effect(coinEffect).active);

    stepReferenceFrames(engine, 15U);
    assert(activeEffectKindCount(engine, PlatformerEffectKind::Score) == 1U);
    stepReferenceFrames(engine, 1U);
    assert(activeEffectKindCount(engine, PlatformerEffectKind::Score) == 0U);

    for (uint8_t use = 0; use < 6U; ++use) {
        engine.debugHitBox(16);
    }
    assert(engine.box(16).opened);
    assert(engine.box(16).remainingUses == 0U);
    assert(engine.snapshot().coinsCollected == 7U);
    assert(engine.snapshot().score == 700U);
    assert(receivedEvent(engine, PlatformerEventType::CoinCollected));
}

void testReferenceBlockBumpFrames() {
    constexpr int8_t EXPECTED_OFFSETS[] = {-2, -3, -4, -5,
                                           -4, -3, -2, 0};

    PlatformerEngine legacy;
    legacy.start();
    legacy.debugHitBox(31);
    assert(legacy.box(31).bumpOffset == 0);
    for (int8_t expected : EXPECTED_OFFSETS) {
        legacy.step(1.0F / 60.0F, PlatformerInput{});
        assert(legacy.box(31).bumpOffset == expected);
    }
    legacy.debugHitBox(31);
    legacy.step(1.0F / 60.0F, PlatformerInput{});
    assert(legacy.box(31).bumpOffset == 0);

    PlatformerEngine campaign;
    assert(campaign.startCampaign(1, 1));
    campaign.step(1.0F / 60.0F, PlatformerInput{});
    assert(campaign.snapshot().enemyCount > 0U);
    constexpr uint8_t bumpedEnemy = 0U;
    assert(campaign.enemy(bumpedEnemy).type ==
           pgos::PlatformerEnemyType::Goomba);
    campaign.debugActivateEnemy(bumpedEnemy, 16.0F * 16.0F, 128.0F,
                                PlatformerEnemyMotion::Walking);
    campaign.debugSetPlayer(16.0F * 16.0F, 160.0F, 0.0F, -60.0F, false);
    campaign.step(1.0F / 60.0F, PlatformerInput{});
    assert(campaign.levelRuntime().modificationAt(16, 9) != nullptr);
    assert(campaign.levelRuntime().blockBumpOffset(16, 9) == 0);
    assert(campaign.enemy(bumpedEnemy).active);
    assert(campaign.enemy(bumpedEnemy).motion ==
           PlatformerEnemyMotion::FallingDefeated);
    assert(campaign.enemy(bumpedEnemy).verticalFlipped);
    for (int8_t expected : EXPECTED_OFFSETS) {
        campaign.step(1.0F / 60.0F, PlatformerInput{});
        assert(campaign.levelRuntime().blockBumpOffset(16, 9) == expected);
    }
}

void testBrickBreakAndSpecialContents() {
    PlatformerEngine engine;
    engine.start();
    engine.debugSetPlayerPower(PlatformerPlayerPower::Big);
    engine.debugHitBox(0);
    assert(!engine.box(0).opened);
    assert(engine.box(0).visible);
    assert(engine.snapshot().effectCount == 0U);
    assert(!receivedEvent(engine, PlatformerEventType::BrickBroken));

    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.box(0).opened);
    assert(!engine.box(0).visible);
    bool hasPiece = false;
    for (uint8_t index = 0; index < engine.snapshot().effectCount; ++index) {
        hasPiece |= engine.effect(index).active &&
                    engine.effect(index).kind == PlatformerEffectKind::BrickPiece;
    }
    assert(hasPiece);
    assert(engine.snapshot().effectCount == 4U);
    const auto brokenBox = engine.box(0);
    for (uint8_t index = 0U; index < 4U; ++index) {
        const auto piece = engine.effect(index);
        assert(piece.kind == PlatformerEffectKind::BrickPiece);
        assert(std::fabs(piece.x - brokenBox.x) < 0.01F);
        assert(std::fabs(piece.y -
                         (index < 2U ? brokenBox.y - 16.0F
                                     : brokenBox.y)) < 0.01F);
        assert(std::fabs(std::fabs(piece.vx) - 240.0F) < 0.01F);
        assert(std::fabs(piece.vy + 60.0F) < 0.01F);
    }
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(engine.effect(0).x - (brokenBox.x - 4.0F)) < 0.01F);
    assert(std::fabs(engine.effect(0).y -
                     (brokenBox.y - 16.0F - 0.7125F)) < 0.01F);
    assert(std::fabs(engine.effect(0).vy + 42.75F) < 0.01F);

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

    PlatformerEngine campaign;
    assert(campaign.startCampaign(1, 1));
    campaign.debugSetPlayerPower(PlatformerPlayerPower::Big);
    campaign.debugSetPlayer(20.0F * 16.0F, 10.0F * 16.0F,
                            0.0F, -60.0F, false);
    campaign.step(1.0F / 60.0F, PlatformerInput{});
    assert(campaign.debugTileSolid(20, 9));
    assert(activeEffectKindCount(campaign,
                                 PlatformerEffectKind::BrickPiece) == 0U);
    campaign.step(1.0F / 60.0F, PlatformerInput{});
    assert(!campaign.debugTileSolid(20, 9));
    assert(activeEffectKindCount(campaign,
                                 PlatformerEffectKind::BrickPiece) == 4U);
    assert(receivedEvent(campaign, PlatformerEventType::BrickBroken));
}

void testDefeatedEnemiesIgnoreRepeatedAttacks() {
    PlatformerEngine projectile;
    projectile.start();
    projectile.debugActivateEnemy(0, 100.0F, 185.0F,
                                  PlatformerEnemyMotion::Walking);
    projectile.debugSpawnProjectile(100.0F, 185.0F, 0.0F, 0.0F);
    stepReferenceFrames(projectile, 1U);
    const auto firstDefeat = projectile.enemy(0);
    assert(firstDefeat.motion == PlatformerEnemyMotion::FallingDefeated);
    PlatformerEvent discarded;
    while (projectile.pollEvent(discarded)) {
    }
    const uint32_t scoreAfterFirstHit = projectile.snapshot().score;
    const uint32_t defeatFrame = firstDefeat.bornFrame;

    projectile.debugSpawnProjectile(firstDefeat.x, firstDefeat.y,
                                    0.0F, 0.0F);
    stepReferenceFrames(projectile, 1U);
    assert(projectile.snapshot().score == scoreAfterFirstHit);
    assert(projectile.enemy(0).bornFrame == defeatFrame);
    assert(activeProjectileCount(projectile) == 1U);
    assert(!receivedEvent(projectile, PlatformerEventType::EnemyDefeated));

    PlatformerEngine shell;
    shell.start();
    shell.debugActivateEnemy(0, 100.0F, 185.0F,
                             PlatformerEnemyMotion::ShellSliding);
    shell.debugActivateEnemy(1, 101.0F, 185.0F,
                             PlatformerEnemyMotion::Walking);
    stepReferenceFrames(shell, 1U);
    assert(shell.enemy(1).motion == PlatformerEnemyMotion::FallingDefeated);
    const uint32_t shellScore = shell.snapshot().score;
    while (shell.pollEvent(discarded)) {
    }
    stepReferenceFrames(shell, 1U);
    assert(shell.snapshot().score == shellScore);
    assert(!receivedEvent(shell, PlatformerEventType::EnemyDefeated));
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
    auto growing = engine.snapshot();
    assert(growing.playerBig);
    assert(growing.playerPower == PlatformerPlayerPower::Small);
    assert(growing.playerPowerTransition ==
           pgos::PlatformerPowerTransition::Grow);
    stepReferenceFrames(engine, 43U);
    assert(engine.snapshot().playerPower == PlatformerPlayerPower::Small);
    stepReferenceFrames(engine, 1U);
    assert(engine.snapshot().playerPower == PlatformerPlayerPower::Big);
    assert(engine.snapshot().playerPowerTransition ==
           pgos::PlatformerPowerTransition::Grow);
    stepReferenceFrames(engine, 1U);
    assert(engine.snapshot().playerPowerTransition ==
           pgos::PlatformerPowerTransition::None);

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
    assert(!engine.snapshot().playerFire);
    assert(engine.snapshot().playerPowerTransition ==
           pgos::PlatformerPowerTransition::Fire);
    stepReferenceFrames(engine, 58U);
    assert(!engine.snapshot().playerFire);
    stepReferenceFrames(engine, 1U);
    assert(engine.snapshot().playerFire);
}

void testReferencePowerupCollectionTransitions() {
    PlatformerEngine smallFlower;
    smallFlower.start();
    stepFor(smallFlower, 240);
    auto state = smallFlower.snapshot();
    const uint32_t scoreBeforeFlower = state.score;
    smallFlower.debugSpawnPowerup(PlatformerPowerupKind::FireFlower,
                                  state.playerX, state.playerY);
    smallFlower.step(1.0F / 60.0F, PlatformerInput{});
    state = smallFlower.snapshot();
    assert(state.playerPower == PlatformerPlayerPower::Small);
    assert(state.playerPowerTransition ==
           pgos::PlatformerPowerTransition::Grow);
    assert(state.score == scoreBeforeFlower + 1000U);
    stepReferenceFrames(smallFlower, 44U);
    assert(smallFlower.snapshot().playerPower == PlatformerPlayerPower::Big);

    PlatformerEngine repeated;
    repeated.start();
    stepFor(repeated, 240);
    repeated.debugSetPlayerPower(PlatformerPlayerPower::Fire);
    state = repeated.snapshot();
    repeated.debugSpawnPowerup(PlatformerPowerupKind::Mushroom,
                               state.playerX, state.playerY);
    repeated.step(1.0F / 60.0F, PlatformerInput{});
    assert(repeated.snapshot().playerPower == PlatformerPlayerPower::Fire);
    assert(repeated.snapshot().playerPowerTransition ==
           pgos::PlatformerPowerTransition::None);
    const uint32_t mushroomScore = repeated.snapshot().score;
    state = repeated.snapshot();
    repeated.debugSpawnPowerup(PlatformerPowerupKind::FireFlower,
                               state.playerX, state.playerY);
    repeated.step(1.0F / 60.0F, PlatformerInput{});
    assert(repeated.snapshot().score == mushroomScore + 1000U);
    assert(repeated.snapshot().playerPowerTransition ==
           pgos::PlatformerPowerTransition::None);

    PlatformerEngine movingWorld;
    movingWorld.start();
    stepFor(movingWorld, 240);
    state = movingWorld.snapshot();
    movingWorld.debugActivateEnemy(0, state.playerX + 100.0F, 185.0F,
                                   PlatformerEnemyMotion::Walking);
    movingWorld.debugSpawnPowerup(PlatformerPowerupKind::Mushroom,
                                  state.playerX, state.playerY);
    movingWorld.step(1.0F / 60.0F, PlatformerInput{});
    const float enemyX = movingWorld.enemy(0).x;
    const float playerX = movingWorld.snapshot().playerX;
    stepReferenceFrames(
        movingWorld, 10U,
        PlatformerInput{1.0F, true, true, false, true, true});
    assert(std::fabs(movingWorld.snapshot().playerX - playerX) < 0.01F);
    assert(std::fabs(movingWorld.enemy(0).x - enemyX) > 0.1F);
}

void testReferenceStarScoreBlinkAndLastFrameProtection() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    const auto start = engine.snapshot();
    engine.debugSpawnPowerup(PlatformerPowerupKind::Star, start.playerX,
                             start.playerY);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.snapshot().score == start.score);
    assert(engine.snapshot().playerInvincible);
    assert(engine.snapshot().playerVisible);

    stepReferenceFrames(engine, 4U);
    assert(engine.snapshot().playerVisible);
    stepReferenceFrames(engine, 1U);
    assert(!engine.snapshot().playerVisible);
    stepReferenceFrames(engine, 593U);
    assert(engine.snapshot().playerInvincible);

    const auto beforeLastFrame = engine.snapshot();
    engine.debugActivateEnemy(0, beforeLastFrame.playerX,
                              beforeLastFrame.playerY,
                              PlatformerEnemyMotion::Walking);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::Running);
    assert(!engine.snapshot().playerInvincible);
    assert(engine.enemy(0).motion ==
           PlatformerEnemyMotion::FallingDefeated);
    assert(engine.enemy(0).active);
    assert(engine.enemy(0).verticalFlipped);
    assert(!engine.snapshot().playerVisible);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.snapshot().playerVisible);
}

void testReferenceDamageBlinkFrames() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    const auto grounded = engine.snapshot();
    engine.debugSetPlayerPower(PlatformerPlayerPower::Big);
    engine.debugActivateEnemy(0, grounded.playerX, 177.0F,
                              PlatformerEnemyMotion::Walking);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(!engine.snapshot().playerDamageBlinking);
    assert(engine.snapshot().playerVisible);
    stepReferenceFrames(engine, 43U);
    assert(!engine.snapshot().playerDamageBlinking);
    stepReferenceFrames(engine, 1U);
    assert(engine.snapshot().playerDamageBlinking);
    assert(engine.snapshot().playerVisible);
    stepReferenceFrames(engine, 9U);
    assert(engine.snapshot().playerVisible);
    stepReferenceFrames(engine, 1U);
    assert(!engine.snapshot().playerVisible);

    engine.debugActivateEnemy(0U, 500.0F, 177.0F,
                              PlatformerEnemyMotion::Walking);
    stepReferenceFrames(engine, 139U);
    assert(engine.snapshot().playerDamageBlinking);
    engine.debugActivateEnemy(0U, engine.snapshot().playerX,
                              engine.snapshot().playerY,
                              PlatformerEnemyMotion::Walking);
    stepReferenceFrames(engine, 1U);
    assert(engine.phase() == PlatformerPhase::Running);
    assert(!engine.snapshot().playerDamageBlinking);
    assert(engine.snapshot().playerVisible);

    engine.debugActivateEnemy(0U, engine.snapshot().playerX,
                              engine.snapshot().playerY,
                              PlatformerEnemyMotion::Walking);
    stepReferenceFrames(engine, 1U);
    assert(engine.phase() == PlatformerPhase::Dying);
}

void testReferenceCoinsDoNotConvertToLife() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    const pgos::PlatformerCampaignLevel* level =
        engine.levelRuntime().level();
    assert(level != nullptr);
    int16_t coinColumn = -1;
    int16_t coinRow = -1;
    for (uint8_t row = 0U; row < level->height && coinColumn < 0; ++row) {
        for (uint16_t column = 0U; column < level->width; ++column) {
            if (engine.levelRuntime().tile(column, row).kind ==
                pgos::PlatformerRuntimeTileKind::Coin) {
                coinColumn = static_cast<int16_t>(column);
                coinRow = row;
                break;
            }
        }
    }
    assert(coinColumn >= 0 && coinRow >= 0);
    PlatformerEvent discarded;
    while (engine.pollEvent(discarded)) {
    }
    engine.debugSetCoinsCollected(99U);
    const uint8_t lives = engine.snapshot().lives;
    engine.debugSetPlayer(coinColumn * 16.0F, coinRow * 16.0F,
                          0.0F, 0.0F, false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.snapshot().coinsCollected == 100U);
    assert(engine.snapshot().lives == lives);
    assert(activeEffectKindCount(engine, PlatformerEffectKind::Score) == 0U);
    assert(!receivedEvent(engine, PlatformerEventType::OneUp));

    PlatformerEngine oneUp;
    oneUp.start();
    stepFor(oneUp, 240);
    oneUp.debugSetLives(99U);
    const auto player = oneUp.snapshot();
    oneUp.debugSpawnPowerup(PlatformerPowerupKind::OneUp,
                            player.playerX, player.playerY);
    oneUp.step(1.0F / 60.0F, PlatformerInput{});
    assert(oneUp.snapshot().lives == 100U);
    assert(activeEffectKindCount(oneUp, PlatformerEffectKind::OneUp) == 1U);
}

void testFireMarioStillCollectsCoinsAndHitsBlocks() {
    PlatformerEngine coin;
    assert(coin.startCampaign(1, 1));
    const pgos::PlatformerCampaignLevel* level = coin.levelRuntime().level();
    assert(level != nullptr);
    int16_t coinColumn = -1;
    int16_t coinRow = -1;
    for (uint8_t row = 0U; row < level->height && coinColumn < 0; ++row) {
        for (uint16_t column = 0U; column < level->width; ++column) {
            if (coin.levelRuntime().tile(column, row).kind ==
                pgos::PlatformerRuntimeTileKind::Coin) {
                coinColumn = static_cast<int16_t>(column);
                coinRow = static_cast<int16_t>(row);
                break;
            }
        }
    }
    assert(coinColumn >= 0 && coinRow >= 0);
    becomeFireMarioThroughPowerup(coin);
    PlatformerEvent discarded;
    while (coin.pollEvent(discarded)) {
    }
    const uint16_t coinsBefore = coin.snapshot().coinsCollected;
    coin.debugSetPlayer(coinColumn * 16.0F,
                        (coinRow + 1) * 16.0F -
                            PlatformerEngine::BIG_PLAYER_HEIGHT,
                        0.0F, 0.0F, false);
    coin.debugSetCamera(
        std::max(0.0F, coinColumn * 16.0F - 120.0F),
        std::max(0.0F, coinRow * 16.0F - 120.0F));
    coin.step(1.0F / 60.0F, PlatformerInput{});
    assert(coin.phase() == PlatformerPhase::Running);
    assert(coin.snapshot().playerPower == PlatformerPlayerPower::Fire);
    assert(coin.snapshot().coinsCollected == coinsBefore + 1U);
    assert(receivedEvent(coin, PlatformerEventType::CoinCollected));

    PlatformerEngine block;
    assert(block.startCampaign(1, 1));
    level = block.levelRuntime().level();
    assert(level != nullptr);
    int16_t blockColumn = -1;
    int16_t blockRow = -1;
    for (uint8_t row = 0U; row < level->height && blockColumn < 0; ++row) {
        for (uint16_t column = 0U; column < level->width; ++column) {
            const auto tile = block.levelRuntime().tile(column, row);
            if (tile.kind == pgos::PlatformerRuntimeTileKind::Question) {
                blockColumn = static_cast<int16_t>(column);
                blockRow = static_cast<int16_t>(row);
                break;
            }
        }
    }
    assert(blockColumn >= 0 && blockRow >= 0);
    becomeFireMarioThroughPowerup(block);
    while (block.pollEvent(discarded)) {
    }
    block.debugSetPlayer(blockColumn * 16.0F,
                         (blockRow + 1) * 16.0F + 1.0F,
                         0.0F, -240.0F, false);
    block.debugSetCamera(
        std::max(0.0F, blockColumn * 16.0F - 120.0F),
        std::max(0.0F, blockRow * 16.0F - 120.0F));
    stepReferenceFrames(block, 2U);
    assert(block.snapshot().playerPower == PlatformerPlayerPower::Fire);
    assert(block.levelRuntime().modificationAt(
               static_cast<uint16_t>(blockColumn),
               static_cast<uint8_t>(blockRow)) != nullptr);
    assert(receivedEvent(block, PlatformerEventType::BlockHit));
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
    assert(std::fabs(engine.snapshot().playerVy - (-12.5F * 30.0F)) <
           0.05F);
    assert(receivedEvent(engine, PlatformerEventType::PlayerDied));
    stepReferenceFrames(engine, 178);
    assert(engine.phase() == PlatformerPhase::Dying);
    stepReferenceFrames(engine, 1);
    assert(engine.phase() == PlatformerPhase::Running);
    assert(engine.snapshot().lives == 2U);
    assert(engine.snapshot().timeRemaining == 400U);

    PlatformerEngine lastLife;
    lastLife.start();
    lastLife.debugSetLives(1U);
    lastLife.debugSetPlayer(1110.0F, 241.0F, 0.0F, 120.0F, false);
    lastLife.step(1.0F / 60.0F, PlatformerInput{});
    assert(lastLife.phase() == PlatformerPhase::Dying);
    receivedEvent(lastLife, PlatformerEventType::PlayerDied);
    stepReferenceFrames(lastLife, 178U);
    assert(lastLife.phase() == PlatformerPhase::Dying);
    lastLife.step(1.0F / 60.0F, PlatformerInput{});
    assert(lastLife.phase() == PlatformerPhase::GameOver);
    assert(lastLife.snapshot().lives == 0U);
    assert(receivedEvent(lastLife, PlatformerEventType::GameOver));
}

void testReferenceCrouchingDamageShrinkAnchor() {
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
    assert(hurt.playerPowerTransition ==
           pgos::PlatformerPowerTransition::Shrink);
    assert(std::fabs(hurt.playerY + PlatformerEngine::BIG_PLAYER_HEIGHT -
                     footBefore) < 0.1F);
    assert(receivedEvent(engine, PlatformerEventType::PlayerHurt));

    stepReferenceFrames(engine, 44U);
    const auto callbackFrame = engine.snapshot();
    assert(callbackFrame.playerPowerTransition ==
           pgos::PlatformerPowerTransition::Shrink);
    assert(std::fabs(callbackFrame.playerY - hurt.playerY) < 0.1F);
    stepReferenceFrames(engine, 1U);
    const auto shrunk = engine.snapshot();
    assert(shrunk.playerPowerTransition ==
           pgos::PlatformerPowerTransition::None);
}

void testKoopaShellLifecycle() {
    PlatformerEngine engine;
    engine.start();
    stepFor(engine, 240);
    engine.debugActivateEnemy(8, 90.0F, 177.0F,
                              PlatformerEnemyMotion::Walking);
    engine.debugSetPlayer(90.0F, 166.0F, 0.0F, 180.0F, false);
    stepFor(engine, 16);
    assert(engine.enemy(8).motion == PlatformerEnemyMotion::ShellIdle);
    assert(engine.snapshot().playerVy < 0.0F);
    const uint32_t scoreAfterStomp = engine.snapshot().score;

    engine.debugSetPlayer(78.0F, 185.0F, 0.0F, 0.0F, true);
    engine.step(0.008F, PlatformerInput{});
    assert(engine.enemy(8).motion == PlatformerEnemyMotion::ShellSliding);
    assert(std::fabs(engine.enemy(8).vx) > 150.0F);
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
    assert(engine.snapshot().score == 200U);
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

void testReferenceStompPropagatesAcrossSameStepOverlaps() {
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
    assert(engine.enemy(1).motion == PlatformerEnemyMotion::Squashed);
    assert(engine.snapshot().playerVy < 0.0F);
    assert(engine.snapshot().score == 200U);
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
    assert(engine.levelRuntime().modificationCount() == 0U);
    engine.step(0.016F, PlatformerInput{});
    assert(engine.levelRuntime().modificationCount() == 1U);
    engine.debugSetPlayer(seamX, belowY, 0.0F, -200.0F, false);
    engine.step(0.016F, PlatformerInput{});
    assert(engine.levelRuntime().modificationCount() == 1U);
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
    stepReferenceFrames(timer, 30);
    assert(timer.phase() == PlatformerPhase::Running);
    assert(timer.snapshot().timeRemaining == 0U);
    stepReferenceFrames(timer, 1);
    assert(timer.phase() == PlatformerPhase::Dying);
    assert(timer.snapshot().deathReason == PlatformerDeathReason::Time);

    PlatformerEngine deathStopsClockBeforeScoreSystem;
    deathStopsClockBeforeScoreSystem.start();
    deathStopsClockBeforeScoreSystem.debugSetTimeRemaining(10U);
    stepReferenceFrames(deathStopsClockBeforeScoreSystem, 29);
    deathStopsClockBeforeScoreSystem.debugSetPlayer(
        1110.0F, 241.0F, 0.0F, 120.0F, false);
    stepReferenceFrames(deathStopsClockBeforeScoreSystem, 1);
    assert(deathStopsClockBeforeScoreSystem.phase() ==
           PlatformerPhase::Dying);
    assert(deathStopsClockBeforeScoreSystem.snapshot().timeRemaining == 10U);

    PlatformerEngine fall;
    fall.start();
    fall.debugSetPlayer(1110.0F, 241.0F, 0.0F, 120.0F, false);
    fall.step(0.008F, PlatformerInput{});
    assert(fall.phase() == PlatformerPhase::Dying);
    assert(fall.snapshot().deathReason == PlatformerDeathReason::Fall);

    PlatformerEngine goal;
    goal.start();
    goal.debugSetPlayer(3150.0F, 48.0F, 0.0F, 0.0F, false);
    const uint32_t scoreBeforeGoal = goal.snapshot().score;
    goal.debugBeginGoal();
    assert(goal.phase() == PlatformerPhase::Flagpole);
    assert(goal.snapshot().score == scoreBeforeGoal);
    stepFor(goal, 1810);
    assert(goal.phase() == PlatformerPhase::CastleWalk);
    const uint16_t castleWalkFrames = static_cast<uint16_t>(std::ceil(
        std::max(0.0F, goal.castleX() - goal.snapshot().playerX)));
    stepReferenceFrames(goal, castleWalkFrames);
    assert(goal.phase() == PlatformerPhase::TimeBonus);
    goal.debugSetTimeRemaining(5);
    stepReferenceFrames(goal, 269);
    assert(goal.phase() == PlatformerPhase::TimeBonus);
    assert(receivedEvent(goal, PlatformerEventType::TimerTick));
    stepReferenceFrames(goal, 1);
    assert(goal.snapshot().playerVisible);
    stepReferenceFrames(goal, 1);
    assert(!goal.snapshot().playerVisible);
    stepReferenceFrames(goal, 118);
    assert(goal.phase() == PlatformerPhase::TimeBonus);
    stepReferenceFrames(goal, 1);
    assert(goal.phase() == PlatformerPhase::Won);
    assert(goal.snapshot().score == scoreBeforeGoal + 500U);
    assert(goal.snapshot().goalReached);
    assert(receivedEvent(goal, PlatformerEventType::CourseClear));
}

void testReferenceFlagSequenceFrames() {
    PlatformerEngine engine;
    engine.start();
    const uint16_t timeAtFlag = engine.snapshot().timeRemaining;
    engine.debugSetPlayer(3150.0F, 185.0F, 0.0F, 0.0F, true);
    engine.debugBeginGoal();
    const float poleX = engine.snapshot().playerX;
    assert(receivedEvent(engine, PlatformerEventType::ReachedGoal));

    stepReferenceFrames(engine, 66);
    assert(engine.phase() == PlatformerPhase::Flagpole);
    assert(std::fabs(engine.snapshot().flagY - 168.0F) < 0.05F);
    stepReferenceFrames(engine, 1);
    assert(std::fabs(engine.snapshot().flagY - 169.0F) < 0.05F);
    stepReferenceFrames(engine, 1);
    assert(std::fabs(engine.snapshot().playerX - (poleX + 17.0F)) <
           0.05F);
    const float shiftedX = engine.snapshot().playerX;
    stepReferenceFrames(engine, 1);
    assert(std::fabs(engine.snapshot().playerX - (shiftedX + 0.25F)) <
           0.05F);
    stepReferenceFrames(engine, 35);
    assert(engine.phase() == PlatformerPhase::Flagpole);
    stepReferenceFrames(engine, 1);
    assert(engine.phase() == PlatformerPhase::CastleWalk);
    assert(std::fabs(engine.snapshot().playerVx - 60.0F) < 0.05F);
    assert(engine.snapshot().timeRemaining == timeAtFlag);
}

void testCampaignRuntimeAndLevelAdvance() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    auto start = engine.snapshot();
    assert(start.campaignMode);
    assert(start.world == 1U && start.stage == 1U);
    assert(std::fabs(start.cameraY) < 0.1F);
    assert(start.playerVisible);
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
    PlatformerEvent discarded;
    while (engine.pollEvent(discarded)) {
    }

    engine.debugBeginGoal();
    for (uint16_t frame = 0;
         frame < 300U && engine.phase() == PlatformerPhase::Flagpole;
         ++frame) {
        engine.step(1.0F / 60.0F, PlatformerInput{});
    }
    assert(engine.phase() == PlatformerPhase::CastleWalk);
    uint16_t campaignCastleWalkFrames = 0U;
    while (engine.phase() == PlatformerPhase::CastleWalk &&
           campaignCastleWalkFrames < 256U) {
        engine.step(1.0F / 60.0F, PlatformerInput{});
        ++campaignCastleWalkFrames;
    }
    assert(campaignCastleWalkFrames < 256U);
    assert(engine.phase() == PlatformerPhase::TimeBonus);
    assert(std::fabs(
               engine.snapshot().playerX -
               (205.0F * 16.0F - PlatformerEngine::PLAYER_WIDTH)) < 0.1F);
    engine.debugSetTimeRemaining(1);
    stepReferenceFrames(engine, 390);
    const auto transition = engine.snapshot();
    assert(transition.phase == PlatformerPhase::LevelTransition);
    assert(transition.world == 1U && transition.stage == 2U);
    assert(transition.campaignMode);
    assert(!transition.playerVisible);
    assert(transition.timeRemaining == 400U);

    PlatformerEvent clear;
    assert(takeEvent(engine, PlatformerEventType::CourseClear, clear));
    assert(clear.value == 0x0101U);
    assert(clear.score == transition.score);

    const float transitionX = transition.playerX;
    stepReferenceFrames(
        engine, 179U,
        PlatformerInput{1.0F, true, true, false, true, true});
    const auto waiting = engine.snapshot();
    assert(waiting.phase == PlatformerPhase::LevelTransition);
    assert(!waiting.playerVisible);
    assert(std::fabs(waiting.playerX - transitionX) < 0.01F);
    assert(waiting.timeRemaining == 400U);

    stepReferenceFrames(engine, 1U);
    const auto next = engine.snapshot();
    assert(next.phase == PlatformerPhase::Running);
    assert(next.playerVisible);
    assert(next.world == 1U && next.stage == 2U);
    assert(receivedEvent(engine, PlatformerEventType::LifeRestarted));
}

void testReferenceInitialAndDeathLevelTransitions() {
    PlatformerEngine initial;
    assert(initial.prepareCampaignTitle(2, 1));
    assert(initial.startPreparedCampaign());
    assert(initial.phase() == PlatformerPhase::LevelTransition);
    const float startX = initial.snapshot().playerX;
    assert(initial.snapshot().animationFrame == 0U);
    stepReferenceFrames(
        initial, 179U,
        PlatformerInput{1.0F, true, true, false, true, true});
    assert(initial.phase() == PlatformerPhase::LevelTransition);
    assert(std::fabs(initial.snapshot().playerX - startX) < 0.01F);
    assert(initial.snapshot().timeRemaining == 400U);
    assert(initial.snapshot().animationFrame == 0U);
    stepReferenceFrames(initial, 1U);
    assert(initial.phase() == PlatformerPhase::Running);
    assert(initial.snapshot().animationFrame == 0U);
    stepReferenceFrames(initial, 1U);
    assert(initial.snapshot().animationFrame == 1U);

    PlatformerEngine death;
    assert(death.startCampaign(1, 1));
    death.debugSetPlayerPower(PlatformerPlayerPower::Fire);
    death.debugSetPlayer(100.0F, 400.0F, 0.0F, 120.0F, false);
    death.step(1.0F / 60.0F, PlatformerInput{});
    assert(death.phase() == PlatformerPhase::Dying);
    assert(death.snapshot().playerPower == PlatformerPlayerPower::Small);
    stepReferenceFrames(death, 178U);
    assert(death.phase() == PlatformerPhase::Dying);
    stepReferenceFrames(death, 1U);
    auto transition = death.snapshot();
    assert(transition.phase == PlatformerPhase::LevelTransition);
    assert(transition.world == 1U && transition.stage == 1U);
    assert(transition.lives == 2U);
    assert(!transition.playerVisible);
    stepReferenceFrames(death, 179U);
    assert(death.phase() == PlatformerPhase::LevelTransition);
    stepReferenceFrames(death, 1U);
    assert(death.phase() == PlatformerPhase::Running);
    assert(death.snapshot().playerVisible);
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
    stepReferenceFrames(
        tunnel, 3,
        PlatformerInput{1.0F, false, false, false, false, true});
    assert(tunnel.snapshot().playerX > tunnelStartX + 0.001F);
}

void testCampaignPipeTransitions() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 1));
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::PiranhaPlant, 400.0F, 128.0F, 44U);
    engine.debugSetPlayer(57.0F * 16.0F - 8.0F, 128.0F,
                          0.0F, 0.0F, true);
    engine.step(0.008F,
                PlatformerInput{0.0F, false, false, true});
    assert(engine.phase() == PlatformerPhase::Running);
    engine.debugSetPlayer(57.0F * 16.0F, 128.0F, 0.0F, 0.0F, true);
    engine.step(0.008F,
                PlatformerInput{0.0F, false, false, true});
    assert(engine.phase() == PlatformerPhase::Warping);
    assert(receivedEvent(engine, PlatformerEventType::WarpStarted));
    const uint16_t timeAtWarp = engine.snapshot().timeRemaining;
    const uint16_t downwardWarpFrames = finishWarp(engine);
    assert(downwardWarpFrames == 68U);
    auto underground = engine.snapshot();
    assert(underground.phase == PlatformerPhase::Running);
    assert(std::fabs(underground.playerX - 32.0F) < 0.1F);
    assert(std::fabs(underground.playerY - 320.0F) < 0.1F);
    assert(std::fabs(underground.cameraY - 288.0F) < 0.1F);
    assert(engine.levelRuntime().activeBackground() ==
           pgos::PlatformerBackgroundColor::Black);
    assert(engine.levelRuntime().activeLevelType() ==
           pgos::PlatformerLevelType::Underground);
    assert(std::fabs(engine.enemy(0).y - 160.0F) < 0.05F);
    assert(std::fabs(engine.enemy(0).vy) < 0.05F);
    assert(underground.timeRemaining == timeAtWarp - 2U);
    assert(receivedEvent(engine, PlatformerEventType::WarpCompleted));

    stepReferenceFrames(engine, 27);
    engine.debugSetPlayer(192.0F, 480.0F, 0.0F, 0.0F, true);
    engine.step(0.008F,
                PlatformerInput{1.0F, false, false, false});
    assert(engine.phase() == PlatformerPhase::Warping);
    finishWarp(engine);
    const auto overworld = engine.snapshot();
    assert(overworld.phase == PlatformerPhase::Running);
    assert(std::fabs(overworld.cameraY) < 0.1F);
    assert(overworld.cameraX >= 159.0F * 16.0F - 0.1F);
    assert(std::fabs(overworld.playerY - 159.0F) < 0.1F);
    assert(engine.levelRuntime().activeBackground() ==
           pgos::PlatformerBackgroundColor::Blue);
    assert(engine.levelRuntime().activeLevelType() ==
           pgos::PlatformerLevelType::Overworld);

    PlatformerEngine warpZone;
    assert(warpZone.startCampaign(1, 2));
    warpZone.debugSetPlayer(144.0F, 192.0F, 0.0F, 0.0F, true);
    warpZone.step(1.0F / 60.0F,
                  PlatformerInput{1.0F, false, false, false});
    assert(warpZone.phase() == PlatformerPhase::Warping);
    finishWarp(warpZone);
    warpZone.debugSetPlayer(178.0F * 16.0F, 27.0F * 16.0F,
                            0.0F, 0.0F, true);
    warpZone.step(1.0F / 60.0F,
                  PlatformerInput{0.0F, false, false, true});
    assert(warpZone.phase() == PlatformerPhase::Warping);
    finishWarp(warpZone);
    auto loading = warpZone.snapshot();
    assert(loading.phase == PlatformerPhase::LevelTransition);
    assert(loading.world == 4U && loading.stage == 1U);
    assert(!loading.playerVisible);
    stepReferenceFrames(warpZone, 179U);
    assert(warpZone.phase() == PlatformerPhase::LevelTransition);
    stepReferenceFrames(warpZone, 1U);
    loading = warpZone.snapshot();
    assert(loading.phase == PlatformerPhase::Running);
    assert(loading.playerVisible);
}

void testCastleTeleportLoops() {
    PlatformerEngine engine;
    assert(engine.startCampaign(8, 4));
    engine.step(1.0F / 60.0F, PlatformerInput{});
    const uint8_t loadedCount = engine.snapshot().enemyCount;
    assert(loadedCount > 0U);
    uint8_t preservedEnemy = 0U;
    while (preservedEnemy < loadedCount &&
           (!engine.enemy(preservedEnemy).active ||
            engine.enemy(preservedEnemy).type ==
                pgos::PlatformerEnemyType::BulletBill)) {
        ++preservedEnemy;
    }
    assert(preservedEnemy < loadedCount);
    const uint16_t preservedSource =
        engine.enemy(preservedEnemy).sourceTileId;

    engine.debugSetPlayer(1598.8F, 96.0F, 200.0F, 0.0F, false);
    engine.step(0.008F,
                PlatformerInput{1.0F, false, false, false, false, true});
    const auto firstLoop = engine.snapshot();
    assert(firstLoop.playerX >= 32.0F * 16.0F);
    assert(firstLoop.playerX < 33.0F * 16.0F);
    assert(firstLoop.enemyCount == loadedCount);
    assert(engine.enemy(preservedEnemy).active);
    assert(engine.enemy(preservedEnemy).sourceTileId == preservedSource);

    engine.debugSetPlayer(2638.8F, 96.0F, 200.0F, 0.0F, false);
    engine.step(0.008F,
                PlatformerInput{1.0F, false, false, false, false, true});
    const auto secondLoop = engine.snapshot();
    assert(secondLoop.playerX >= 100.0F * 16.0F);
    assert(secondLoop.playerX < 101.0F * 16.0F);
}

void testCampaignPreloadsReferenceEnemiesWithinFixedPool() {
    uint8_t maximumLoaded = 0U;
    bool sawFlyingCheep = false;
    bool sawUnderwaterCheep = false;
    for (uint8_t world = 1U; world <= 8U; ++world) {
        for (uint8_t stage = 1U; stage <= 4U; ++stage) {
            const auto* level = pgos::platformerCampaignLevel(world, stage);
            assert(level != nullptr);
            const uint8_t expected = expectedLoadedCampaignEnemies(*level);
            assert(expected <= PlatformerEngine::MAX_LEVEL_ENEMIES);

            PlatformerEngine engine;
            assert(engine.startCampaign(world, stage));
            engine.debugLoadCampaignEnemies();
            assert(engine.snapshot().enemyCount == expected);
            maximumLoaded = std::max(maximumLoaded, expected);

            uint8_t enemyIndex = 0U;
            const float cameraX =
                static_cast<float>(level->cameraStart.x * 16);
            const float cameraY =
                static_cast<float>(level->cameraStart.y * 16);
            for (uint8_t row = 0U; row < level->height; ++row) {
                for (uint16_t column = 0U; column < level->width; ++column) {
                    const uint16_t source = pgos::platformerCampaignTileAt(
                        *level, pgos::PlatformerMapLayer::Enemies, column,
                        row);
                    if (!pgos::platformerEnemySourceCreatesEntity(source)) {
                        continue;
                    }
                    const uint16_t reference =
                        pgos::PLATFORMER_ENEMY_REFERENCE_IDS[source];
                    float expectedX = static_cast<float>(column * 16U);
                    float expectedY = static_cast<float>(row * 16U);
                    if (reference == 39U || reference == 71U) {
                        expectedX += 8.0F;
                    }
                    if (reference == 90U &&
                        (expectedX + 16.0F < cameraX ||
                         expectedX > cameraX + 320.0F ||
                         expectedY + 16.0F < cameraY ||
                         expectedY > cameraY + 240.0F)) {
                        continue;
                    }

                    bool flyingCheep = false;
                    if (reference == 81U) {
                        sawUnderwaterCheep = true;
                    }
                    if (reference == 498U) {
                        const uint16_t background =
                            pgos::platformerCampaignTileAt(
                                *level,
                                pgos::PlatformerMapLayer::Background,
                                column, row);
                        flyingCheep =
                            background >= pgos::PLATFORMER_BLOCK_TILE_COUNT ||
                            pgos::PLATFORMER_BLOCK_REFERENCE_IDS[background] !=
                                186U;
                        if (flyingCheep) {
                            expectedY += 16.0F;
                            sawFlyingCheep = true;
                        }
                    }
                    if (reference == 38U || reference == 39U ||
                        reference == 40U || reference == 455U) {
                        expectedY += 8.0F;
                    } else if (reference == 44U) {
                        expectedX += 8.0F;
                    } else if (reference == 504U) {
                        expectedY += 16.0F;
                    }

                    assert(enemyIndex < engine.snapshot().enemyCount);
                    const auto enemy = engine.enemy(enemyIndex++);
                    assert(enemy.sourceTileId == source);
                    assert(std::fabs(enemy.x - expectedX) < 0.01F);
                    assert(std::fabs(enemy.y - expectedY) < 0.01F);
                    assert(enemy.flyingCheep == flyingCheep);
                }
            }
            assert(enemyIndex == engine.snapshot().enemyCount);
        }
    }
    assert(maximumLoaded == PlatformerEngine::MAX_LEVEL_ENEMIES);
    assert(sawFlyingCheep);
    assert(sawUnderwaterCheep);
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
    finishWarp(engine);
    assert(engine.levelRuntime().activeLevelType() ==
           pgos::PlatformerLevelType::Underwater);
    engine.debugSetPlayer(64.0F, 96.0F, 0.0F, 40.0F, false);
    engine.step(0.008F, PlatformerInput{0.0F, true, true});
    const auto swimming = engine.snapshot();
    assert(swimming.playerVy < -100.0F);
    assert(!swimming.grounded);
    assert(swimming.underwater);
    assert(swimming.swimStrokeFrame == 1U);
    PlatformerEvent swimEvent;
    assert(takeEvent(engine, PlatformerEventType::SwimStroke, swimEvent));
    stepReferenceFrames(engine, 16U);
    assert(engine.snapshot().swimStrokeFrame == 17U);
    stepReferenceFrames(engine, 1U);
    assert(engine.snapshot().swimStrokeFrame == 0U);
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
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(engine.movingPlatform(0).x - platform.x) < 0.01F);

    engine.debugSetCamera(400.0F, 0.0F);
    engine.debugSetPlayer(platform.x + 8.0F,
                          platform.y - PlatformerEngine::PLAYER_HEIGHT,
                          0.0F, 0.0F, true);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    const auto startedPlatform = engine.movingPlatform(0);
    assert(std::fabs(startedPlatform.x - platform.x) < 0.01F);
    assert(startedPlatform.vx < 0.0F);
    assert(engine.snapshot().playerX < platform.x + 8.0F);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    const auto movedPlatform = engine.movingPlatform(0);
    assert(movedPlatform.x < platform.x);

    const auto pulley = engine.movingPlatform(7);
    assert(pulley.pulley);
    assert(pulley.pairIndex == 8);
    const auto pulleyPair = engine.movingPlatform(8);
    engine.debugSetCamera(1240.0F, 0.0F);
    engine.debugSetPlayer(pulley.x + 8.0F,
                          pulley.y - PlatformerEngine::PLAYER_HEIGHT,
                          0.0F, 0.0F, true);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(engine.movingPlatform(7).accelerationY - 216.0F) <
           0.01F);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(engine.movingPlatform(7).y - pulley.y) < 0.01F);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.movingPlatform(7).y > pulley.y);
    assert(engine.movingPlatform(8).y < pulleyPair.y);
}

void testWorldOneTwoDescendingPlatformsDoNotKillOnContact() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 2));
    stepFor(engine, 4500U);
    assert(engine.phase() == PlatformerPhase::Running);
    moveCampaignEnemiesAway(engine);
    assert(engine.snapshot().movingPlatformCount == 4U);
    const auto upper = engine.movingPlatform(0U);
    const auto lower = engine.movingPlatform(1U);
    assert(upper.motion == pgos::PlatformerMotionType::OneDirectionRepeated);
    assert(upper.direction == pgos::PlatformerDirection::Down);
    assert(lower.direction == pgos::PlatformerDirection::Down);

    engine.debugSetCamera(upper.x - 120.0F, 18.0F * 16.0F);
    engine.debugSetPlayerPower(PlatformerPlayerPower::Fire);
    engine.debugSetPlayer(upper.x + 8.0F,
                          upper.y - PlatformerEngine::BIG_PLAYER_HEIGHT -
                              2.0F,
                          0.0F, 120.0F, false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::Running);
    assert(engine.snapshot().grounded);
    assert(std::fabs(engine.snapshot().playerY +
                         PlatformerEngine::BIG_PLAYER_HEIGHT -
                     engine.movingPlatform(0U).y) < 0.01F);

    const float landedY = engine.snapshot().playerY;
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::Running);
    assert(engine.snapshot().grounded);
    assert(engine.snapshot().playerY > landedY);

    PlatformerEngine lowerContact;
    assert(lowerContact.startCampaign(1, 2));
    stepFor(lowerContact, 4500U);
    assert(lowerContact.phase() == PlatformerPhase::Running);
    moveCampaignEnemiesAway(lowerContact);
    lowerContact.debugSetCamera(lower.x - 120.0F, 18.0F * 16.0F);
    lowerContact.debugSetPlayer(lower.x + 8.0F,
                                lower.y - PlatformerEngine::PLAYER_HEIGHT -
                                    2.0F,
                                0.0F, 120.0F, false);
    lowerContact.step(1.0F / 60.0F, PlatformerInput{});
    assert(lowerContact.phase() == PlatformerPhase::Running);
    assert(lowerContact.snapshot().grounded);
}

void testReferenceCloudPlatformStartsAfterLanding() {
    PlatformerEngine engine;
    assert(engine.startCampaign(2, 1));
    assert(!engine.debugTileSolid(95U, 10U));

    uint8_t cloudIndex = 0xFFU;
    for (uint8_t index = 0U;
         index < engine.snapshot().movingPlatformCount; ++index) {
        if (engine.movingPlatform(index).cloudPlatform) {
            cloudIndex = index;
            break;
        }
    }
    assert(cloudIndex != 0xFFU);
    const auto cloud = engine.movingPlatform(cloudIndex);
    assert(cloud.sourceTileId == 857U);
    assert(cloud.widthTiles == 3U);
    assert(!cloud.triggered);
    assert(std::fabs(cloud.vx) < 0.01F);

    engine.debugSetCamera(cloud.x - 120.0F, 0.0F);
    engine.debugSetPlayer(cloud.x + 8.0F, cloud.y - 18.0F,
                          0.0F, 200.0F, false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    const auto triggered = engine.movingPlatform(cloudIndex);
    assert(triggered.triggered);
    assert(std::fabs(triggered.x - cloud.x) < 0.01F);
    assert(std::fabs(triggered.vx - 60.0F) < 0.01F);
    assert(std::fabs(engine.snapshot().playerY -
                     (cloud.y - PlatformerEngine::PLAYER_HEIGHT)) < 0.01F);

    const float playerX = engine.snapshot().playerX;
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(engine.movingPlatform(cloudIndex).x -
                     (cloud.x + 1.0F)) < 0.01F);
    assert(std::fabs(engine.snapshot().playerX - playerX) < 0.01F);
}

void testCampaignFireBars() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 4));
    const auto start = engine.snapshot();
    assert(start.fireBarCount == 7U);
    const auto initial = engine.fireBar(0);
    assert(initial.active && initial.length == 6U);
    engine.debugSetCamera(400.0F);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.fireBar(0).animationFrames[0] == 1U);
    stepReferenceFrames(engine, 4U);
    assert(std::fabs(engine.fireBar(0).angleDegrees - initial.angleDegrees) <
           0.01F);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    const auto rotated = engine.fireBar(0);
    assert(std::fabs(rotated.angleDegrees - (initial.angleDegrees + 10.0F)) <
           0.01F);
    assert(rotated.animationFrames[0] == 2U);
    engine.debugSetCamera(0.0F);
    stepReferenceFrames(engine, 10U);
    assert(engine.fireBar(0).animationFrames[0] == 2U);

    constexpr float PI = 3.14159265358979323846F;
    const auto collisionBar = engine.fireBar(0);
    const float radians = collisionBar.angleDegrees * PI / 180.0F;
    engine.debugSetPlayerPower(PlatformerPlayerPower::Big);
    engine.debugSetPlayer(collisionBar.x + std::cos(radians) * 16.0F,
                          collisionBar.y - std::sin(radians) * 16.0F,
                          0.0F, 0.0F, false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.snapshot().playerPower == PlatformerPlayerPower::Small);
    assert(receivedEvent(engine, PlatformerEventType::PlayerHurt));
}

void testStartUndergroundIntro() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 2));
    const float startingX = engine.snapshot().playerX;
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(engine.snapshot().playerX - startingX - 0.8F) < 0.01F);
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
    uint8_t trampolineIndex = 0xFFU;
    for (uint8_t index = 0U;
         index < engine.levelRuntime().trampolineCount(); ++index) {
        const auto* candidate = engine.levelRuntime().trampoline(index);
        if (candidate != nullptr &&
            candidate->column == static_cast<uint16_t>(
                                     trampolineX /
                                     PlatformerEngine::TILE_SIZE) &&
            candidate->row == static_cast<uint8_t>(
                                  trampolineY /
                                  PlatformerEngine::TILE_SIZE)) {
            trampolineIndex = index;
            break;
        }
    }
    assert(trampolineIndex != 0xFFU);
    engine.debugSetCamera(std::max(
        0.0F, static_cast<float>(trampolineX) -
                  PlatformerEngine::VIEWPORT_WIDTH * 0.5F),
                          std::max(0.0F,
                                   static_cast<float>(trampolineY) - 120.0F));
    engine.debugSetPlayer(static_cast<float>(trampolineX),
                          static_cast<float>(trampolineY - 18), 0.0F, 200.0F,
                          false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    const auto* trampoline =
        engine.levelRuntime().trampoline(trampolineIndex);
    assert(trampoline != nullptr);
    assert(trampoline->activated);
    assert(trampoline->sequenceIndex == 1U);
    assert(trampoline->visualState == 1U);
    assert(std::fabs(engine.snapshot().playerVy) < 0.1F);
    assert(!engine.debugTileSolid(
        static_cast<uint16_t>(trampolineX / PlatformerEngine::TILE_SIZE),
        static_cast<uint8_t>(trampolineY / PlatformerEngine::TILE_SIZE)));

    engine.step(1.0F / 60.0F, PlatformerInput{});
    trampoline = engine.levelRuntime().trampoline(trampolineIndex);
    assert(trampoline->sequenceIndex == 0U);
    assert(trampoline->visualState == 1U);
    assert(engine.snapshot().playerVy > 0.0F);

    uint8_t launchFrames = 0U;
    while (engine.snapshot().playerVy > -329.0F && launchFrames < 20U) {
        engine.step(1.0F / 60.0F, PlatformerInput{});
        ++launchFrames;
    }
    assert(launchFrames > 0U && launchFrames < 20U);
    assert(std::fabs(engine.snapshot().playerVy + 330.0F) < 0.1F);
    assert(receivedEvent(engine, PlatformerEventType::TrampolineBounced));
    trampoline = engine.levelRuntime().trampoline(trampolineIndex);
    assert(trampoline->sequenceIndex == 3U);
    assert(trampoline->visualState == 1U);

    engine.step(1.0F / 60.0F, PlatformerInput{});
    trampoline = engine.levelRuntime().trampoline(trampolineIndex);
    assert(trampoline->sequenceIndex == 4U);
    assert(trampoline->visualState == 0U);
    assert(trampoline->activated);
}

void testCastleBridgeSequence() {
    PlatformerEngine engine;
    assert(engine.startCampaign(1, 4));
    engine.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Bowser, engine.goalX() - 48.0F,
        176.0F, 61U);
    engine.debugSetPlayer(engine.goalX(), 48.0F, 0.0F, 0.0F, false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::CastleBridge);
    assert(!receivedEvent(engine, PlatformerEventType::ReachedGoal));
    const auto frozenBowser = engine.enemy(0);
    stepReferenceFrames(engine, 3U);
    const auto stillFrozenBowser = engine.enemy(0);
    assert(std::fabs(stillFrozenBowser.x - frozenBowser.x) < 0.01F);
    assert(std::fabs(stillFrozenBowser.y - frozenBowser.y) < 0.01F);
    assert(std::fabs(stillFrozenBowser.vx - frozenBowser.vx) < 0.01F);
    assert(std::fabs(stillFrozenBowser.vy - frozenBowser.vy) < 0.01F);
    assert(stillFrozenBowser.stateMs == frozenBowser.stateMs);
    assert(engine.levelRuntime().modificationCount() == 0U);
    stepReferenceFrames(engine, 1U);
    assert(engine.levelRuntime().modificationCount() > 0U);
    const uint8_t firstRemovalCount =
        engine.levelRuntime().modificationCount();
    stepReferenceFrames(engine, 4U);
    assert(engine.levelRuntime().modificationCount() == firstRemovalCount);
    stepReferenceFrames(engine, 1U);
    assert(engine.levelRuntime().modificationCount() > firstRemovalCount);
    uint16_t frames = 0U;
    while (engine.phase() == PlatformerPhase::CastleBridge && frames < 720U) {
        engine.step(1.0F / 60.0F, PlatformerInput{});
        ++frames;
    }
    assert(frames < 720U);
    assert(engine.phase() == PlatformerPhase::LevelTransition);
    assert(engine.snapshot().world == 2U && engine.snapshot().stage == 1U);
    bool heardBowserFall = false;
    bool heardCastleClear = false;
    PlatformerEvent event;
    while (engine.pollEvent(event)) {
        heardBowserFall |= event.type == PlatformerEventType::BowserFell;
        heardCastleClear |= event.type == PlatformerEventType::CastleClear;
    }
    assert(heardBowserFall);
    assert(heardCastleClear);
}

void testFinalCastleRemainsWon() {
    PlatformerEngine engine;
    assert(engine.startCampaign(8, 4));
    PlatformerEvent discarded;
    while (engine.pollEvent(discarded)) {
    }
    engine.debugSetPlayer(engine.goalX(), 48.0F, 0.0F, 0.0F, false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::CastleBridge);
    uint16_t frames = 0U;
    while (engine.phase() == PlatformerPhase::CastleBridge && frames < 720U) {
        engine.step(1.0F / 60.0F, PlatformerInput{});
        ++frames;
    }
    assert(frames < 720U);
    const auto won = engine.snapshot();
    assert(won.phase == PlatformerPhase::Won);
    assert(won.world == 8U && won.stage == 4U);
    assert(won.playerVisible);
    PlatformerEvent clear;
    assert(takeEvent(engine, PlatformerEventType::CourseClear, clear));
    assert(clear.value == 0x0804U);
}

void testCampaignVineRoute() {
    PlatformerEngine engine;
    assert(engine.startCampaign(5, 2));
    constexpr float blockX = 85.0F * 16.0F;
    constexpr float blockY = 20.0F * 16.0F;
    engine.debugSetPlayer(blockX, blockY + 17.0F, 0.0F, -200.0F, false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.snapshot().vineActive);
    assert(std::fabs(engine.vine().grownPixels) < 0.01F);

    stepReferenceFrames(engine, 31U);
    assert(std::fabs(engine.vine().grownPixels - 15.5F) < 0.01F);
    stepReferenceFrames(engine, 1U);
    assert(std::fabs(engine.vine().grownPixels - 16.0F) < 0.01F);
    stepReferenceFrames(engine, 160U);
    const auto grown = engine.vine();
    assert(std::fabs(grown.grownPixels - 96.0F) < 0.01F);

    engine.debugSetPlayer(grown.x, grown.baseY - grown.grownPixels + 8.0F,
                          0.0F, 0.0F, false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(engine.phase() == PlatformerPhase::VineClimb);
    assert(std::fabs(engine.snapshot().playerX - (grown.x + 8.0F)) < 0.01F);
    assert(std::fabs(engine.snapshot().playerVy) < 0.01F);

    const float attachedY = engine.snapshot().playerY;
    stepReferenceFrames(engine, 5U);
    assert(engine.phase() == PlatformerPhase::VineClimb);
    assert(std::fabs(engine.snapshot().playerY - attachedY) < 0.01F);

    engine.step(1.0F / 60.0F,
                PlatformerInput{0.0F, false, true});
    assert(std::fabs(engine.snapshot().playerVy + 45.0F) < 0.01F);
    stepReferenceFrames(engine, 205U);
    assert(engine.phase() == PlatformerPhase::VineClimb);
    stepReferenceFrames(engine, 1U);
    assert(engine.phase() == PlatformerPhase::Running);
    assert(std::fabs(engine.snapshot().cameraX - 81.0F * 16.0F) < 0.1F);
    assert(std::fabs(engine.snapshot().cameraY) < 0.1F);
    assert(std::fabs(engine.snapshot().playerX - 86.0F * 16.0F) < 0.1F);
    assert(std::fabs(engine.snapshot().playerVy + 45.0F) < 0.1F);
    assert(std::fabs(engine.vine().baseY - 239.5F) < 0.1F);

    engine.debugSetPlayer(engine.snapshot().playerX, 16.0F * 16.0F + 2.0F,
                          0.0F, 0.0F, false);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(std::fabs(engine.snapshot().playerX - 86.0F * 16.0F) < 0.1F);
    stepReferenceFrames(engine, 118U);
    assert(std::fabs(engine.snapshot().playerX - 86.0F * 16.0F) < 0.1F);
    stepReferenceFrames(engine, 1U);
    assert(engine.snapshot().vineActive);
    assert(std::fabs(engine.snapshot().playerX - 130.0F * 16.0F) < 0.1F);
    assert(std::fabs(engine.snapshot().playerY - 16.0F * 16.0F) < 0.1F);
    assert(std::fabs(engine.snapshot().cameraX - 128.0F * 16.0F) < 0.1F);
    assert(std::fabs(engine.snapshot().cameraY - 15.0F * 16.0F) < 0.1F);
}

void testReferenceCannonTimerAndSpawnOrdering() {
    constexpr uint16_t CANNON_COLUMN = 159U;
    constexpr uint8_t CANNON_ROW = 11U;
    constexpr float CANNON_X = CANNON_COLUMN * 16.0F;
    constexpr float CANNON_Y = CANNON_ROW * 16.0F;
    constexpr float CAMERA_X = (CANNON_COLUMN - 10U) * 16.0F;

    PlatformerEngine engine;
    assert(engine.prepareCampaignTitle(5, 1));
    assert(engine.startPreparedCampaign());
    engine.debugSetPlayer(CANNON_X, CANNON_Y - 16.0F, 0.0F, 0.0F,
                          true);
    engine.debugSetCamera(CAMERA_X);

    stepReferenceFrames(engine, 180U);
    assert(engine.phase() == PlatformerPhase::Running);
    assert(activeEnemyTypeCount(
               engine, pgos::PlatformerEnemyType::BulletBill) == 0U);

    stepReferenceFrames(engine, 299U);
    assert(activeEnemyTypeCount(
               engine, pgos::PlatformerEnemyType::BulletBill) == 0U);

    const uint32_t beforePauseFrame = engine.snapshot().logicFrame;
    engine.togglePause();
    stepReferenceFrames(engine, 120U);
    assert(engine.snapshot().logicFrame == beforePauseFrame);
    assert(activeEnemyTypeCount(
               engine, pgos::PlatformerEnemyType::BulletBill) == 0U);
    engine.togglePause();

    engine.step(1.0F / 60.0F, PlatformerInput{});
    const uint32_t shotFrame = engine.snapshot().logicFrame;
    assert(activeEnemyTypeCount(
               engine, pgos::PlatformerEnemyType::BulletBill) == 1U);

    pgos::PlatformerEnemyState shot;
    bool foundShot = false;
    for (uint8_t index = 0; index < engine.snapshot().enemyCount; ++index) {
        const auto enemy = engine.enemy(index);
        if (enemy.active &&
            enemy.type == pgos::PlatformerEnemyType::BulletBill) {
            shot = enemy;
            foundShot = true;
            break;
        }
    }
    assert(foundShot);
    assert(shot.bornFrame == shotFrame);
    assert(shot.sourceTileId == 90U);
    assert(std::fabs(std::fabs(shot.vx) - 90.0F) < 0.01F);
    const float expectedSpawnX =
        CANNON_X + (shot.vx > 0.0F ? 16.0F : -16.0F);
    assert(std::fabs(shot.x - expectedSpawnX) < 0.01F);
    assert(std::fabs(shot.y - CANNON_Y) < 0.01F);
    assert(receivedEvent(engine, PlatformerEventType::CannonFired));

    engine.step(1.0F / 60.0F, PlatformerInput{});
    bool foundMovedShot = false;
    for (uint8_t index = 0; index < engine.snapshot().enemyCount; ++index) {
        const auto enemy = engine.enemy(index);
        if (enemy.active && enemy.bornFrame == shotFrame &&
            enemy.type == pgos::PlatformerEnemyType::BulletBill) {
            assert(std::fabs(enemy.x - (shot.x + shot.vx / 60.0F)) < 0.01F);
            foundMovedShot = true;
        }
    }
    assert(foundMovedShot);

    PlatformerEngine offscreen;
    assert(offscreen.startCampaign(5, 1));
    offscreen.debugSetPlayer(CANNON_X, CANNON_Y - 16.0F, 0.0F, 0.0F,
                             true);
    offscreen.debugSetCamera(0.0F);
    receivedEvent(offscreen, PlatformerEventType::LifeRestarted);
    stepReferenceFrames(offscreen, 300U);
    assert(activeEnemyTypeCount(
               offscreen, pgos::PlatformerEnemyType::BulletBill) == 0U);
    assert(!receivedEvent(offscreen, PlatformerEventType::CannonFired));

    offscreen.debugSetCamera(CAMERA_X);
    stepReferenceFrames(offscreen, 299U);
    assert(activeEnemyTypeCount(
               offscreen, pgos::PlatformerEnemyType::BulletBill) == 0U);
    offscreen.step(1.0F / 60.0F, PlatformerInput{});
    assert(activeEnemyTypeCount(
               offscreen, pgos::PlatformerEnemyType::BulletBill) == 1U);
    assert(receivedEvent(offscreen, PlatformerEventType::CannonFired));
}

void testReferencePreplacedBulletBillLifetimeAndSound() {
    PlatformerEngine engine;
    assert(engine.startCampaign(5, 3));
    receivedEvent(engine, PlatformerEventType::LifeRestarted);

    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(activeEnemyTypeCount(
               engine, pgos::PlatformerEnemyType::BulletBill) == 1U);
    bool foundInitialShot = false;
    for (uint8_t index = 0; index < engine.snapshot().enemyCount; ++index) {
        const auto enemy = engine.enemy(index);
        if (!enemy.active ||
            enemy.type != pgos::PlatformerEnemyType::BulletBill) {
            continue;
        }
        assert(enemy.sourceTileId == 90U);
        assert(enemy.bornFrame == 1U);
        assert(std::fabs(enemy.x - (16.0F * 16.0F - 1.5F)) < 0.01F);
        foundInitialShot = true;
    }
    assert(foundInitialShot);
    assert(receivedEvent(engine, PlatformerEventType::CannonFired));

    engine.debugSetCamera(79.0F * 16.0F);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(activeEnemyTypeCount(
               engine, pgos::PlatformerEnemyType::BulletBill) == 0U);
    assert(!receivedEvent(engine, PlatformerEventType::CannonFired));
    engine.debugSetCamera(99.0F * 16.0F);
    engine.step(1.0F / 60.0F, PlatformerInput{});
    assert(activeEnemyTypeCount(
               engine, pgos::PlatformerEnemyType::BulletBill) == 0U);
    assert(!receivedEvent(engine, PlatformerEventType::CannonFired));
}

void testCampaignEnemyStateMachines() {
    PlatformerEngine wallTurn;
    assert(wallTurn.startCampaign(1, 1));
    wallTurn.debugSetCamera(320.0F);
    wallTurn.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::Koopa, 480.0F, 184.0F, 38U);
    wallTurn.step(0.016F, PlatformerInput{});
    assert(wallTurn.enemy(0).vx > 0.0F);
    assert(!wallTurn.enemy(0).facingLeft);

    PlatformerEngine paratroopa;
    assert(paratroopa.startCampaign(1, 1));
    paratroopa.debugSpawnCampaignEnemy(
        pgos::PlatformerEnemyType::KoopaParatroopa, 100.0F, 160.0F, 40U);
    paratroopa.debugSetPlayer(100.0F, 150.0F, 0.0F, 180.0F, false);
    paratroopa.step(0.016F, PlatformerInput{});
    assert(paratroopa.enemy(0).type == pgos::PlatformerEnemyType::Koopa);
    assert(paratroopa.enemy(0).motion == PlatformerEnemyMotion::Walking);
    paratroopa.debugSetPlayer(100.0F, paratroopa.enemy(0).y - 7.0F,
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
        pgos::PlatformerEnemyType::HammerBro, 100.0F, 160.0F, 56U);
    stepReferenceFrames(hammer, 149);
    assert(hammer.snapshot().enemyHazardCount >= 1U);
    assert(hammer.enemyHazard(0).active);
    assert(hammer.enemyHazard(0).kind ==
           pgos::PlatformerEnemyHazardKind::Hammer);
}

}  // namespace

int main() {
    testReferenceGroundMotionTrace();
    testReferenceJumpMotionTrace();
    testReferenceHasNoCoyoteTimeOrJumpBuffer();
    testReferenceCrouchMotionAndHitbox();
    testReferenceUnderwaterMotionTrace();
    testReferenceIndependentAnimationClocks();
    testReferenceBasicEnemyAndShellMotion();
    testReferenceOrderedEnemyPairCollisions();
    testReferenceStompBounceVelocity();
    testReferenceShellAndCrushableRules();
    testReferencePowerupEmergenceAndMotion();
    testReferencePiranhaFrameCycle();
    testReferenceBlooperAttackCycle();
    testReferenceLakituControlAndSpinyDelay();
    testReferenceParatroopaAndProjectileEnemyMotion();
    testReferenceHammerBroReleaseDelay();
    testReferenceEnemyHazardsHaveNoAgeLimit();
    testReferenceScriptedPhasesKeepWorldRunning();
    testReferenceLavaBubbleCycle();
    testReferenceBowserTimersAndDelayedAttack();
    testReferenceFlyingCheepCallbackRange();
    testReferenceRisingCheepStompAtPlayerApex();
    testReferenceFireballSpawnAndFirstPhysicsFrame();
    testReferenceEnemyHitboxes();
    testReferenceFireballDirectionalTileCollisions();
    testReferenceFireballEnemyFilteringAndHitboxes();
    testGroundingAndJump();
    testStandingRemainsGrounded();
    testWalkingOffLedgeStartsFalling();
    testVariableJumpHeight();
    testWalkRunTurnAndCamera();
    testQuestionBoxWinsAdjacentOverlap();
    testPauseFreezesSimulation();
    testReferencePauseRunsOnlyQueuedCommands();
    testReferenceLevelData();
    testReferenceIgnoredEnemyLayerTilesDoNotSpawn();
    testCheckpointEnemySpawnClearsPipeEdge();
    testMapPanoramaMode();
    testCoinBoxAndMultiCoinBrick();
    testReferenceBlockBumpFrames();
    testBrickBreakAndSpecialContents();
    testDefeatedEnemiesIgnoreRepeatedAttacks();
    testGrowthFireAndCrouch();
    testReferencePowerupCollectionTransitions();
    testReferenceStarScoreBlinkAndLastFrameProtection();
    testReferenceDamageBlinkFrames();
    testReferenceCoinsDoNotConvertToLife();
    testFireMarioStillCollectsCoinsAndHitsBlocks();
    testFireFlowerRestsAndHiddenBlockBecomesFloor();
    testDamageDeathAndLifeRestart();
    testReferenceCrouchingDamageShrinkAnchor();
    testKoopaShellLifecycle();
    testSimultaneousEnemyStomp();
    testSquashedEnemyStopsCollidingImmediately();
    testReferenceStompPropagatesAcrossSameStepOverlaps();
    testCampaignBlockSeamAdvancesPastBrokenTile();
    testCampaignFlagUsesMovingFlagAnchor();
    testFireballPoolReusesExpiredSlots();
    testTimerAndCompleteGoalSequence();
    testReferenceFlagSequenceFrames();
    testCampaignRuntimeAndLevelAdvance();
    testReferenceInitialAndDeathLevelTransitions();
    testReferenceTileRoundnessAllowsTightOpenings();
    testCampaignPipeTransitions();
    testCampaignPreloadsReferenceEnemiesWithinFixedPool();
    testCastleTeleportLoops();
    testUnderwaterMovement();
    testCampaignMovingPlatformsAndPulleys();
    testWorldOneTwoDescendingPlatformsDoNotKillOnContact();
    testReferenceCloudPlatformStartsAfterLanding();
    testCampaignFireBars();
    testStartUndergroundIntro();
    testCampaignTrampoline();
    testCastleBridgeSequence();
    testFinalCastleRemainsWon();
    testCampaignVineRoute();
    testReferenceCannonTimerAndSpawnOrdering();
    testReferencePreplacedBulletBillLifetimeAndSound();
    testCampaignEnemyStateMachines();
    return 0;
}
