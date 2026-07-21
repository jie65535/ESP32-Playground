#include "games/PlatformerEngine.h"

#include <algorithm>
#include <cmath>

namespace pgos {

namespace {

constexpr float EPSILON = 0.01F;

template <typename Value>
Value clampValue(Value value, Value minimum, Value maximum) {
    return std::max(minimum, std::min(value, maximum));
}

}  // namespace

PlatformerEngine::PlatformerEngine() {
    reset();
}

void PlatformerEngine::reset() {
    phase_ = PlatformerPhase::Title;
    deathReason_ = PlatformerDeathReason::None;
    score_ = 0;
    coinsCollected_ = 0;
    lives_ = 3;
    timeRemaining_ = 400;
    levelClockMs_ = 0;
    phaseElapsedMs_ = 0;
    cameraX_ = 0.0F;
    flagY_ = static_cast<float>(PLATFORMER_LEVEL_1_1.flagTopY);
    goalX_ = static_cast<float>(PLATFORMER_LEVEL_1_1.goalX);
    castleX_ = static_cast<float>(PLATFORMER_LEVEL_1_1.castleX);
    playerPower_ = PlatformerPlayerPower::Small;
    playerCrouching_ = false;
    playerFacingLeft_ = false;
    playerSkidding_ = false;
    timeWarningSent_ = false;
    mapTestMode_ = false;
    jumpHoldMs_ = 0;
    coyoteMs_ = 0;
    jumpBufferMs_ = 0;
    powerTransitionMs_ = 0;
    hurtInvincibleMs_ = 0;
    starInvincibleMs_ = 0;
    fireCooldownMs_ = 0;
    stompChain_ = 0;
    eventRead_ = 0;
    eventWrite_ = 0;
    eventCount_ = 0;
    buildLevel();
    resetActors();
}

void PlatformerEngine::start() {
    reset();
    phase_ = PlatformerPhase::Running;
    player_.grounded = false;
    // The reference game lets the first simulation tick settle Mario onto the
    // floor. Keeping this false also makes the initial jump edge deterministic.
}

void PlatformerEngine::startMapTest() {
    reset();
    phase_ = PlatformerPhase::Running;
    mapTestMode_ = true;
    player_.grounded = true;
    player_.y = static_cast<float>(PLATFORMER_LEVEL_1_1.groundY) -
                playerHeight();
    cameraX_ = 0.0F;

    // Recreate every reference checkpoint spawn without walking Mario through
    // the course. Enemies stay fixed so overlapping panorama captures show
    // their exact initial world positions.
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        const float cameraAtCheckpoint = std::max(
            0.0F, enemy.activationX - REFERENCE_VIEWPORT_WIDTH / 3.0F);
        enemy.actor.x = cameraAtCheckpoint + REFERENCE_VIEWPORT_WIDTH +
                        enemy.spawnOrder * ENEMY_GROUP_SPACING;
        enemy.actor.vx = 0.0F;
        enemy.actor.vy = 0.0F;
        enemy.spawned = true;
        enemy.actor.active = true;
        enemy.motion = PlatformerEnemyMotion::Walking;
        placeEnemyAtSpawn(enemy);
    }
}

void PlatformerEngine::advanceMapTest() {
    if (!mapTestMode_) {
        return;
    }
    const float maximum = static_cast<float>(WORLD_WIDTH - VIEWPORT_WIDTH);
    cameraX_ = std::min(maximum, cameraX_ + MAP_TEST_CAMERA_STEP);
}

void PlatformerEngine::togglePause() {
    if (phase_ == PlatformerPhase::Running) {
        phase_ = PlatformerPhase::Paused;
    } else if (phase_ == PlatformerPhase::Paused) {
        phase_ = PlatformerPhase::Running;
    }
}

void PlatformerEngine::step(float deltaSeconds, const PlatformerInput& input) {
    const float dt = clampValue(deltaSeconds, 0.001F, 0.05F);
    const uint16_t dtMs = static_cast<uint16_t>(
        clampValue(static_cast<int32_t>(std::lround(dt * 1000.0F)),
                   static_cast<int32_t>(1), static_cast<int32_t>(50)));

    if (mapTestMode_) {
        if (phase_ == PlatformerPhase::Running) {
            updateMapTest(dt, dtMs);
        }
        return;
    }

    switch (phase_) {
        case PlatformerPhase::Running:
            updateRunning(dt, dtMs, input);
            break;
        case PlatformerPhase::Dying:
        case PlatformerPhase::Flagpole:
        case PlatformerPhase::CastleWalk:
        case PlatformerPhase::TimeBonus:
            updateScriptedPhase(dt, dtMs);
            break;
        case PlatformerPhase::Title:
        case PlatformerPhase::Paused:
        case PlatformerPhase::GameOver:
        case PlatformerPhase::Won:
            break;
    }
}

PlatformerPhase PlatformerEngine::phase() const {
    return phase_;
}

PlatformerSnapshot PlatformerEngine::snapshot() const {
    PlatformerSnapshot snapshot;
    snapshot.phase = phase_;
    snapshot.deathReason = deathReason_;
    snapshot.playerPower = playerPower_;
    snapshot.playerX = player_.x;
    snapshot.playerY = player_.y;
    snapshot.playerVx = player_.vx;
    snapshot.playerVy = player_.vy;
    snapshot.cameraX = cameraX_;
    snapshot.flagY = flagY_;
    snapshot.grounded = player_.grounded;
    snapshot.playerBig = playerPower_ != PlatformerPlayerPower::Small;
    snapshot.playerFire = playerPower_ == PlatformerPlayerPower::Fire;
    snapshot.playerCrouching = playerCrouching_;
    snapshot.playerFacingLeft = playerFacingLeft_;
    snapshot.playerSkidding = playerSkidding_;
    snapshot.playerVisible = player_.active &&
                             (powerTransitionMs_ == 0 ||
                              ((powerTransitionMs_ / 65U) & 1U) == 0U);
    snapshot.playerInvincible = starInvincibleMs_ > 0;
    snapshot.goalReached = phase_ == PlatformerPhase::Flagpole ||
                           phase_ == PlatformerPhase::CastleWalk ||
                           phase_ == PlatformerPhase::TimeBonus ||
                           phase_ == PlatformerPhase::Won;
    snapshot.mapTestMode = mapTestMode_;
    snapshot.score = score_;
    snapshot.timeRemaining = timeRemaining_;
    snapshot.phaseElapsedMs = phaseElapsedMs_;
    snapshot.lives = lives_;
    snapshot.coinsCollected = coinsCollected_;
    snapshot.totalBoxes = boxCount_;
    snapshot.enemyCount = enemyCount_;
    snapshot.powerupCount = powerupCount_;
    snapshot.effectCount = effectCount_;
    snapshot.projectileCount = projectileCount_;
    return snapshot;
}

PlatformerBox PlatformerEngine::box(uint8_t index) const {
    return index < boxCount_ ? boxes_[index] : PlatformerBox{};
}

PlatformerEnemyState PlatformerEngine::enemy(uint8_t index) const {
    if (index >= enemyCount_) {
        return PlatformerEnemyState{};
    }
    const EnemyActor& source = enemies_[index];
    PlatformerEnemyState state;
    state.type = source.type;
    state.motion = source.motion;
    state.x = source.actor.x;
    state.y = source.actor.y;
    state.vx = source.actor.vx;
    state.vy = source.actor.vy;
    state.width = source.width;
    state.height = source.height;
    state.stateMs = source.stateMs;
    state.active = source.spawned && source.actor.active;
    state.facingLeft = source.actor.vx < 0.0F;
    return state;
}

