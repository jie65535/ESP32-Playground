#include "games/PlatformerEngine.h"

#include "games/PlatformerTileAssets.h"

#include <algorithm>
#include <cmath>

namespace pgos {

namespace {

constexpr float EPSILON = 0.01F;

template <typename Value>
Value clampValue(Value value, Value minimum, Value maximum) {
    return std::max(minimum, std::min(value, maximum));
}

bool campaignEnemyType(uint16_t reference, PlatformerEnemyType& type) {
    switch (reference) {
        case 38:
        case 39:
        case 455:
            type = PlatformerEnemyType::Koopa;
            return true;
        case 40:
            type = PlatformerEnemyType::KoopaParatroopa;
            return true;
        case 44:
            type = PlatformerEnemyType::PiranhaPlant;
            return true;
        case 48:
            type = PlatformerEnemyType::Blooper;
            return true;
        case 50:
            type = PlatformerEnemyType::Lakitu;
            return true;
        case 56:
            type = PlatformerEnemyType::HammerBro;
            return true;
        case 61:
            type = PlatformerEnemyType::Bowser;
            return true;
        case 81:
        case 498:
            type = PlatformerEnemyType::CheepCheep;
            return true;
        case 87:
            type = PlatformerEnemyType::BuzzyBeetle;
            return true;
        case 90:
            type = PlatformerEnemyType::BulletBill;
            return true;
        case 504:
            type = PlatformerEnemyType::LavaBubble;
            return true;
        case 70:
        case 71:
            type = PlatformerEnemyType::Goomba;
            return true;
        default:
            return false;
    }
}

uint16_t cannonBulletSource(uint16_t cannonSource) {
    switch (cannonSource) {
        case 79U:
            return 195U;
        case 95U:
            return 300U;
        case 591U:
            return 405U;
        case 63U:
        default:
            return 90U;
    }
}

bool campaignEnemyUsesGravity(PlatformerEnemyType type) {
    switch (type) {
        case PlatformerEnemyType::PiranhaPlant:
        case PlatformerEnemyType::Blooper:
        case PlatformerEnemyType::CheepCheep:
        case PlatformerEnemyType::Lakitu:
        case PlatformerEnemyType::BulletBill:
        case PlatformerEnemyType::LavaBubble:
            return false;
        default:
            return true;
    }
}

bool isDefeatedParticle(PlatformerEnemyMotion motion) {
    return motion == PlatformerEnemyMotion::FallingDefeated ||
           motion == PlatformerEnemyMotion::Defeated;
}

uint8_t enemyAnimationDelay(PlatformerEnemyType type) {
    switch (type) {
        case PlatformerEnemyType::Goomba:
        case PlatformerEnemyType::Koopa:
        case PlatformerEnemyType::BuzzyBeetle:
        case PlatformerEnemyType::CheepCheep:
            return 10U;
        case PlatformerEnemyType::KoopaParatroopa:
        case PlatformerEnemyType::PiranhaPlant:
        case PlatformerEnemyType::Spiny:
        case PlatformerEnemyType::HammerBro:
            return 15U;
        case PlatformerEnemyType::Blooper:
        case PlatformerEnemyType::Bowser:
            return 30U;
        default:
            return 0U;
    }
}

}  // namespace

PlatformerEngine::PlatformerEngine() {
    reset();
}

void PlatformerEngine::reset() {
    logicFrame_ = 0U;
    animationFrame_ = 0U;
    randomState_ = 0x6D2B79F5UL;
    playerAnimationMode_ = 0U;
    playerAnimationFrame_ = 0U;
    playerAnimationTimer_ = 0U;
    phase_ = PlatformerPhase::Title;
    deathReason_ = PlatformerDeathReason::None;
    score_ = 0;
    coinsCollected_ = 0;
    lives_ = 3;
    timeRemaining_ = 400;
    levelClockFrames_ = 0;
    phaseElapsedMs_ = 0;
    phaseFrames_ = 0;
    cameraX_ = 0.0F;
    cameraY_ = 0.0F;
    flagY_ = static_cast<float>(PLATFORMER_LEVEL_1_1.flagTopY);
    goalX_ = static_cast<float>(PLATFORMER_LEVEL_1_1.goalX);
    castleX_ = static_cast<float>(PLATFORMER_LEVEL_1_1.castleX);
    playerPower_ = PlatformerPlayerPower::Small;
    playerCrouching_ = false;
    playerFacingLeft_ = false;
    playerSkidding_ = false;
    playerRunning_ = false;
    trampolineCollided_ = false;
    swimStrokeFrames_ = 0U;
    mapTestMode_ = false;
    campaignMode_ = false;
    cameraFrozen_ = false;
    warpState_ = 0U;
    startIntro_ = false;
    vineReturnActive_ = false;
    vineSequenceState_ = 0U;
    vineReturnFrames_ = 0U;
    flagLanded_ = false;
    flagShifted_ = false;
    timeBonusReady_ = false;
    timeBonusCompletionFrames_ = 0U;
    vine_ = PlatformerVineState{};
    activeWarpIndex_ = 0;
    powerTransitionFrames_ = 0;
    powerTransitionElapsedFrames_ = 0;
    hurtInvincibleFrames_ = 0;
    starInvincibleFrames_ = 0;
    starBlinkFrames_ = 0;
    fireballPoseFrames_ = 0U;
    powerTransition_ = PlatformerPowerTransition::None;
    starProtectedThisFrame_ = false;
    hurtProtectedThisFrame_ = false;
    stompChain_ = 0;
    playerAccelerationX_ = 0.0F;
    playerAccelerationY_ = 0.0F;
    cameraAdvanceX_ = 0.0F;
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

bool PlatformerEngine::startCampaign(uint8_t world, uint8_t stage) {
    reset();
    if (!levelRuntime_.load(world, stage)) {
        return false;
    }
    campaignMode_ = true;
    resetCampaignLevel(true);
    return true;
}

bool PlatformerEngine::prepareCampaignTitle(uint8_t world, uint8_t stage) {
    if (!startCampaign(world, stage)) {
        return false;
    }
    phase_ = PlatformerPhase::Title;
    phaseElapsedMs_ = 0;
    phaseFrames_ = 0;
    player_.active = false;
    startIntro_ = false;
    cameraFrozen_ = true;
    return true;
}

bool PlatformerEngine::startPreparedCampaign() {
    if (!campaignMode_ || phase_ != PlatformerPhase::Title ||
        levelRuntime_.level() == nullptr) {
        return false;
    }
    phase_ = PlatformerPhase::LevelTransition;
    phaseElapsedMs_ = 0U;
    phaseFrames_ = 0U;
    player_.active = false;
    startIntro_ = levelRuntime_.level()->levelType ==
                  PlatformerLevelType::StartUnderground;
    cameraFrozen_ = true;
    return true;
}

bool PlatformerEngine::advanceCampaign() {
    if (!campaignMode_ || phase_ != PlatformerPhase::Won ||
        levelRuntime_.level() == nullptr) {
        return false;
    }
    const PlatformerCampaignLevel& current = *levelRuntime_.level();
    if (current.nextWorld == 0U || current.nextStage == 0U) {
        return false;
    }
    return beginLevelTransition(current.nextWorld, current.nextStage);
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
        queueEvent(PlatformerEventType::Paused);
    } else if (phase_ == PlatformerPhase::Paused) {
        phase_ = PlatformerPhase::Running;
    }
}

void PlatformerEngine::step(float deltaSeconds, const PlatformerInput& input) {
    if (phase_ == PlatformerPhase::Title ||
        phase_ == PlatformerPhase::GameOver ||
        phase_ == PlatformerPhase::Won) {
        return;
    }
    if (phase_ == PlatformerPhase::Paused) {
        updatePausedCommands();
        return;
    }
    const bool cannonCallbacksEnabled =
        campaignMode_ && phase_ != PlatformerPhase::LevelTransition;
    const bool animationsEnabled =
        phase_ != PlatformerPhase::LevelTransition;
    const PlatformerPhase phaseAtFrameStart = phase_;
    ++logicFrame_;
    if (phase_ != PlatformerPhase::LevelTransition) {
        ++animationFrame_;
    }
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
        case PlatformerPhase::Warping:
        case PlatformerPhase::VineClimb:
        case PlatformerPhase::CastleBridge:
        case PlatformerPhase::Flagpole:
        case PlatformerPhase::CastleWalk:
        case PlatformerPhase::TimeBonus:
        case PlatformerPhase::LevelTransition:
            updateScriptedPhase(dt, dtMs, input);
            break;
        case PlatformerPhase::Title:
        case PlatformerPhase::Paused:
        case PlatformerPhase::GameOver:
        case PlatformerPhase::Won:
            break;
    }
    updatePlayerAnimation();
    if (campaignMode_ && animationsEnabled) {
        levelRuntime_.updateAnimations(
            cameraX_, cameraY_, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    }
    completePendingBrickBreak();
    if (cannonCallbacksEnabled &&
        phase_ != PlatformerPhase::LevelTransition &&
        phase_ != PlatformerPhase::GameOver) {
        updateCloudPlatformCallbacks();
        updateEnemyActivationCallbacks();
        updateCannonTimers();
    }
    if (phaseAtFrameStart == PlatformerPhase::Running &&
        (phase_ == PlatformerPhase::Running ||
         phase_ == PlatformerPhase::Warping ||
         phase_ == PlatformerPhase::VineClimb)) {
        updateLevelTimer();
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
    snapshot.cameraY = cameraY_;
    snapshot.flagX = flagX_;
    snapshot.flagY = flagY_;
    snapshot.flagTileId = flagTileId_;
    snapshot.grounded = player_.grounded;
    snapshot.playerBig = playerPower_ != PlatformerPlayerPower::Small ||
                         powerTransition_ != PlatformerPowerTransition::None;
    snapshot.playerFire = playerPower_ == PlatformerPlayerPower::Fire;
    snapshot.playerCrouching = playerCrouching_;
    snapshot.playerFacingLeft = playerFacingLeft_;
    snapshot.playerSkidding = playerSkidding_;
    snapshot.playerRunning = playerRunning_;
    snapshot.playerWalking = playerAnimationMode_ == 1U;
    snapshot.underwater =
        campaignMode_ && levelRuntime_.activeLevelType() ==
                             PlatformerLevelType::Underwater;
    snapshot.swimStrokeFrame = swimStrokeFrames_;
    snapshot.playerAnimationFrame = playerAnimationFrame_;
    const bool damageBlinking =
        hurtInvincibleFrames_ > 0U && powerTransitionFrames_ == 0U;
    const uint16_t hurtBlinkElapsed = damageBlinking
                                          ? static_cast<uint16_t>(
                                                150U - hurtInvincibleFrames_)
                                          : 0U;
    const uint16_t starBlinkElapsed = static_cast<uint16_t>(
        600U - std::min<uint16_t>(600U, starBlinkFrames_));
    const bool damageVisible =
        !damageBlinking || ((hurtBlinkElapsed / 10U) & 1U) == 0U;
    const bool starVisible =
        starBlinkFrames_ == 0U || ((starBlinkElapsed / 5U) & 1U) == 0U;
    snapshot.playerVisible = player_.active && damageVisible && starVisible;
    snapshot.playerInvincible = starInvincibleFrames_ > 0;
    snapshot.playerDamageBlinking = damageBlinking;
    snapshot.playerFireballPose = fireballPoseFrames_ > 0U;
    snapshot.playerPowerTransition = powerTransition_;
    snapshot.playerPowerTransitionFrame = powerTransitionElapsedFrames_;
    snapshot.goalReached = phase_ == PlatformerPhase::Flagpole ||
                           phase_ == PlatformerPhase::CastleBridge ||
                           phase_ == PlatformerPhase::CastleWalk ||
                           phase_ == PlatformerPhase::TimeBonus ||
                           phase_ == PlatformerPhase::Won;
    snapshot.mapTestMode = mapTestMode_;
    snapshot.campaignMode = campaignMode_;
    snapshot.score = score_;
    snapshot.logicFrame = logicFrame_;
    snapshot.animationFrame = animationFrame_;
    snapshot.timeRemaining = timeRemaining_;
    snapshot.phaseElapsedMs = phaseElapsedMs_;
    snapshot.phaseFrames = phaseFrames_;
    snapshot.lives = lives_;
    snapshot.coinsCollected = coinsCollected_;
    snapshot.totalBoxes = boxCount_;
    snapshot.enemyCount = enemyCount_;
    snapshot.powerupCount = powerupCount_;
    snapshot.effectCount = effectCount_;
    snapshot.projectileCount = projectileCount_;
    snapshot.movingPlatformCount = movingPlatformCount_;
    snapshot.fireBarCount = fireBarCount_;
    snapshot.vineActive = vine_.active;
    snapshot.enemyHazardCount = enemyHazardCount_;
    snapshot.world = levelRuntime_.world() == 0U ? 1U : levelRuntime_.world();
    snapshot.stage = levelRuntime_.stage() == 0U ? 1U : levelRuntime_.stage();
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
    state.sourceTileId = source.sourceTileId;
    state.bornFrame = source.bornFrame;
    state.animationFrame = source.animationFrame;
    state.heldHammer = source.type == PlatformerEnemyType::HammerBro &&
                       source.motion == PlatformerEnemyMotion::Walking &&
                       source.callbackFrames > 0U;
    state.heldHammerX =
        source.actor.x + source.width * 0.5F - TILE_SIZE * 0.5F;
    state.heldHammerY = source.heldHammerY;
    state.active = source.spawned && source.actor.active;
    state.facingLeft =
        source.type == PlatformerEnemyType::Bowser ||
                source.type == PlatformerEnemyType::HammerBro ||
                source.type == PlatformerEnemyType::Lakitu ||
                source.motion == PlatformerEnemyMotion::FallingDefeated ||
                source.motion == PlatformerEnemyMotion::Defeated
            ? source.facingLeft
            : source.actor.vx < 0.0F;
    state.alternatePose =
        (source.type == PlatformerEnemyType::HammerBro &&
         source.callbackFrames > 0U) ||
        (source.type == PlatformerEnemyType::Lakitu &&
         source.callbackFrames > 0U) ||
        (source.type == PlatformerEnemyType::Bowser &&
         source.fireCallbackFrames > 0U);
    state.verticalFlipped = source.verticalFlipped;
    state.flyingCheep = source.flyingCheep;
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

PlatformerMovingPlatformState PlatformerEngine::movingPlatform(
    uint8_t index) const {
    return index < movingPlatformCount_ ? movingPlatforms_[index]
                                        : PlatformerMovingPlatformState{};
}

PlatformerFireBarState PlatformerEngine::fireBar(uint8_t index) const {
    return index < fireBarCount_ ? fireBars_[index]
                                 : PlatformerFireBarState{};
}

PlatformerVineState PlatformerEngine::vine() const {
    return vine_;
}

PlatformerEnemyHazardState PlatformerEngine::enemyHazard(
    uint8_t index) const {
    return index < enemyHazardCount_ ? enemyHazards_[index]
                                     : PlatformerEnemyHazardState{};
}

float PlatformerEngine::goalX() const {
    return goalX_;
}

float PlatformerEngine::castleX() const {
    return castleX_;
}

const PlatformerLevelRuntime& PlatformerEngine::levelRuntime() const {
    return levelRuntime_;
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
    playerAccelerationX_ = 0.0F;
    playerAccelerationY_ = 0.0F;
    cameraAdvanceX_ = 0.0F;
    updateCamera();
}

void PlatformerEngine::debugSetCamera(float x, float y) {
    cameraX_ = x;
    cameraY_ = y;
}

void PlatformerEngine::debugSetPlayerPower(PlatformerPlayerPower power) {
    const float oldHeight = playerHeight();
    powerTransition_ = PlatformerPowerTransition::None;
    powerTransitionFrames_ = 0U;
    powerTransitionElapsedFrames_ = 0U;
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
    enemy.bornFrame = logicFrame_;
    enemy.motion = motion;
    if (motion == PlatformerEnemyMotion::ShellIdle ||
        motion == PlatformerEnemyMotion::ShellSliding) {
        enemy.height = 16.0F;
        enemy.actor.vx = motion == PlatformerEnemyMotion::ShellSliding
                             ? SHELL_SPEED
                             : 0.0F;
    }
}

void PlatformerEngine::debugSpawnCampaignEnemy(PlatformerEnemyType type,
                                                float x, float y,
                                                uint16_t sourceTileId) {
    campaignEnemiesLoaded_ = true;
    if (enemyCount_ >= MAX_ENEMIES) {
        return;
    }
    EnemyActor& enemy = enemies_[enemyCount_++];
    enemy = EnemyActor{};
    enemy.type = type;
    enemy.sourceTileId = sourceTileId;
    enemy.bornFrame = logicFrame_;
    enemy.actor.x = enemy.originX = x;
    enemy.actor.y = enemy.originY = y;
    enemy.actor.vx = -ENEMY_SPEED;
    enemy.actor.active = true;
    enemy.spawned = true;
    enemy.flyingCheep =
        type == PlatformerEnemyType::CheepCheep && sourceTileId == 498U &&
        levelRuntime_.activeLevelType() != PlatformerLevelType::Underwater;
    if (type == PlatformerEnemyType::Koopa ||
        type == PlatformerEnemyType::KoopaParatroopa) {
        enemy.height = 24.0F;
    } else if (type == PlatformerEnemyType::PiranhaPlant ||
               type == PlatformerEnemyType::Blooper ||
               type == PlatformerEnemyType::Lakitu ||
               type == PlatformerEnemyType::HammerBro) {
        enemy.height = 32.0F;
    } else if (type == PlatformerEnemyType::Bowser) {
        enemy.width = enemy.height = 32.0F;
        enemy.health = 5U;
    }
    if (type == PlatformerEnemyType::PiranhaPlant ||
        type == PlatformerEnemyType::Blooper ||
        type == PlatformerEnemyType::Lakitu ||
        type == PlatformerEnemyType::LavaBubble ||
        type == PlatformerEnemyType::Bowser) {
        enemy.actor.vx = 0.0F;
    } else if (type == PlatformerEnemyType::HammerBro) {
        enemy.actor.vx = 2.0F * REFERENCE_VELOCITY_SCALE;
    } else if (type == PlatformerEnemyType::BulletBill) {
        enemy.actor.vx = -3.0F * REFERENCE_VELOCITY_SCALE;
    }
    if (type == PlatformerEnemyType::Blooper) {
        enemy.accelerationY =
            -0.47480F * REFERENCE_VELOCITY_SCALE *
            REFERENCE_TICKS_PER_SECOND;
    } else if (type == PlatformerEnemyType::Bowser) {
        enemy.accelerationY =
            -0.30F * REFERENCE_VELOCITY_SCALE *
            REFERENCE_TICKS_PER_SECOND;
    }
}

void PlatformerEngine::debugSetEnemyVelocity(uint8_t index, float vx,
                                              float vy,
                                              float accelerationY) {
    if (index >= enemyCount_) {
        return;
    }
    enemies_[index].actor.vx = vx;
    enemies_[index].actor.vy = vy;
    enemies_[index].accelerationY = accelerationY;
}

void PlatformerEngine::debugSpawnProjectile(float x, float y, float vx,
                                             float vy) {
    if (projectileCount_ >= MAX_PROJECTILES) {
        return;
    }
    PlatformerProjectile& projectile = projectiles_[projectileCount_++];
    projectile = PlatformerProjectile{};
    projectile.x = x;
    projectile.y = y;
    projectile.vx = vx;
    projectile.vy = vy;
    projectile.bornFrame = logicFrame_ - 1U;
    projectile.active = true;
}

void PlatformerEngine::debugSpawnEnemyHazard(
    PlatformerEnemyHazardKind kind, float x, float y, float vx, float vy,
    float accelerationY) {
    spawnEnemyHazard(kind, x, y, vx, vy, accelerationY);
}

void PlatformerEngine::debugBeginGoal() {
    beginGoal();
}

void PlatformerEngine::debugSetTimeRemaining(uint16_t value) {
    timeRemaining_ = value;
}

void PlatformerEngine::debugSetCoinsCollected(uint16_t value) {
    coinsCollected_ = value;
}

void PlatformerEngine::debugSetLives(uint16_t value) {
    lives_ = value;
}

void PlatformerEngine::debugSetRandomState(uint32_t value) {
    randomState_ = value;
}

void PlatformerEngine::debugLoadCampaignEnemies() {
    if (!campaignMode_ || campaignEnemiesLoaded_) {
        return;
    }
    loadCampaignEnemies();
    campaignEnemiesLoaded_ = true;
}

bool PlatformerEngine::debugTileSolid(uint16_t column, uint8_t row) const {
    if (campaignMode_) {
        return levelRuntime_.isSolid(column, row);
    }
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
        boxBumpFrames_[boxCount_] = 0;
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
    campaignEnemiesLoaded_ = false;
    powerupCount_ = 0;
    effectCount_ = 0;
    projectileCount_ = 0;
    movingPlatformCount_ = 0;
    fireBarCount_ = 0;
    cannonCount_ = 0;
    cannonTimerFrames_ = CANNON_TIMER_FRAMES;
    enemyHazardCount_ = 0;
    pendingBrickBreak_ = PendingBrickBreak{};
    for (auto& powerup : powerups_) {
        powerup = PlatformerPowerup{};
    }
    for (auto& effect : effects_) {
        effect = PlatformerEffect{};
    }
    for (auto& projectile : projectiles_) {
        projectile = PlatformerProjectile{};
    }
    for (auto& hazard : enemyHazards_) {
        hazard = PlatformerEnemyHazardState{};
    }
    for (auto& platform : movingPlatforms_) {
        platform = PlatformerMovingPlatformState{};
    }
    for (auto& fireBar : fireBars_) {
        fireBar = PlatformerFireBarState{};
    }
    for (auto& cannon : cannons_) {
        cannon = CannonState{};
    }
    vine_ = PlatformerVineState{};
    vineReturnActive_ = false;
    vineSequenceState_ = 0U;
    vineReturnFrames_ = 0U;
    activeVineIndex_ = 0;
    bridgeStartColumn_ = -1;
    bridgeEndColumn_ = -1;
    bridgeRow_ = 0;
    bridgeRemovedCount_ = 0;
    bridgeSequenceState_ = 0U;
    bridgeStepFrames_ = 0U;
    bridgeDelayFrames_ = 0U;
    castleClearFrames_ = 0U;
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
        enemy.originX = spawn.x;
        enemy.originY = spawn.y;
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
        enemy.bornFrame = logicFrame_;
    }
    updateCamera();
}

void PlatformerEngine::resetCampaignLevel(bool resetPower,
                                          bool queueRestartEvent) {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (level == nullptr) {
        phase_ = PlatformerPhase::Title;
        return;
    }

    levelRuntime_.resetChanges();
    animationFrame_ = 0U;
    playerAnimationMode_ = 0U;
    playerAnimationFrame_ = 0U;
    playerAnimationTimer_ = 0U;
    boxCount_ = 0;
    enemyCount_ = 0;
    campaignEnemiesLoaded_ = false;
    powerupCount_ = 0;
    effectCount_ = 0;
    projectileCount_ = 0;
    enemyHazardCount_ = 0;
    for (auto& enemy : enemies_) {
        enemy = EnemyActor{};
    }
    for (auto& powerup : powerups_) {
        powerup = PlatformerPowerup{};
    }
    for (auto& effect : effects_) {
        effect = PlatformerEffect{};
    }
    for (auto& projectile : projectiles_) {
        projectile = PlatformerProjectile{};
    }
    for (auto& hazard : enemyHazards_) {
        hazard = PlatformerEnemyHazardState{};
    }
    resetCampaignDynamics();

    if (resetPower) {
        playerPower_ = PlatformerPlayerPower::Small;
    }
    playerCrouching_ = false;
    playerSkidding_ = false;
    playerFacingLeft_ = false;
    playerRunning_ = false;
    trampolineCollided_ = false;
    swimStrokeFrames_ = 0U;
    player_ = Actor{};
    player_.x = static_cast<float>(level->playerStart.x * TILE_SIZE);
    player_.y = static_cast<float>(level->playerStart.y * TILE_SIZE);
    if (playerPower_ != PlatformerPlayerPower::Small) {
        player_.y -= BIG_PLAYER_HEIGHT - PLAYER_HEIGHT;
    }
    player_.active = true;
    player_.grounded = false;

    cameraX_ = static_cast<float>(level->cameraStart.x * TILE_SIZE);
    cameraY_ = static_cast<float>(level->cameraStart.y * TILE_SIZE);
#if !defined(PGOS_PLATFORMER_TESTING)
    loadCampaignEnemies();
    campaignEnemiesLoaded_ = true;
#endif
    const int16_t flagColumn = levelRuntime_.goalColumn();
    const int16_t axeColumn = levelRuntime_.axeColumn();
    const int16_t finishColumn = flagColumn >= 0 ? flagColumn : axeColumn;
    goalX_ = finishColumn >= 0
                 ? static_cast<float>(finishColumn * TILE_SIZE)
                 : static_cast<float>(level->cameraMaximum * TILE_SIZE);
    castleX_ = std::min(
        static_cast<float>(levelRuntime_.widthPixels() - TILE_SIZE),
        goalX_ + 100.0F);
    flagY_ = 32.0F;
    flagX_ = goalX_ + TILE_SIZE * 0.5F;
    flagTileId_ = PLATFORMER_EMPTY_TILE;
    if (flagColumn >= 0) {
        for (uint8_t row = 0; row < level->height &&
                              flagTileId_ == PLATFORMER_EMPTY_TILE;
             ++row) {
            for (uint16_t column = 0; column < level->width; ++column) {
                for (PlatformerMapLayer layer : {
                         PlatformerMapLayer::Foreground,
                         PlatformerMapLayer::Underground}) {
                    const uint16_t source = platformerCampaignTileAt(
                        *level, layer, column, row);
                    if (source < PLATFORMER_BLOCK_TILE_COUNT &&
                        PLATFORMER_BLOCK_REFERENCE_IDS[source] == 152U) {
                        flagX_ = column * TILE_SIZE + TILE_SIZE * 0.5F;
                        flagY_ = static_cast<float>(row * TILE_SIZE);
                        flagTileId_ = source;
                        break;
                    }
                }
                if (flagTileId_ != PLATFORMER_EMPTY_TILE) {
                    break;
                }
            }
        }
    }

    phase_ = PlatformerPhase::Running;
    deathReason_ = PlatformerDeathReason::None;
    phaseElapsedMs_ = 0;
    phaseFrames_ = 0U;
    levelClockFrames_ = 0;
    timeRemaining_ = 400;
    powerTransitionFrames_ = 0;
    powerTransitionElapsedFrames_ = 0;
    hurtInvincibleFrames_ = 0;
    starInvincibleFrames_ = 0;
    starBlinkFrames_ = 0;
    fireballPoseFrames_ = 0U;
    powerTransition_ = PlatformerPowerTransition::None;
    starProtectedThisFrame_ = false;
    hurtProtectedThisFrame_ = false;
    stompChain_ = 0;
    playerAccelerationX_ = 0.0F;
    playerAccelerationY_ = 0.0F;
    cameraAdvanceX_ = 0.0F;
    jumpActive_ = false;
    mapTestMode_ = false;
    startIntro_ = level->levelType == PlatformerLevelType::StartUnderground;
    cameraFrozen_ = startIntro_;
    warpState_ = 0U;
    activeWarpIndex_ = 0;
    if (queueRestartEvent) {
        queueEvent(PlatformerEventType::LifeRestarted, lives_);
    }
}

bool PlatformerEngine::beginLevelTransition(uint8_t world, uint8_t stage,
                                            bool resetPower) {
    if (!campaignMode_ || world == 0U || stage == 0U ||
        !levelRuntime_.load(world, stage)) {
        return false;
    }
    resetCampaignLevel(resetPower, false);
    phase_ = PlatformerPhase::LevelTransition;
    phaseElapsedMs_ = 0U;
    phaseFrames_ = 0U;
    player_.active = false;
    cameraFrozen_ = true;
    return true;
}

void PlatformerEngine::completeCourse() {
    const PlatformerCampaignLevel* cleared = levelRuntime_.level();
    const uint16_t course =
        cleared == nullptr
            ? 0U
            : static_cast<uint16_t>((static_cast<uint16_t>(cleared->world) << 8U) |
                                    cleared->stage);
    const uint8_t nextWorld = cleared == nullptr ? 0U : cleared->nextWorld;
    const uint8_t nextStage = cleared == nullptr ? 0U : cleared->nextStage;
    queueEvent(PlatformerEventType::CourseClear, course);

    if (beginLevelTransition(nextWorld, nextStage)) {
        return;
    }

    phase_ = PlatformerPhase::Won;
    phaseElapsedMs_ = 0U;
    phaseFrames_ = 0U;
}

void PlatformerEngine::resetCampaignDynamics() {
    movingPlatformCount_ = 0;
    fireBarCount_ = 0;
    cannonCount_ = 0;
    cannonTimerFrames_ = CANNON_TIMER_FRAMES;
    for (auto& platform : movingPlatforms_) {
        platform = PlatformerMovingPlatformState{};
    }
    for (auto& fireBar : fireBars_) {
        fireBar = PlatformerFireBarState{};
    }
    for (auto& cannon : cannons_) {
        cannon = CannonState{};
    }
    vine_ = PlatformerVineState{};
    vineReturnActive_ = false;
    vineSequenceState_ = 0U;
    vineReturnFrames_ = 0U;
    activeVineIndex_ = 0;
    bridgeStartColumn_ = -1;
    bridgeEndColumn_ = -1;
    bridgeRow_ = 0;
    bridgeRemovedCount_ = 0;
    bridgeSequenceState_ = 0U;
    bridgeStepFrames_ = 0U;
    bridgeDelayFrames_ = 0U;
    castleClearFrames_ = 0U;
    pendingBrickBreak_ = PendingBrickBreak{};

    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (level == nullptr) {
        return;
    }
    const auto sourceAt = [level](PlatformerPoint position) {
        uint16_t source = platformerCampaignTileAt(
            *level, PlatformerMapLayer::Foreground,
            static_cast<uint16_t>(position.x), static_cast<uint8_t>(position.y));
        if (source == PLATFORMER_EMPTY_TILE) {
            source = platformerCampaignTileAt(
                *level, PlatformerMapLayer::Underground,
                static_cast<uint16_t>(position.x),
                static_cast<uint8_t>(position.y));
        }
        return source;
    };

    for (PlatformerMapLayer layer : {PlatformerMapLayer::Underground,
                                     PlatformerMapLayer::Foreground}) {
        for (uint8_t row = 0; row < level->height; ++row) {
            for (uint16_t column = 0; column < level->width; ++column) {
                const uint16_t source = platformerCampaignTileAt(
                    *level, layer, column, row);
                if (source >= PLATFORMER_BLOCK_TILE_COUNT ||
                    PLATFORMER_BLOCK_REFERENCE_IDS[source] != 63U) {
                    continue;
                }
                if (cannonCount_ < MAX_CANNONS) {
                    cannons_[cannonCount_++] =
                        CannonState{column, row, source};
                }
            }
        }
    }

    for (uint8_t index = 0;
         index < level->movingPlatforms.count &&
         movingPlatformCount_ < MAX_MOVING_PLATFORMS;
         ++index) {
        const PlatformerMovingPlatformData& source =
            PLATFORMER_CAMPAIGN_MOVING_PLATFORMS[
                level->movingPlatforms.offset + index];
        PlatformerMovingPlatformState& platform =
            movingPlatforms_[movingPlatformCount_++];
        platform.x = static_cast<float>(source.position.x * TILE_SIZE) +
                     (source.halfTileShift ? TILE_SIZE * 0.5F : 0.0F);
        platform.y = static_cast<float>(source.position.y * TILE_SIZE);
        platform.minimum = static_cast<float>(source.minimum * TILE_SIZE);
        platform.maximum =
            static_cast<float>((source.maximum + 1) * TILE_SIZE);
        platform.sourceTileId = sourceAt(source.position);
        platform.motion = source.motion;
        platform.direction = source.direction;
        platform.widthTiles = 3;
        if (platform.sourceTileId < PLATFORMER_BLOCK_TILE_COUNT &&
            PLATFORMER_BLOCK_REFERENCE_IDS[platform.sourceTileId] == 761U) {
            platform.widthTiles = 2;
        }
        if (platform.motion == PlatformerMotionType::OneDirectionRepeated ||
            platform.motion == PlatformerMotionType::OneDirectionContinuous) {
            switch (platform.direction) {
                case PlatformerDirection::Left:
                    platform.vx = -2.0F * REFERENCE_VELOCITY_SCALE;
                    break;
                case PlatformerDirection::Right:
                    platform.vx = 2.0F * REFERENCE_VELOCITY_SCALE;
                    break;
                case PlatformerDirection::Up:
                    platform.vy = -2.0F * REFERENCE_VELOCITY_SCALE;
                    break;
                case PlatformerDirection::Down:
                    platform.vy = 2.0F * REFERENCE_VELOCITY_SCALE;
                    break;
                default:
                    break;
            }
        }
        platform.active = true;
    }

    for (uint8_t index = 0;
         index < level->pulleys.count &&
         movingPlatformCount_ + 1U < MAX_MOVING_PLATFORMS;
         ++index) {
        const PlatformerPulleyData& source =
            PLATFORMER_CAMPAIGN_PULLEYS[level->pulleys.offset + index];
        const uint8_t leftIndex = movingPlatformCount_++;
        const uint8_t rightIndex = movingPlatformCount_++;
        PlatformerMovingPlatformState& left = movingPlatforms_[leftIndex];
        PlatformerMovingPlatformState& right = movingPlatforms_[rightIndex];
        left.x = static_cast<float>(source.left.x * TILE_SIZE);
        left.y = static_cast<float>(source.left.y * TILE_SIZE);
        right.x = static_cast<float>(source.right.x * TILE_SIZE);
        right.y = static_cast<float>(source.right.y * TILE_SIZE);
        left.sourceTileId = sourceAt(source.left);
        right.sourceTileId = sourceAt(source.right);
        left.widthTiles = right.widthTiles = 3;
        left.pulley = right.pulley = true;
        left.pairIndex = static_cast<int8_t>(rightIndex);
        right.pairIndex = static_cast<int8_t>(leftIndex);
        left.pulleyTop = right.pulleyTop =
            static_cast<float>((source.pulleyY + 1) * TILE_SIZE);
        left.active = right.active = true;
    }

    for (uint8_t row = 0; row < level->height; ++row) {
        for (uint16_t column = 0;
             column < level->width &&
             movingPlatformCount_ < MAX_MOVING_PLATFORMS;
             ++column) {
            const uint16_t source = platformerCampaignTileAt(
                *level, PlatformerMapLayer::Foreground, column, row);
            if (source >= PLATFORMER_BLOCK_TILE_COUNT ||
                PLATFORMER_BLOCK_REFERENCE_IDS[source] != 857U) {
                continue;
            }
            PlatformerMovingPlatformState& cloud =
                movingPlatforms_[movingPlatformCount_++];
            cloud.x = static_cast<float>(column * TILE_SIZE);
            cloud.y = static_cast<float>(row * TILE_SIZE);
            cloud.sourceTileId = source;
            cloud.widthTiles = 3U;
            cloud.cloudPlatform = true;
            cloud.active = true;
        }
    }

    for (uint8_t index = 0;
         index < level->fireBars.count && fireBarCount_ < MAX_FIRE_BARS;
         ++index) {
        const PlatformerFireBarData& source =
            PLATFORMER_CAMPAIGN_FIRE_BARS[level->fireBars.offset + index];
        PlatformerFireBarState& fireBar = fireBars_[fireBarCount_++];
        fireBar.x = static_cast<float>(source.position.x * TILE_SIZE);
        fireBar.y = static_cast<float>(source.position.y * TILE_SIZE);
        fireBar.angleDegrees = static_cast<float>(source.startingAngle);
        fireBar.length = source.length;
        fireBar.direction = source.direction;
        fireBar.active = true;
    }

    for (uint8_t row = 0; row < level->height; ++row) {
        for (uint16_t column = 0; column < level->width; ++column) {
            const uint16_t tileId = platformerCampaignTileAt(
                *level, PlatformerMapLayer::Foreground, column, row);
            if (tileId >= PLATFORMER_BLOCK_TILE_COUNT ||
                PLATFORMER_BLOCK_REFERENCE_IDS[tileId] != 392U) {
                continue;
            }
            if (bridgeStartColumn_ < 0) {
                bridgeStartColumn_ = static_cast<int16_t>(column);
                bridgeRow_ = row;
            }
            if (row == bridgeRow_) {
                bridgeEndColumn_ = static_cast<int16_t>(column);
            }
        }
    }
}

int8_t PlatformerEngine::standingPlatform() const {
    if (!campaignMode_) {
        return -1;
    }
    const float bottom = player_.y + playerHeight();
    for (uint8_t index = 0; index < movingPlatformCount_; ++index) {
        const PlatformerMovingPlatformState& platform = movingPlatforms_[index];
        const float width = static_cast<float>(platform.widthTiles * TILE_SIZE);
        if (platform.active && std::fabs(bottom - platform.y) <= 2.0F &&
            player_.x + playerWidth() > platform.x + 1.0F &&
            player_.x < platform.x + width - 1.0F) {
            return static_cast<int8_t>(index);
        }
    }
    return -1;
}

int8_t PlatformerEngine::landingPlatform(float previousBottom,
                                          float nextBottom) const {
    if (!campaignMode_ || nextBottom < previousBottom) {
        return -1;
    }
    int8_t result = -1;
    float nearestTop = nextBottom + 1.0F;
    for (uint8_t index = 0; index < movingPlatformCount_; ++index) {
        const PlatformerMovingPlatformState& platform = movingPlatforms_[index];
        const float width = static_cast<float>(platform.widthTiles * TILE_SIZE);
        if (!platform.active || previousBottom > platform.y + 2.0F ||
            nextBottom < platform.y ||
            player_.x + playerWidth() <= platform.x + 1.0F ||
            player_.x >= platform.x + width - 1.0F ||
            platform.y >= nearestTop) {
            continue;
        }
        nearestTop = platform.y;
        result = static_cast<int8_t>(index);
    }
    return result;
}

void PlatformerEngine::updateMovingPlatforms(float dt, bool carryPlayer) {
    const int8_t stoodOn = standingPlatform();
    constexpr float PLATFORM_GRAVITY =
        0.10F * REFERENCE_VELOCITY_SCALE * REFERENCE_TICKS_PER_SECOND;
    constexpr float PULLEY_GRAVITY =
        0.12F * REFERENCE_VELOCITY_SCALE * REFERENCE_TICKS_PER_SECOND;
    constexpr float REFERENCE_CAMERA_HEIGHT = 15.0F * TILE_SIZE;
    const auto inCamera = [this](const PlatformerMovingPlatformState& platform) {
        const float width = static_cast<float>(platform.widthTiles * TILE_SIZE);
        return platform.x + width >= cameraX_ &&
               platform.x <= cameraX_ + VIEWPORT_WIDTH &&
               platform.y + TILE_SIZE >= cameraY_ &&
               platform.y <= cameraY_ + REFERENCE_CAMERA_HEIGHT;
    };
    const auto integrate = [dt](PlatformerMovingPlatformState& platform) {
        platform.x += platform.vx * dt;
        platform.y += platform.vy * dt;
        platform.vy = std::min(
            MAX_FALL_SPEED, platform.vy + platform.accelerationY * dt);
    };

    for (uint8_t index = 0; index < movingPlatformCount_; ++index) {
        PlatformerMovingPlatformState& platform = movingPlatforms_[index];
        if (!platform.active) {
            continue;
        }
        if (platform.detachedFalling) {
            if (!inCamera(platform)) {
                platform.active = false;
                continue;
            }
            platform.vy = std::min(
                MAX_FALL_SPEED,
                platform.vy + REFERENCE_GRAVITY *
                                      REFERENCE_VELOCITY_SCALE);
            platform.x += platform.vx * dt;
            platform.y += platform.vy * dt;
            continue;
        }
        if (platform.pulley || !inCamera(platform)) {
            continue;
        }
        if (platform.cloudPlatform) {
            const float nextX = platform.x + platform.vx * dt;
            const float width =
                static_cast<float>(platform.widthTiles * TILE_SIZE);
            if (platform.vx != 0.0F &&
                rectHitsSolid(nextX, platform.y, width, TILE_SIZE)) {
                platform.vx = 0.0F;
            } else {
                platform.x = nextX;
            }
            continue;
        }
        integrate(platform);

        if (platform.motion == PlatformerMotionType::BackAndForth) {
            const float width = static_cast<float>(platform.widthTiles * TILE_SIZE);
            if (platform.direction == PlatformerDirection::Left &&
                platform.x <= platform.minimum) {
                platform.direction = PlatformerDirection::Right;
            } else if (platform.direction == PlatformerDirection::Right &&
                       platform.x + width >= platform.maximum) {
                platform.direction = PlatformerDirection::Left;
            } else if (platform.direction == PlatformerDirection::Up &&
                       platform.y <= platform.minimum) {
                platform.direction = PlatformerDirection::Down;
            } else if (platform.direction == PlatformerDirection::Down &&
                       platform.y + TILE_SIZE >= platform.maximum) {
                platform.direction = PlatformerDirection::Up;
            } else {
                const float travel =
                    (platform.maximum - platform.minimum) / 3.8F;
                if (travel > EPSILON) {
                    float position = 0.0F;
                    switch (platform.direction) {
                        case PlatformerDirection::Left:
                            position = platform.x + width - platform.minimum;
                            platform.vx =
                                -2.0F * std::exp(
                                            -std::pow(position - 1.9F * travel,
                                                      2.0F) /
                                            (2.0F * travel * travel)) *
                                REFERENCE_VELOCITY_SCALE;
                            break;
                        case PlatformerDirection::Right:
                            position = platform.maximum - platform.x;
                            platform.vx =
                                2.0F * std::exp(
                                           -std::pow(position - 1.9F * travel,
                                                     2.0F) /
                                           (2.0F * travel * travel)) *
                                REFERENCE_VELOCITY_SCALE;
                            break;
                        case PlatformerDirection::Up:
                            position = platform.y + TILE_SIZE - platform.minimum;
                            platform.vy =
                                -2.0F * std::exp(
                                            -std::pow(position - 1.9F * travel,
                                                      2.0F) /
                                            (2.0F * travel * travel)) *
                                REFERENCE_VELOCITY_SCALE;
                            break;
                        case PlatformerDirection::Down:
                            position = platform.maximum - platform.y;
                            platform.vy =
                                2.0F * std::exp(
                                           -std::pow(position - 1.9F * travel,
                                                     2.0F) /
                                           (2.0F * travel * travel)) *
                                REFERENCE_VELOCITY_SCALE;
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        if (platform.motion == PlatformerMotionType::Gravity) {
            if (stoodOn == static_cast<int8_t>(index)) {
                platform.accelerationY = PLATFORM_GRAVITY;
            } else {
                platform.accelerationY = 0.0F;
                platform.vy *= REFERENCE_FRICTION;
            }
        }
        if (platform.motion == PlatformerMotionType::OneDirectionRepeated) {
            if ((platform.direction == PlatformerDirection::Left ||
                 platform.direction == PlatformerDirection::Right) &&
                platform.x < platform.minimum) {
                platform.x = platform.maximum;
            } else if ((platform.direction == PlatformerDirection::Left ||
                        platform.direction == PlatformerDirection::Right) &&
                       platform.x > platform.maximum) {
                platform.x = platform.minimum;
            } else if ((platform.direction == PlatformerDirection::Up ||
                        platform.direction == PlatformerDirection::Down) &&
                       platform.y > platform.maximum) {
                platform.y = platform.minimum;
            } else if ((platform.direction == PlatformerDirection::Up ||
                        platform.direction == PlatformerDirection::Down) &&
                       platform.y < platform.minimum) {
                platform.y = platform.maximum;
            }
        }
    }

    for (uint8_t index = 0; index < movingPlatformCount_; ++index) {
        PlatformerMovingPlatformState& platform = movingPlatforms_[index];
        if (!platform.active || !platform.pulley || platform.pairIndex < 0 ||
            index > static_cast<uint8_t>(platform.pairIndex)) {
            continue;
        }
        PlatformerMovingPlatformState& other =
            movingPlatforms_[static_cast<uint8_t>(platform.pairIndex)];
        const bool platformVisible = inCamera(platform);
        const bool otherVisible = inCamera(other);
        if (!platformVisible && !otherVisible) {
            continue;
        }
        if (platformVisible) {
            integrate(platform);
        }
        if (otherVisible) {
            integrate(other);
        }

        const auto detachPair = [](PlatformerMovingPlatformState& stopped,
                                   PlatformerMovingPlatformState& falling) {
            stopped.y = stopped.pulleyTop;
            stopped.vx = stopped.vy = stopped.accelerationY = 0.0F;
            stopped.pulley = false;
            stopped.pairIndex = -1;
            stopped.motion = PlatformerMotionType::None;
            falling.accelerationY = 0.0F;
            falling.pulley = false;
            falling.pairIndex = -1;
            falling.detachedFalling = true;
        };
        if (platformVisible && platform.y < platform.pulleyTop) {
            detachPair(platform, other);
            continue;
        }

        if (platformVisible) {
            if (stoodOn == static_cast<int8_t>(index)) {
                platform.accelerationY = PULLEY_GRAVITY;
                other.vy = -platform.vy;
            } else {
                if (other.accelerationY == 0.0F) {
                    platform.vy *= REFERENCE_FRICTION;
                    other.vy = -platform.vy;
                }
                platform.accelerationY = 0.0F;
            }
        }
        if (otherVisible && other.y < other.pulleyTop) {
            detachPair(other, platform);
            continue;
        }
        if (otherVisible) {
            if (stoodOn == platform.pairIndex) {
                other.accelerationY = PULLEY_GRAVITY;
                platform.vy = -other.vy;
            } else {
                if (platform.accelerationY == 0.0F) {
                    other.vy *= REFERENCE_FRICTION;
                    platform.vy = -other.vy;
                }
                other.accelerationY = 0.0F;
            }
        }
    }

    if (carryPlayer && stoodOn >= 0 && movingPlatforms_[stoodOn].active &&
        !movingPlatforms_[stoodOn].pulley &&
        !movingPlatforms_[stoodOn].cloudPlatform) {
        player_.x += movingPlatforms_[stoodOn].vx * dt;
        player_.y += movingPlatforms_[stoodOn].vy * dt;
        player_.grounded = true;
    }
}

void PlatformerEngine::updateCloudPlatformCallbacks() {
    const int8_t stoodOn = standingPlatform();
    if (stoodOn < 0) {
        return;
    }
    PlatformerMovingPlatformState& platform = movingPlatforms_[stoodOn];
    if (!platform.cloudPlatform || platform.triggered) {
        return;
    }
    platform.triggered = true;
    platform.vx = 2.0F * REFERENCE_VELOCITY_SCALE;
}

void PlatformerEngine::updateTrampolines() {
    bool anyCollision = false;
    for (uint8_t index = 0; index < levelRuntime_.trampolineCount(); ++index) {
        const PlatformerTrampolineRuntimeState* state =
            levelRuntime_.trampoline(index);
        if (state == nullptr) {
            continue;
        }
        const float x = static_cast<float>(state->column * TILE_SIZE);
        const float y = static_cast<float>(state->row * TILE_SIZE);
        const bool inCamera =
            x + TILE_SIZE >= cameraX_ && x <= cameraX_ + VIEWPORT_WIDTH &&
            y + TILE_SIZE >= cameraY_ &&
            y <= cameraY_ + VIEWPORT_WIDTH * 0.75F;
        const float hitboxY =
            y + (state->visualState == 0U
                     ? 0.0F
                     : state->visualState == 1U ? TILE_SIZE * 0.5F
                                                : TILE_SIZE);
        const float hitboxHeight =
            state->visualState == 0U
                ? TILE_SIZE
                : state->visualState == 1U ? TILE_SIZE * 0.5F : 0.0F;
        const bool collided =
            inCamera && player_.x <= x + TILE_SIZE &&
            player_.x + playerWidth() >= x &&
            player_.y <= hitboxY + hitboxHeight &&
            player_.y + playerHeight() >= hitboxY;
        if (!collided) {
            if (state->sequenceIndex != 0U) {
                levelRuntime_.setTrampolineState(
                    index, 0U, state->visualState, state->activated);
            }
            continue;
        }

        anyCollision = true;
        if (state->sequenceIndex > 20U) {
            continue;
        }

        uint8_t visualState = state->visualState;
        switch (state->sequenceIndex) {
            case 0U:
                visualState = 1U;
                break;
            case 1U:
                visualState = 2U;
                break;
            case 2U:
                visualState = 1U;
                player_.vy = -11.0F * REFERENCE_VELOCITY_SCALE;
                queueEvent(PlatformerEventType::TrampolineBounced);
                break;
            case 3U:
                visualState = 0U;
                break;
            default:
                break;
        }
        levelRuntime_.setTrampolineState(
            index, static_cast<uint8_t>(state->sequenceIndex + 1U),
            visualState, true);
        player_.grounded = false;
        const float trampolineBottom = y + TILE_SIZE;
        if (player_.y + playerHeight() * 0.5F > trampolineBottom) {
            player_.y = trampolineBottom - playerHeight() * 0.5F;
        }
    }
    trampolineCollided_ = anyCollision;
}

void PlatformerEngine::updateFireBars(float dt) {
    (void)dt;
    constexpr float PI = 3.14159265358979323846F;
    for (uint8_t index = 0; index < fireBarCount_; ++index) {
        PlatformerFireBarState& fireBar = fireBars_[index];
        if (!fireBar.active) {
            continue;
        }
        const float radians = fireBar.angleDegrees * PI / 180.0F;
        for (uint8_t element = 0U; element < fireBar.length; ++element) {
            const float distance = static_cast<float>(element) *
                                   TILE_SIZE * 0.5F;
            const float x = fireBar.x + std::cos(radians) * distance;
            const float y = fireBar.y - std::sin(radians) * distance;
            const bool inCamera =
                x + TILE_SIZE >= cameraX_ &&
                x <= cameraX_ + VIEWPORT_WIDTH &&
                y + TILE_SIZE >= cameraY_ &&
                y <= cameraY_ + VIEWPORT_HEIGHT;
            if (inCamera) {
                platformerAdvanceReferenceAnimation(
                    fireBar.animationFrames[element],
                    fireBar.animationTimers[element], 5U, 4U);
            }
        }
        ++fireBar.timerFrames;
        if (fireBar.timerFrames < 6U) {
            continue;
        }
        fireBar.timerFrames = 0U;
        const float direction =
            fireBar.direction == PlatformerRotationDirection::Clockwise
                ? -1.0F
                : 1.0F;
        fireBar.angleDegrees += direction * 10.0F;
        if (fireBar.angleDegrees >= 360.0F) {
            fireBar.angleDegrees -= 360.0F;
        } else if (fireBar.angleDegrees < 0.0F) {
            fireBar.angleDegrees += 360.0F;
        }
    }
}

void PlatformerEngine::updateCannonTimers() {
    if (cannonCount_ == 0U) {
        return;
    }
    if (cannonTimerFrames_ > 0U) {
        --cannonTimerFrames_;
    }
    if (cannonTimerFrames_ != 0U) {
        return;
    }
    cannonTimerFrames_ = CANNON_TIMER_FRAMES;

    for (uint8_t index = 0; index < cannonCount_; ++index) {
        const CannonState& cannon = cannons_[index];
        const float x = static_cast<float>(cannon.column * TILE_SIZE);
        const float y = static_cast<float>(cannon.row * TILE_SIZE);
        const bool inCamera =
            x + TILE_SIZE >= cameraX_ && x <= cameraX_ + VIEWPORT_WIDTH &&
            y + TILE_SIZE >= cameraY_ && y <= cameraY_ + VIEWPORT_HEIGHT;
        if (!inCamera) {
            continue;
        }
        const bool movingRight = (nextRandom() & 1U) != 0U;
        if (spawnCannonBullet(cannon, movingRight)) {
            queueEvent(PlatformerEventType::CannonFired);
        }
    }
}

bool PlatformerEngine::spawnCannonBullet(const CannonState& cannon,
                                          bool movingRight) {
    uint8_t slot = MAX_ENEMIES;
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        if (!enemies_[index].actor.active) {
            slot = index;
            break;
        }
    }
    if (slot == MAX_ENEMIES && enemyCount_ < MAX_ENEMIES) {
        slot = enemyCount_++;
    }
    if (slot >= MAX_ENEMIES) {
        return false;
    }

    EnemyActor& bullet = enemies_[slot];
    bullet = EnemyActor{};
    bullet.type = PlatformerEnemyType::BulletBill;
    bullet.sourceTileId = cannonBulletSource(cannon.sourceTileId);
    bullet.bornFrame = logicFrame_;
    bullet.actor.x = static_cast<float>(
        (static_cast<int32_t>(cannon.column) + (movingRight ? 1 : -1)) *
        TILE_SIZE);
    bullet.actor.y = static_cast<float>(cannon.row * TILE_SIZE);
    bullet.actor.vx =
        (movingRight ? 3.0F : -3.0F) * REFERENCE_VELOCITY_SCALE;
    bullet.actor.active = true;
    bullet.spawned = true;
    bullet.originX = bullet.actor.x;
    bullet.originY = bullet.actor.y;
    return true;
}

void PlatformerEngine::checkFireBarCollisions() {
    constexpr float PI = 3.14159265358979323846F;
    for (uint8_t index = 0; index < fireBarCount_; ++index) {
        const PlatformerFireBarState& fireBar = fireBars_[index];
        if (!fireBar.active || fireBar.length < 2U) {
            continue;
        }
        const float radians = fireBar.angleDegrees * PI / 180.0F;
        for (uint8_t element = 0; element + 1U < fireBar.length; ++element) {
            const float distance =
                static_cast<float>(element) * TILE_SIZE * 0.5F;
            const float x = fireBar.x + std::cos(radians) * distance;
            const float y = fireBar.y - std::sin(radians) * distance;
            if (!overlaps(player_.x, player_.y, playerWidth(), playerHeight(),
                          x, y, 4.0F, 4.0F)) {
                continue;
            }
            hurtPlayer();
            return;
        }
    }
}

void PlatformerEngine::spawnVine(uint16_t column, uint8_t row) {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (!campaignMode_ || level == nullptr || vine_.active) {
        return;
    }
    for (uint8_t index = 0; index < level->vines.count; ++index) {
        const PlatformerVineData& source =
            PLATFORMER_CAMPAIGN_VINES[level->vines.offset + index];
        if (source.block.x != static_cast<int16_t>(column) ||
            source.block.y != static_cast<int16_t>(row)) {
            continue;
        }
        activeVineIndex_ = index;
        vine_.x = static_cast<float>(column * TILE_SIZE);
        vine_.baseY = static_cast<float>(row * TILE_SIZE);
        vine_.grownPixels = 0.0F;
        vine_.active = true;
        queueEvent(PlatformerEventType::PowerupAppeared,
                   static_cast<uint16_t>(PlatformerRuntimeReward::Vine));
        return;
    }
}

void PlatformerEngine::updateVine(float dt, const PlatformerInput& input) {
    if (!campaignMode_ || !vine_.active) {
        return;
    }
    if (vineSequenceState_ != 0U) {
        return;
    }
    vine_.grownPixels =
        std::min(96.0F, vine_.grownPixels + 30.0F * dt);
    if (vine_.grownPixels <= 0.0F) {
        return;
    }
    const float vineTop = vine_.baseY - vine_.grownPixels;
    const float visibleHeight =
        std::ceil(vine_.grownPixels / TILE_SIZE) * TILE_SIZE;
    if (!overlaps(player_.x, player_.y, playerWidth(), playerHeight(),
                  vine_.x, vineTop, TILE_SIZE, visibleHeight)) {
        return;
    }
    vinePreviousLevelType_ = levelRuntime_.activeLevelType();
    vinePreviousBackground_ = levelRuntime_.activeBackground();
    phase_ = PlatformerPhase::VineClimb;
    phaseElapsedMs_ = 0U;
    phaseFrames_ = 0U;
    vineSequenceState_ = 1U;
    player_.x = vine_.x + TILE_SIZE * 0.5F;
    player_.vx = 0.0F;
    player_.vy = 0.0F;
    player_.grounded = false;
    playerAccelerationX_ = 0.0F;
    playerAccelerationY_ = 0.0F;
    if (input.jumpHeld) {
        player_.vy = -45.0F;
        vineSequenceState_ = 2U;
    }
}

void PlatformerEngine::updateVineClimb(float dt,
                                        const PlatformerInput& input) {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (level == nullptr || activeVineIndex_ >= level->vines.count) {
        phase_ = PlatformerPhase::Running;
        vineSequenceState_ = 0U;
        return;
    }
    const PlatformerVineData& source =
        PLATFORMER_CAMPAIGN_VINES[level->vines.offset + activeVineIndex_];

    if (vineSequenceState_ == 1U) {
        player_.x = vine_.x + TILE_SIZE * 0.5F;
        if (input.jumpHeld) {
            player_.vy = -45.0F;
            vineSequenceState_ = 2U;
        }
        return;
    }

    if (vineSequenceState_ == 2U) {
        player_.y += player_.vy * dt;
        if (player_.y + playerHeight() < cameraY_) {
            vineSequenceState_ = 3U;
        }
        return;
    }

    // SequenceCommand advances by one child per scheduler frame. This is the
    // RunCommand after WaitUntil(player outside the old camera).
    if (vineSequenceState_ == 3U) {
        player_.y += player_.vy * dt;
        cameraX_ = static_cast<float>(source.camera.x * TILE_SIZE);
        cameraY_ = static_cast<float>(source.camera.y * TILE_SIZE);
        levelRuntime_.setSection(source.levelType, source.background);
        player_.x = vine_.x + TILE_SIZE * 0.5F;
        player_.y = vine_.baseY - TILE_SIZE;
        vineReturnActive_ = true;
        vineSequenceState_ = 4U;
        return;
    }

    if (vineSequenceState_ == 4U) {
        vine_.baseY -= 30.0F * dt;
        player_.y += player_.vy * dt;
        const float destinationTop =
            static_cast<float>((source.destination.y - 4) * TILE_SIZE);
        if (vine_.baseY - vine_.grownPixels <= destinationTop) {
            vineSequenceState_ = 5U;
        }
        return;
    }

    // The stop RunCommand follows on the next frame, after one final physics
    // movement for both the vine and Mario.
    if (vineSequenceState_ == 5U) {
        vine_.baseY -= 30.0F * dt;
        player_.y += player_.vy * dt;
        vineSequenceState_ = 6U;
        return;
    }

    const float secondPieceBottom =
        vine_.baseY - vine_.grownPixels + TILE_SIZE * 2.0F;
    if (vineSequenceState_ == 6U) {
        player_.y += player_.vy * dt;
        if (player_.y + playerHeight() <= secondPieceBottom) {
            vineSequenceState_ = 7U;
        }
        return;
    }

    // The reference restores gravity and input without clearing the upward
    // velocity established by VineCommand.
    player_.y += player_.vy * dt;
    player_.x = vine_.x + TILE_SIZE;
    playerAccelerationY_ = 0.0F;
    player_.grounded = false;
    phase_ = PlatformerPhase::Running;
    phaseElapsedMs_ = 0U;
    phaseFrames_ = 0U;
}

void PlatformerEngine::checkVineReturn() {
    if (!vineReturnActive_) {
        return;
    }
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (level == nullptr || activeVineIndex_ >= level->vines.count) {
        vineReturnActive_ = false;
        return;
    }
    const PlatformerVineData& source =
        PLATFORMER_CAMPAIGN_VINES[level->vines.offset + activeVineIndex_];
    if (vineReturnFrames_ > 0U ||
        player_.y <= static_cast<float>(
                         (source.resetBelowY + 3) * TILE_SIZE)) {
        return;
    }
    vineReturnFrames_ = 119U;
    player_.vx = 0.0F;
    player_.vy = 0.0F;
    playerAccelerationX_ = 0.0F;
    playerAccelerationY_ = 0.0F;
}

void PlatformerEngine::completeVineReturn() {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (!vineReturnActive_ || level == nullptr ||
        activeVineIndex_ >= level->vines.count) {
        vineReturnFrames_ = 0U;
        return;
    }
    const PlatformerVineData& source =
        PLATFORMER_CAMPAIGN_VINES[level->vines.offset + activeVineIndex_];
    player_.x = static_cast<float>(source.resetDestination.x * TILE_SIZE);
    player_.y = static_cast<float>(source.resetDestination.y * TILE_SIZE);
    player_.vx = 0.0F;
    player_.vy = 0.0F;
    cameraX_ = static_cast<float>(
        std::max<int16_t>(0, source.resetDestination.x - 2) * TILE_SIZE);
    cameraY_ = static_cast<float>(
        std::max<int16_t>(0, source.resetDestination.y - 1) * TILE_SIZE);
    levelRuntime_.setSection(vinePreviousLevelType_, vinePreviousBackground_);
    vineReturnActive_ = false;
    vineReturnFrames_ = 0U;
    cameraFrozen_ = false;
}

void PlatformerEngine::resetLife() {
    if (campaignMode_) {
        const PlatformerCampaignLevel* level = levelRuntime_.level();
        if (level != nullptr &&
            beginLevelTransition(level->world, level->stage, true)) {
            return;
        }
        phase_ = PlatformerPhase::GameOver;
        return;
    }
    buildLevel();
    resetActors();
    animationFrame_ = 0U;
    playerAnimationMode_ = 0U;
    playerAnimationFrame_ = 0U;
    playerAnimationTimer_ = 0U;
    phase_ = PlatformerPhase::Running;
    deathReason_ = PlatformerDeathReason::None;
    phaseElapsedMs_ = 0;
    phaseFrames_ = 0U;
    levelClockFrames_ = 0;
    timeRemaining_ = 400;
    flagY_ = static_cast<float>(PLATFORMER_LEVEL_1_1.flagTopY);
    playerPower_ = PlatformerPlayerPower::Small;
    playerCrouching_ = false;
    playerSkidding_ = false;
    playerRunning_ = false;
    trampolineCollided_ = false;
    swimStrokeFrames_ = 0U;
    powerTransitionFrames_ = 0;
    powerTransitionElapsedFrames_ = 0;
    hurtInvincibleFrames_ = 0;
    starInvincibleFrames_ = 0;
    starBlinkFrames_ = 0;
    fireballPoseFrames_ = 0U;
    powerTransition_ = PlatformerPowerTransition::None;
    starProtectedThisFrame_ = false;
    hurtProtectedThisFrame_ = false;
    stompChain_ = 0;
    playerAccelerationX_ = 0.0F;
    playerAccelerationY_ = 0.0F;
    cameraAdvanceX_ = 0.0F;
    queueEvent(PlatformerEventType::LifeRestarted, lives_);
}

void PlatformerEngine::updateRunning(float dt, uint16_t dtMs,
                                     const PlatformerInput& input) {
    if (campaignMode_ && !campaignEnemiesLoaded_) {
        loadCampaignEnemies();
        campaignEnemiesLoaded_ = true;
    }
    playerRunning_ = input.actionHeld;
    starProtectedThisFrame_ = starInvincibleFrames_ > 0U;
    hurtProtectedThisFrame_ = hurtInvincibleFrames_ > 0U;
    if (timeRemaining_ == 0U) {
        beginDeath(PlatformerDeathReason::Time);
        return;
    }
    const bool playerFrozen = powerTransitionFrames_ > 0U;
    updatePlayerStateTimers();
    updateBoxes();
    updateEffects(dt, dtMs);

    if (phase_ != PlatformerPhase::Running) {
        return;
    }

    if (campaignMode_) {
        updateMovingPlatforms(dt);
        updateFireBars(dt);
    }

    if (playerFrozen) {
        updateEnemies(dt, dtMs);
        updateEnemyHazards(dt, dtMs);
        updatePowerups(dt, dtMs);
        updateProjectiles(dt, dtMs);
        checkEnemyPairCollisions();
        updateCamera();
        return;
    }

    if (vineReturnFrames_ > 0U) {
        --vineReturnFrames_;
        if (vineReturnFrames_ == 0U) {
            completeVineReturn();
        }
        return;
    }

    if (startIntro_) {
        player_.vx = 48.0F;
        playerFacingLeft_ = false;
        moveHorizontal(player_.vx * dt);
        PlatformerInput pipeInput;
        pipeInput.moveAxis = 1.0F;
        if (!tryEnterWarp(pipeInput)) {
            updateCamera();
        }
        return;
    }

    updateVine(dt, input);
    if (phase_ != PlatformerPhase::Running) {
        return;
    }

    const bool underwater =
        campaignMode_ && levelRuntime_.activeLevelType() ==
                             PlatformerLevelType::Underwater;
    updateCrouch(!underwater && input.crouchHeld);

    if (input.actionPressed && playerPower_ == PlatformerPlayerPower::Fire) {
        shootFireball();
    }

    const float previousBottom = player_.y + playerHeight();
    const float previousX = player_.x;
    updatePlayerHorizontal(dt, input);
    updatePlayerVertical(dt, dtMs, input);
    if (campaignMode_) {
        updateTrampolines();
        applyTeleportPoints(previousX);
        collectMapCoins();
        checkVineReturn();
        if (vineReturnFrames_ > 0U) {
            return;
        }
        if (tryEnterWarp(input)) {
            return;
        }
    }

    updateEnemies(dt, dtMs);
    updateEnemyHazards(dt, dtMs);
    updatePowerups(dt, dtMs);
    updateProjectiles(dt, dtMs);
    collectPowerups();
    checkEnemyCollisions(previousBottom);
    checkEnemyPairCollisions();
    if (campaignMode_ && phase_ == PlatformerPhase::Running) {
        checkFireBarCollisions();
    }

    if (phase_ != PlatformerPhase::Running) {
        updateCamera();
        return;
    }

    const float fallBoundary = campaignMode_
                                   ? cameraY_ + VIEWPORT_WIDTH * 0.75F + TILE_SIZE
                                   : WORLD_HEIGHT + TILE_SIZE;
    if (player_.y > fallBoundary) {
        beginDeath(PlatformerDeathReason::Fall);
    } else if (player_.x + playerWidth() >= goalX_) {
        const PlatformerCampaignLevel* level = levelRuntime_.level();
        if (campaignMode_ && level != nullptr &&
            level->levelType == PlatformerLevelType::Castle) {
            beginCastleClear();
        } else {
            beginGoal();
        }
    }
    updateCamera();
}

void PlatformerEngine::updateMapTest(float dt, uint16_t dtMs) {
    (void)dt;
    phaseElapsedMs_ = static_cast<uint16_t>(
        std::min<uint32_t>(65535U, phaseElapsedMs_ + dtMs));
}

const PlatformerWarpData* PlatformerEngine::activeWarp() const {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (level == nullptr || activeWarpIndex_ >= level->warps.count) {
        return nullptr;
    }
    return &PLATFORMER_CAMPAIGN_WARPS[level->warps.offset + activeWarpIndex_];
}

bool PlatformerEngine::tryEnterWarp(const PlatformerInput& input) {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (!campaignMode_ || level == nullptr) {
        return false;
    }
    const float playerLeft = player_.x;
    const float playerRight = player_.x + playerWidth();
    const float playerTop = player_.y;
    const float playerBottom = player_.y + playerHeight();
    for (uint8_t index = 0; index < level->warps.count; ++index) {
        const PlatformerWarpData& warp =
            PLATFORMER_CAMPAIGN_WARPS[level->warps.offset + index];
        const float pipeX = static_cast<float>(warp.pipe.x * TILE_SIZE);
        const float pipeY = static_cast<float>(warp.pipe.y * TILE_SIZE);
        const bool verticalTrigger =
            playerLeft <= pipeX + TILE_SIZE + EPSILON &&
            playerRight >= pipeX + TILE_SIZE - EPSILON &&
            playerTop <= pipeY + TILE_SIZE + EPSILON &&
            playerBottom >= pipeY - EPSILON;
        const bool horizontalTrigger =
            playerLeft <= pipeX + TILE_SIZE + EPSILON &&
            playerRight >= pipeX - EPSILON &&
            playerTop <= pipeY + TILE_SIZE + EPSILON &&
            playerBottom >= pipeY + TILE_SIZE - EPSILON;
        bool requested = false;
        switch (warp.enterDirection) {
            case PlatformerDirection::Down:
                requested = verticalTrigger &&
                            (input.crouchHeld || player_.vy > 0.0F);
                break;
            case PlatformerDirection::Up:
                requested = verticalTrigger &&
                            (input.jumpHeld || player_.vy < 0.0F);
                break;
            case PlatformerDirection::Right:
                requested = horizontalTrigger &&
                            (input.moveAxis > 0.0F || player_.vx > 0.0F);
                break;
            case PlatformerDirection::Left:
                requested = horizontalTrigger &&
                            (input.moveAxis < 0.0F || player_.vx < 0.0F);
                break;
            case PlatformerDirection::None:
                break;
        }
        if (!requested) {
            continue;
        }
        activeWarpIndex_ = index;
        warpState_ = 0U;
        phase_ = PlatformerPhase::Warping;
        phaseElapsedMs_ = 0;
        phaseFrames_ = 0U;
        player_.vx = 0.0F;
        player_.vy = 0.0F;
        player_.grounded = false;
        queueEvent(PlatformerEventType::WarpStarted, index);
        return true;
    }
    return false;
}

void PlatformerEngine::applyTeleportPoints(float previousX) {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (!campaignMode_ || level == nullptr || level->teleports.count == 0U ||
        player_.x <= previousX) {
        return;
    }
    for (uint8_t index = 0; index < level->teleports.count; ++index) {
        const PlatformerTeleportData& point =
            PLATFORMER_CAMPAIGN_TELEPORTS[level->teleports.offset + index];
        const float trigger = static_cast<float>(point.triggerX * TILE_SIZE);
        if (previousX < trigger && player_.x >= trigger) {
            const float difference =
                static_cast<float>((point.destinationX - point.triggerX) *
                                   TILE_SIZE);
            player_.x += difference;
            cameraX_ = std::max(0.0F, cameraX_ + difference);
            return;
        }
    }
}

void PlatformerEngine::updateWarp(float dt) {
    const PlatformerWarpData* warp = activeWarp();
    if (warp == nullptr) {
        phase_ = PlatformerPhase::Running;
        return;
    }
    auto moveInDirection = [this, dt](PlatformerDirection direction) {
        constexpr float WARP_SPEED = REFERENCE_VELOCITY_SCALE;
        player_.vx = 0.0F;
        player_.vy = 0.0F;
        switch (direction) {
            case PlatformerDirection::Up:
                player_.y -= WARP_SPEED * dt;
                player_.vy = -WARP_SPEED;
                break;
            case PlatformerDirection::Down:
                player_.y += WARP_SPEED * dt;
                player_.vy = WARP_SPEED;
                break;
            case PlatformerDirection::Left:
                player_.x -= WARP_SPEED * dt;
                player_.vx = -WARP_SPEED;
                break;
            case PlatformerDirection::Right:
                player_.x += WARP_SPEED * dt;
                player_.vx = WARP_SPEED;
                break;
            case PlatformerDirection::None:
                break;
        }
    };

    auto crossed = [this, warp](PlatformerDirection direction,
                                bool entering) {
        const float anchorX = static_cast<float>(
            (entering ? warp->pipe.x : warp->destination.x) * TILE_SIZE);
        const float anchorY = static_cast<float>(
            (entering ? warp->pipe.y : warp->destination.y) * TILE_SIZE);
        switch (direction) {
            case PlatformerDirection::Up:
                return player_.y + playerHeight() <
                       anchorY - (entering ? TILE_SIZE : 0.0F);
            case PlatformerDirection::Down:
                return player_.y >
                       anchorY + (entering ? TILE_SIZE : 0.0F);
            case PlatformerDirection::Left:
                return player_.x + playerWidth() <
                       anchorX - (entering ? TILE_SIZE : 0.0F);
            case PlatformerDirection::Right:
                return player_.x >
                       anchorX + (entering ? TILE_SIZE : 0.0F);
            case PlatformerDirection::None:
                return true;
        }
        return false;
    };

    if (warpState_ == 0U) {
        moveInDirection(warp->enterDirection);
        if (crossed(warp->enterDirection, true)) {
            warpState_ = 1U;
        }
        return;
    }

    if (warpState_ == 1U) {
        moveInDirection(warp->enterDirection);

        if (warp->destinationWorld != 0U && warp->destinationStage != 0U) {
            const uint8_t destinationWorld = warp->destinationWorld;
            const uint8_t destinationStage = warp->destinationStage;
            if (beginLevelTransition(destinationWorld, destinationStage)) {
                queueEvent(PlatformerEventType::WarpCompleted);
            } else {
                phase_ = PlatformerPhase::Running;
            }
            return;
        }

        resetPiranhasForWarp();
        player_.x = static_cast<float>(warp->destination.x * TILE_SIZE);
        player_.y = static_cast<float>(warp->destination.y * TILE_SIZE);
        switch (warp->exitDirection) {
            case PlatformerDirection::Up:
                player_.y += TILE_SIZE;
                break;
            case PlatformerDirection::Down:
                player_.y -= TILE_SIZE;
                break;
            case PlatformerDirection::Left:
                player_.x += TILE_SIZE;
                break;
            case PlatformerDirection::Right:
                player_.x -= TILE_SIZE;
                break;
            case PlatformerDirection::None:
                break;
        }
        cameraX_ = static_cast<float>(warp->camera.x * TILE_SIZE);
        cameraY_ = static_cast<float>(warp->camera.y * TILE_SIZE);
        cameraFrozen_ = warp->freezeCamera;
        levelRuntime_.setSection(warp->levelType, warp->background);
        startIntro_ = false;
        warpState_ = 2U;
        phaseElapsedMs_ = 0;
        phaseFrames_ = 0U;
        return;
    }

    if (warpState_ == 2U) {
        moveInDirection(warp->exitDirection);
        if (crossed(warp->exitDirection, false)) {
            warpState_ = 3U;
        }
        return;
    }

    if (warpState_ == 3U) {
        moveInDirection(warp->exitDirection);
        phase_ = PlatformerPhase::Running;
        startIntro_ = false;
        player_.vx = 0.0F;
        player_.vy = 0.0F;
        queueEvent(PlatformerEventType::WarpCompleted);
    }
}

void PlatformerEngine::resetPiranhasForWarp() {
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        if (!enemy.actor.active ||
            enemy.type != PlatformerEnemyType::PiranhaPlant ||
            enemy.behaviorState != 0U) {
            continue;
        }
        enemy.actor.y = enemy.originY + 32.0F;
        enemy.actor.vy = 0.0F;
        enemy.behaviorState = 1U;
        enemy.moveFrames = 0U;
        enemy.stateMs = 0U;
    }
}

void PlatformerEngine::updateCastleBridge(float dt) {
    if (castleClearFrames_ > 0U) {
        --castleClearFrames_;
        if (castleClearFrames_ == 0U) {
            queueEvent(PlatformerEventType::CastleClear);
        }
    }
    const uint8_t bridgeLength =
        bridgeStartColumn_ >= 0 && bridgeEndColumn_ >= bridgeStartColumn_
            ? static_cast<uint8_t>(bridgeEndColumn_ - bridgeStartColumn_ + 1)
            : 0U;
    if (bridgeSequenceState_ == 0U) {
        if (bridgeLength == 0U) {
            bridgeSequenceState_ = 1U;
            return;
        }
        if (bridgeStepFrames_ > 0U) {
            --bridgeStepFrames_;
        }
        if (bridgeStepFrames_ > 0U) {
            return;
        }
        const uint16_t column = static_cast<uint16_t>(
            bridgeEndColumn_ - bridgeRemovedCount_);
        levelRuntime_.removeTile(column, bridgeRow_);
        ++bridgeRemovedCount_;
        queueEvent(PlatformerEventType::BrickBroken);
        if (bridgeRemovedCount_ >= bridgeLength) {
            // WaitUntil completes here; its following RunCommand executes on
            // the next scheduler frame.
            bridgeSequenceState_ = 1U;
        } else {
            bridgeStepFrames_ = 5U;
        }
        return;
    }

    const auto advanceFallingBowser = [this, dt]() {
        bool visible = false;
        for (uint8_t index = 0; index < enemyCount_; ++index) {
            EnemyActor& enemy = enemies_[index];
            if (!enemy.actor.active ||
                enemy.type != PlatformerEnemyType::Bowser) {
                continue;
            }
            enemy.actor.vy = std::min(
                MAX_FALL_SPEED,
                enemy.actor.vy + REFERENCE_GRAVITY *
                                     REFERENCE_VELOCITY_SCALE);
            enemy.actor.y += enemy.actor.vy * dt;
            enemy.actor.vy = std::min(
                MAX_FALL_SPEED, enemy.actor.vy + enemy.accelerationY * dt);
            visible |= enemy.actor.x + enemy.width >= cameraX_ &&
                       enemy.actor.x <= cameraX_ + VIEWPORT_WIDTH &&
                       enemy.actor.y + enemy.height >= cameraY_ &&
                       enemy.actor.y <= cameraY_ + 15.0F * TILE_SIZE;
            if (!visible) {
                enemy.actor.active = false;
                enemy.motion = PlatformerEnemyMotion::Defeated;
            }
        }
        return visible;
    };

    if (bridgeSequenceState_ == 1U) {
        // Bowser is unfrozen and marked dead after the world tick, so falling
        // starts on the following frame.
        queueEvent(PlatformerEventType::BowserFell);
        bridgeSequenceState_ = 2U;
        return;
    }

    if (bridgeSequenceState_ == 2U) {
        if (!advanceFallingBowser()) {
            bridgeSequenceState_ = 3U;
        }
        return;
    }

    if (bridgeSequenceState_ == 3U) {
        advanceFallingBowser();
        player_.vx = 3.0F * REFERENCE_VELOCITY_SCALE;
        player_.vy = 0.0F;
        playerAccelerationY_ = 0.0F;
        player_.grounded = false;
        castleClearFrames_ = 19U;
        bridgeSequenceState_ = 4U;
        return;
    }

    if (bridgeSequenceState_ == 4U) {
        player_.vy = std::min(
            MAX_FALL_SPEED,
            player_.vy + REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE);
        moveHorizontal(player_.vx * dt);
        moveVertical(player_.vy * dt);
        updateCamera();
        if (player_.vx == 0.0F) {
            bridgeSequenceState_ = 5U;
        }
        return;
    }

    if (bridgeSequenceState_ == 5U) {
        // DelayedCommand(5.0) consumes its first tick on this RunCommand
        // frame, leaving 299 subsequent scheduler ticks.
        bridgeDelayFrames_ = 299U;
        bridgeSequenceState_ = 6U;
        return;
    }

    if (bridgeDelayFrames_ > 0U) {
        --bridgeDelayFrames_;
    }
    if (bridgeDelayFrames_ == 0U) {
        const PlatformerCampaignLevel* level = levelRuntime_.level();
        if (level == nullptr || level->nextWorld != 0U ||
            level->nextStage != 0U) {
            player_.active = false;
        }
        completeCourse();
    }
}

void PlatformerEngine::updateScriptedPhase(float dt, uint16_t dtMs,
                                            const PlatformerInput& input) {
    phaseElapsedMs_ = static_cast<uint16_t>(
        std::min<uint32_t>(65535U, phaseElapsedMs_ + dtMs));
    phaseFrames_ = static_cast<uint16_t>(
        std::min<uint32_t>(65535U, phaseFrames_ + 1U));
    if (phase_ != PlatformerPhase::LevelTransition) {
        updatePlayerStateTimers();
    }
    if (phase_ == PlatformerPhase::Warping ||
        phase_ == PlatformerPhase::VineClimb) {
        updateLevelTimer();
    }
    updateBoxes();
    updateEffects(dt, dtMs);

    if (phase_ == PlatformerPhase::LevelTransition) {
        if (phaseFrames_ >= 180U) {
            phase_ = PlatformerPhase::Running;
            phaseElapsedMs_ = 0U;
            phaseFrames_ = 0U;
            player_.active = true;
            cameraFrozen_ = startIntro_;
            queueEvent(PlatformerEventType::LifeRestarted, lives_);
        }
        return;
    }

    if (campaignMode_) {
        updateMovingPlatforms(dt, false);
        updateFireBars(dt);
    }
    const auto updateWorldActors = [this, dt, dtMs]() {
        if (phase_ == PlatformerPhase::LevelTransition ||
            phase_ == PlatformerPhase::GameOver ||
            phase_ == PlatformerPhase::Won) {
            return;
        }
        updateEnemies(dt, dtMs);
        updateEnemyHazards(dt, dtMs);
        updatePowerups(dt, dtMs);
        updateProjectiles(dt, dtMs);
        checkEnemyPairCollisions();
    };

    if (phase_ == PlatformerPhase::Warping) {
        updateWarp(dt);
        updateWorldActors();
        return;
    }

    if (phase_ == PlatformerPhase::VineClimb) {
        updateVineClimb(dt, input);
        updateWorldActors();
        return;
    }

    if (phase_ == PlatformerPhase::CastleBridge) {
        updateCastleBridge(dt);
        updateWorldActors();
        return;
    }

    if (phase_ == PlatformerPhase::Dying) {
        player_.vy = std::min(player_.vy + GRAVITY * dt, MAX_FALL_SPEED);
        player_.y += player_.vy * dt;
        if (phaseFrames_ >= 179U) {
            if (lives_ > 1) {
                --lives_;
                resetLife();
            } else {
                lives_ = 0;
                phase_ = PlatformerPhase::GameOver;
                phaseElapsedMs_ = 0;
                phaseFrames_ = 0U;
                queueEvent(PlatformerEventType::GameOver);
            }
        }
        updateCamera();
        updateWorldActors();
        return;
    }

    if (phase_ == PlatformerPhase::Flagpole) {
        const PlatformerCampaignLevel* level = levelRuntime_.level();
        const float floorY = campaignMode_ && level != nullptr
                                 ? static_cast<float>(
                                       (level->playerStart.y + 1) * TILE_SIZE) -
                                       playerHeight()
                                 : static_cast<float>(
                                       PLATFORMER_LEVEL_1_1.groundY) -
                                       playerHeight();
        if (!flagShifted_) {
            player_.x = flagClimbPlayerX();
        }
        if (!flagLanded_) {
            player_.y = std::min(floorY, player_.y + 120.0F * dt);
        }
        const float flagSlideY = campaignMode_ && level != nullptr
                                       ? static_cast<float>(
                                             (level->playerStart.y + 1) *
                                             TILE_SIZE - TILE_SIZE)
                                       : campaignMode_ ? floorY
                                               : static_cast<float>(
                                                     PLATFORMER_LEVEL_1_1.flagSlideY);
        if (!flagLanded_) {
            flagY_ = std::min(flagSlideY, flagY_ + 120.0F * dt);
            if (player_.y >= floorY && flagY_ >= flagSlideY) {
                flagLanded_ = true;
                phaseFrames_ = 0U;
                player_.vy = 0.0F;
            }
        } else if (!flagShifted_ && phaseFrames_ >= 1U) {
            player_.x += 17.0F;
            flagShifted_ = true;
        } else if (flagShifted_) {
            player_.x = std::min(
                flagClimbPlayerX() + TILE_SIZE * 1.5F,
                player_.x + 0.25F);
        }
        if (flagLanded_ && phaseFrames_ >= 38U) {
            phase_ = PlatformerPhase::CastleWalk;
            phaseElapsedMs_ = 0;
            phaseFrames_ = 0U;
            player_.y = floorY;
            player_.vx = 2.0F * REFERENCE_VELOCITY_SCALE;
        }
        updateCamera();
        updateWorldActors();
        return;
    }

    if (phase_ == PlatformerPhase::CastleWalk) {
        const PlatformerCampaignLevel* level = levelRuntime_.level();
        player_.vx = 2.0F * REFERENCE_VELOCITY_SCALE;
        if (campaignMode_ && level != nullptr) {
            player_.vy = std::min(
                MAX_FALL_SPEED,
                player_.vy + REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE);
            moveHorizontal(player_.vx * dt);
            moveVertical(player_.vy * dt);
        } else {
            player_.x = std::min(castleX_, player_.x + player_.vx * dt);
            player_.y =
                static_cast<float>(PLATFORMER_LEVEL_1_1.groundY) -
                playerHeight();
        }
        if (player_.vx == 0.0F ||
            (!campaignMode_ && player_.x >= castleX_)) {
            phase_ = PlatformerPhase::TimeBonus;
            phaseElapsedMs_ = 0;
            phaseFrames_ = 0U;
            timeBonusReady_ = false;
            timeBonusCompletionFrames_ = 0U;
            player_.vx = 0.0F;
        }
        updateCamera();
        updateWorldActors();
        return;
    }

    if (phase_ == PlatformerPhase::TimeBonus) {
        if (!timeBonusReady_ && timeRemaining_ > 0U) {
            --timeRemaining_;
            addScore(100, player_.x, player_.y);
            queueEvent(PlatformerEventType::TimerTick);
        }
        if (!timeBonusReady_ && timeRemaining_ == 0U &&
            phaseFrames_ >= 270U) {
            timeBonusReady_ = true;
            timeBonusCompletionFrames_ = 0U;
        } else if (timeBonusReady_) {
            ++timeBonusCompletionFrames_;
            if (timeBonusCompletionFrames_ == 1U) {
                player_.active = false;
            }
        }
        if (timeBonusReady_ && timeBonusCompletionFrames_ >= 120U) {
            completeCourse();
        }
        updateCamera();
        updateWorldActors();
    }
}

void PlatformerEngine::updatePlayerAnimation() {
    constexpr uint8_t ANIMATION_NONE = 0U;
    constexpr uint8_t ANIMATION_WALK = 1U;
    constexpr uint8_t ANIMATION_SWIM = 2U;
    constexpr uint8_t ANIMATION_SWIM_WALK = 3U;
    constexpr uint8_t ANIMATION_CLIMB = 4U;

    uint8_t mode = ANIMATION_NONE;
    uint8_t frameDelay = 0U;
    uint8_t frameCount = 0U;
    const bool animationBlocked =
        !player_.active ||
        powerTransition_ != PlatformerPowerTransition::None ||
        fireballPoseFrames_ > 0U || playerCrouching_ || playerSkidding_ ||
        phase_ == PlatformerPhase::Dying ||
        phase_ == PlatformerPhase::Flagpole ||
        phase_ == PlatformerPhase::TimeBonus ||
        phase_ == PlatformerPhase::LevelTransition;
    if (!animationBlocked && phase_ == PlatformerPhase::VineClimb &&
        std::fabs(player_.vy) > EPSILON) {
        mode = ANIMATION_CLIMB;
        frameDelay = 8U;
        frameCount = 2U;
    } else if (!animationBlocked &&
               campaignMode_ &&
               levelRuntime_.activeLevelType() ==
                   PlatformerLevelType::Underwater &&
               swimStrokeFrames_ == 0U) {
        if (player_.grounded && std::fabs(player_.vx) > EPSILON) {
            mode = ANIMATION_SWIM_WALK;
            frameDelay = 15U;
            frameCount = 3U;
        } else if (!player_.grounded) {
            mode = ANIMATION_SWIM;
            frameDelay = 4U;
            frameCount = 2U;
        }
    } else if (!animationBlocked &&
               (std::fabs(player_.vx) > EPSILON ||
                std::fabs(playerAccelerationX_) > EPSILON) &&
               (player_.grounded || startIntro_ ||
                phase_ == PlatformerPhase::Warping ||
                phase_ == PlatformerPhase::CastleBridge ||
                phase_ == PlatformerPhase::CastleWalk)) {
        mode = ANIMATION_WALK;
        frameDelay = playerRunning_ ? 5U : 8U;
        frameCount = 3U;
    }

    if (mode != playerAnimationMode_) {
        playerAnimationMode_ = mode;
        playerAnimationFrame_ = 0U;
        playerAnimationTimer_ = 0U;
    }
    if (mode == ANIMATION_NONE) {
        playerAnimationFrame_ = 0U;
        playerAnimationTimer_ = 0U;
        return;
    }

    const bool inCamera =
        player_.x + playerWidth() >= cameraX_ &&
        player_.x <= cameraX_ + VIEWPORT_WIDTH &&
        player_.y + playerHeight() >= cameraY_ &&
        player_.y <= cameraY_ + VIEWPORT_HEIGHT;
    if (inCamera) {
        platformerAdvanceReferenceAnimation(
            playerAnimationFrame_, playerAnimationTimer_, frameDelay,
            frameCount);
    }
}

void PlatformerEngine::updatePlayerStateTimers() {
    auto reduceFrame = [](uint16_t& timer) {
        timer = static_cast<uint16_t>(timer > 0U ? timer - 1U : 0U);
    };
    if (powerTransition_ != PlatformerPowerTransition::None) {
        if (powerTransitionFrames_ > 0U) {
            --powerTransitionFrames_;
            ++powerTransitionElapsedFrames_;
            if (powerTransitionFrames_ == 0U) {
                if (powerTransition_ == PlatformerPowerTransition::Grow) {
                    playerPower_ = PlatformerPlayerPower::Big;
                } else if (powerTransition_ ==
                           PlatformerPowerTransition::Fire) {
                    playerPower_ = PlatformerPlayerPower::Fire;
                }
            }
        } else {
            powerTransition_ = PlatformerPowerTransition::None;
            powerTransitionElapsedFrames_ = 0U;
        }
    }
    reduceFrame(hurtInvincibleFrames_);
    reduceFrame(starInvincibleFrames_);
    reduceFrame(starBlinkFrames_);
    if (fireballPoseFrames_ > 0U) {
        --fireballPoseFrames_;
    }
}

void PlatformerEngine::updateLevelTimer() {
    ++levelClockFrames_;
    while (levelClockFrames_ >= LEVEL_TICK_FRAMES && timeRemaining_ > 0) {
        levelClockFrames_ = static_cast<uint8_t>(
            levelClockFrames_ - LEVEL_TICK_FRAMES);
        --timeRemaining_;
    }
}

void PlatformerEngine::updateCamera() {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    const float maximum = campaignMode_ && level != nullptr
                              ? std::max(
                                    0.0F,
                                    static_cast<float>(level->cameraMaximum *
                                                       TILE_SIZE) -
                                        VIEWPORT_WIDTH)
                              : static_cast<float>(WORLD_WIDTH - VIEWPORT_WIDTH);
    if (phase_ == PlatformerPhase::Title || phase_ == PlatformerPhase::GameOver) {
        cameraX_ = campaignMode_ && level != nullptr
                       ? static_cast<float>(level->cameraStart.x * TILE_SIZE)
                       : 0.0F;
        return;
    }
    if (campaignMode_ && cameraFrozen_) {
        return;
    }

    const float playerCenter = player_.x + playerWidth() * 0.5F;
    const float cameraCenter = cameraX_ + VIEWPORT_WIDTH * 0.5F;
    if (playerCenter > cameraCenter && player_.vx > 0.0F) {
        cameraX_ += cameraAdvanceX_;
    }
    const float minimum = campaignMode_ && level != nullptr
                              ? static_cast<float>(level->cameraStart.x *
                                                   TILE_SIZE)
                              : 0.0F;
    cameraX_ = clampValue(cameraX_, minimum, maximum);
}

void PlatformerEngine::updatePlayerHorizontal(float dt,
                                               const PlatformerInput& input) {
    cameraAdvanceX_ = 0.0F;
    const float axis = clampValue(input.moveAxis, -1.0F, 1.0F);
    const bool underwater = campaignMode_ &&
                            levelRuntime_.activeLevelType() ==
                                PlatformerLevelType::Underwater;
    if (underwater) {
        playerAccelerationX_ = 0.0F;
        playerSkidding_ = false;
        moveHorizontal(player_.vx * dt);
        return;
    }

    // Reference PhysicsSystem moves with the current velocity, then applies
    // the acceleration selected by PlayerSystem on the preceding 60Hz tick.
    moveHorizontal(player_.vx * dt);
    player_.vx += playerAccelerationX_ * dt;
    const float referenceFrames = dt * REFERENCE_TICKS_PER_SECOND;
    player_.vx *= std::pow(REFERENCE_FRICTION, referenceFrames);
    player_.vx = clampValue(player_.vx, -MAX_PLAYER_SPEED, MAX_PLAYER_SPEED);
    if (std::fabs(player_.vx) <
            REFERENCE_ACCELERATION * REFERENCE_VELOCITY_SCALE * 0.5F &&
        playerAccelerationX_ == 0.0F) {
        player_.vx = 0.0F;
    }

    float referenceAcceleration = axis * REFERENCE_ACCELERATION;
    if (player_.grounded) {
        if (axis < -0.01F) {
            playerFacingLeft_ = true;
        } else if (axis > 0.01F) {
            playerFacingLeft_ = false;
        }
        if (playerCrouching_) {
            referenceAcceleration = 0.0F;
            const float crouchSlowdown =
                0.5F * REFERENCE_VELOCITY_SCALE * referenceFrames;
            if (player_.vx > 1.5F * REFERENCE_VELOCITY_SCALE) {
                player_.vx -= crouchSlowdown;
            } else if (player_.vx < -1.5F * REFERENCE_VELOCITY_SCALE) {
                player_.vx += crouchSlowdown;
            }
        } else {
            referenceAcceleration *=
                input.actionHeld ? REFERENCE_RUN_MULTIPLIER
                                 : REFERENCE_WALK_MULTIPLIER;
        }
    } else if (input.actionHeld) {
        const float referenceVelocity =
            player_.vx / REFERENCE_VELOCITY_SCALE;
        const bool sameDirection =
            (referenceAcceleration >= 0.0F && referenceVelocity >= 0.0F) ||
            (referenceAcceleration <= 0.0F && referenceVelocity <= 0.0F);
        referenceAcceleration *=
            sameDirection ? REFERENCE_RUN_MULTIPLIER : 0.35F;
    }
    playerAccelerationX_ =
        referenceAcceleration * REFERENCE_VELOCITY_SCALE *
        REFERENCE_TICKS_PER_SECOND;
    cameraAdvanceX_ = std::max(0.0F, player_.vx * dt);
    playerSkidding_ =
        player_.grounded && referenceAcceleration * player_.vx < 0.0F;
    if (player_.grounded && player_.vy == 0.0F && std::fabs(axis) < 0.01F) {
        stompChain_ = 0;
    }
}

void PlatformerEngine::updatePlayerVertical(float dt, uint16_t dtMs,
                                             const PlatformerInput& input) {
    (void)dtMs;
    if (campaignMode_ && levelRuntime_.activeLevelType() ==
                             PlatformerLevelType::Underwater) {
        if (swimStrokeFrames_ > 0U) {
            ++swimStrokeFrames_;
            const uint8_t finalFrame =
                playerPower_ == PlatformerPlayerPower::Small ? 17U : 21U;
            if (swimStrokeFrames_ > finalFrame) {
                swimStrokeFrames_ = 0U;
            }
        }
        const float referenceFrames = dt * REFERENCE_TICKS_PER_SECOND;
        player_.vy += REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE *
                      referenceFrames;
        moveVertical(player_.vy * dt);
        player_.vy += playerAccelerationY_ * dt;
        player_.vy = std::min(
            player_.vy,
            REFERENCE_MAX_FALL_SPEED * REFERENCE_VELOCITY_SCALE);

        constexpr float WATER_PID_GAIN = 0.20F + 0.02F / 60.0F;
        const float referenceVelocityX =
            player_.vx / REFERENCE_VELOCITY_SCALE;
        const float waterTarget =
            clampValue(input.moveAxis, -1.0F, 1.0F) *
            (player_.grounded ? 1.0F : 3.0F);
        const float controlledVelocity =
            referenceVelocityX +
            (waterTarget - referenceVelocityX) * WATER_PID_GAIN *
                referenceFrames;
        player_.vx = controlledVelocity * REFERENCE_VELOCITY_SCALE;
        if (input.moveAxis < -0.01F) {
            playerFacingLeft_ = true;
        } else if (input.moveAxis > 0.01F) {
            playerFacingLeft_ = false;
        }

        playerAccelerationY_ =
            -0.45480F * REFERENCE_VELOCITY_SCALE *
            REFERENCE_TICKS_PER_SECOND;
        player_.vy = std::min(player_.vy,
                              2.0F * REFERENCE_VELOCITY_SCALE);
        if (input.jumpPressed) {
            player_.vy = -3.53F * REFERENCE_VELOCITY_SCALE;
            swimStrokeFrames_ = 1U;
            queueEvent(PlatformerEventType::SwimStroke);
        }
        cameraAdvanceX_ = std::max(0.0F, player_.vx * dt);
        jumpActive_ = true;
        return;
    }

    swimStrokeFrames_ = 0U;

    const float referenceFrames = dt * REFERENCE_TICKS_PER_SECOND;
    player_.vy += REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE *
                  referenceFrames;
    const float previousY = player_.y;
    const float verticalVelocity = player_.vy;
    moveVertical(player_.vy * dt);
    player_.vy += playerAccelerationY_ * dt;
    player_.vy = std::min(
        player_.vy,
        REFERENCE_MAX_FALL_SPEED * REFERENCE_VELOCITY_SCALE);
    if (std::fabs(player_.vy) <
            REFERENCE_ACCELERATION * REFERENCE_VELOCITY_SCALE * 0.5F &&
        playerAccelerationY_ == 0.0F) {
        player_.vy = 0.0F;
    }

    if (verticalVelocity < 0.0F) {
        checkBoxCollision(previousY, verticalVelocity);
    }

    if (player_.grounded) {
        playerAccelerationY_ = 0.0F;
        if (input.jumpPressed && !trampolineCollided_) {
            player_.vy = REFERENCE_JUMP_VELOCITY *
                         REFERENCE_VELOCITY_SCALE;
            player_.grounded = false;
            jumpActive_ = true;
            queueEvent(PlatformerEventType::Jumped);
        } else {
            player_.vy = 0.0F;
            jumpActive_ = false;
        }
    } else {
        const bool runningJump = input.actionHeld &&
            std::fabs(player_.vx) > 3.5F * REFERENCE_VELOCITY_SCALE;
        const float referenceAcceleration =
            input.jumpHeld &&
                    player_.vy < -1.0F * REFERENCE_VELOCITY_SCALE
                ? (runningJump ? REFERENCE_RUNNING_JUMP_ACCELERATION
                               : REFERENCE_JUMP_ACCELERATION)
                : 0.0F;
        playerAccelerationY_ =
            referenceAcceleration * REFERENCE_VELOCITY_SCALE *
            REFERENCE_TICKS_PER_SECOND;
        jumpActive_ = true;
    }
}

void PlatformerEngine::moveHorizontal(float distance) {
    if (std::fabs(distance) < EPSILON) {
        return;
    }
    const float previousX = player_.x;
    player_.x += distance;
    if (!rectHitsPlayerHorizontal(player_.x, player_.y, playerWidth(),
                                  playerHeight())) {
        const PlatformerCampaignLevel* level = levelRuntime_.level();
        const float minimum = campaignMode_ ? cameraX_ : 0.0F;
        const float maximum = campaignMode_ && level != nullptr
                                  ? static_cast<float>(level->cameraMaximum *
                                                       TILE_SIZE) -
                                        playerWidth()
                                  : static_cast<float>(WORLD_WIDTH) -
                                        playerWidth();
        player_.x = clampValue(player_.x, minimum, maximum);
        return;
    }

    if (campaignMode_) {
        const int32_t rowStart = static_cast<int32_t>(
            std::floor(player_.y / TILE_SIZE));
        const int32_t rowEnd = static_cast<int32_t>(std::floor(
            (player_.y + playerHeight() -
             TILE_COLLISION_ROUNDNESS * 2.0F - EPSILON) /
            TILE_SIZE));
        const int32_t column = distance > 0.0F
                                   ? static_cast<int32_t>(std::floor(
                                         (player_.x + playerWidth() - EPSILON) /
                                         TILE_SIZE))
                                   : static_cast<int32_t>(
                                         std::floor(player_.x / TILE_SIZE));
        for (int32_t row = rowStart; row <= rowEnd; ++row) {
            if (column < 0 || row < 0 || row > 255 ||
                !levelRuntime_.isSolid(static_cast<uint16_t>(column),
                                       static_cast<uint8_t>(row))) {
                continue;
            }
            player_.x = distance > 0.0F
                            ? static_cast<float>(column * TILE_SIZE) -
                                  playerWidth()
                            : static_cast<float>((column + 1) * TILE_SIZE);
            break;
        }
        player_.x = std::max(player_.x, cameraX_);
        player_.vx = 0.0F;
        playerAccelerationX_ = 0.0F;
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
    playerAccelerationX_ = 0.0F;
}

void PlatformerEngine::moveVertical(float distance) {
    if (std::fabs(distance) < EPSILON && player_.grounded) {
        if (rectHitsPlayerVertical(player_.x, player_.y + 1.0F,
                                   playerWidth(), playerHeight(), false) ||
            standingPlatform() >= 0) {
            return;
        }
        player_.grounded = false;
    }
    const float previousY = player_.y;
    const float previousBottom = previousY + playerHeight();
    player_.grounded = false;
    player_.y += distance;
    const int8_t platformIndex =
        landingPlatform(previousBottom, player_.y + playerHeight());
    if (platformIndex >= 0) {
        player_.y = movingPlatforms_[platformIndex].y - playerHeight();
        player_.grounded = true;
        player_.vy = 0.0F;
        playerAccelerationY_ = 0.0F;
        return;
    }
    const bool rising = distance < 0.0F;
    if (!rectHitsPlayerVertical(player_.x, player_.y, playerWidth(),
                                playerHeight(), rising)) {
        return;
    }

    if (campaignMode_) {
        const float horizontalInset =
            rising ? TILE_COLLISION_ROUNDNESS
                   : TILE_COLLISION_ROUNDNESS * 0.5F;
        const int32_t columnStart = static_cast<int32_t>(
            std::floor((player_.x + horizontalInset) / TILE_SIZE));
        const int32_t columnEnd = static_cast<int32_t>(std::floor(
            (player_.x + playerWidth() - horizontalInset - EPSILON) /
            TILE_SIZE));
        const int32_t row = distance > 0.0F
                                ? static_cast<int32_t>(std::floor(
                                      (player_.y + playerHeight() - EPSILON) /
                                      TILE_SIZE))
                                : static_cast<int32_t>(
                                      std::floor(player_.y / TILE_SIZE));
        for (int32_t column = columnStart; column <= columnEnd; ++column) {
            if (column < 0 || row < 0 || row > 255 ||
                !levelRuntime_.isSolid(static_cast<uint16_t>(column),
                                       static_cast<uint8_t>(row))) {
                continue;
            }
            player_.y = distance > 0.0F
                            ? static_cast<float>(row * TILE_SIZE) -
                                  playerHeight()
                            : static_cast<float>((row + 1) * TILE_SIZE);
            player_.grounded = distance > 0.0F;
            break;
        }
        player_.vy = 0.0F;
        playerAccelerationY_ = 0.0F;
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
    playerAccelerationY_ = 0.0F;
}

bool PlatformerEngine::rectHitsSolid(float x, float y, float width, float height,
                                     bool includeHidden) const {
    if (campaignMode_) {
        (void)includeHidden;
        return levelRuntime_.rectHitsSolid(x, y, width, height);
    }
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

bool PlatformerEngine::rectHitsPlayerTiles(
    float x, float y, float width, float height, float horizontalInset,
    float bottomTrim, float tileHorizontalInset) const {
    const float actorX = x + horizontalInset;
    const float actorWidth = width - horizontalInset * 2.0F;
    const float actorHeight = height - bottomTrim;
    if (actorWidth <= 0.0F || actorHeight <= 0.0F) {
        return false;
    }

    if (campaignMode_) {
        const PlatformerCampaignLevel* level = levelRuntime_.level();
        if (level == nullptr) {
            return false;
        }
        const int32_t firstColumn = static_cast<int32_t>(
            std::floor(actorX / TILE_SIZE));
        const int32_t lastColumn = static_cast<int32_t>(std::floor(
            (actorX + actorWidth - EPSILON) / TILE_SIZE));
        const int32_t firstRow = static_cast<int32_t>(std::floor(y / TILE_SIZE));
        const int32_t lastRow = static_cast<int32_t>(
            std::floor((y + actorHeight - EPSILON) / TILE_SIZE));
        const float tileWidth = TILE_SIZE - tileHorizontalInset * 2.0F;
        for (int32_t row = firstRow; row <= lastRow; ++row) {
            if (row < 0 || row >= level->height) {
                continue;
            }
            for (int32_t column = firstColumn; column <= lastColumn;
                 ++column) {
                if (column < 0 || column >= level->width ||
                    !levelRuntime_.isSolid(static_cast<uint16_t>(column),
                                           static_cast<uint8_t>(row))) {
                    continue;
                }
                if (overlaps(actorX, y, actorWidth, actorHeight,
                             column * TILE_SIZE + tileHorizontalInset,
                             row * TILE_SIZE, tileWidth, TILE_SIZE)) {
                    return true;
                }
            }
        }
        return false;
    }

    for (uint8_t index = 0; index < PLATFORMER_LEVEL_1_1.solidCount;
         ++index) {
        const auto& solid = PLATFORMER_LEVEL_1_1.solids[index];
        if (overlaps(actorX, y, actorWidth, actorHeight,
                     solid.x + tileHorizontalInset, solid.y,
                     solid.width - tileHorizontalInset * 2.0F,
                     solid.height)) {
            return true;
        }
    }
    for (uint8_t index = 0; index < boxCount_; ++index) {
        const PlatformerBox& box = boxes_[index];
        if (box.visible &&
            overlaps(actorX, y, actorWidth, actorHeight,
                     box.x + tileHorizontalInset, box.y,
                     TILE_SIZE - tileHorizontalInset * 2.0F, TILE_SIZE)) {
            return true;
        }
    }
    return false;
}

bool PlatformerEngine::rectHitsPlayerVertical(float x, float y, float width,
                                               float height,
                                               bool rising) const {
    const float inset = rising ? TILE_COLLISION_ROUNDNESS
                               : TILE_COLLISION_ROUNDNESS * 0.5F;
    return rectHitsPlayerTiles(x, y, width, height, inset, 0.0F, inset);
}

bool PlatformerEngine::rectHitsPlayerHorizontal(float x, float y, float width,
                                                 float height) const {
    return rectHitsPlayerTiles(x, y, width, height, 0.0F,
                               TILE_COLLISION_ROUNDNESS * 2.0F, 0.0F);
}

void PlatformerEngine::updateBoxes() {
    static constexpr int8_t OFFSETS[] = {-2, -3, -4, -5,
                                          -4, -3, -2, 0};
    if (campaignMode_) {
        levelRuntime_.updateBlockBumps();
        for (uint8_t index = 0;
             index < PlatformerLevelRuntime::MAX_BLOCK_BUMPS; ++index) {
            const PlatformerBlockBumpState* bump =
                levelRuntime_.blockBump(index);
            if (bump == nullptr) {
                continue;
            }
            PlatformerBox block;
            block.x = static_cast<int16_t>(bump->column * TILE_SIZE);
            block.y = static_cast<int16_t>(bump->row * TILE_SIZE +
                                           bump->offset);
            bumpEnemiesAbove(block);
        }
        return;
    }
    for (uint8_t index = 0; index < boxCount_; ++index) {
        if (boxBumpFrames_[index] == 0U) {
            boxes_[index].bumpOffset = 0;
            continue;
        }
        const uint8_t step = static_cast<uint8_t>(boxBumpFrames_[index] - 1U);
        boxes_[index].bumpOffset = OFFSETS[step];
        if (step + 1U < sizeof(OFFSETS) / sizeof(OFFSETS[0])) {
            ++boxBumpFrames_[index];
            bumpEnemiesAbove(boxes_[index]);
        } else {
            boxBumpFrames_[index] = 0U;
            boxes_[index].bumpOffset = 0;
        }
    }
}

void PlatformerEngine::updateEnemies(float dt, uint16_t dtMs) {
    if (campaignMode_ && !campaignEnemiesLoaded_) {
        loadCampaignEnemies();
        campaignEnemiesLoaded_ = true;
    }
    const uint8_t enemiesAtFrameStart = enemyCount_;
    for (uint8_t index = 0; index < enemiesAtFrameStart; ++index) {
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
            enemy.bornFrame = logicFrame_;
        }
        if (!enemy.actor.active) {
            if (phase_ == PlatformerPhase::CastleBridge &&
                enemy.type == PlatformerEnemyType::Bowser) {
                updateBowserCallbacks(enemy);
            }
            continue;
        }
        if (enemy.bornFrame == logicFrame_ &&
            enemy.type == PlatformerEnemyType::Spiny &&
            enemy.sourceTileId == 500U) {
            continue;
        }
        if (enemy.motion == PlatformerEnemyMotion::FallingDefeated) {
            if (enemy.bornFrame == logicFrame_) {
                updateQueuedEnemyCommands(enemy);
                continue;
            }
            if (enemy.actor.x + enemy.width < cameraX_ ||
                enemy.actor.x > cameraX_ + VIEWPORT_WIDTH ||
                enemy.actor.y + enemy.height < cameraY_ ||
                enemy.actor.y > cameraY_ + VIEWPORT_HEIGHT) {
                enemy.actor.active = false;
                enemy.motion = PlatformerEnemyMotion::Defeated;
                continue;
            }
            if (enemy.type != PlatformerEnemyType::BulletBill) {
                enemy.actor.vy = std::min(
                    enemy.actor.vy +
                        REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE,
                    MAX_FALL_SPEED);
            }
            enemy.actor.x += enemy.actor.vx * dt;
            enemy.actor.y += enemy.actor.vy * dt;
            updateQueuedEnemyCommands(enemy);
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
            if (enemy.bornFrame == logicFrame_) {
                continue;
            }
            enemy.actor.active = false;
            continue;
        }

        const bool inCamera =
            enemy.actor.x + enemy.width >= cameraX_ &&
            enemy.actor.x <= cameraX_ + VIEWPORT_WIDTH &&
            enemy.actor.y + enemy.height >= cameraY_ &&
            enemy.actor.y <= cameraY_ + VIEWPORT_HEIGHT;
        if (inCamera) {
            const uint8_t frameDelay = enemyAnimationDelay(enemy.type);
            if (frameDelay > 0U) {
                platformerAdvanceReferenceAnimation(
                    enemy.animationFrame, enemy.animationTimer, frameDelay,
                    2U);
            }
        }
        if (campaignMode_ && enemy.motion == PlatformerEnemyMotion::Walking &&
            enemy.type == PlatformerEnemyType::PiranhaPlant) {
            enemy.actor.y += enemy.actor.vy * dt;
            if (enemy.actor.vy > 0.0F &&
                enemy.actor.y >= enemy.originY + 32.0F) {
                enemy.actor.y = enemy.originY + 32.0F;
                enemy.actor.vy = 0.0F;
            } else if (enemy.actor.vy < 0.0F &&
                       enemy.actor.y <= enemy.originY) {
                enemy.actor.y = enemy.originY;
                enemy.actor.vy = 0.0F;
            }
            ++enemy.moveFrames;
            if (enemy.moveFrames >= 180U) {
                enemy.moveFrames = 0U;
                if (enemy.behaviorState != 0U) {
                    enemy.actor.vy = -REFERENCE_VELOCITY_SCALE;
                    enemy.behaviorState = 0U;
                } else {
                    enemy.actor.vy = REFERENCE_VELOCITY_SCALE;
                    enemy.behaviorState = 1U;
                }
            }
            enemy.stateMs = enemy.moveFrames;
            continue;
        }

        if (campaignMode_ && enemy.motion == PlatformerEnemyMotion::Walking &&
            enemy.type == PlatformerEnemyType::Blooper) {
            if (inCamera) {
                enemy.actor.vy += REFERENCE_GRAVITY *
                                  REFERENCE_VELOCITY_SCALE;
                enemy.actor.x += enemy.actor.vx * dt;
                enemy.actor.y += enemy.actor.vy * dt;
                enemy.actor.vy += enemy.accelerationY * dt;
            }

            if (enemy.callbackFrames > 0U) {
                --enemy.callbackFrames;
                if (enemy.callbackFrames == 0U) {
                    enemy.behaviorState = 0U;
                    enemy.actor.vx = 0.0F;
                    enemy.actor.vy = 0.0F;
                    enemy.accelerationY =
                        -0.47480F * REFERENCE_VELOCITY_SCALE *
                        REFERENCE_TICKS_PER_SECOND;
                }
            }
            ++enemy.attackFrames;
            if (enemy.attackFrames >= 60U) {
                enemy.attackFrames = 0U;
                if (inCamera) {
                    enemy.behaviorState = 1U;
                    enemy.callbackFrames = 30U;
                    enemy.accelerationY = 0.0F;
                    enemy.actor.vx =
                        player_.x > enemy.actor.x ? 90.0F : -90.0F;
                    const float cameraCenterY =
                        cameraY_ + VIEWPORT_HEIGHT * 0.5F;
                    enemy.actor.vy = enemy.actor.y < cameraCenterY
                                         ? 90.0F
                                         : -90.0F;
                }
            }
            enemy.stateMs = enemy.attackFrames;
            continue;
        }

        if (campaignMode_ && enemy.motion == PlatformerEnemyMotion::Walking &&
            enemy.type == PlatformerEnemyType::Lakitu) {
            if (inCamera) {
                enemy.facingLeft = player_.x > enemy.actor.x;
                enemy.actor.x += enemy.actor.vx * dt;

                ++enemy.moveFrames;
                if (enemy.moveFrames >= 480U) {
                    enemy.moveFrames = 0U;
                    enemy.behaviorState ^= 1U;
                }
                if (flagX_ - player_.x < 30.0F * TILE_SIZE) {
                    enemy.actor.vx = -4.0F * REFERENCE_VELOCITY_SCALE;
                } else {
                    const float target = cameraX_ + VIEWPORT_WIDTH * 0.5F +
                        (enemy.behaviorState != 0U ? 6.0F * TILE_SIZE
                                                   : -6.0F * TILE_SIZE);
                    enemy.actor.vx = clampValue(
                        (target - enemy.actor.x) * 3.6F, -180.0F, 180.0F);
                }
            }

            updateQueuedEnemyCommands(enemy);
            ++enemy.attackFrames;
            if (enemy.attackFrames >= 180U) {
                enemy.attackFrames = 0U;
                if (inCamera) {
                    enemy.actor.y += TILE_SIZE;
                    enemy.height = TILE_SIZE;
                    enemy.callbackFrames = 46U;
                }
            }
            enemy.stateMs = enemy.attackFrames;
            continue;
        }

        if (campaignMode_ && enemy.motion == PlatformerEnemyMotion::Walking &&
            enemy.type == PlatformerEnemyType::CheepCheep &&
            !enemy.flyingCheep) {
            if (!inCamera) {
                continue;
            }
            enemy.actor.vx = -ENEMY_SPEED;
            enemy.actor.vy = 0.0F;
            enemy.actor.x += enemy.actor.vx * dt;
            continue;
        }

        if (campaignMode_ && enemy.motion == PlatformerEnemyMotion::Walking &&
            enemy.type == PlatformerEnemyType::CheepCheep) {
            if (enemy.attackFrames == 0U && enemy.callbackFrames == 0U &&
                enemy.accelerationY == 0.0F &&
                std::fabs(enemy.actor.x - enemy.originX) < EPSILON &&
                enemy.actor.vx == -ENEMY_SPEED) {
                enemy.actor.vx = 0.0F;
            }
            enemy.actor.vy += REFERENCE_GRAVITY *
                              REFERENCE_VELOCITY_SCALE;
            enemy.actor.x += enemy.actor.vx * dt;
            enemy.actor.y += enemy.actor.vy * dt;
            enemy.actor.vy = std::min(
                MAX_FALL_SPEED,
                enemy.actor.vy + enemy.accelerationY * dt);

            if (enemy.callbackFrames > 0U) {
                --enemy.callbackFrames;
                if (enemy.callbackFrames == 0U) {
                    const bool inCameraY =
                        enemy.actor.y + enemy.height >= cameraY_ &&
                        enemy.actor.y <= cameraY_ + 15.0F * TILE_SIZE;
                    if (!inCameraY) {
                        enemy.actor.x = enemy.originX;
                        enemy.actor.y = enemy.originY;
                        const bool inCameraX =
                            enemy.actor.x + enemy.width >= cameraX_ &&
                            enemy.actor.x <= cameraX_ + VIEWPORT_WIDTH;
                        if (inCameraX) {
                            const float randomX =
                                static_cast<float>(nextRandom()) /
                                4294967295.0F;
                            enemy.actor.vx =
                                (2.0F + randomX * 3.0F) *
                                REFERENCE_VELOCITY_SCALE;
                            enemy.actor.vy =
                                -10.0F * REFERENCE_VELOCITY_SCALE;
                            enemy.accelerationY =
                                -0.4542F * REFERENCE_VELOCITY_SCALE *
                                REFERENCE_TICKS_PER_SECOND;
                        }
                    }
                }
            }
            ++enemy.attackFrames;
            if (enemy.attackFrames >= 150U) {
                enemy.attackFrames = 0U;
                const float randomDelay =
                    static_cast<float>(nextRandom()) / 4294967295.0F;
                enemy.callbackFrames = static_cast<uint16_t>(
                    (0.5F + randomDelay * 2.0F) *
                    REFERENCE_TICKS_PER_SECOND);
            }
            enemy.stateMs = enemy.attackFrames;
            continue;
        }

        if (campaignMode_ && enemy.motion == PlatformerEnemyMotion::Walking &&
            enemy.type == PlatformerEnemyType::BulletBill) {
            if (!inCamera) {
                enemy.actor.active = false;
                enemy.motion = PlatformerEnemyMotion::Defeated;
                continue;
            }
            enemy.actor.x += enemy.actor.vx * dt;
            if (enemy.actor.x + enemy.width < cameraX_) {
                enemy.actor.active = false;
                enemy.motion = PlatformerEnemyMotion::Defeated;
            }
            continue;
        }

        if (campaignMode_ && enemy.motion == PlatformerEnemyMotion::Walking &&
            enemy.type == PlatformerEnemyType::Bowser) {
            if (phase_ == PlatformerPhase::CastleBridge) {
                updateBowserCallbacks(enemy);
            } else {
                updateBowserBehavior(enemy, dt);
            }
            continue;
        }

        if (campaignMode_ && enemy.motion == PlatformerEnemyMotion::Walking &&
            enemy.type == PlatformerEnemyType::LavaBubble) {
            if (inCamera) {
                enemy.actor.vy +=
                    REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE;
                enemy.actor.y += enemy.actor.vy * dt;
                enemy.actor.vy = std::min(
                    MAX_FALL_SPEED,
                    enemy.actor.vy + enemy.accelerationY * dt);
            }
            ++enemy.attackFrames;
            if (enemy.attackFrames >= 360U) {
                enemy.attackFrames = 0U;
                enemy.actor.y = enemy.originY;
                enemy.actor.vy = -10.0F * REFERENCE_VELOCITY_SCALE;
                enemy.accelerationY =
                    -0.40F * REFERENCE_VELOCITY_SCALE *
                    REFERENCE_TICKS_PER_SECOND;
            }
            enemy.stateMs = enemy.attackFrames;
            continue;
        }

        if (!inCamera) {
            if (enemy.type == PlatformerEnemyType::HammerBro) {
                updateHammerBroCallbacks(enemy);
            }
            // Reference CSV enemies freeze offscreen and can reappear after an
            // 8-4 loop. Only shells and Lakitu's spawned Spinies carry the
            // reference DestroyOutsideCameraComponent here.
            if (enemy.motion == PlatformerEnemyMotion::ShellIdle ||
                enemy.motion == PlatformerEnemyMotion::ShellSliding ||
                (enemy.type == PlatformerEnemyType::Spiny &&
                 enemy.sourceTileId >= 500U)) {
                enemy.actor.active = false;
                enemy.motion = PlatformerEnemyMotion::Defeated;
            }
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
        if (!campaignEnemyUsesGravity(enemy.type)) {
            if (enemy.type == PlatformerEnemyType::BulletBill ||
                enemy.type == PlatformerEnemyType::CheepCheep ||
                enemy.type == PlatformerEnemyType::Blooper ||
                enemy.type == PlatformerEnemyType::Lakitu) {
                enemy.actor.y += enemy.actor.vy * dt;
            }
            if (campaignMode_ &&
                (enemy.actor.x + enemy.width < cameraX_ - 64.0F ||
                 enemy.actor.x > cameraX_ + VIEWPORT_WIDTH + 96.0F)) {
                enemy.actor.active = false;
                enemy.motion = PlatformerEnemyMotion::Defeated;
            }
            continue;
        }
        enemy.actor.vy = std::min(enemy.actor.vy + GRAVITY * dt,
                                  MAX_FALL_SPEED);
        const float nextY = enemy.actor.y + enemy.actor.vy * dt;
        if (!rectHitsSolid(enemy.actor.x, nextY, enemy.width, enemy.height)) {
            enemy.actor.y = nextY;
            enemy.actor.vy = std::min(
                enemy.actor.vy + enemy.accelerationY * dt,
                MAX_FALL_SPEED);
            if (enemy.type == PlatformerEnemyType::Koopa &&
                enemy.motion == PlatformerEnemyMotion::Walking) {
                enemy.actor.vx = -enemy.actor.vx;
            }
            if (enemy.type == PlatformerEnemyType::HammerBro) {
                updateHammerBroBehavior(enemy);
            }
            continue;
        }
        if (campaignMode_) {
            const int32_t row = static_cast<int32_t>(std::floor(
                (nextY + enemy.height - EPSILON) / TILE_SIZE));
            enemy.actor.y = static_cast<float>(row * TILE_SIZE) - enemy.height;
            enemy.actor.vy = 0.0F;
            enemy.accelerationY = 0.0F;
            if (enemy.type == PlatformerEnemyType::KoopaParatroopa) {
                enemy.actor.vy = -8.0F * REFERENCE_VELOCITY_SCALE;
                enemy.accelerationY =
                    -0.22F * REFERENCE_VELOCITY_SCALE *
                    REFERENCE_TICKS_PER_SECOND;
            }
            if (enemy.type == PlatformerEnemyType::Spiny &&
                enemy.sourceTileId == 500U) {
                enemy.sourceTileId = 502U;
                enemy.bornFrame = logicFrame_;
            }
            if (enemy.actor.x + enemy.width < cameraX_ - 64.0F ||
                enemy.actor.y > cameraY_ + VIEWPORT_WIDTH) {
                enemy.actor.active = false;
                enemy.motion = PlatformerEnemyMotion::Defeated;
            }
            if (enemy.actor.active &&
                enemy.type == PlatformerEnemyType::HammerBro) {
                updateHammerBroBehavior(enemy);
            }
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
                enemy.accelerationY = 0.0F;
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
        if (enemy.actor.active &&
            enemy.type == PlatformerEnemyType::HammerBro) {
            enemy.facingLeft = player_.x > enemy.actor.x;
            updateHammerBroBehavior(enemy);
        }
    }
}

void PlatformerEngine::updateHammerBroBehavior(EnemyActor& enemy) {
    enemy.facingLeft = player_.x > enemy.actor.x;
    ++enemy.attackFrames;
    ++enemy.jumpFrames;
    ++enemy.moveFrames;

    if (enemy.attackFrames == 120U) {
        enemy.callbackFrames = 30U;
        enemy.heldHammerY = enemy.actor.y;
        if (enemy.jumpFrames >= 240U) {
            // DelayedCommand(0.75) is first executed later in this same frame.
            enemy.stateMs = 45U;
        }
    }
    if (enemy.moveFrames >= 180U) {
        enemy.actor.vx = -enemy.actor.vx;
        enemy.moveFrames = 0U;
    }

    updateHammerBroCallbacks(enemy);
}

void PlatformerEngine::updateHammerBroCallbacks(EnemyActor& enemy) {
    // CallbackSystem runs after EnemySystem. A callback created above consumes
    // its first frame immediately, while the delayed jump does the same in the
    // command scheduler after the world tick.
    if (enemy.callbackFrames > 0U) {
        --enemy.callbackFrames;
        if (enemy.callbackFrames == 0U) {
            const bool right = enemy.facingLeft;
            spawnEnemyHazard(
                PlatformerEnemyHazardKind::Hammer,
                right ? enemy.actor.x + enemy.width
                      : enemy.actor.x - TILE_SIZE,
                enemy.actor.y - TILE_SIZE,
                right ? 90.0F : -90.0F,
                -6.0F * REFERENCE_VELOCITY_SCALE);
            enemy.attackFrames = 0U;
        }
    }
    updateQueuedEnemyCommands(enemy);
}

void PlatformerEngine::updateQueuedEnemyCommands(EnemyActor& enemy) {
    if (enemy.type == PlatformerEnemyType::Lakitu &&
        enemy.callbackFrames > 0U) {
        --enemy.callbackFrames;
        if (enemy.callbackFrames == 0U) {
            enemy.actor.y -= TILE_SIZE;
            enemy.height = TILE_SIZE * 2.0F;
            spawnSpiny(enemy);
        }
    }
    if (enemy.type == PlatformerEnemyType::HammerBro &&
        enemy.stateMs > 0U) {
        --enemy.stateMs;
        if (enemy.stateMs == 0U) {
            enemy.actor.vy = -10.0F * REFERENCE_VELOCITY_SCALE;
            enemy.jumpFrames = 0U;
        }
    }
}

void PlatformerEngine::updatePausedCommands() {
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        if (!enemy.actor.active) {
            continue;
        }
        updateQueuedEnemyCommands(enemy);
    }
}

uint32_t PlatformerEngine::nextRandom() {
    uint32_t value = randomState_;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    randomState_ = value;
    return value;
}

void PlatformerEngine::updateBowserBehavior(EnemyActor& enemy, float dt) {
    const bool inCamera =
        enemy.actor.x + enemy.width >= cameraX_ &&
        enemy.actor.x <= cameraX_ + VIEWPORT_WIDTH &&
        enemy.actor.y + enemy.height >= cameraY_ &&
        enemy.actor.y <= cameraY_ + 15.0F * TILE_SIZE;
    if (!inCamera) {
        updateBowserCallbacks(enemy);
        return;
    }

    enemy.actor.vy = std::min(
        MAX_FALL_SPEED,
        enemy.actor.vy + REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE);
    const float nextX = enemy.actor.x + enemy.actor.vx * dt;
    if (!rectHitsSolid(nextX, enemy.actor.y, enemy.width, enemy.height)) {
        enemy.actor.x = nextX;
    } else {
        enemy.actor.vx = -enemy.actor.vx;
    }
    const float nextY = enemy.actor.y + enemy.actor.vy * dt;
    if (!rectHitsSolid(enemy.actor.x, nextY, enemy.width, enemy.height)) {
        enemy.actor.y = nextY;
        enemy.actor.vy = std::min(
            MAX_FALL_SPEED, enemy.actor.vy + enemy.accelerationY * dt);
    } else {
        const int32_t row = static_cast<int32_t>(std::floor(
            (nextY + enemy.height - EPSILON) / TILE_SIZE));
        enemy.actor.y = static_cast<float>(row * TILE_SIZE) - enemy.height;
        enemy.actor.vy = 0.0F;
        enemy.accelerationY = 0.0F;
    }

    const bool flipped = player_.x > enemy.actor.x;
    if (flipped != enemy.facingLeft) {
        enemy.bowserMoveDirection =
            enemy.bowserMoveDirection == -1 ? 1 : -1;
        enemy.facingLeft = flipped;
    }

    ++enemy.moveFrames;
    ++enemy.stateMs;
    ++enemy.jumpFrames;
    ++enemy.attackFrames;

    const auto move = [&enemy]() {
        enemy.actor.vx = REFERENCE_VELOCITY_SCALE;
        enemy.bowserMoveDirection =
            enemy.bowserMoveDirection == -1 ? 1 : -1;
        enemy.moveFrames = 0U;
    };
    const auto stop = [&enemy]() {
        enemy.actor.vx = 0.0F;
        enemy.stateMs = 0U;
    };
    const auto jump = [&enemy]() {
        enemy.actor.vy = -5.0F * REFERENCE_VELOCITY_SCALE;
        enemy.accelerationY =
            -0.35F * REFERENCE_VELOCITY_SCALE *
            REFERENCE_TICKS_PER_SECOND;
        enemy.jumpFrames = 0U;
    };

    switch (enemy.behaviorState) {
        case 0U:
            if (enemy.stateMs >= 120U) {
                move();
                jump();
                enemy.behaviorState = 1U;
            }
            break;
        case 1U:
            if (enemy.moveFrames >= 180U) {
                stop();
                enemy.behaviorState = 2U;
            }
            break;
        case 2U:
            if (enemy.stateMs >= 120U) {
                move();
                jump();
                enemy.behaviorState = 3U;
            }
            break;
        case 3U:
            if (enemy.moveFrames >= 180U) {
                stop();
                enemy.behaviorState = 4U;
            }
            break;
        case 4U:
            if (enemy.stateMs >= 120U) {
                move();
                enemy.behaviorState = 5U;
            }
            break;
        case 5U:
            if (enemy.moveFrames >= 180U) {
                stop();
                enemy.behaviorState = 0U;
            }
            break;
        default:
            enemy.behaviorState = 0U;
            break;
    }

    if (enemy.attackFrames >= 120U) {
        enemy.attackFrames = 0U;
        const bool fireAttack = (nextRandom() & 1U) == 0U;
        const uint8_t hammerAmount =
            static_cast<uint8_t>(nextRandom() % 5U + 6U);
        if (fireAttack) {
            enemy.fireCallbackFrames = 120U;
        } else {
            enemy.hammerBurstRemaining =
                static_cast<uint8_t>(hammerAmount - 1U);
            enemy.hammerBurstFrames = 4U;
        }
    }

    updateBowserCallbacks(enemy);
}

void PlatformerEngine::updateBowserCallbacks(EnemyActor& enemy) {
    if (enemy.fireCallbackFrames > 0U) {
        --enemy.fireCallbackFrames;
        if (enemy.fireCallbackFrames == 0U) {
            const bool right = enemy.facingLeft;
            if (spawnEnemyHazard(
                PlatformerEnemyHazardKind::BowserFire,
                right ? enemy.actor.x + enemy.width
                      : enemy.actor.x - 24.0F,
                enemy.actor.y + 2.0F,
                right ? 3.0F * REFERENCE_VELOCITY_SCALE
                      : -3.0F * REFERENCE_VELOCITY_SCALE,
                0.0F)) {
                queueEvent(PlatformerEventType::BowserFire);
            }
        }
    }

    if (enemy.hammerBurstRemaining > 0U && enemy.hammerBurstFrames > 0U) {
        --enemy.hammerBurstFrames;
        if (enemy.hammerBurstFrames == 0U) {
            const float randomX =
                static_cast<float>(nextRandom()) / 4294967295.0F;
            const float randomY =
                static_cast<float>(nextRandom()) / 4294967295.0F;
            float vx = -(randomX + 2.25F) * REFERENCE_VELOCITY_SCALE;
            if (enemy.facingLeft) {
                vx = -vx;
            }
            spawnEnemyHazard(
                PlatformerEnemyHazardKind::Hammer,
                enemy.facingLeft ? enemy.actor.x + enemy.width
                                 : enemy.actor.x - TILE_SIZE,
                enemy.actor.y, vx,
                -(randomY * 0.5F + 6.0F) *
                    REFERENCE_VELOCITY_SCALE,
                -0.35F * REFERENCE_VELOCITY_SCALE *
                    REFERENCE_TICKS_PER_SECOND);
            --enemy.hammerBurstRemaining;
            if (enemy.hammerBurstRemaining > 0U) {
                enemy.hammerBurstFrames = 4U;
            }
        }
    }
}

void PlatformerEngine::spawnSpiny(const EnemyActor& lakitu) {
    uint8_t slot = MAX_ENEMIES;
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        if (!enemies_[index].actor.active) {
            slot = index;
            break;
        }
    }
    if (slot == MAX_ENEMIES && enemyCount_ < MAX_ENEMIES) {
        slot = enemyCount_++;
    }
    if (slot >= MAX_ENEMIES) {
        return;
    }
    EnemyActor& spiny = enemies_[slot];
    spiny = EnemyActor{};
    spiny.type = PlatformerEnemyType::Spiny;
    spiny.sourceTileId = 500U;
    spiny.bornFrame = logicFrame_;
    spiny.actor.x = lakitu.actor.x;
    spiny.actor.y = lakitu.actor.y;
    spiny.actor.vx = (lakitu.facingLeft ? 2.5F : -2.5F) *
                     REFERENCE_VELOCITY_SCALE;
    spiny.actor.vy = 0.0F;
    spiny.originX = spiny.actor.x;
    spiny.originY = spiny.actor.y;
    spiny.actor.active = true;
    spiny.spawned = true;
}

bool PlatformerEngine::spawnEnemyHazard(PlatformerEnemyHazardKind kind,
                                         float x, float y, float vx,
                                         float vy, float accelerationY) {
    uint8_t slot = MAX_ENEMY_HAZARDS;
    for (uint8_t index = 0; index < enemyHazardCount_; ++index) {
        if (!enemyHazards_[index].active) {
            slot = index;
            break;
        }
    }
    if (slot == MAX_ENEMY_HAZARDS &&
        enemyHazardCount_ < MAX_ENEMY_HAZARDS) {
        slot = enemyHazardCount_++;
    }
    if (slot >= MAX_ENEMY_HAZARDS) {
        return false;
    }
    PlatformerEnemyHazardState& hazard = enemyHazards_[slot];
    hazard = PlatformerEnemyHazardState{};
    hazard.kind = kind;
    hazard.x = x;
    hazard.y = y;
    hazard.vx = vx;
    hazard.vy = vy;
    hazard.accelerationY = accelerationY;
    hazard.bornFrame = logicFrame_;
    hazard.sourceTileId =
        kind == PlatformerEnemyHazardKind::Hammer ? 60U : 470U;
    hazard.active = true;
    return true;
}

void PlatformerEngine::updateEnemyHazards(float dt, uint16_t dtMs) {
    for (uint8_t index = 0; index < enemyHazardCount_; ++index) {
        PlatformerEnemyHazardState& hazard = enemyHazards_[index];
        if (!hazard.active) {
            continue;
        }
        if (hazard.bornFrame == logicFrame_) {
            hazard.ageMs = dtMs;
            continue;
        }
        hazard.ageMs = static_cast<uint16_t>(std::min<uint32_t>(
            65535U, static_cast<uint32_t>(hazard.ageMs) + dtMs));
        const float width =
            hazard.kind == PlatformerEnemyHazardKind::BowserFire ? 24.0F
                                                                 : 16.0F;
        const float height =
            hazard.kind == PlatformerEnemyHazardKind::BowserFire ? 8.0F
                                                                 : 16.0F;
        if (hazard.x + width < cameraX_ ||
            hazard.x > cameraX_ + VIEWPORT_WIDTH ||
            hazard.y + height < cameraY_ ||
            hazard.y > cameraY_ + VIEWPORT_HEIGHT) {
            hazard.active = false;
            continue;
        }
        if (hazard.kind == PlatformerEnemyHazardKind::BowserFire) {
            platformerAdvanceReferenceAnimation(
                hazard.animationFrame, hazard.animationTimer, 4U, 2U);
        }
        if (hazard.kind == PlatformerEnemyHazardKind::Hammer) {
            hazard.vy = std::min(
                MAX_FALL_SPEED,
                hazard.vy + REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE);
        }
        hazard.x += hazard.vx * dt;
        hazard.y += hazard.vy * dt;
        hazard.vy = std::min(
            MAX_FALL_SPEED, hazard.vy + hazard.accelerationY * dt);
        if (phase_ == PlatformerPhase::Running &&
            overlaps(player_.x, player_.y, playerWidth(), playerHeight(),
                     hazard.x, hazard.y, width, height) &&
            powerTransition_ == PlatformerPowerTransition::None &&
            hurtInvincibleFrames_ == 0U && !starProtectedThisFrame_) {
            hurtPlayer();
        }
    }
}

void PlatformerEngine::updateEnemyActivationCallbacks() {
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        if (!enemy.activationSoundPending) {
            continue;
        }
        if (!enemy.actor.active) {
            enemy.activationSoundPending = false;
            continue;
        }
        const bool inCamera =
            enemy.actor.x + enemy.width >= cameraX_ &&
            enemy.actor.x <= cameraX_ + VIEWPORT_WIDTH &&
            enemy.actor.y + enemy.height >= cameraY_ &&
            enemy.actor.y <= cameraY_ + VIEWPORT_HEIGHT;
        if (inCamera) {
            enemy.activationSoundPending = false;
            queueEvent(PlatformerEventType::CannonFired);
        }
    }
}

void PlatformerEngine::loadCampaignEnemies() {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (level == nullptr) {
        return;
    }
    for (uint8_t row = 0U; row < level->height; ++row) {
        for (uint16_t column = 0U; column < level->width; ++column) {
            const uint16_t sourceId = platformerCampaignTileAt(
                *level, PlatformerMapLayer::Enemies, column, row);
            if (!platformerEnemySourceCreatesEntity(sourceId)) {
                continue;
            }
            const uint16_t reference =
                PLATFORMER_ENEMY_REFERENCE_IDS[sourceId];
            PlatformerEnemyType type = PlatformerEnemyType::Goomba;
            if (!campaignEnemyType(reference, type)) {
                continue;
            }
            float spawnX = static_cast<float>(column * TILE_SIZE);
            float spawnY = static_cast<float>(row * TILE_SIZE);
            if (reference == 39U || reference == 71U) {
                spawnX += TILE_SIZE * 0.5F;
            }
            bool flyingCheep = false;
            if (reference == 498U) {
                const uint16_t background = platformerCampaignTileAt(
                    *level, PlatformerMapLayer::Background, column, row);
                const bool underwaterBackground =
                    background < PLATFORMER_BLOCK_TILE_COUNT &&
                    PLATFORMER_BLOCK_REFERENCE_IDS[background] == 186U;
                flyingCheep = !underwaterBackground;
                if (flyingCheep) {
                    spawnY += TILE_SIZE;
                }
            }
            if (type == PlatformerEnemyType::BulletBill) {
                // Pre-placed Bullet Bills have DestroyOutsideCameraComponent
                // in the reference and disappear in the first PhysicsSystem
                // tick unless they start inside the initial viewport.
                const bool inInitialCamera =
                    spawnX + TILE_SIZE >= cameraX_ &&
                    spawnX <= cameraX_ + VIEWPORT_WIDTH &&
                    spawnY + TILE_SIZE >= cameraY_ &&
                    spawnY <= cameraY_ + VIEWPORT_HEIGHT;
                if (!inInitialCamera) {
                    continue;
                }
            }
            if (enemyCount_ >= MAX_LEVEL_ENEMIES) {
                return;
            }

            EnemyActor& enemy = enemies_[enemyCount_++];
            enemy = EnemyActor{};
            enemy.type = type;
            enemy.sourceTileId = sourceId;
            enemy.bornFrame = logicFrame_;
            enemy.actor.x = spawnX;
            enemy.actor.y = spawnY;
            enemy.actor.vx = -ENEMY_SPEED;
            enemy.actor.active = true;
            enemy.spawned = true;
            enemy.flyingCheep = flyingCheep;
            if (enemy.type == PlatformerEnemyType::Koopa ||
                enemy.type == PlatformerEnemyType::KoopaParatroopa) {
                enemy.actor.y += 8.0F;
                enemy.height = 24.0F;
                enemy.actor.vx = -KOOPA_SPEED;
            } else if (enemy.type == PlatformerEnemyType::HammerBro ||
                       enemy.type == PlatformerEnemyType::Bowser) {
                enemy.height = 32.0F;
                if (enemy.type == PlatformerEnemyType::HammerBro) {
                    enemy.actor.vx = 2.0F * REFERENCE_VELOCITY_SCALE;
                } else {
                    enemy.actor.vx = 0.0F;
                    enemy.width = 32.0F;
                    enemy.health = 5U;
                    enemy.accelerationY =
                        -0.30F * REFERENCE_VELOCITY_SCALE *
                        REFERENCE_TICKS_PER_SECOND;
                }
            } else if (enemy.type == PlatformerEnemyType::PiranhaPlant ||
                       enemy.type == PlatformerEnemyType::LavaBubble) {
                enemy.actor.vx = 0.0F;
                if (enemy.type == PlatformerEnemyType::PiranhaPlant) {
                    enemy.actor.x += 8.0F;
                    enemy.height = 32.0F;
                } else {
                    enemy.actor.y += TILE_SIZE;
                }
            } else if (enemy.type == PlatformerEnemyType::BulletBill) {
                enemy.actor.vx = -3.0F * REFERENCE_VELOCITY_SCALE;
                enemy.activationSoundPending = true;
            } else if (enemy.type == PlatformerEnemyType::CheepCheep ||
                       enemy.type == PlatformerEnemyType::Blooper ||
                       enemy.type == PlatformerEnemyType::Lakitu) {
                enemy.actor.vx =
                    enemy.type == PlatformerEnemyType::CheepCheep
                        ? -ENEMY_SPEED
                        : 0.0F;
                enemy.actor.vy = 0.0F;
                if (enemy.type != PlatformerEnemyType::CheepCheep) {
                    enemy.height = 32.0F;
                }
                if (enemy.type == PlatformerEnemyType::Blooper) {
                    enemy.accelerationY =
                        -0.47480F * REFERENCE_VELOCITY_SCALE *
                        REFERENCE_TICKS_PER_SECOND;
                }
            }
            enemy.originX = enemy.actor.x;
            enemy.originY = enemy.actor.y;
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
        const bool inCamera =
            powerup.x + TILE_SIZE >= cameraX_ &&
            powerup.x <= cameraX_ + VIEWPORT_WIDTH &&
            powerup.y + TILE_SIZE >= cameraY_ &&
            powerup.y <= cameraY_ + VIEWPORT_HEIGHT;
        if (inCamera &&
            (powerup.kind == PlatformerPowerupKind::FireFlower ||
             powerup.kind == PlatformerPowerupKind::Star)) {
            platformerAdvanceReferenceAnimation(
                powerup.animationFrame, powerup.animationTimer, 8U, 4U);
        }
        if (powerup.bornFrame == logicFrame_) {
            continue;
        }
        if (!inCamera) {
            if (powerup.x + TILE_SIZE < cameraX_) {
                powerup.active = false;
            }
            continue;
        }
        powerup.ageMs = static_cast<uint16_t>(
            std::min<uint32_t>(65535U, powerup.ageMs + dtMs));
        powerup.stateFrames = static_cast<uint16_t>(
            std::min<uint32_t>(65535U, powerup.stateFrames + 1U));
        if (powerup.state == PlatformerPowerupState::Emerging) {
            powerup.y += powerup.vy * dt;
            // The reference starts the collectible inside the block at vy=-1
            // and enables collisions only once its 32px body is fully above it.
            // With 16px PGOS tiles that is the 33rd 60Hz logic frame.
            if (powerup.stateFrames >= 33U) {
                powerup.state = powerup.kind == PlatformerPowerupKind::FireFlower
                                    ? PlatformerPowerupState::Resting
                                    : PlatformerPowerupState::Moving;
                powerup.vx = powerup.kind == PlatformerPowerupKind::FireFlower
                                 ? 0.0F
                                 : POWERUP_SPEED;
                powerup.vy = 0.0F;
                powerup.stateFrames = 0U;
            }
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
            if (campaignMode_) {
                const int32_t firstColumn = static_cast<int32_t>(
                    std::floor(powerup.x / TILE_SIZE));
                const int32_t lastColumn = static_cast<int32_t>(std::floor(
                    (powerup.x + TILE_SIZE - EPSILON) / TILE_SIZE));
                const int32_t firstRow = static_cast<int32_t>(
                    std::floor((powerup.y + TILE_SIZE) / TILE_SIZE));
                const int32_t lastRow = static_cast<int32_t>(std::floor(
                    (nextY + TILE_SIZE) / TILE_SIZE));
                for (int32_t row = firstRow; row <= lastRow; ++row) {
                    for (int32_t column = firstColumn; column <= lastColumn;
                         ++column) {
                        if (column < 0 || row < 0 || row > 255 ||
                            !levelRuntime_.isSolid(
                                static_cast<uint16_t>(column),
                                static_cast<uint8_t>(row))) {
                            continue;
                        }
                        considerTop(static_cast<float>(column * TILE_SIZE),
                                    static_cast<float>(row * TILE_SIZE),
                                    TILE_SIZE);
                    }
                }
            } else {
                for (uint8_t solidIndex = 0;
                     solidIndex < PLATFORMER_LEVEL_1_1.solidCount;
                     ++solidIndex) {
                    const auto& solid =
                        PLATFORMER_LEVEL_1_1.solids[solidIndex];
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
            }
            if (foundLanding) {
                powerup.y = landingY;
            }
            if (powerup.kind == PlatformerPowerupKind::Star) {
                powerup.vy = -10.0F * REFERENCE_VELOCITY_SCALE;
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
        if (effect.kind == PlatformerEffectKind::RisingCoin) {
            const bool inCamera =
                effect.x + TILE_SIZE >= cameraX_ &&
                effect.x <= cameraX_ + VIEWPORT_WIDTH &&
                effect.y + TILE_SIZE >= cameraY_ &&
                effect.y <= cameraY_ + VIEWPORT_HEIGHT;
            if (inCamera) {
                platformerAdvanceReferenceAnimation(
                    effect.animationFrame, effect.animationTimer, 8U, 4U);
            }
            effect.vy += REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE;
            effect.y += effect.vy * dt;
            effect.vy = std::min(
                MAX_FALL_SPEED,
                effect.vy + 0.3F * REFERENCE_VELOCITY_SCALE);
            if (effect.vy >= 0.0F && effect.y + TILE_SIZE >= effect.value) {
                effect.active = false;
            }
        } else if (effect.kind == PlatformerEffectKind::BrickPiece) {
            if (effect.x + TILE_SIZE < cameraX_ ||
                effect.x > cameraX_ + VIEWPORT_WIDTH ||
                effect.y + TILE_SIZE < cameraY_ ||
                effect.y > cameraY_ + VIEWPORT_HEIGHT) {
                effect.active = false;
                continue;
            }
            effect.vy = std::min(
                MAX_FALL_SPEED,
                effect.vy +
                    REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE);
            effect.x += effect.vx * dt;
            effect.y += effect.vy * dt;
        } else if ((effect.kind == PlatformerEffectKind::Score ||
                    effect.kind == PlatformerEffectKind::OneUp) &&
                   logicFrame_ - effect.bornFrame >= 35U) {
            effect.active = false;
        } else {
            effect.x += effect.vx * dt;
            effect.y += effect.vy * dt;
        }
    }
}

void PlatformerEngine::updateProjectiles(float dt, uint16_t dtMs) {
    for (uint8_t index = 0; index < projectileCount_; ++index) {
        PlatformerProjectile& projectile = projectiles_[index];
        if (!projectile.active) {
            continue;
        }
        if (projectile.bornFrame == logicFrame_) {
            continue;
        }
        projectile.ageMs = static_cast<uint16_t>(
            std::min<uint32_t>(65535U, projectile.ageMs + dtMs));
        if (projectile.exploding) {
            if (projectile.explosionFrames > 0U) {
                --projectile.explosionFrames;
            }
            if (projectile.explosionFrames == 0U) {
                projectile.active = false;
            }
            continue;
        }
        if (projectile.x + 8.0F < cameraX_ ||
            projectile.x > cameraX_ + VIEWPORT_WIDTH ||
            projectile.y + 8.0F < cameraY_ ||
            projectile.y > cameraY_ + VIEWPORT_HEIGHT) {
            projectile.active = false;
            continue;
        }
        projectile.vy = std::min(projectile.vy + GRAVITY * dt,
                                 MAX_FALL_SPEED);
        const float previousY = projectile.y;
        projectile.x += projectile.vx * dt;
        projectile.y += projectile.vy * dt;

        const bool falling = projectile.vy >= 0.0F;
        const float verticalInset =
            falling ? TILE_COLLISION_ROUNDNESS * 0.5F
                    : TILE_COLLISION_ROUNDNESS;
        const float predictedY = projectile.y + projectile.vy * dt;
        float verticalEdge = falling ? 1000000.0F : -1000000.0F;
        bool verticalCollision = false;
        const auto considerVertical = [&](float solidX, float solidY,
                                          float solidWidth,
                                          float solidHeight) {
            if (!overlaps(projectile.x + verticalInset, predictedY,
                          8.0F - verticalInset * 2.0F, 8.0F,
                          solidX + verticalInset, solidY,
                          solidWidth - verticalInset * 2.0F,
                          solidHeight)) {
                return;
            }
            if (falling && previousY + 8.0F <= solidY + EPSILON &&
                solidY < verticalEdge) {
                verticalEdge = solidY;
                verticalCollision = true;
            } else if (!falling &&
                       previousY >= solidY + solidHeight - EPSILON &&
                       solidY + solidHeight > verticalEdge) {
                verticalEdge = solidY + solidHeight;
                verticalCollision = true;
            }
        };

        if (campaignMode_) {
            const PlatformerCampaignLevel* level = levelRuntime_.level();
            if (level != nullptr) {
                const int32_t firstColumn = static_cast<int32_t>(std::floor(
                    (projectile.x + verticalInset) / TILE_SIZE));
                const int32_t lastColumn = static_cast<int32_t>(std::floor(
                    (projectile.x + 8.0F - verticalInset - EPSILON) /
                    TILE_SIZE));
                const int32_t firstRow = static_cast<int32_t>(
                    std::floor(predictedY / TILE_SIZE));
                const int32_t lastRow = static_cast<int32_t>(std::floor(
                    (predictedY + 8.0F - EPSILON) / TILE_SIZE));
                for (int32_t row = firstRow; row <= lastRow; ++row) {
                    for (int32_t column = firstColumn;
                         column <= lastColumn; ++column) {
                        if (column < 0 || column >= level->width || row < 0 ||
                            row >= level->height ||
                            !levelRuntime_.isSolid(
                                static_cast<uint16_t>(column),
                                static_cast<uint8_t>(row))) {
                            continue;
                        }
                        considerVertical(column * TILE_SIZE,
                                         row * TILE_SIZE, TILE_SIZE,
                                         TILE_SIZE);
                    }
                }
            }
        } else {
            for (uint8_t solidIndex = 0;
                 solidIndex < PLATFORMER_LEVEL_1_1.solidCount;
                 ++solidIndex) {
                const auto& solid = PLATFORMER_LEVEL_1_1.solids[solidIndex];
                considerVertical(static_cast<float>(solid.x),
                                 static_cast<float>(solid.y),
                                 static_cast<float>(solid.width),
                                 static_cast<float>(solid.height));
            }
            for (uint8_t boxIndex = 0; boxIndex < boxCount_; ++boxIndex) {
                const PlatformerBox& box = boxes_[boxIndex];
                if (box.visible) {
                    considerVertical(static_cast<float>(box.x),
                                     static_cast<float>(box.y), TILE_SIZE,
                                     TILE_SIZE);
                }
            }
        }

        if (verticalCollision) {
            projectile.y = falling ? verticalEdge - 8.0F : verticalEdge;
            projectile.vy = falling
                                ? -4.0F * REFERENCE_VELOCITY_SCALE
                                : 0.0F;
        }

        const float predictedX = projectile.x + projectile.vx * dt;
        if (rectHitsSolid(predictedX,
                          projectile.y + TILE_COLLISION_ROUNDNESS,
                          8.0F, 8.0F - TILE_COLLISION_ROUNDNESS * 2.0F)) {
            projectile.exploding = true;
            projectile.explosionFrames = 4U;
            projectile.ageMs = 0U;
            queueEvent(PlatformerEventType::FireballHit);
            continue;
        }
        for (uint8_t enemyIndex = 0; enemyIndex < enemyCount_; ++enemyIndex) {
            EnemyActor& enemy = enemies_[enemyIndex];
            float enemyX = 0.0F;
            float enemyY = 0.0F;
            float enemyWidth = 0.0F;
            float enemyHeight = 0.0F;
            enemyCollisionBounds(enemy, enemyX, enemyY, enemyWidth,
                                 enemyHeight);
            if (!enemy.actor.active || isDefeatedParticle(enemy.motion) ||
                !overlaps(projectile.x, projectile.y, 8.0F, 8.0F,
                          enemyX, enemyY, enemyWidth, enemyHeight)) {
                continue;
            }
            if (enemy.type == PlatformerEnemyType::LavaBubble ||
                enemy.type == PlatformerEnemyType::BulletBill) {
                continue;
            }
            if (enemy.type == PlatformerEnemyType::Bowser) {
                continue;
            }
            defeatEnemy(enemy, 0, true);
            projectile.active = false;
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
                if (playerPower_ == PlatformerPlayerPower::Small &&
                    powerTransition_ == PlatformerPowerTransition::None) {
                    player_.y -= BIG_PLAYER_HEIGHT - PLAYER_HEIGHT;
                    powerTransition_ = PlatformerPowerTransition::Grow;
                    powerTransitionFrames_ = 44U;
                    powerTransitionElapsedFrames_ = 0U;
                }
                addScore(1000, player_.x, player_.y);
                spawnEffect(PlatformerEffectKind::Score, player_.x,
                            player_.y, 0.0F, -30.0F, 1000U);
                break;
            case PlatformerPowerupKind::FireFlower:
                if (playerPower_ == PlatformerPlayerPower::Small) {
                    player_.y -= BIG_PLAYER_HEIGHT - PLAYER_HEIGHT;
                    powerTransition_ = PlatformerPowerTransition::Grow;
                    powerTransitionFrames_ = 44U;
                    powerTransitionElapsedFrames_ = 0U;
                } else if (playerPower_ == PlatformerPlayerPower::Big) {
                    powerTransition_ = PlatformerPowerTransition::Fire;
                    powerTransitionFrames_ = 59U;
                    powerTransitionElapsedFrames_ = 0U;
                }
                addScore(1000, player_.x, player_.y);
                spawnEffect(PlatformerEffectKind::Score, player_.x,
                            player_.y, 0.0F, -30.0F, 1000U);
                break;
            case PlatformerPowerupKind::Star:
                starInvincibleFrames_ = 599U;
                starBlinkFrames_ = 600U;
                break;
            case PlatformerPowerupKind::OneUp:
                ++lives_;
                spawnEffect(PlatformerEffectKind::OneUp, player_.x,
                            player_.y - 2.0F, 0.0F, -30.0F);
                queueEvent(PlatformerEventType::OneUp, lives_);
                break;
        }
        queueEvent(PlatformerEventType::PowerupCollected,
                   static_cast<uint16_t>(powerup.kind));
    }
}

void PlatformerEngine::collectMapCoins() {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (level == nullptr) {
        return;
    }
    const int32_t firstColumn = std::max<int32_t>(
        0, static_cast<int32_t>(std::floor(player_.x / TILE_SIZE)));
    const int32_t lastColumn = std::min<int32_t>(
        level->width - 1,
        static_cast<int32_t>(std::floor(
            (player_.x + playerWidth() - EPSILON) / TILE_SIZE)));
    const int32_t firstRow = std::max<int32_t>(
        0, static_cast<int32_t>(std::floor(player_.y / TILE_SIZE)));
    const int32_t lastRow = std::min<int32_t>(
        level->height - 1,
        static_cast<int32_t>(std::floor(
            (player_.y + playerHeight() - EPSILON) / TILE_SIZE)));
    for (int32_t row = firstRow; row <= lastRow; ++row) {
        for (int32_t column = firstColumn; column <= lastColumn; ++column) {
            if (!levelRuntime_.collectCoin(static_cast<uint16_t>(column),
                                           static_cast<uint8_t>(row))) {
                continue;
            }
            ++coinsCollected_;
            addScore(100, static_cast<float>(column * TILE_SIZE),
                     static_cast<float>(row * TILE_SIZE));
            queueEvent(PlatformerEventType::CoinCollected);
        }
    }
}

void PlatformerEngine::checkBoxCollision(float previousY,
                                          float verticalVelocity) {
    if (verticalVelocity >= 0.0F) {
        return;
    }

    if (campaignMode_) {
        const float horizontalInset = TILE_COLLISION_ROUNDNESS;
        const int32_t firstColumn = static_cast<int32_t>(
            std::floor((player_.x + horizontalInset) / TILE_SIZE));
        const int32_t lastColumn = static_cast<int32_t>(std::floor(
            (player_.x + playerWidth() - horizontalInset - EPSILON) /
            TILE_SIZE));
        const int32_t firstRow = std::max<int32_t>(
            0, static_cast<int32_t>(std::floor(player_.y / TILE_SIZE)) - 1);
        const int32_t lastRow = static_cast<int32_t>(
            std::floor(previousY / TILE_SIZE));
        int32_t selectedColumn = -1;
        int32_t selectedRow = -1;
        float bestOverlap = 0.0F;
        for (int32_t row = firstRow; row <= lastRow; ++row) {
            const float bottom = static_cast<float>((row + 1) * TILE_SIZE);
            if (previousY < bottom - EPSILON || player_.y > bottom + EPSILON) {
                continue;
            }
            for (int32_t column = firstColumn; column <= lastColumn; ++column) {
                if (column < 0 || row > 255) {
                    continue;
                }
                const PlatformerRuntimeTile tile = levelRuntime_.tile(
                    static_cast<uint16_t>(column), static_cast<uint8_t>(row));
                if (tile.kind != PlatformerRuntimeTileKind::Question &&
                    tile.kind != PlatformerRuntimeTileKind::Brick &&
                    tile.kind != PlatformerRuntimeTileKind::Hidden) {
                    continue;
                }
                if (levelRuntime_.modificationAt(
                        static_cast<uint16_t>(column),
                        static_cast<uint8_t>(row)) != nullptr) {
                    continue;
                }
                const float left = static_cast<float>(column * TILE_SIZE);
                const float overlap =
                    std::min(player_.x + playerWidth(), left + TILE_SIZE) -
                    std::max(player_.x, left);
                const float playerCenter = player_.x + playerWidth() * 0.5F;
                const float centerDistance =
                    std::abs(playerCenter - (left + TILE_SIZE * 0.5F));
                const float selectedCenterDistance =
                    selectedColumn < 0
                        ? static_cast<float>(TILE_SIZE * 2)
                        : std::abs(playerCenter -
                                   (selectedColumn * TILE_SIZE +
                                    TILE_SIZE * 0.5F));
                if (overlap > bestOverlap + EPSILON ||
                    (std::abs(overlap - bestOverlap) <= EPSILON &&
                     centerDistance < selectedCenterDistance)) {
                    bestOverlap = overlap;
                    selectedColumn = column;
                    selectedRow = row;
                }
            }
        }
        if (selectedColumn < 0 || selectedRow < 0) {
            return;
        }

        const uint16_t column = static_cast<uint16_t>(selectedColumn);
        const uint8_t row = static_cast<uint8_t>(selectedRow);
        const float blockX = static_cast<float>(column * TILE_SIZE);
        const float blockY = static_cast<float>(row * TILE_SIZE);
        const PlatformerRuntimeTile source = levelRuntime_.tile(column, row);
        const bool shouldBreak =
            playerPower_ != PlatformerPlayerPower::Small &&
            source.kind == PlatformerRuntimeTileKind::Brick &&
            source.reward == PlatformerRuntimeReward::None;
        const PlatformerBlockHitResult hit =
            levelRuntime_.hitBlock(column, row, false);
        if (!hit.accepted) {
            return;
        }
        player_.y = blockY + TILE_SIZE;
        player_.vy = 10.0F;
        PlatformerBox bumped;
        bumped.x = static_cast<int16_t>(blockX);
        bumped.y = static_cast<int16_t>(blockY);
        bumpEnemiesAbove(bumped);

        if (shouldBreak) {
            queueCampaignBrickBreak(column, row);
            return;
        }
        levelRuntime_.startBlockBump(column, row);
        queueEvent(PlatformerEventType::BlockHit);

        switch (hit.reward) {
            case PlatformerRuntimeReward::Coin:
                ++coinsCollected_;
                addScore(100, blockX, blockY - 8.0F);
                spawnEffect(PlatformerEffectKind::Score, blockX,
                            blockY - 2.0F, 0.0F, -30.0F, 100U);
                spawnEffect(PlatformerEffectKind::RisingCoin,
                            blockX, blockY, 0.0F, -300.0F,
                            static_cast<uint16_t>(blockY));
                queueEvent(PlatformerEventType::CoinCollected);
                break;
            case PlatformerRuntimeReward::Mushroom:
                spawnPowerup(playerPower_ == PlatformerPlayerPower::Small
                                 ? PlatformerPowerupKind::Mushroom
                                 : PlatformerPowerupKind::FireFlower,
                             blockX, blockY);
                break;
            case PlatformerRuntimeReward::Star:
                spawnPowerup(PlatformerPowerupKind::Star, blockX, blockY);
                break;
            case PlatformerRuntimeReward::OneUp:
                spawnPowerup(PlatformerPowerupKind::OneUp, blockX, blockY);
                break;
            case PlatformerRuntimeReward::Vine:
                spawnVine(column, row);
                break;
            case PlatformerRuntimeReward::None:
                break;
        }
        return;
    }

    uint8_t selected = boxCount_;
    float bestOverlap = 0.0F;
    float bestCenterDistance = static_cast<float>(WORLD_WIDTH);
    for (uint8_t index = 0; index < boxCount_; ++index) {
        PlatformerBox& box = boxes_[index];
        bool canHit = box.visible && !box.opened;
        if (!box.visible && !box.opened) {
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
    if (box.opened) {
        return;
    }

    if (box.type == PlatformerObjectType::CoinBrick &&
        box.reward == PlatformerBoxReward::None) {
        if (playerPower_ != PlatformerPlayerPower::Small) {
            breakBrick(index);
            return;
        }
        boxBumpFrames_[index] = 1U;
        queueEvent(PlatformerEventType::BlockHit);
        bumpEnemiesAbove(box);
        return;
    }

    boxBumpFrames_[index] = 1U;
    queueEvent(PlatformerEventType::BlockHit);

    if (box.reward == PlatformerBoxReward::MultiCoin && box.remainingUses > 0) {
        --box.remainingUses;
        ++coinsCollected_;
        addScore(100, box.x, box.y - 8);
        spawnEffect(PlatformerEffectKind::Score, box.x, box.y - 2.0F,
                    0.0F, -30.0F, 100U);
        spawnEffect(PlatformerEffectKind::RisingCoin, box.x, box.y,
                    0.0F, -300.0F, static_cast<uint16_t>(box.y));
        if (box.remainingUses == 0) {
            box.opened = true;
        }
        queueEvent(PlatformerEventType::CoinCollected,
                   box.remainingUses);
        bumpEnemiesAbove(box);
        return;
    }

    box.opened = true;
    switch (box.reward) {
        case PlatformerBoxReward::Coin:
            ++coinsCollected_;
            addScore(100, box.x, box.y - 8);
            spawnEffect(PlatformerEffectKind::Score, box.x,
                        box.y - 2.0F, 0.0F, -30.0F, 100U);
            spawnEffect(PlatformerEffectKind::RisingCoin, box.x, box.y,
                        0.0F, -300.0F,
                        static_cast<uint16_t>(box.y));
            queueEvent(PlatformerEventType::CoinCollected);
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
    bumpEnemiesAbove(box);
}

void PlatformerEngine::breakBrick(uint8_t index) {
    if (index >= boxCount_ || pendingBrickBreak_.active) {
        return;
    }
    PlatformerBox& box = boxes_[index];
    pendingBrickBreak_.legacyBox = index;
    pendingBrickBreak_.bornFrame = logicFrame_;
    pendingBrickBreak_.campaign = false;
    pendingBrickBreak_.active = true;
    bumpEnemiesAbove(box);
}

void PlatformerEngine::queueCampaignBrickBreak(uint16_t column, uint8_t row) {
    if (pendingBrickBreak_.active) {
        return;
    }
    pendingBrickBreak_.column = column;
    pendingBrickBreak_.row = row;
    pendingBrickBreak_.bornFrame = logicFrame_;
    pendingBrickBreak_.campaign = true;
    pendingBrickBreak_.active = true;
}

void PlatformerEngine::completePendingBrickBreak() {
    if (!pendingBrickBreak_.active ||
        pendingBrickBreak_.bornFrame == logicFrame_) {
        return;
    }

    float blockX = 0.0F;
    float blockY = 0.0F;
    if (pendingBrickBreak_.campaign) {
        if (!levelRuntime_.removeTile(pendingBrickBreak_.column,
                                      pendingBrickBreak_.row)) {
            pendingBrickBreak_ = PendingBrickBreak{};
            return;
        }
        blockX = static_cast<float>(pendingBrickBreak_.column * TILE_SIZE);
        blockY = static_cast<float>(pendingBrickBreak_.row * TILE_SIZE);
    } else {
        if (pendingBrickBreak_.legacyBox >= boxCount_) {
            pendingBrickBreak_ = PendingBrickBreak{};
            return;
        }
        PlatformerBox& box = boxes_[pendingBrickBreak_.legacyBox];
        box.opened = true;
        box.visible = false;
        box.remainingUses = 0;
        blockX = static_cast<float>(box.x);
        blockY = static_cast<float>(box.y);
    }
    pendingBrickBreak_ = PendingBrickBreak{};

    spawnEffect(PlatformerEffectKind::BrickPiece, blockX,
                blockY - TILE_SIZE, -240.0F, -60.0F);
    spawnEffect(PlatformerEffectKind::BrickPiece, blockX,
                blockY - TILE_SIZE, 240.0F, -60.0F);
    spawnEffect(PlatformerEffectKind::BrickPiece, blockX,
                blockY, -240.0F, -60.0F);
    spawnEffect(PlatformerEffectKind::BrickPiece, blockX,
                blockY, 240.0F, -60.0F);
    queueEvent(PlatformerEventType::BrickBroken);
}

void PlatformerEngine::bumpEnemiesAbove(const PlatformerBox& box) {
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        float enemyX = 0.0F;
        float enemyY = 0.0F;
        float enemyWidth = 0.0F;
        float enemyHeight = 0.0F;
        enemyCollisionBounds(enemy, enemyX, enemyY, enemyWidth, enemyHeight);
        if (!enemy.actor.active ||
            !overlaps(static_cast<float>(box.x), box.y - 6.0F, TILE_SIZE, 8.0F,
                      enemyX, enemyY, enemyWidth, enemyHeight)) {
            continue;
        }
        defeatEnemy(enemy, 0, true);
    }
}

void PlatformerEngine::checkEnemyCollisions(float previousBottom) {
    (void)previousBottom;
    const float collisionX = player_.x;
    const float collisionY = player_.y;
    const float collisionWidth = playerWidth();
    const float collisionHeight = playerHeight();
    const bool descending = player_.vy > 0.0F;
    bool stomped = false;
    float bounceY = player_.y;
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        float enemyX = 0.0F;
        float enemyY = 0.0F;
        float enemyWidth = 0.0F;
        float enemyHeight = 0.0F;
        enemyCollisionBounds(enemy, enemyX, enemyY, enemyWidth, enemyHeight);
        if (!enemy.actor.active ||
            enemy.motion == PlatformerEnemyMotion::Squashed ||
            enemy.motion == PlatformerEnemyMotion::FallingDefeated ||
            enemy.motion == PlatformerEnemyMotion::Defeated ||
            !overlaps(collisionX, collisionY, collisionWidth, collisionHeight,
                      enemyX, enemyY, enemyWidth, enemyHeight)) {
            continue;
        }
        if (starProtectedThisFrame_) {
            if (enemy.type != PlatformerEnemyType::Bowser &&
                enemy.type != PlatformerEnemyType::HammerBro &&
                enemy.type != PlatformerEnemyType::Lakitu) {
                enemy.facingLeft = enemy.actor.vx < 0.0F;
            }
            enemy.actor.vx = 0.0F;
            defeatEnemy(enemy, 100, true);
            continue;
        }
        if (enemy.motion == PlatformerEnemyMotion::ShellIdle ||
            enemy.motion == PlatformerEnemyMotion::ShellSliding) {
            if (descending) {
                if (std::fabs(enemy.actor.vx) > EPSILON) {
                    enemy.motion = PlatformerEnemyMotion::ShellIdle;
                    enemy.actor.vx = 0.0F;
                    player_.vy = -STOMP_BOUNCE_SPEED;
                    stomped = true;
                } else {
                    enemy.motion = PlatformerEnemyMotion::ShellSliding;
                    enemy.actor.vx = SHELL_SPEED;
                }
            } else if (player_.x <= enemyX &&
                       player_.x + collisionWidth <
                           enemyX + enemyWidth) {
                enemy.motion = PlatformerEnemyMotion::ShellSliding;
                enemy.actor.vx = SHELL_SPEED;
            } else if (player_.x > enemyX &&
                       player_.x + collisionWidth >
                           enemyX + enemyWidth) {
                enemy.motion = PlatformerEnemyMotion::ShellSliding;
                enemy.actor.vx = -SHELL_SPEED;
            }
            continue;
        }
        const bool risingCheepContact =
            enemy.type == PlatformerEnemyType::CheepCheep &&
            player_.vy == 0.0F && enemy.actor.vy < 0.0F;
        if ((descending || risingCheepContact) &&
            enemyIsCrushable(enemy)) {
            crushEnemy(enemy);
            addScore(100, enemy.actor.x, enemy.actor.y);
            spawnEffect(PlatformerEffectKind::Score, enemy.actor.x,
                        enemy.actor.y - 2.0F, 0.0F, -30.0F, 100U);
            queueEvent(PlatformerEventType::EnemyStomped, 100);
            bounceY = stomped
                          ? std::min(bounceY,
                                     enemyY - collisionHeight)
                          : enemyY - collisionHeight;
            stomped = true;
            continue;
        }
        if (!stomped && player_.vy <= 0.0F) {
            hurtPlayer();
        } else if (stomped) {
            if (enemyIsCrushable(enemy)) {
                crushEnemy(enemy);
            }
            addScore(100, enemy.actor.x, enemy.actor.y);
            spawnEffect(PlatformerEffectKind::Score, enemy.actor.x,
                        enemy.actor.y - 2.0F, 0.0F, -30.0F, 100U);
            queueEvent(PlatformerEventType::EnemyStomped, 100);
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

void PlatformerEngine::enemyCollisionBounds(const EnemyActor& enemy,
                                             float& x, float& y,
                                             float& width,
                                             float& height) const {
    x = enemy.actor.x;
    y = enemy.actor.y;
    width = enemy.width;
    height = enemy.height;

    if ((enemy.type == PlatformerEnemyType::Koopa ||
         enemy.type == PlatformerEnemyType::KoopaParatroopa) &&
        enemy.motion != PlatformerEnemyMotion::ShellIdle &&
        enemy.motion != PlatformerEnemyMotion::ShellSliding) {
        y += 8.0F;
        width = 16.0F;
        height = 16.0F;
    } else if (enemy.type == PlatformerEnemyType::PiranhaPlant) {
        x += 12.0F;
        y += 24.0F;
        width = 8.0F;
        height = 8.0F;
    } else if (enemy.type == PlatformerEnemyType::Blooper) {
        y += 8.0F;
        width = 16.0F;
        height = 16.0F;
    } else if (enemy.type == PlatformerEnemyType::BulletBill) {
        y += 16.0F;
        width = 16.0F;
        height = 16.0F;
    }
}

bool PlatformerEngine::enemyIsCrushable(const EnemyActor& enemy) const {
    switch (enemy.type) {
        case PlatformerEnemyType::Goomba:
        case PlatformerEnemyType::Koopa:
        case PlatformerEnemyType::KoopaParatroopa:
        case PlatformerEnemyType::BuzzyBeetle:
        case PlatformerEnemyType::Lakitu:
        case PlatformerEnemyType::HammerBro:
        case PlatformerEnemyType::BulletBill:
            return true;
        case PlatformerEnemyType::CheepCheep:
            return enemy.flyingCheep;
        default:
            return false;
    }
}

void PlatformerEngine::crushEnemy(EnemyActor& enemy) {
    const bool facingLeft = enemy.actor.vx < 0.0F;
    enemy.actor.vx = 0.0F;
    if (enemy.type == PlatformerEnemyType::KoopaParatroopa) {
        enemy.type = PlatformerEnemyType::Koopa;
        enemy.sourceTileId = enemy.sourceTileId >= 2U
                                 ? enemy.sourceTileId - 2U
                                 : enemy.sourceTileId;
        return;
    }
    if (enemy.type == PlatformerEnemyType::Koopa ||
        enemy.type == PlatformerEnemyType::BuzzyBeetle) {
        const float previousHeight = enemy.height;
        enemy.motion = PlatformerEnemyMotion::ShellIdle;
        enemy.height = 16.0F;
        enemy.actor.y += previousHeight - enemy.height;
        return;
    }
    if (enemy.type == PlatformerEnemyType::Goomba) {
        enemy.motion = PlatformerEnemyMotion::Squashed;
        enemy.stateMs = 20U * 17U;
        enemy.height = 8.0F;
        enemy.actor.y += 8.0F;
        return;
    }
    enemy.motion = PlatformerEnemyMotion::FallingDefeated;
    enemy.bornFrame = logicFrame_;
    enemy.facingLeft = facingLeft;
    enemy.verticalFlipped = enemy.type != PlatformerEnemyType::BulletBill;
}

// Flags assigned to an already-visited entity persist until the next tick,
// matching the reference EnemySystem's ordered ECS iteration.
void PlatformerEngine::checkEnemyPairCollisions() {
    for (uint8_t currentIndex = 0; currentIndex < enemyCount_;
         ++currentIndex) {
        EnemyActor& current = enemies_[currentIndex];
        const bool currentCanSetCollisions =
            current.actor.active &&
            !isDefeatedParticle(current.motion) &&
            current.type != PlatformerEnemyType::PiranhaPlant &&
            current.type != PlatformerEnemyType::Spiny &&
            current.type != PlatformerEnemyType::BulletBill;
        if (currentCanSetCollisions) {
            float currentX = 0.0F;
            float currentY = 0.0F;
            float currentWidth = 0.0F;
            float currentHeight = 0.0F;
            enemyCollisionBounds(current, currentX, currentY, currentWidth,
                                 currentHeight);
            for (uint8_t otherIndex = 0; otherIndex < enemyCount_;
                 ++otherIndex) {
                if (currentIndex == otherIndex) {
                    continue;
                }
                EnemyActor& other = enemies_[otherIndex];
                if (!other.actor.active ||
                    isDefeatedParticle(other.motion)) {
                    continue;
                }
                float otherX = 0.0F;
                float otherY = 0.0F;
                float otherWidth = 0.0F;
                float otherHeight = 0.0F;
                enemyCollisionBounds(other, otherX, otherY, otherWidth,
                                     otherHeight);
                if (!overlaps(currentX, currentY, currentWidth, currentHeight,
                              otherX, otherY, otherWidth, otherHeight)) {
                    continue;
                }
                if (other.motion ==
                    PlatformerEnemyMotion::ShellSliding) {
                    defeatEnemy(current, 100U, true);
                    break;
                }
                if (otherX < currentX &&
                    otherX + otherWidth < currentX + currentWidth) {
                    other.enemyRightCollision = true;
                } else if (otherX > currentX &&
                           otherX + otherWidth >
                               currentX + currentWidth) {
                    other.enemyLeftCollision = true;
                }
            }
        }

        const bool reversesOnEnemyCollision =
            current.type != PlatformerEnemyType::PiranhaPlant &&
            current.type != PlatformerEnemyType::CheepCheep &&
            current.type != PlatformerEnemyType::Blooper &&
            current.type != PlatformerEnemyType::Lakitu &&
            current.type != PlatformerEnemyType::LavaBubble &&
            current.type != PlatformerEnemyType::BulletBill;
        if (reversesOnEnemyCollision && current.enemyLeftCollision) {
            current.actor.vx =
                current.motion == PlatformerEnemyMotion::ShellSliding
                    ? SHELL_SPEED
                    : ENEMY_SPEED;
        } else if (reversesOnEnemyCollision &&
                   current.enemyRightCollision) {
            current.actor.vx =
                current.motion == PlatformerEnemyMotion::ShellSliding
                    ? -SHELL_SPEED
                    : -ENEMY_SPEED;
        }
        if (reversesOnEnemyCollision) {
            current.enemyLeftCollision = false;
            current.enemyRightCollision = false;
        }
    }
}

void PlatformerEngine::hurtPlayer() {
    if (hurtProtectedThisFrame_ || starProtectedThisFrame_ ||
        powerTransition_ != PlatformerPowerTransition::None ||
        phase_ != PlatformerPhase::Running) {
        return;
    }
    if (playerPower_ != PlatformerPlayerPower::Small) {
        if (playerCrouching_) {
            player_.y -= BIG_PLAYER_HEIGHT - CROUCH_PLAYER_HEIGHT;
        }
        playerPower_ = PlatformerPlayerPower::Small;
        playerCrouching_ = false;
        powerTransition_ = PlatformerPowerTransition::Shrink;
        powerTransitionFrames_ = 44U;
        powerTransitionElapsedFrames_ = 0U;
        hurtInvincibleFrames_ = 194U;
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
    if (reason != PlatformerDeathReason::Enemy &&
        playerPower_ != PlatformerPlayerPower::Small) {
        if (playerCrouching_) {
            player_.y -= BIG_PLAYER_HEIGHT - CROUCH_PLAYER_HEIGHT;
        }
        playerPower_ = PlatformerPlayerPower::Small;
        playerCrouching_ = false;
    }
    powerTransition_ = PlatformerPowerTransition::None;
    powerTransitionFrames_ = 0U;
    powerTransitionElapsedFrames_ = 0U;
    starInvincibleFrames_ = 0U;
    starBlinkFrames_ = 0U;
    starProtectedThisFrame_ = false;
    hurtProtectedThisFrame_ = false;
    deathReason_ = reason;
    phaseElapsedMs_ = 0;
    phaseFrames_ = 0;
    player_.vx = 0.0F;
    player_.vy = -12.5F * REFERENCE_VELOCITY_SCALE;
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
    phaseFrames_ = 0U;
    flagLanded_ = false;
    flagShifted_ = false;
    timeBonusReady_ = false;
    timeBonusCompletionFrames_ = 0U;
    player_.x = flagClimbPlayerX();
    player_.vx = 0.0F;
    player_.vy = 0.0F;
    player_.grounded = false;
    queueEvent(PlatformerEventType::ReachedGoal);
}

float PlatformerEngine::flagClimbPlayerX() const {
    return campaignMode_ ? flagX_ : goalX_ - playerWidth();
}

void PlatformerEngine::beginCastleClear() {
    if (phase_ != PlatformerPhase::Running) {
        return;
    }
    phase_ = PlatformerPhase::CastleBridge;
    phaseElapsedMs_ = 0;
    phaseFrames_ = 0U;
    bridgeRemovedCount_ = 0;
    bridgeSequenceState_ = 0U;
    bridgeStepFrames_ = 4U;
    bridgeDelayFrames_ = 0U;
    castleClearFrames_ = 0U;
    player_.vx = 0.0F;
    player_.vy = 0.0F;
    player_.grounded = true;
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
    powerup.vx = 0.0F;
    powerup.vy = -REFERENCE_VELOCITY_SCALE;
    powerup.bornFrame = logicFrame_;
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
    PlatformerEffect& effect = effects_[slot];
    effect = PlatformerEffect{};
    effect.kind = kind;
    effect.x = x;
    effect.y = y;
    effect.vx = vx;
    effect.vy = vy;
    effect.value = value;
    effect.bornFrame = logicFrame_;
    effect.active = true;
    if (kind == PlatformerEffectKind::RisingCoin) {
        effect.animationFrame = 1U;
        effect.animationTimer = 8U;
    }
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
    projectile.y = player_.y + 2.0F;
    projectile.vx = playerFacingLeft_
                        ? -10.0F * REFERENCE_VELOCITY_SCALE
                        : 10.0F * REFERENCE_VELOCITY_SCALE;
    projectile.vy = 5.0F * REFERENCE_VELOCITY_SCALE;
    projectile.bornFrame = logicFrame_;
    projectile.active = true;
    fireballPoseFrames_ = 6U;
    queueEvent(PlatformerEventType::FireballShot);
}

void PlatformerEngine::defeatEnemy(EnemyActor& enemy, uint16_t points,
                                   bool launch) {
    if (!enemy.actor.active) {
        return;
    }
    (void)launch;
    if (enemy.type != PlatformerEnemyType::Bowser &&
        enemy.type != PlatformerEnemyType::HammerBro &&
        enemy.type != PlatformerEnemyType::Lakitu &&
        std::fabs(enemy.actor.vx) > EPSILON) {
        enemy.facingLeft = enemy.actor.vx < 0.0F;
    }
    enemy.bornFrame = logicFrame_;
    enemy.verticalFlipped = enemy.type != PlatformerEnemyType::PiranhaPlant &&
                            enemy.type != PlatformerEnemyType::BulletBill;
    if (enemy.type == PlatformerEnemyType::PiranhaPlant) {
        enemy.actor.vx = 0.0F;
        enemy.actor.vy = 0.0F;
        enemy.motion = PlatformerEnemyMotion::Defeated;
    } else {
        if (enemy.type != PlatformerEnemyType::BulletBill) {
            enemy.actor.vy = -6.0F * REFERENCE_VELOCITY_SCALE;
        }
        enemy.motion = PlatformerEnemyMotion::FallingDefeated;
    }
    spawnEffect(PlatformerEffectKind::Score, enemy.actor.x,
                enemy.actor.y - 2.0F, 0.0F, -30.0F, 100U);
    if (points > 0U) {
        addScore(points, enemy.actor.x, enemy.actor.y);
    }
    queueEvent(PlatformerEventType::EnemyDefeated, points);
}

void PlatformerEngine::addScore(uint16_t points, float x, float y) {
    (void)x;
    (void)y;
    score_ += points;
}

void PlatformerEngine::queueEvent(PlatformerEventType type, uint16_t value) {
    if (type == PlatformerEventType::TimerTick && eventCount_ > 0U) {
        const uint8_t previous = static_cast<uint8_t>(
            (eventWrite_ + EVENT_QUEUE_SIZE - 1U) % EVENT_QUEUE_SIZE);
        if (events_[previous].type == PlatformerEventType::TimerTick) {
            events_[previous] = PlatformerEvent{type, score_, value};
            return;
        }
    }
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
    if (crouchHeld) {
        if (!playerCrouching_) {
            player_.y += heightDelta;
            playerCrouching_ = true;
        }
        return;
    }
    if (!playerCrouching_) {
        return;
    }
    player_.y -= heightDelta;
    playerCrouching_ = false;
}

float PlatformerEngine::playerWidth() const {
    return playerPower_ == PlatformerPlayerPower::Small ? PLAYER_WIDTH
                                                        : BIG_PLAYER_WIDTH;
}

float PlatformerEngine::playerHeight() const {
    if (playerCrouching_) {
        return CROUCH_PLAYER_HEIGHT;
    }
    if (powerTransition_ != PlatformerPowerTransition::None) {
        return BIG_PLAYER_HEIGHT;
    }
    return playerPower_ == PlatformerPlayerPower::Small ? PLAYER_HEIGHT
                                                         : BIG_PLAYER_HEIGHT;
}

bool PlatformerEngine::overlaps(float ax, float ay, float aw, float ah,
                                float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

}  // namespace pgos