PlatformerPowerup PlatformerEngine::powerup(uint8_t index) const {
    return index < powerupCount_ ? powerups_[index] : PlatformerPowerup{};
}

PlatformerEffect PlatformerEngine::effect(uint8_t index) const {
    return index < effectCount_ ? effects_[index] : PlatformerEffect{};
}

PlatformerProjectile PlatformerEngine::projectile(uint8_t index) const {
    return index < projectileCount_ ? projectiles_[index]
                                    : PlatformerProjectile{};
}

float PlatformerEngine::goalX() const {
    return goalX_;
}

float PlatformerEngine::castleX() const {
    return castleX_;
}

bool PlatformerEngine::pollEvent(PlatformerEvent& event) {
    if (eventCount_ == 0) {
        return false;
    }
    event = events_[eventRead_];
    eventRead_ = static_cast<uint8_t>((eventRead_ + 1U) % EVENT_QUEUE_SIZE);
    --eventCount_;
    return true;
}

#if defined(PGOS_PLATFORMER_TESTING)
void PlatformerEngine::debugSetPlayer(float x, float y, float vx, float vy,
                                      bool grounded) {
    player_.x = x;
    player_.y = y;
    player_.vx = vx;
    player_.vy = vy;
    player_.grounded = grounded;
    updateCamera();
}

void PlatformerEngine::debugSetPlayerPower(PlatformerPlayerPower power) {
    const float oldHeight = playerHeight();
    playerPower_ = power;
    playerCrouching_ = false;
    player_.y += oldHeight - playerHeight();
}

void PlatformerEngine::debugHitBox(uint8_t index) {
    hitBox(index);
}

void PlatformerEngine::debugSpawnPowerup(PlatformerPowerupKind kind, float x,
                                         float y) {
    spawnPowerup(kind, x, y);
}

void PlatformerEngine::debugActivateEnemy(uint8_t index, float x, float y,
                                          PlatformerEnemyMotion motion) {
    if (index >= enemyCount_) {
        return;
    }
    EnemyActor& enemy = enemies_[index];
    enemy.spawned = true;
    enemy.actor.active = true;
    enemy.actor.x = x;
    enemy.actor.y = y;
    enemy.actor.vy = 0.0F;
    enemy.motion = motion;
    if (motion == PlatformerEnemyMotion::ShellIdle ||
        motion == PlatformerEnemyMotion::ShellSliding) {
        enemy.height = 16.0F;
        enemy.actor.vx = motion == PlatformerEnemyMotion::ShellSliding
                             ? SHELL_SPEED
                             : 0.0F;
    }
}

void PlatformerEngine::debugBeginGoal() {
    beginGoal();
}

void PlatformerEngine::debugSetTimeRemaining(uint16_t value) {
    timeRemaining_ = value;
}

bool PlatformerEngine::debugTileSolid(uint16_t column, uint8_t row) const {
    if (column >= MAP_WIDTH || row >= MAP_HEIGHT) {
        return false;
    }
    const float x = static_cast<float>(column * TILE_SIZE);
    const float y = static_cast<float>(row * TILE_SIZE);
    for (uint8_t index = 0; index < PLATFORMER_LEVEL_1_1.solidCount;
         ++index) {
        const auto& solid = PLATFORMER_LEVEL_1_1.solids[index];
        if (overlaps(x, y, TILE_SIZE, TILE_SIZE,
                     static_cast<float>(solid.x),
                     static_cast<float>(solid.y),
                     static_cast<float>(solid.width),
                     static_cast<float>(solid.height))) {
            return true;
        }
    }
    return false;
}
#endif

void PlatformerEngine::buildLevel() {
    boxCount_ = 0;

    for (uint8_t index = 0; index < PLATFORMER_LEVEL_1_1.objectCount &&
                           index < MAX_BOXES;
         ++index) {
        const PlatformerObjectSpawn& spawn =
            PLATFORMER_LEVEL_1_1.objects[index];
        PlatformerBox box;
        box.x = spawn.x;
        box.y = spawn.y;
        box.reward = spawn.reward;
        box.type = spawn.type;
        box.visible = spawn.type != PlatformerObjectType::HiddenBox;
        box.remainingUses = spawn.uses == 0 ? 1 : spawn.uses;
        boxes_[boxCount_] = box;
        boxBumpMs_[boxCount_] = 0;
        ++boxCount_;
    }
}

void PlatformerEngine::resetActors() {
    player_ = Actor{};
    player_.x = 41.0F;
    player_.y = static_cast<float>(PLATFORMER_LEVEL_1_1.groundY) -
                PLAYER_HEIGHT;
    player_.active = true;
    enemyCount_ = 0;
    powerupCount_ = 0;
    effectCount_ = 0;
    projectileCount_ = 0;
    for (auto& powerup : powerups_) {
        powerup = PlatformerPowerup{};
    }
    for (auto& effect : effects_) {
        effect = PlatformerEffect{};
    }
    for (auto& projectile : projectiles_) {
        projectile = PlatformerProjectile{};
    }

    for (uint8_t index = 0; index < PLATFORMER_LEVEL_1_1.enemyCount &&
                           index < MAX_ENEMIES;
         ++index) {
        const PlatformerEnemySpawn& spawn =
            PLATFORMER_LEVEL_1_1.enemies[index];
        EnemyActor& enemy = enemies_[enemyCount_++];
        enemy = EnemyActor{};
        enemy.type = spawn.type;
        enemy.actor.x = spawn.x;
        enemy.actor.y = spawn.y;
        enemy.actor.vx = spawn.type == PlatformerEnemyType::Koopa
                             ? -KOOPA_SPEED
                             : -ENEMY_SPEED;
        enemy.actor.active = true;
        enemy.activationX = spawn.leftLimit;
        enemy.width = ENEMY_WIDTH;
        enemy.height = spawn.type == PlatformerEnemyType::Koopa ? 24.0F
                                                                  : ENEMY_HEIGHT;
        uint8_t order = 0;
        for (uint8_t previous = 0; previous + 1U < enemyCount_; ++previous) {
            if (std::fabs(enemies_[previous].activationX - enemy.activationX) <
                0.1F) {
                ++order;
            }
        }
        enemy.spawnOrder = order;
    }
    updateCamera();
}

void PlatformerEngine::resetLife() {
    buildLevel();
    resetActors();
    phase_ = PlatformerPhase::Running;
    deathReason_ = PlatformerDeathReason::None;
    phaseElapsedMs_ = 0;
    levelClockMs_ = 0;
    timeRemaining_ = 400;
    flagY_ = static_cast<float>(PLATFORMER_LEVEL_1_1.flagTopY);
    playerPower_ = PlatformerPlayerPower::Small;
    playerCrouching_ = false;
    playerSkidding_ = false;
    powerTransitionMs_ = 0;
    hurtInvincibleMs_ = 0;
    starInvincibleMs_ = 0;
    fireCooldownMs_ = 0;
    jumpHoldMs_ = 0;
    coyoteMs_ = 0;
    jumpBufferMs_ = 0;
    stompChain_ = 0;
    timeWarningSent_ = false;
    queueEvent(PlatformerEventType::LifeRestarted, lives_);
}

void PlatformerEngine::updateRunning(float dt, uint16_t dtMs,
                                     const PlatformerInput& input) {
    updateTimers(dtMs);
    updateBoxes(dtMs);
    updateEffects(dt, dtMs);

    if (phase_ != PlatformerPhase::Running) {
        return;
    }

    if (powerTransitionMs_ > 0) {
        return;
    }

    updateCrouch(input.crouchHeld);

    if (input.jumpPressed) {
        jumpBufferMs_ = JUMP_BUFFER_MS;
    } else if (jumpBufferMs_ > 0) {
        jumpBufferMs_ = static_cast<uint16_t>(
            jumpBufferMs_ > dtMs ? jumpBufferMs_ - dtMs : 0);
    }
    if (player_.grounded) {
        coyoteMs_ = COYOTE_TIME_MS;
    } else if (coyoteMs_ > 0) {
        coyoteMs_ = static_cast<uint16_t>(
            coyoteMs_ > dtMs ? coyoteMs_ - dtMs : 0);
    }

    if (input.actionPressed && playerPower_ == PlatformerPlayerPower::Fire &&
        fireCooldownMs_ == 0) {
        shootFireball();
    }

    const float previousBottom = player_.y + playerHeight();
    updatePlayerHorizontal(dt, input);
    updatePlayerVertical(dt, dtMs, input);

    updateEnemies(dt, dtMs);
    updatePowerups(dt, dtMs);
    updateProjectiles(dt, dtMs);
    collectPowerups();
    checkEnemyCollisions(previousBottom);
    checkEnemyPairCollisions();

    if (phase_ != PlatformerPhase::Running) {
        updateCamera();
        return;
    }

    if (player_.y > WORLD_HEIGHT + TILE_SIZE) {
        beginDeath(PlatformerDeathReason::Fall);
    } else if (player_.x + playerWidth() >= goalX_) {
        beginGoal();
    }
    updateCamera();
}

void PlatformerEngine::updateMapTest(float dt, uint16_t dtMs) {
    (void)dt;
    phaseElapsedMs_ = static_cast<uint16_t>(
        std::min<uint32_t>(65535U, phaseElapsedMs_ + dtMs));
}

void PlatformerEngine::updateScriptedPhase(float dt, uint16_t dtMs) {
    phaseElapsedMs_ = static_cast<uint16_t>(
        std::min<uint32_t>(65535U, phaseElapsedMs_ + dtMs));
    updateBoxes(dtMs);
    updateEffects(dt, dtMs);

    if (phase_ == PlatformerPhase::Dying) {
        if (phaseElapsedMs_ > 350U) {
            player_.vy = std::min(player_.vy + GRAVITY * dt, MAX_FALL_SPEED);
            player_.y += player_.vy * dt;
        }
        if (phaseElapsedMs_ >= 2200U) {
            if (lives_ > 1) {
                --lives_;
                resetLife();
            } else {
                lives_ = 0;
                phase_ = PlatformerPhase::GameOver;
                phaseElapsedMs_ = 0;
            }
        }
        updateCamera();
        return;
    }

    if (phase_ == PlatformerPhase::Flagpole) {
        const float floorY = static_cast<float>(PLATFORMER_LEVEL_1_1.groundY) -
                             playerHeight();
        player_.x = goalX_ - playerWidth();
        player_.y = std::min(floorY, player_.y + 90.0F * dt);
        flagY_ = std::min(
            static_cast<float>(PLATFORMER_LEVEL_1_1.flagSlideY),
            flagY_ + 94.0F * dt);
        if (phaseElapsedMs_ >= 1800U) {
            phase_ = PlatformerPhase::CastleWalk;
            phaseElapsedMs_ = 0;
            player_.x = goalX_ - playerWidth();
            player_.y = floorY;
            player_.vx = 0.0F;
        }
        updateCamera();
        return;
    }

    if (phase_ == PlatformerPhase::CastleWalk) {
        player_.x = std::min(castleX_, player_.x + 72.0F * dt);
        player_.y = static_cast<float>(PLATFORMER_LEVEL_1_1.groundY) -
                    playerHeight();
        player_.vx = 72.0F;
        if (player_.x >= castleX_) {
            phase_ = PlatformerPhase::TimeBonus;
            phaseElapsedMs_ = 0;
            player_.active = false;
        }
        updateCamera();
        return;
    }

    if (phase_ == PlatformerPhase::TimeBonus) {
        player_.active = false;
        if (timeRemaining_ > 0 && phaseElapsedMs_ % 16U < dtMs) {
            --timeRemaining_;
            addScore(50, player_.x, player_.y);
        }
        if (timeRemaining_ == 0 && phaseElapsedMs_ >= 500U) {
            phase_ = PlatformerPhase::Won;
            phaseElapsedMs_ = 0;
            queueEvent(PlatformerEventType::CourseClear);
        }
        updateCamera();
    }
}

void PlatformerEngine::updateTimers(uint16_t dtMs) {
    auto reduce = [dtMs](uint16_t& timer) {
        timer = static_cast<uint16_t>(timer > dtMs ? timer - dtMs : 0);
    };
    reduce(powerTransitionMs_);
    reduce(hurtInvincibleMs_);
    reduce(starInvincibleMs_);
    reduce(fireCooldownMs_);

    levelClockMs_ = static_cast<uint16_t>(levelClockMs_ + dtMs);
    while (levelClockMs_ >= LEVEL_TICK_MS && timeRemaining_ > 0) {
        levelClockMs_ = static_cast<uint16_t>(levelClockMs_ - LEVEL_TICK_MS);
        --timeRemaining_;
        if (timeRemaining_ <= 100 && !timeWarningSent_) {
            timeWarningSent_ = true;
            queueEvent(PlatformerEventType::TimeWarning);
        }
        if (timeRemaining_ == 0) {
            beginDeath(PlatformerDeathReason::Time);
            return;
        }
    }
}

void PlatformerEngine::updateCamera() {
    const float maximum = static_cast<float>(WORLD_WIDTH - VIEWPORT_WIDTH);
    if (phase_ == PlatformerPhase::Title || phase_ == PlatformerPhase::GameOver) {
        cameraX_ = 0.0F;
        return;
    }

    constexpr float CAMERA_LEFT_EDGE = 80.0F;
    constexpr float CAMERA_RIGHT_EDGE = REFERENCE_VIEWPORT_WIDTH / 3.0F;
    const float screenX = player_.x - cameraX_;
    if (screenX < CAMERA_LEFT_EDGE) {
        cameraX_ = player_.x - CAMERA_LEFT_EDGE;
    } else if (screenX > CAMERA_RIGHT_EDGE) {
        cameraX_ = player_.x - CAMERA_RIGHT_EDGE;
    }
    cameraX_ = clampValue(cameraX_, 0.0F, maximum);
}

void PlatformerEngine::updatePlayerHorizontal(float dt,
                                               const PlatformerInput& input) {
    const float axis = clampValue(input.moveAxis, -1.0F, 1.0F);
    if (axis < -0.01F) {
        playerFacingLeft_ = true;
    } else if (axis > 0.01F) {
        playerFacingLeft_ = false;
    }

    const float target = axis * (input.actionHeld ? RUN_SPEED : WALK_SPEED);
    const bool reversing = std::fabs(target) > 0.01F &&
                           std::fabs(player_.vx) > 0.01F &&
                           ((target > 0.0F) != (player_.vx > 0.0F));
    const float acceleration = reversing
                                   ? TURN_ACCELERATION
                                   : player_.grounded
                                         ? (input.actionHeld ? RUN_ACCELERATION
                                                              : WALK_ACCELERATION)
                                         : AIR_ACCELERATION;
    if (target > player_.vx) {
        player_.vx = std::min(target, player_.vx + acceleration * dt);
    } else if (target < player_.vx) {
        player_.vx = std::max(target, player_.vx - acceleration * dt);
    } else if (player_.vx > 0.0F) {
        player_.vx = std::max(0.0F, player_.vx - WALK_ACCELERATION * dt);
    } else if (player_.vx < 0.0F) {
        player_.vx = std::min(0.0F, player_.vx + WALK_ACCELERATION * dt);
    }
    playerSkidding_ = reversing && std::fabs(player_.vx) > 20.0F;
    if (std::fabs(player_.vx) < 0.5F) {
        player_.vx = 0.0F;
        playerSkidding_ = false;
    }
    if (player_.grounded && player_.vy == 0.0F && std::fabs(axis) < 0.01F) {
        stompChain_ = 0;
    }
    moveHorizontal(player_.vx * dt);
}

void PlatformerEngine::updatePlayerVertical(float dt, uint16_t dtMs,
                                             const PlatformerInput& input) {
    if (jumpBufferMs_ > 0 && !playerCrouching_ &&
        (player_.grounded || coyoteMs_ > 0)) {
        player_.vy = std::fabs(player_.vx) > WALK_SPEED * 0.75F
                         ? -FAST_JUMP_SPEED
                         : -JUMP_SPEED;
        player_.grounded = false;
        jumpBufferMs_ = 0;
        jumpHoldMs_ = 0;
        jumpActive_ = true;
        queueEvent(PlatformerEventType::Jumped);
    }

    const float previousY = player_.y;
    const float verticalVelocity = player_.vy;
    moveVertical(player_.vy * dt);
    if (player_.grounded) {
        player_.vy = 0.0F;
        jumpActive_ = false;
        jumpHoldMs_ = 0;
        coyoteMs_ = COYOTE_TIME_MS;
    } else if (jumpActive_ && player_.vy < 0.0F &&
               (input.jumpHeld || jumpHoldMs_ < MIN_JUMP_HOLD_MS) &&
               jumpHoldMs_ < MAX_JUMP_HOLD_MS) {
        player_.vy = std::min(player_.vy + JUMP_HOLD_GRAVITY * dt,
                              MAX_FALL_SPEED);
        jumpHoldMs_ = static_cast<uint16_t>(
            std::min<uint32_t>(MAX_JUMP_HOLD_MS, jumpHoldMs_ + dtMs));
    } else {
        jumpActive_ = false;
        player_.vy = std::min(player_.vy + GRAVITY * dt, MAX_FALL_SPEED);
    }

    if (verticalVelocity < 0.0F) {
        checkBoxCollision(previousY, verticalVelocity);
    }
}

void PlatformerEngine::moveHorizontal(float distance) {
    if (std::fabs(distance) < EPSILON) {
        return;
    }
    const float previousX = player_.x;
    player_.x += distance;
    if (!rectHitsSolid(player_.x, player_.y, playerWidth(), playerHeight())) {
        player_.x = clampValue(player_.x, 0.0F,
                               static_cast<float>(WORLD_WIDTH) - playerWidth());
        return;
    }

    float edge = distance > 0.0F ? static_cast<float>(WORLD_WIDTH) : 0.0F;
    bool found = false;
    auto consider = [&](float rx, float ry, float rw, float rh) {
        if (!overlaps(player_.x, player_.y, playerWidth(), playerHeight(), rx,
                      ry, rw, rh) ||
            player_.y + playerHeight() <= ry || player_.y >= ry + rh) {
            return;
        }
        if (distance > 0.0F && previousX + playerWidth() <= rx + EPSILON &&
            (!found || rx < edge)) {
            edge = rx;
            found = true;
        } else if (distance < 0.0F && previousX >= rx + rw - EPSILON &&
                   (!found || rx + rw > edge)) {
            edge = rx + rw;
            found = true;
        }
    };
    for (uint8_t index = 0; index < PLATFORMER_LEVEL_1_1.solidCount;
         ++index) {
        const auto& solid = PLATFORMER_LEVEL_1_1.solids[index];
        consider(static_cast<float>(solid.x), static_cast<float>(solid.y),
                 static_cast<float>(solid.width),
                 static_cast<float>(solid.height));
    }
    for (uint8_t index = 0; index < boxCount_; ++index) {
        const PlatformerBox& box = boxes_[index];
        if (!box.visible) {
            continue;
        }
        consider(static_cast<float>(box.x), static_cast<float>(box.y),
                 TILE_SIZE, TILE_SIZE);
    }
    if (found) {
        player_.x = distance > 0.0F ? edge - playerWidth() : edge;
    }
    player_.x = clampValue(player_.x, 0.0F,
                           static_cast<float>(WORLD_WIDTH) - playerWidth());
    player_.vx = 0.0F;
}

void PlatformerEngine::moveVertical(float distance) {
    if (std::fabs(distance) < EPSILON && player_.grounded) {
        if (rectHitsSolid(player_.x, player_.y + 1.0F, playerWidth(),
                          playerHeight())) {
            return;
        }
        player_.grounded = false;
    }
    const float previousY = player_.y;
    const float previousBottom = previousY + playerHeight();
    player_.grounded = false;
    player_.y += distance;
    if (!rectHitsSolid(player_.x, player_.y, playerWidth(), playerHeight())) {
        return;
    }

    float edge = distance > 0.0F ? static_cast<float>(WORLD_HEIGHT) : 0.0F;
    bool found = false;
    auto consider = [&](float rx, float ry, float rw, float rh) {
        if (!overlaps(player_.x, player_.y, playerWidth(), playerHeight(), rx,
                      ry, rw, rh) ||
            player_.x + playerWidth() <= rx || player_.x >= rx + rw) {
            return;
        }
        if (distance > 0.0F && previousBottom <= ry + EPSILON &&
            (!found || ry < edge)) {
            edge = ry;
            found = true;
        } else if (distance < 0.0F && previousY >= ry + rh - EPSILON &&
                   (!found || ry + rh > edge)) {
            edge = ry + rh;
            found = true;
        }
    };
    for (uint8_t index = 0; index < PLATFORMER_LEVEL_1_1.solidCount;
         ++index) {
        const auto& solid = PLATFORMER_LEVEL_1_1.solids[index];
        consider(static_cast<float>(solid.x), static_cast<float>(solid.y),
                 static_cast<float>(solid.width),
                 static_cast<float>(solid.height));
    }
    for (uint8_t index = 0; index < boxCount_; ++index) {
        const PlatformerBox& box = boxes_[index];
        if (!box.visible) {
            continue;
        }
        consider(static_cast<float>(box.x), static_cast<float>(box.y),
                 TILE_SIZE, TILE_SIZE);
    }
    if (found) {
        player_.y = distance > 0.0F ? edge - playerHeight() : edge;
        player_.grounded = distance > 0.0F;
    }
    player_.vy = 0.0F;
}

bool PlatformerEngine::rectHitsSolid(float x, float y, float width, float height,
                                     bool includeHidden) const {
    for (uint8_t index = 0; index < PLATFORMER_LEVEL_1_1.solidCount;
         ++index) {
        const auto& solid = PLATFORMER_LEVEL_1_1.solids[index];
        if (overlaps(x, y, width, height, static_cast<float>(solid.x),
                     static_cast<float>(solid.y),
                     static_cast<float>(solid.width),
                     static_cast<float>(solid.height))) {
            return true;
        }
    }
    for (uint8_t index = 0; index < boxCount_; ++index) {
        const PlatformerBox& box = boxes_[index];
        if (!box.visible && !includeHidden) {
            continue;
        }
        if (overlaps(x, y, width, height, static_cast<float>(box.x),
                     static_cast<float>(box.y), TILE_SIZE, TILE_SIZE)) {
            return true;
        }
    }
    return false;
}

void PlatformerEngine::updateBoxes(uint16_t dtMs) {
    for (uint8_t index = 0; index < boxCount_; ++index) {
        if (boxBumpMs_[index] == 0) {
            boxes_[index].bumpOffset = 0;
            continue;
        }
        boxBumpMs_[index] = static_cast<uint16_t>(
            boxBumpMs_[index] > dtMs ? boxBumpMs_[index] - dtMs : 0);
        const float progress =
            1.0F - static_cast<float>(boxBumpMs_[index]) / 180.0F;
        boxes_[index].bumpOffset = static_cast<int8_t>(
            std::lround(-5.0F * std::sin(progress * 3.14159265F)));
    }
}

void PlatformerEngine::updateEnemies(float dt, uint16_t dtMs) {
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        if (!enemy.spawned) {
            if (player_.x < enemy.activationX) {
                continue;
            }
            enemy.actor.x = cameraX_ + REFERENCE_VIEWPORT_WIDTH +
                            enemy.spawnOrder * ENEMY_GROUP_SPACING;
            placeEnemyAtSpawn(enemy);
            enemy.spawned = true;
            enemy.actor.active = true;
        }
        if (!enemy.actor.active) {
            continue;
        }
        if (enemy.motion == PlatformerEnemyMotion::Squashed) {
            enemy.stateMs = static_cast<uint16_t>(
                enemy.stateMs > dtMs ? enemy.stateMs - dtMs : 0);
            if (enemy.stateMs == 0) {
                enemy.actor.active = false;
                enemy.motion = PlatformerEnemyMotion::Defeated;
            }
            continue;
        }
        if (enemy.motion == PlatformerEnemyMotion::Defeated) {
            enemy.actor.active = false;
            continue;
        }

        if (enemy.motion == PlatformerEnemyMotion::ShellSliding) {
            enemy.actor.x += enemy.actor.vx * dt;
            if (rectHitsSolid(enemy.actor.x, enemy.actor.y, enemy.width,
                              enemy.height)) {
                enemy.actor.x -= enemy.actor.vx * dt;
                enemy.actor.vx = -enemy.actor.vx;
            }
        } else if (enemy.motion == PlatformerEnemyMotion::Walking) {
            const float nextX = enemy.actor.x + enemy.actor.vx * dt;
            if (!rectHitsSolid(nextX, enemy.actor.y, enemy.width,
                               enemy.height)) {
                enemy.actor.x = nextX;
            } else {
                enemy.actor.vx = -enemy.actor.vx;
            }
        }

        if (enemy.motion == PlatformerEnemyMotion::ShellIdle) {
            enemy.actor.vx = 0.0F;
        }
        enemy.actor.vy = std::min(enemy.actor.vy + GRAVITY * dt,
                                  MAX_FALL_SPEED);
        const float nextY = enemy.actor.y + enemy.actor.vy * dt;
        if (!rectHitsSolid(enemy.actor.x, nextY, enemy.width, enemy.height)) {
            enemy.actor.y = nextY;
            continue;
        }
        bool landed = false;
        for (uint8_t solidIndex = 0;
             solidIndex < PLATFORMER_LEVEL_1_1.solidCount; ++solidIndex) {
            const auto& solid = PLATFORMER_LEVEL_1_1.solids[solidIndex];
            if (!overlaps(enemy.actor.x, nextY, enemy.width, enemy.height,
                          static_cast<float>(solid.x),
                          static_cast<float>(solid.y),
                          static_cast<float>(solid.width),
                          static_cast<float>(solid.height)) ||
                enemy.actor.x + enemy.width <= solid.x ||
                enemy.actor.x >= solid.x + solid.width) {
                continue;
            }
            if (enemy.actor.vy >= 0.0F &&
                enemy.actor.y + enemy.height <= solid.y + EPSILON) {
                enemy.actor.y = static_cast<float>(solid.y) - enemy.height;
                enemy.actor.vy = 0.0F;
                landed = true;
            }
            break;
        }
        if (!landed) {
            enemy.actor.vy = 0.0F;
        }
        if (enemy.actor.y > WORLD_HEIGHT + 32.0F) {
            enemy.actor.active = false;
            enemy.motion = PlatformerEnemyMotion::Defeated;
        }
    }
}

void PlatformerEngine::placeEnemyAtSpawn(EnemyActor& enemy) {
    const float requestedX = enemy.actor.x;
    // In the reference game a checkpoint creates enemies exactly at the
    // logical viewport edge. Rounding the 2.679x world to integer LCD pixels
    // can put a 16px collision box one or two pixels inside a pipe or step.
    // Resolve that only at spawn, preserving the edge placement without
    // letting the enemy remain embedded and reverse forever.
    for (uint8_t offset = 0; offset <= 64U; ++offset) {
        const float candidate = requestedX - static_cast<float>(offset);
        if (!rectHitsSolid(candidate, enemy.actor.y, enemy.width,
                           enemy.height)) {
            enemy.actor.x = candidate;
            return;
        }
    }
    enemy.actor.x = requestedX;
}

void PlatformerEngine::updatePowerups(float dt, uint16_t dtMs) {
    for (uint8_t index = 0; index < powerupCount_; ++index) {
        PlatformerPowerup& powerup = powerups_[index];
        if (!powerup.active) {
            continue;
        }
        powerup.ageMs = static_cast<uint16_t>(
            std::min<uint32_t>(65535U, powerup.ageMs + dtMs));
        if (powerup.state == PlatformerPowerupState::Emerging) {
            powerup.y -= 80.0F * dt;
            if (powerup.ageMs >= 200U) {
                powerup.state = powerup.kind == PlatformerPowerupKind::FireFlower
                                    ? PlatformerPowerupState::Resting
                                    : PlatformerPowerupState::Moving;
            }
            continue;
        }

        if (powerup.kind == PlatformerPowerupKind::FireFlower) {
            continue;
        }

        const float nextX = powerup.x + powerup.vx * dt;
        if (!rectHitsSolid(nextX, powerup.y, 16.0F, 16.0F)) {
            powerup.x = nextX;
        } else {
            powerup.vx = -powerup.vx;
        }
        powerup.vy = std::min(powerup.vy + GRAVITY * dt, MAX_FALL_SPEED);
        const float nextY = powerup.y + powerup.vy * dt;
        if (!rectHitsSolid(powerup.x, nextY, 16.0F, 16.0F)) {
            powerup.y = nextY;
            continue;
        }
        if (powerup.vy >= 0.0F) {
            float landingY = powerup.y;
            bool foundLanding = false;
            auto considerTop = [&](float rx, float ry, float rw) {
                if (powerup.x + 16.0F <= rx || powerup.x >= rx + rw ||
                    powerup.y + 16.0F > ry + EPSILON ||
                    nextY + 16.0F < ry) {
                    return;
                }
                const float candidate = ry - 16.0F;
                if (!foundLanding || candidate < landingY) {
                    landingY = candidate;
                    foundLanding = true;
                }
            };
            for (uint8_t solidIndex = 0;
                 solidIndex < PLATFORMER_LEVEL_1_1.solidCount; ++solidIndex) {
                const auto& solid = PLATFORMER_LEVEL_1_1.solids[solidIndex];
                considerTop(static_cast<float>(solid.x),
                            static_cast<float>(solid.y),
                            static_cast<float>(solid.width));
            }
            for (uint8_t boxIndex = 0; boxIndex < boxCount_; ++boxIndex) {
                const auto& box = boxes_[boxIndex];
                if (box.visible) {
                    considerTop(static_cast<float>(box.x),
                                static_cast<float>(box.y), TILE_SIZE);
                }
            }
            if (foundLanding) {
                powerup.y = landingY;
            }
            if (powerup.kind == PlatformerPowerupKind::Star) {
                powerup.vy = -180.0F;
                powerup.state = PlatformerPowerupState::Bouncing;
            } else {
                powerup.vy = 0.0F;
            }
        } else {
            powerup.vy = 0.0F;
        }
    }
}

void PlatformerEngine::updateEffects(float dt, uint16_t dtMs) {
    for (uint8_t index = 0; index < effectCount_; ++index) {
        PlatformerEffect& effect = effects_[index];
        if (!effect.active) {
            continue;
        }
        effect.ageMs = static_cast<uint16_t>(
            std::min<uint32_t>(65535U, effect.ageMs + dtMs));
        effect.x += effect.vx * dt;
        effect.y += effect.vy * dt;
        if (effect.kind == PlatformerEffectKind::RisingCoin) {
            effect.vy += 620.0F * dt;
            if (effect.ageMs > 750U) {
                effect.active = false;
            }
        } else if (effect.kind == PlatformerEffectKind::BrickPiece) {
            effect.vy += 720.0F * dt;
            if (effect.y > WORLD_HEIGHT + 20.0F) {
                effect.active = false;
            }
        } else if (effect.ageMs > 900U) {
            effect.active = false;
        }
    }
}

void PlatformerEngine::updateProjectiles(float dt, uint16_t dtMs) {
    for (uint8_t index = 0; index < projectileCount_; ++index) {
        PlatformerProjectile& projectile = projectiles_[index];
        if (!projectile.active) {
            continue;
        }
        projectile.ageMs = static_cast<uint16_t>(
            std::min<uint32_t>(65535U, projectile.ageMs + dtMs));
        if (projectile.exploding) {
            if (projectile.ageMs > 120U) {
                projectile.active = false;
            }
            continue;
        }
        projectile.x += projectile.vx * dt;
        projectile.vy = std::min(projectile.vy + 820.0F * dt, 210.0F);
        projectile.y += projectile.vy * dt;
        if (rectHitsSolid(projectile.x, projectile.y, 8.0F, 8.0F)) {
            projectile.y -= projectile.vy * dt;
            projectile.vy = -180.0F;
        }
        if (projectile.x < cameraX_ - 64.0F ||
            projectile.x > cameraX_ + VIEWPORT_WIDTH + 128.0F ||
            projectile.y > WORLD_HEIGHT + 24.0F) {
            projectile.active = false;
            continue;
        }
        for (uint8_t enemyIndex = 0; enemyIndex < enemyCount_; ++enemyIndex) {
            EnemyActor& enemy = enemies_[enemyIndex];
            if (!enemy.actor.active ||
                !overlaps(projectile.x, projectile.y, 8.0F, 8.0F,
                          enemy.actor.x, enemy.actor.y, enemy.width,
                          enemy.height)) {
                continue;
            }
            defeatEnemy(enemy, 200, true);
            projectile.exploding = true;
            projectile.ageMs = 0;
            break;
        }
    }
}

void PlatformerEngine::collectPowerups() {
    for (uint8_t index = 0; index < powerupCount_; ++index) {
        PlatformerPowerup& powerup = powerups_[index];
        if (!powerup.active ||
            !overlaps(player_.x, player_.y, playerWidth(), playerHeight(),
                      powerup.x, powerup.y, 16.0F, 16.0F)) {
            continue;
        }
        powerup.active = false;
        switch (powerup.kind) {
            case PlatformerPowerupKind::Mushroom:
                if (playerPower_ == PlatformerPlayerPower::Small) {
                    playerPower_ = PlatformerPlayerPower::Big;
                    player_.y -= BIG_PLAYER_HEIGHT - PLAYER_HEIGHT;
                    powerTransitionMs_ = 950;
                }
                addScore(1000, player_.x, player_.y);
                break;
            case PlatformerPowerupKind::FireFlower:
                if (playerPower_ == PlatformerPlayerPower::Small) {
                    playerPower_ = PlatformerPlayerPower::Big;
                    player_.y -= BIG_PLAYER_HEIGHT - PLAYER_HEIGHT;
                }
                playerPower_ = PlatformerPlayerPower::Fire;
                powerTransitionMs_ = 950;
                addScore(1000, player_.x, player_.y);
                break;
            case PlatformerPowerupKind::Star:
                starInvincibleMs_ = 11000;
                addScore(1000, player_.x, player_.y);
                break;
            case PlatformerPowerupKind::OneUp:
                lives_ = static_cast<uint8_t>(std::min<uint16_t>(99, lives_ + 1));
                queueEvent(PlatformerEventType::OneUp, lives_);
                break;
        }
        queueEvent(PlatformerEventType::PowerupCollected,
                   static_cast<uint16_t>(powerup.kind));
    }
}

void PlatformerEngine::checkBoxCollision(float previousY,
                                          float verticalVelocity) {
    if (verticalVelocity >= 0.0F) {
        return;
    }

    uint8_t selected = boxCount_;
    float bestOverlap = 0.0F;
    float bestCenterDistance = static_cast<float>(WORLD_WIDTH);
    for (uint8_t index = 0; index < boxCount_; ++index) {
        PlatformerBox& box = boxes_[index];
        bool canHit = box.visible;
        if (!box.visible) {
            const float hiddenBottom = static_cast<float>(box.y + TILE_SIZE);
            canHit = previousY >= hiddenBottom - EPSILON &&
                     player_.y <= hiddenBottom + EPSILON;
        }
        if (!canHit) {
            continue;
        }

        const float boxBottom = static_cast<float>(box.y + TILE_SIZE);
        const bool crossesBottom = previousY >= boxBottom - EPSILON &&
                                   player_.y <= boxBottom + EPSILON;
        const float overlap =
            std::min(player_.x + playerWidth(),
                     static_cast<float>(box.x + TILE_SIZE)) -
            std::max(player_.x, static_cast<float>(box.x));
        if (!crossesBottom || overlap <= 0.0F) {
            continue;
        }

        const float centerDistance = std::fabs(
            player_.x + playerWidth() * 0.5F -
            (static_cast<float>(box.x) + TILE_SIZE * 0.5F));
        if (overlap > bestOverlap + EPSILON ||
            (std::fabs(overlap - bestOverlap) <= EPSILON &&
             centerDistance < bestCenterDistance)) {
            selected = index;
            bestOverlap = overlap;
            bestCenterDistance = centerDistance;
        }
    }

    if (selected < boxCount_) {
        PlatformerBox& box = boxes_[selected];
        box.visible = true;
        hitBox(selected);
        player_.y = static_cast<float>(box.y + TILE_SIZE);
        player_.vy = 10.0F;
    }
}

void PlatformerEngine::hitBox(uint8_t index) {
    if (index >= boxCount_) {
        return;
    }
    PlatformerBox& box = boxes_[index];
    box.visible = true;
    boxBumpMs_[index] = 180;

    if (box.type == PlatformerObjectType::CoinBrick &&
        box.reward == PlatformerBoxReward::None) {
        if (box.opened) {
            return;
        }
        if (playerPower_ != PlatformerPlayerPower::Small) {
            breakBrick(index);
        }
        queueEvent(PlatformerEventType::CoinBoxHit);
        bumpEnemiesAbove(box);
        return;
    }

    if (box.reward == PlatformerBoxReward::MultiCoin && box.remainingUses > 0) {
        --box.remainingUses;
        ++coinsCollected_;
        addScore(200, box.x, box.y - 8);
        spawnEffect(PlatformerEffectKind::RisingCoin, box.x + 4.0F,
                    box.y - 16.0F, 0.0F, -220.0F);
        if (box.remainingUses == 0) {
            box.opened = true;
        }
        queueEvent(PlatformerEventType::CoinBoxHit, box.remainingUses);
        bumpEnemiesAbove(box);
        return;
    }

    if (box.opened) {
        queueEvent(PlatformerEventType::CoinBoxHit);
        return;
    }
    box.opened = true;
    switch (box.reward) {
        case PlatformerBoxReward::Coin:
            ++coinsCollected_;
            addScore(200, box.x, box.y - 8);
            spawnEffect(PlatformerEffectKind::RisingCoin, box.x + 4.0F,
                        box.y - 16.0F, 0.0F, -220.0F);
            break;
        case PlatformerBoxReward::Mushroom:
            spawnPowerup(playerPower_ == PlatformerPlayerPower::Small
                             ? PlatformerPowerupKind::Mushroom
                             : PlatformerPowerupKind::FireFlower,
                         box.x, box.y);
            break;
        case PlatformerBoxReward::Star:
            spawnPowerup(PlatformerPowerupKind::Star, box.x, box.y);
            break;
        case PlatformerBoxReward::OneUp:
            spawnPowerup(PlatformerPowerupKind::OneUp, box.x, box.y);
            break;
        case PlatformerBoxReward::None:
        case PlatformerBoxReward::MultiCoin:
            break;
    }
    queueEvent(PlatformerEventType::CoinBoxHit);
    bumpEnemiesAbove(box);
}

void PlatformerEngine::breakBrick(uint8_t index) {
    if (index >= boxCount_) {
        return;
    }
    PlatformerBox& box = boxes_[index];
    box.opened = true;
    box.visible = false;
    box.remainingUses = 0;
    for (int8_t xSign : {-1, 1}) {
        for (int8_t ySign : {-1, 1}) {
            spawnEffect(PlatformerEffectKind::BrickPiece,
                        box.x + (xSign > 0 ? 8.0F : 0.0F),
                        box.y + (ySign > 0 ? 8.0F : 0.0F),
                        xSign * 66.0F, ySign < 0 ? -235.0F : -155.0F);
        }
    }
    addScore(50, box.x, box.y);
    queueEvent(PlatformerEventType::BrickBroken, 50);
    bumpEnemiesAbove(box);
}

void PlatformerEngine::bumpEnemiesAbove(const PlatformerBox& box) {
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        if (!enemy.actor.active ||
            !overlaps(static_cast<float>(box.x), box.y - 6.0F, TILE_SIZE, 8.0F,
                      enemy.actor.x, enemy.actor.y, enemy.width,
                      enemy.height)) {
            continue;
        }
        defeatEnemy(enemy, 100, true);
    }
}

void PlatformerEngine::checkEnemyCollisions(float previousBottom) {
    const float collisionX = player_.x;
    const float collisionY = player_.y;
    const float collisionWidth = playerWidth();
    const float collisionHeight = playerHeight();
    const bool descending = player_.vy > 0.0F;
    bool stomped = false;
    float bounceY = player_.y;
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        if (!enemy.actor.active ||
            !overlaps(collisionX, collisionY, collisionWidth, collisionHeight,
                      enemy.actor.x, enemy.actor.y, enemy.width,
                      enemy.height)) {
            continue;
        }
        if (starInvincibleMs_ > 0) {
            defeatEnemy(enemy, enemy.motion == PlatformerEnemyMotion::ShellSliding
                                  ? 200
                                  : 100,
                        true);
            continue;
        }
        if (enemy.motion == PlatformerEnemyMotion::ShellIdle &&
            !(descending && previousBottom <= enemy.actor.y + 5.0F)) {
            enemy.motion = PlatformerEnemyMotion::ShellSliding;
            enemy.actor.vx = player_.x < enemy.actor.x ? SHELL_SPEED
                                                        : -SHELL_SPEED;
            addScore(400, enemy.actor.x, enemy.actor.y);
            queueEvent(PlatformerEventType::ShellKicked, 400);
            continue;
        }
        if (descending && previousBottom <= enemy.actor.y + 5.0F) {
            if (enemy.motion == PlatformerEnemyMotion::Walking) {
                if (enemy.type == PlatformerEnemyType::Koopa) {
                    enemy.motion = PlatformerEnemyMotion::ShellIdle;
                    enemy.height = 16.0F;
                    enemy.actor.y += 8.0F;
                    enemy.actor.vx = 0.0F;
                } else {
                    enemy.motion = PlatformerEnemyMotion::Squashed;
                    enemy.stateMs = 600;
                    enemy.height = 8.0F;
                    enemy.actor.y += 8.0F;
                }
                ++stompChain_;
                const uint16_t points = stompChain_ == 1
                                             ? 100
                                             : stompChain_ == 2
                                                   ? 200
                                                   : stompChain_ == 3 ? 500
                                                                      : 1000;
                addScore(points, enemy.actor.x, enemy.actor.y);
                queueEvent(PlatformerEventType::EnemyStomped, points);
            } else if (enemy.motion == PlatformerEnemyMotion::ShellIdle) {
                enemy.motion = PlatformerEnemyMotion::ShellSliding;
                enemy.actor.vx = player_.x < enemy.actor.x ? SHELL_SPEED
                                                            : -SHELL_SPEED;
                addScore(400, enemy.actor.x, enemy.actor.y);
                queueEvent(PlatformerEventType::ShellKicked, 400);
            } else if (enemy.motion == PlatformerEnemyMotion::ShellSliding) {
                enemy.motion = PlatformerEnemyMotion::ShellIdle;
                enemy.actor.vx = 0.0F;
                addScore(400, enemy.actor.x, enemy.actor.y);
                queueEvent(PlatformerEventType::ShellKicked, 400);
            }
            bounceY = stomped
                          ? std::min(bounceY,
                                     enemy.actor.y - collisionHeight)
                          : enemy.actor.y - collisionHeight;
            stomped = true;
            continue;
        }
        if (enemy.motion == PlatformerEnemyMotion::ShellSliding) {
            hurtPlayer();
        } else if (enemy.motion != PlatformerEnemyMotion::Squashed) {
            hurtPlayer();
        }
        if (phase_ != PlatformerPhase::Running) {
            return;
        }
    }
    if (stomped && phase_ == PlatformerPhase::Running) {
        player_.y = bounceY;
        player_.vy = -STOMP_BOUNCE_SPEED;
        player_.grounded = false;
    }
}

void PlatformerEngine::checkEnemyPairCollisions() {
    for (uint8_t left = 0; left < enemyCount_; ++left) {
        EnemyActor& first = enemies_[left];
        if (!first.actor.active ||
            first.motion != PlatformerEnemyMotion::ShellSliding) {
            continue;
        }
        for (uint8_t right = 0; right < enemyCount_; ++right) {
            if (left == right) {
                continue;
            }
            EnemyActor& second = enemies_[right];
            if (!second.actor.active ||
                second.motion == PlatformerEnemyMotion::ShellSliding ||
                !overlaps(first.actor.x, first.actor.y, first.width,
                          first.height, second.actor.x, second.actor.y,
                          second.width, second.height)) {
                continue;
            }
            defeatEnemy(second, 100, true);
        }
    }
}

void PlatformerEngine::hurtPlayer() {
    if (hurtInvincibleMs_ > 0 || phase_ != PlatformerPhase::Running) {
        return;
    }
    if (playerPower_ != PlatformerPlayerPower::Small) {
        playerPower_ = PlatformerPlayerPower::Small;
        player_.y += BIG_PLAYER_HEIGHT - PLAYER_HEIGHT;
        powerTransitionMs_ = 915;
        hurtInvincibleMs_ = 1100;
        queueEvent(PlatformerEventType::PlayerHurt);
        return;
    }
    beginDeath(PlatformerDeathReason::Enemy);
}

void PlatformerEngine::beginDeath(PlatformerDeathReason reason) {
    if (phase_ != PlatformerPhase::Running) {
        return;
    }
    phase_ = PlatformerPhase::Dying;
    deathReason_ = reason;
    phaseElapsedMs_ = 0;
    player_.vx = 0.0F;
    player_.vy = -245.0F;
    player_.grounded = false;
    queueEvent(PlatformerEventType::PlayerDied,
               static_cast<uint16_t>(reason));
}

void PlatformerEngine::beginGoal() {
    if (phase_ != PlatformerPhase::Running) {
        return;
    }
    phase_ = PlatformerPhase::Flagpole;
    phaseElapsedMs_ = 0;
    player_.x = goalX_ - playerWidth();
    player_.vx = 0.0F;
    player_.vy = 0.0F;
    player_.grounded = false;
    const float bottom = player_.y + playerHeight();
    const uint16_t points = bottom > 171.0F
                                ? 100
                                : bottom > 126.0F
                                      ? 400
                                      : bottom > 96.0F ? 800
                                                       : bottom > 52.0F ? 2000
                                                                         : 5000;
    addScore(points, goalX_, player_.y);
    queueEvent(PlatformerEventType::ReachedGoal, points);
}

void PlatformerEngine::spawnPowerup(PlatformerPowerupKind kind, float x,
                                     float y) {
    uint8_t slot = MAX_POWERUPS;
    for (uint8_t index = 0; index < powerupCount_; ++index) {
        if (!powerups_[index].active) {
            slot = index;
            break;
        }
    }
    if (slot == MAX_POWERUPS && powerupCount_ < MAX_POWERUPS) {
        slot = powerupCount_++;
    }
    if (slot >= MAX_POWERUPS) {
        return;
    }
    PlatformerPowerup& powerup = powerups_[slot];
    powerup = PlatformerPowerup{};
    powerup.kind = kind;
    powerup.state = PlatformerPowerupState::Emerging;
    powerup.x = x;
    powerup.y = y;
    powerup.vx = kind == PlatformerPowerupKind::Star ? STAR_SPEED
                                                     : POWERUP_SPEED;
    powerup.vy = kind == PlatformerPowerupKind::Star ? -180.0F : 0.0F;
    powerup.active = true;
    queueEvent(PlatformerEventType::PowerupAppeared,
               static_cast<uint16_t>(kind));
}

void PlatformerEngine::spawnEffect(PlatformerEffectKind kind, float x, float y,
                                   float vx, float vy, uint16_t value) {
    uint8_t slot = MAX_EFFECTS;
    for (uint8_t index = 0; index < effectCount_; ++index) {
        if (!effects_[index].active) {
            slot = index;
            break;
        }
    }
    if (slot == MAX_EFFECTS && effectCount_ < MAX_EFFECTS) {
        slot = effectCount_++;
    }
    if (slot >= MAX_EFFECTS) {
        return;
    }
    effects_[slot] = PlatformerEffect{kind, x, y, vx, vy, value, 0, true};
}

void PlatformerEngine::shootFireball() {
    uint8_t slot = MAX_PROJECTILES;
    for (uint8_t index = 0; index < projectileCount_; ++index) {
        if (!projectiles_[index].active) {
            slot = index;
            break;
        }
    }
    if (slot == MAX_PROJECTILES && projectileCount_ < MAX_PROJECTILES) {
        slot = projectileCount_++;
    }
    if (slot >= MAX_PROJECTILES) {
        return;
    }
    PlatformerProjectile& projectile = projectiles_[slot];
    projectile = PlatformerProjectile{};
    projectile.x = playerFacingLeft_ ? player_.x - 8.0F
                                     : player_.x + playerWidth();
    projectile.y = player_.y + 10.0F;
    projectile.vx = playerFacingLeft_ ? -185.0F : 185.0F;
    projectile.active = true;
    fireCooldownMs_ = 220;
    queueEvent(PlatformerEventType::FireballShot);
}

void PlatformerEngine::defeatEnemy(EnemyActor& enemy, uint16_t points,
                                   bool launch) {
    if (!enemy.actor.active) {
        return;
    }
    enemy.motion = PlatformerEnemyMotion::Defeated;
    enemy.actor.active = false;
    if (launch) {
        spawnEffect(PlatformerEffectKind::BrickPiece, enemy.actor.x,
                    enemy.actor.y, enemy.actor.vx < 0.0F ? -50.0F : 50.0F,
                    -150.0F);
    }
    addScore(points, enemy.actor.x, enemy.actor.y);
    queueEvent(PlatformerEventType::EnemyDefeated, points);
}

void PlatformerEngine::addScore(uint16_t points, float x, float y) {
    score_ += points;
    spawnEffect(PlatformerEffectKind::Score, x, y, 0.0F, -30.0F, points);
}

void PlatformerEngine::queueEvent(PlatformerEventType type, uint16_t value) {
    if (eventCount_ >= EVENT_QUEUE_SIZE) {
        return;
    }
    events_[eventWrite_] = PlatformerEvent{type, score_, value};
    eventWrite_ = static_cast<uint8_t>((eventWrite_ + 1U) % EVENT_QUEUE_SIZE);
    ++eventCount_;
}

void PlatformerEngine::updateCrouch(bool crouchHeld) {
    if (playerPower_ == PlatformerPlayerPower::Small) {
        playerCrouching_ = false;
        return;
    }
    const float heightDelta = BIG_PLAYER_HEIGHT - CROUCH_PLAYER_HEIGHT;
    if (crouchHeld && player_.grounded) {
        if (!playerCrouching_) {
            player_.y += heightDelta;
            playerCrouching_ = true;
        }
        return;
    }
    if (!playerCrouching_) {
        return;
    }
    const float standingY = player_.y - heightDelta;
    if (!rectHitsSolid(player_.x, standingY, BIG_PLAYER_WIDTH,
                       BIG_PLAYER_HEIGHT)) {
        player_.y = standingY;
        playerCrouching_ = false;
    }
}

float PlatformerEngine::playerWidth() const {
    return playerPower_ == PlatformerPlayerPower::Small ? PLAYER_WIDTH
                                                        : BIG_PLAYER_WIDTH;
}

float PlatformerEngine::playerHeight() const {
    if (playerCrouching_) {
        return CROUCH_PLAYER_HEIGHT;
    }
    return playerPower_ == PlatformerPlayerPower::Small ? PLAYER_HEIGHT
                                                        : BIG_PLAYER_HEIGHT;
}

bool PlatformerEngine::overlaps(float ax, float ay, float aw, float ah,
                                float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

}  // namespace pgos
