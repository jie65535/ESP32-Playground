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

PlatformerEnemyType campaignEnemyType(uint16_t reference) {
    switch (reference) {
        case 38:
        case 39:
        case 455:
            return PlatformerEnemyType::Koopa;
        case 40:
            return PlatformerEnemyType::KoopaParatroopa;
        case 44:
            return PlatformerEnemyType::PiranhaPlant;
        case 48:
            return PlatformerEnemyType::Blooper;
        case 50:
            return PlatformerEnemyType::Lakitu;
        case 56:
            return PlatformerEnemyType::HammerBro;
        case 61:
            return PlatformerEnemyType::Bowser;
        case 81:
        case 498:
            return PlatformerEnemyType::CheepCheep;
        case 87:
            return PlatformerEnemyType::BuzzyBeetle;
        case 90:
            return PlatformerEnemyType::BulletBill;
        case 504:
            return PlatformerEnemyType::LavaBubble;
        case 70:
        case 71:
        default:
            return PlatformerEnemyType::Goomba;
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
    cameraY_ = 0.0F;
    flagY_ = static_cast<float>(PLATFORMER_LEVEL_1_1.flagTopY);
    goalX_ = static_cast<float>(PLATFORMER_LEVEL_1_1.goalX);
    castleX_ = static_cast<float>(PLATFORMER_LEVEL_1_1.castleX);
    playerPower_ = PlatformerPlayerPower::Small;
    playerCrouching_ = false;
    playerFacingLeft_ = false;
    playerSkidding_ = false;
    timeWarningSent_ = false;
    mapTestMode_ = false;
    campaignMode_ = false;
    cameraFrozen_ = false;
    warpTeleported_ = false;
    startIntro_ = false;
    vineReturnActive_ = false;
    vine_ = PlatformerVineState{};
    activeWarpIndex_ = 0;
    jumpHoldMs_ = 0;
    coyoteMs_ = 0;
    jumpBufferMs_ = 0;
    powerTransitionMs_ = 0;
    hurtInvincibleMs_ = 0;
    starInvincibleMs_ = 0;
    fireCooldownMs_ = 0;
    warpCooldownMs_ = 0;
    swimCooldownMs_ = 0;
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
    player_.active = false;
    startIntro_ = false;
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
    if (!levelRuntime_.load(current.nextWorld, current.nextStage)) {
        return false;
    }
    resetCampaignLevel(false);
    return true;
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
        case PlatformerPhase::Warping:
        case PlatformerPhase::VineClimb:
        case PlatformerPhase::CastleBridge:
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
    snapshot.cameraY = cameraY_;
    snapshot.flagX = flagX_;
    snapshot.flagY = flagY_;
    snapshot.flagTileId = flagTileId_;
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
    snapshot.playerDamageBlinking =
        hurtInvincibleMs_ > 0 && powerTransitionMs_ == 0;
    snapshot.goalReached = phase_ == PlatformerPhase::Flagpole ||
                           phase_ == PlatformerPhase::CastleBridge ||
                           phase_ == PlatformerPhase::CastleWalk ||
                           phase_ == PlatformerPhase::TimeBonus ||
                           phase_ == PlatformerPhase::Won;
    snapshot.mapTestMode = mapTestMode_;
    snapshot.campaignMode = campaignMode_;
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

void PlatformerEngine::debugSpawnCampaignEnemy(PlatformerEnemyType type,
                                                float x, float y,
                                                uint16_t sourceTileId) {
    if (enemyCount_ >= MAX_ENEMIES) {
        return;
    }
    EnemyActor& enemy = enemies_[enemyCount_++];
    enemy = EnemyActor{};
    enemy.type = type;
    enemy.sourceTileId = sourceTileId;
    enemy.actor.x = enemy.originX = x;
    enemy.actor.y = enemy.originY = y;
    enemy.actor.vx = -ENEMY_SPEED;
    enemy.actor.active = true;
    enemy.spawned = true;
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
}

void PlatformerEngine::debugBeginGoal() {
    beginGoal();
}

void PlatformerEngine::debugSetTimeRemaining(uint16_t value) {
    timeRemaining_ = value;
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
    movingPlatformCount_ = 0;
    fireBarCount_ = 0;
    enemyHazardCount_ = 0;
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
    vine_ = PlatformerVineState{};
    vineReturnActive_ = false;
    activeVineIndex_ = 0;
    bridgeStartColumn_ = -1;
    bridgeEndColumn_ = -1;
    bridgeRow_ = 0;
    bridgeRemovedCount_ = 0;
    for (auto& word : campaignEnemySpawned_) {
        word = 0U;
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
    }
    updateCamera();
}

void PlatformerEngine::resetCampaignLevel(bool resetPower) {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (level == nullptr) {
        phase_ = PlatformerPhase::Title;
        return;
    }

    levelRuntime_.resetChanges();
    boxCount_ = 0;
    enemyCount_ = 0;
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
    for (auto& word : campaignEnemySpawned_) {
        word = 0U;
    }
    resetCampaignDynamics();

    if (resetPower) {
        playerPower_ = PlatformerPlayerPower::Small;
    }
    playerCrouching_ = false;
    playerSkidding_ = false;
    playerFacingLeft_ = false;
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
    levelClockMs_ = 0;
    timeRemaining_ = 400;
    jumpHoldMs_ = 0;
    coyoteMs_ = 0;
    jumpBufferMs_ = 0;
    powerTransitionMs_ = 0;
    hurtInvincibleMs_ = 0;
    starInvincibleMs_ = 0;
    fireCooldownMs_ = 0;
    warpCooldownMs_ = 0;
    swimCooldownMs_ = 0;
    stompChain_ = 0;
    timeWarningSent_ = false;
    jumpActive_ = false;
    mapTestMode_ = false;
    startIntro_ = level->levelType == PlatformerLevelType::StartUnderground;
    cameraFrozen_ = startIntro_;
    warpTeleported_ = false;
    activeWarpIndex_ = 0;
    queueEvent(PlatformerEventType::LifeRestarted, lives_);
}

void PlatformerEngine::resetCampaignDynamics() {
    movingPlatformCount_ = 0;
    fireBarCount_ = 0;
    for (auto& platform : movingPlatforms_) {
        platform = PlatformerMovingPlatformState{};
    }
    for (auto& fireBar : fireBars_) {
        fireBar = PlatformerFireBarState{};
    }
    vine_ = PlatformerVineState{};
    vineReturnActive_ = false;
    activeVineIndex_ = 0;
    bridgeStartColumn_ = -1;
    bridgeEndColumn_ = -1;
    bridgeRow_ = 0;
    bridgeRemovedCount_ = 0;

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

void PlatformerEngine::updateMovingPlatforms(float dt) {
    const int8_t stoodOn = standingPlatform();
    float previousX = 0.0F;
    float previousY = 0.0F;
    if (stoodOn >= 0) {
        previousX = movingPlatforms_[stoodOn].x;
        previousY = movingPlatforms_[stoodOn].y;
    }
    constexpr float PLATFORM_SPEED = 45.0F;
    constexpr float PLATFORM_GRAVITY = 160.0F;
    const float damping = std::pow(0.92F, dt * 60.0F);

    for (uint8_t index = 0; index < movingPlatformCount_; ++index) {
        PlatformerMovingPlatformState& platform = movingPlatforms_[index];
        if (!platform.active || platform.pulley) {
            continue;
        }
        platform.vx = 0.0F;
        if (platform.motion != PlatformerMotionType::Gravity) {
            platform.vy = 0.0F;
        }
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
            }
        }
        switch (platform.direction) {
            case PlatformerDirection::Left:
                platform.vx = -PLATFORM_SPEED;
                break;
            case PlatformerDirection::Right:
                platform.vx = PLATFORM_SPEED;
                break;
            case PlatformerDirection::Up:
                platform.vy = -PLATFORM_SPEED;
                break;
            case PlatformerDirection::Down:
                platform.vy = PLATFORM_SPEED;
                break;
            default:
                break;
        }
        if (platform.motion == PlatformerMotionType::Gravity) {
            if (stoodOn == static_cast<int8_t>(index)) {
                platform.vy = std::min(120.0F,
                                       platform.vy + PLATFORM_GRAVITY * dt);
            } else {
                platform.vy *= damping;
            }
        }
        platform.x += platform.vx * dt;
        platform.y += platform.vy * dt;
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
        if (stoodOn == static_cast<int8_t>(index)) {
            platform.vy = std::min(120.0F,
                                   platform.vy + PLATFORM_GRAVITY * dt);
            other.vy = -platform.vy;
        } else if (stoodOn == platform.pairIndex) {
            other.vy = std::min(120.0F, other.vy + PLATFORM_GRAVITY * dt);
            platform.vy = -other.vy;
        } else {
            platform.vy *= damping;
            other.vy = -platform.vy;
        }
        platform.y += platform.vy * dt;
        other.y += other.vy * dt;
        if (platform.y < platform.pulleyTop || other.y < other.pulleyTop) {
            if (platform.y < platform.pulleyTop) {
                platform.y = platform.pulleyTop;
            }
            if (other.y < other.pulleyTop) {
                other.y = other.pulleyTop;
            }
            platform.vy = other.vy = 0.0F;
        }
    }

    if (stoodOn >= 0 && movingPlatforms_[stoodOn].active) {
        player_.x += movingPlatforms_[stoodOn].x - previousX;
        player_.y += movingPlatforms_[stoodOn].y - previousY;
        player_.grounded = true;
    }
}

void PlatformerEngine::updateFireBars(float dt) {
    constexpr float FIRE_BAR_DEGREES_PER_SECOND = 100.0F;
    for (uint8_t index = 0; index < fireBarCount_; ++index) {
        PlatformerFireBarState& fireBar = fireBars_[index];
        if (!fireBar.active) {
            continue;
        }
        const float direction =
            fireBar.direction == PlatformerRotationDirection::Clockwise
                ? -1.0F
                : 1.0F;
        fireBar.angleDegrees += direction * FIRE_BAR_DEGREES_PER_SECOND * dt;
        if (fireBar.angleDegrees >= 360.0F) {
            fireBar.angleDegrees -= 360.0F;
        } else if (fireBar.angleDegrees < 0.0F) {
            fireBar.angleDegrees += 360.0F;
        }
    }
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
            const float distance = static_cast<float>(element * TILE_SIZE);
            const float x = fireBar.x + std::cos(radians) * distance + 6.0F;
            const float y = fireBar.y - std::sin(radians) * distance + 6.0F;
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
    vine_.grownPixels =
        std::min(96.0F, vine_.grownPixels + 60.0F * dt);
    if (vine_.grownPixels < 48.0F || !input.jumpPressed) {
        return;
    }
    const float vineTop = vine_.baseY - vine_.grownPixels;
    if (!overlaps(player_.x, player_.y, playerWidth(), playerHeight(),
                  vine_.x + 4.0F, vineTop, 8.0F,
                  vine_.grownPixels + TILE_SIZE)) {
        return;
    }
    vinePreviousLevelType_ = levelRuntime_.activeLevelType();
    vinePreviousBackground_ = levelRuntime_.activeBackground();
    phase_ = PlatformerPhase::VineClimb;
    phaseElapsedMs_ = 0;
    player_.vx = 0.0F;
    player_.vy = 0.0F;
    player_.grounded = false;
}

void PlatformerEngine::updateVineClimb(float dt) {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (level == nullptr || activeVineIndex_ >= level->vines.count) {
        phase_ = PlatformerPhase::Running;
        return;
    }
    const PlatformerVineData& source =
        PLATFORMER_CAMPAIGN_VINES[level->vines.offset + activeVineIndex_];
    player_.x = vine_.x + 6.0F;
    player_.y -= 58.0F * dt;
    if (phaseElapsedMs_ < 900U) {
        return;
    }

    vine_.x = static_cast<float>(source.destination.x * TILE_SIZE);
    vine_.baseY = static_cast<float>(source.destination.y * TILE_SIZE);
    vine_.grownPixels = 96.0F;
    player_.x = vine_.x + 6.0F;
    player_.y = static_cast<float>((source.destination.y - 4) * TILE_SIZE);
    cameraX_ = static_cast<float>(source.camera.x * TILE_SIZE);
    cameraY_ = static_cast<float>(source.camera.y * TILE_SIZE);
    cameraFrozen_ = false;
    levelRuntime_.setSection(source.levelType, source.background);
    vineReturnActive_ =
        source.resetBelowY < 1000 &&
        (source.resetDestination.x != 0 || source.resetDestination.y != 0);
    warpCooldownMs_ = 450U;
    phase_ = PlatformerPhase::Running;
    phaseElapsedMs_ = 0;
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
    if (player_.y <= static_cast<float>(source.resetBelowY * TILE_SIZE)) {
        return;
    }
    player_.x = static_cast<float>(source.resetDestination.x * TILE_SIZE);
    player_.y = static_cast<float>(source.resetDestination.y * TILE_SIZE) -
                (playerPower_ == PlatformerPlayerPower::Small
                     ? 0.0F
                     : BIG_PLAYER_HEIGHT - PLAYER_HEIGHT);
    player_.vx = 0.0F;
    player_.vy = 0.0F;
    cameraX_ = static_cast<float>(
        std::max<int16_t>(0, source.resetDestination.x - 2) * TILE_SIZE);
    cameraY_ = static_cast<float>(
        std::max<int16_t>(0, source.resetDestination.y - 1) * TILE_SIZE);
    levelRuntime_.setSection(vinePreviousLevelType_, vinePreviousBackground_);
    vine_.active = false;
    vineReturnActive_ = false;
    cameraFrozen_ = false;
}

void PlatformerEngine::resetLife() {
    if (campaignMode_) {
        resetCampaignLevel(true);
        return;
    }
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

    if (campaignMode_) {
        updateMovingPlatforms(dt);
        updateFireBars(dt);
    }

    if (startIntro_) {
        player_.vx = 38.0F;
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
    const float previousX = player_.x;
    updatePlayerHorizontal(dt, input);
    updatePlayerVertical(dt, dtMs, input);
    if (campaignMode_) {
        applyTeleportPoints(previousX);
        collectMapCoins();
        checkVineReturn();
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
    if (!campaignMode_ || level == nullptr || warpCooldownMs_ > 0U) {
        return false;
    }
    const float playerLeft = player_.x;
    const float playerRight = player_.x + playerWidth();
    const float playerCenter = player_.x + playerWidth() * 0.5F;
    const float playerTop = player_.y;
    const float playerBottom = player_.y + playerHeight();
    for (uint8_t index = 0; index < level->warps.count; ++index) {
        const PlatformerWarpData& warp =
            PLATFORMER_CAMPAIGN_WARPS[level->warps.offset + index];
        const float pipeX = static_cast<float>(warp.pipe.x * TILE_SIZE);
        const float pipeY = static_cast<float>(warp.pipe.y * TILE_SIZE);
        const bool horizontallyAligned =
            playerCenter >= pipeX - 3.0F &&
            playerCenter <= pipeX + TILE_SIZE * 2.0F + 3.0F;
        const bool verticallyAligned =
            player_.y + playerHeight() * 0.5F >= pipeY - TILE_SIZE &&
            player_.y + playerHeight() * 0.5F <= pipeY + TILE_SIZE * 3.0F;
        bool requested = false;
        switch (warp.enterDirection) {
            case PlatformerDirection::Down:
                requested = input.crouchHeld && horizontallyAligned &&
                            std::fabs(playerBottom - pipeY) <= 6.0F;
                break;
            case PlatformerDirection::Up:
                requested = input.jumpPressed && horizontallyAligned &&
                            std::fabs(playerTop - (pipeY + TILE_SIZE)) <= 8.0F;
                break;
            case PlatformerDirection::Right:
                requested = input.moveAxis > 0.5F && verticallyAligned &&
                            std::fabs(playerRight - pipeX) <= 7.0F;
                break;
            case PlatformerDirection::Left:
                requested = input.moveAxis < -0.5F && verticallyAligned &&
                            std::fabs(playerLeft - (pipeX + TILE_SIZE)) <= 7.0F;
                break;
            case PlatformerDirection::None:
                break;
        }
        if (!requested) {
            continue;
        }
        activeWarpIndex_ = index;
        warpTeleported_ = false;
        phase_ = PlatformerPhase::Warping;
        phaseElapsedMs_ = 0;
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
            for (uint8_t enemyIndex = 0; enemyIndex < enemyCount_;
                 ++enemyIndex) {
                enemies_[enemyIndex].actor.active = false;
                enemies_[enemyIndex].motion = PlatformerEnemyMotion::Defeated;
            }
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
        constexpr float WARP_SPEED = 38.0F;
        switch (direction) {
            case PlatformerDirection::Up:
                player_.y -= WARP_SPEED * dt;
                break;
            case PlatformerDirection::Down:
                player_.y += WARP_SPEED * dt;
                break;
            case PlatformerDirection::Left:
                player_.x -= WARP_SPEED * dt;
                break;
            case PlatformerDirection::Right:
                player_.x += WARP_SPEED * dt;
                break;
            case PlatformerDirection::None:
                break;
        }
    };

    if (!warpTeleported_) {
        moveInDirection(warp->enterDirection);
        if (phaseElapsedMs_ < 430U) {
            return;
        }

        if (warp->destinationWorld != 0U && warp->destinationStage != 0U) {
            const uint8_t destinationWorld = warp->destinationWorld;
            const uint8_t destinationStage = warp->destinationStage;
            if (levelRuntime_.load(destinationWorld, destinationStage)) {
                resetCampaignLevel(false);
                warpCooldownMs_ = 450U;
                queueEvent(PlatformerEventType::WarpCompleted);
            } else {
                phase_ = PlatformerPhase::Running;
            }
            return;
        }

        player_.x = static_cast<float>(warp->destination.x * TILE_SIZE);
        player_.y = static_cast<float>(warp->destination.y * TILE_SIZE);
        cameraX_ = static_cast<float>(warp->camera.x * TILE_SIZE);
        cameraY_ = static_cast<float>(warp->camera.y * TILE_SIZE);
        cameraFrozen_ = warp->freezeCamera;
        levelRuntime_.setSection(warp->levelType, warp->background);
        startIntro_ = false;
        warpTeleported_ = true;
        phaseElapsedMs_ = 0;
        if (warp->exitDirection == PlatformerDirection::None) {
            phase_ = PlatformerPhase::Running;
            warpCooldownMs_ = 450U;
            queueEvent(PlatformerEventType::WarpCompleted);
        }
        return;
    }

    moveInDirection(warp->exitDirection);
    if (phaseElapsedMs_ >= 430U) {
        phase_ = PlatformerPhase::Running;
        startIntro_ = false;
        warpCooldownMs_ = 450U;
        queueEvent(PlatformerEventType::WarpCompleted);
    }
}

void PlatformerEngine::updateCastleBridge(float dt) {
    const uint8_t bridgeLength =
        bridgeStartColumn_ >= 0 && bridgeEndColumn_ >= bridgeStartColumn_
            ? static_cast<uint8_t>(bridgeEndColumn_ - bridgeStartColumn_ + 1)
            : 0U;
    constexpr uint16_t COLLAPSE_STEP_MS = 80U;
    const uint8_t targetRemoved = static_cast<uint8_t>(std::min<uint16_t>(
        bridgeLength, phaseElapsedMs_ / COLLAPSE_STEP_MS));
    while (bridgeRemovedCount_ < targetRemoved) {
        const uint16_t column = static_cast<uint16_t>(
            bridgeEndColumn_ - bridgeRemovedCount_);
        levelRuntime_.removeTile(column, bridgeRow_);
        ++bridgeRemovedCount_;
        queueEvent(PlatformerEventType::BrickBroken);
    }
    if (bridgeRemovedCount_ < bridgeLength) {
        return;
    }

    const uint16_t collapseDuration =
        static_cast<uint16_t>(bridgeLength * COLLAPSE_STEP_MS);
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        if (!enemy.actor.active || enemy.type != PlatformerEnemyType::Bowser) {
            continue;
        }
        enemy.actor.vy = std::min(enemy.actor.vy + GRAVITY * dt,
                                  MAX_FALL_SPEED);
        enemy.actor.y += enemy.actor.vy * dt;
        if (enemy.actor.y > cameraY_ + VIEWPORT_WIDTH * 0.75F + 48.0F) {
            enemy.actor.active = false;
            enemy.motion = PlatformerEnemyMotion::Defeated;
        }
    }

    const uint16_t postCollapse = static_cast<uint16_t>(
        phaseElapsedMs_ > collapseDuration ? phaseElapsedMs_ - collapseDuration
                                           : 0U);
    if (postCollapse >= 900U) {
        player_.vx = 72.0F;
        player_.x += player_.vx * dt;
        updateCamera();
    }
    if (postCollapse >= 2200U) {
        phase_ = PlatformerPhase::TimeBonus;
        phaseElapsedMs_ = 0;
        player_.active = false;
    }
}

void PlatformerEngine::updateScriptedPhase(float dt, uint16_t dtMs) {
    phaseElapsedMs_ = static_cast<uint16_t>(
        std::min<uint32_t>(65535U, phaseElapsedMs_ + dtMs));
    updateBoxes(dtMs);
    updateEffects(dt, dtMs);

    if (phase_ == PlatformerPhase::Warping) {
        updateWarp(dt);
        return;
    }

    if (phase_ == PlatformerPhase::VineClimb) {
        updateVineClimb(dt);
        return;
    }

    if (phase_ == PlatformerPhase::CastleBridge) {
        updateCastleBridge(dt);
        return;
    }

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
        const PlatformerCampaignLevel* level = levelRuntime_.level();
        const float floorY = campaignMode_ && level != nullptr
                                 ? static_cast<float>(
                                       (level->playerStart.y + 1) * TILE_SIZE) -
                                       playerHeight()
                                 : static_cast<float>(
                                       PLATFORMER_LEVEL_1_1.groundY) -
                                       playerHeight();
        player_.x = flagClimbPlayerX();
        player_.y = std::min(floorY, player_.y + 90.0F * dt);
        const float flagSlideY = campaignMode_ && level != nullptr
                                       ? static_cast<float>(
                                             (level->playerStart.y + 1) *
                                             TILE_SIZE - TILE_SIZE)
                                       : campaignMode_ ? floorY
                                               : static_cast<float>(
                                                     PLATFORMER_LEVEL_1_1.flagSlideY);
        flagY_ = std::min(flagSlideY, flagY_ + 94.0F * dt);
        if (phaseElapsedMs_ >= 1800U) {
            phase_ = PlatformerPhase::CastleWalk;
            phaseElapsedMs_ = 0;
            player_.x = flagClimbPlayerX();
            player_.y = floorY;
            player_.vx = 0.0F;
        }
        updateCamera();
        return;
    }

    if (phase_ == PlatformerPhase::CastleWalk) {
        player_.x = std::min(castleX_, player_.x + 72.0F * dt);
        const PlatformerCampaignLevel* level = levelRuntime_.level();
        player_.y = campaignMode_ && level != nullptr
                        ? static_cast<float>((level->playerStart.y + 1) *
                                             TILE_SIZE) -
                              playerHeight()
                        : static_cast<float>(PLATFORMER_LEVEL_1_1.groundY) -
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
    reduce(warpCooldownMs_);
    reduce(swimCooldownMs_);

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

    constexpr float CAMERA_LEFT_EDGE = 80.0F;
    constexpr float CAMERA_RIGHT_EDGE = REFERENCE_VIEWPORT_WIDTH / 3.0F;
    const float screenX = player_.x - cameraX_;
    if (!campaignMode_ && screenX < CAMERA_LEFT_EDGE) {
        cameraX_ = player_.x - CAMERA_LEFT_EDGE;
    } else if (screenX > CAMERA_RIGHT_EDGE) {
        cameraX_ = player_.x - CAMERA_RIGHT_EDGE;
    }
    const float minimum = campaignMode_ && level != nullptr
                              ? static_cast<float>(level->cameraStart.x *
                                                   TILE_SIZE)
                              : 0.0F;
    cameraX_ = clampValue(cameraX_, minimum, maximum);
}

void PlatformerEngine::updatePlayerHorizontal(float dt,
                                               const PlatformerInput& input) {
    const float axis = clampValue(input.moveAxis, -1.0F, 1.0F);
    if (axis < -0.01F) {
        playerFacingLeft_ = true;
    } else if (axis > 0.01F) {
        playerFacingLeft_ = false;
    }

    const bool underwater = campaignMode_ &&
                            levelRuntime_.activeLevelType() ==
                                PlatformerLevelType::Underwater;
    const float speedScale = underwater ? 0.68F : 1.0F;
    const float target =
        axis * (input.actionHeld ? RUN_SPEED : WALK_SPEED) * speedScale;
    const bool reversing = std::fabs(target) > 0.01F &&
                           std::fabs(player_.vx) > 0.01F &&
                           ((target > 0.0F) != (player_.vx > 0.0F));
    const float acceleration = (reversing
                                   ? TURN_ACCELERATION
                                   : player_.grounded
                                         ? (input.actionHeld ? RUN_ACCELERATION
                                                              : WALK_ACCELERATION)
                                         : AIR_ACCELERATION) *
                               (underwater ? 0.72F : 1.0F);
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
    if (campaignMode_ && levelRuntime_.activeLevelType() ==
                             PlatformerLevelType::Underwater) {
        if (input.jumpPressed && swimCooldownMs_ == 0U &&
            !playerCrouching_) {
            player_.vy = -122.0F;
            swimCooldownMs_ = 180U;
            queueEvent(PlatformerEventType::Jumped);
        }
        moveVertical(player_.vy * dt);
        player_.grounded = false;
        player_.vy = std::min(92.0F, player_.vy + 310.0F * dt);
        jumpBufferMs_ = 0;
        jumpActive_ = false;
        return;
    }
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
            const PlatformerRuntimeTile collisionTile = levelRuntime_.tile(
                static_cast<uint16_t>(column), static_cast<uint8_t>(row));
            if (distance > 0.0F && collisionTile.kind ==
                                       PlatformerRuntimeTileKind::Trampoline) {
                player_.grounded = false;
                player_.vy = -310.0F;
                jumpActive_ = false;
                queueEvent(PlatformerEventType::Jumped);
                return;
            }
            player_.grounded = distance > 0.0F;
            break;
        }
        player_.vy = 0.0F;
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
    if (campaignMode_) {
        activateCampaignEnemies();
    }
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

        if (campaignMode_ && enemy.motion == PlatformerEnemyMotion::Walking) {
            const uint16_t previousState = enemy.stateMs;
            switch (enemy.type) {
                case PlatformerEnemyType::PiranhaPlant: {
                    constexpr uint16_t EXPOSED_MS = 3000U;
                    constexpr uint16_t TRAVEL_MS = 533U;
                    constexpr uint16_t HIDDEN_MS = 3000U;
                    constexpr uint16_t CYCLE_MS =
                        EXPOSED_MS + TRAVEL_MS + HIDDEN_MS + TRAVEL_MS;
                    enemy.actor.vx = 0.0F;
                    enemy.stateMs = static_cast<uint16_t>(
                        (enemy.stateMs + dtMs) % CYCLE_MS);
                    const uint16_t cycle = enemy.stateMs;
                    if (cycle < EXPOSED_MS) {
                        enemy.actor.y = enemy.originY;
                    } else if (cycle < EXPOSED_MS + TRAVEL_MS) {
                        enemy.actor.y = enemy.originY +
                            32.0F * (cycle - EXPOSED_MS) / TRAVEL_MS;
                    } else if (cycle <
                               EXPOSED_MS + TRAVEL_MS + HIDDEN_MS) {
                        enemy.actor.y = enemy.originY + 32.0F;
                    } else {
                        enemy.actor.y = enemy.originY +
                            32.0F * (CYCLE_MS - cycle) / TRAVEL_MS;
                    }
                    continue;
                }
                case PlatformerEnemyType::Blooper: {
                    enemy.stateMs = static_cast<uint16_t>(
                        (enemy.stateMs + dtMs) % 1500U);
                    if (enemy.stateMs < 500U) {
                        enemy.actor.vx = player_.x >= enemy.actor.x ? 64.0F
                                                                    : -64.0F;
                        enemy.actor.vy =
                            player_.y >= enemy.actor.y ? 58.0F : -58.0F;
                    } else {
                        const float damping = std::pow(0.94F, dt * 60.0F);
                        enemy.actor.vx *= damping;
                        enemy.actor.vy = std::min(52.0F,
                                                  enemy.actor.vy + 48.0F * dt);
                    }
                } break;
                case PlatformerEnemyType::CheepCheep: {
                    enemy.stateMs = static_cast<uint16_t>(
                        (enemy.stateMs + dtMs) % 2500U);
                    if (levelRuntime_.activeLevelType() ==
                        PlatformerLevelType::Underwater) {
                        enemy.actor.vx = -48.0F;
                        enemy.actor.vy = 0.0F;
                        enemy.actor.y = enemy.originY +
                            std::sin(enemy.stateMs * 0.004F) * 8.0F;
                    } else {
                        if (previousState > enemy.stateMs ||
                            (previousState == 0U && enemy.actor.vy == 0.0F)) {
                            enemy.actor.x = enemy.originX;
                            enemy.actor.y = enemy.originY + TILE_SIZE;
                            enemy.actor.vx = 88.0F;
                            enemy.actor.vy = -270.0F;
                        }
                        enemy.actor.x += enemy.actor.vx * dt;
                        enemy.actor.y += enemy.actor.vy * dt;
                        enemy.actor.vy = std::min(
                            MAX_FALL_SPEED, enemy.actor.vy + GRAVITY * dt);
                        continue;
                    }
                } break;
                case PlatformerEnemyType::Lakitu: {
                    enemy.stateMs = static_cast<uint16_t>(
                        enemy.stateMs + dtMs);
                    const float target = cameraX_ + VIEWPORT_WIDTH * 0.5F +
                        std::sin(enemy.stateMs * 0.0012F) * 86.0F;
                    enemy.actor.vx = clampValue(
                        (target - enemy.actor.x) * 1.8F, -105.0F, 105.0F);
                    enemy.actor.vy = 0.0F;
                    if (enemy.stateMs >= 3000U) {
                        enemy.stateMs = 0U;
                        spawnSpiny(enemy);
                    }
                } break;
                case PlatformerEnemyType::HammerBro:
                    enemy.stateMs = static_cast<uint16_t>(
                        enemy.stateMs + dtMs);
                    if (enemy.stateMs >= 2000U) {
                        enemy.stateMs = 0U;
                        enemy.actor.vx = -enemy.actor.vx;
                        if (std::fabs(enemy.actor.vx) < 1.0F) {
                            enemy.actor.vx = player_.x < enemy.actor.x
                                                 ? -60.0F
                                                 : 60.0F;
                        }
                        if ((enemy.spawnOrder++ & 1U) != 0U) {
                            enemy.actor.vy = -230.0F;
                        }
                        spawnEnemyHazard(
                            PlatformerEnemyHazardKind::Hammer,
                            enemy.actor.x, enemy.actor.y,
                            player_.x < enemy.actor.x ? -82.0F : 82.0F,
                            -205.0F);
                    }
                    break;
                case PlatformerEnemyType::LavaBubble:
                    enemy.stateMs = static_cast<uint16_t>(
                        enemy.stateMs + dtMs);
                    if (enemy.stateMs >= 6000U) {
                        enemy.stateMs = 0U;
                        enemy.actor.y = enemy.originY;
                        enemy.actor.vy = -300.0F;
                    }
                    if (enemy.actor.vy != 0.0F) {
                        enemy.actor.y += enemy.actor.vy * dt;
                        enemy.actor.vy = std::min(
                            MAX_FALL_SPEED, enemy.actor.vy + GRAVITY * dt);
                        if (enemy.actor.y >= enemy.originY) {
                            enemy.actor.y = enemy.originY;
                            enemy.actor.vy = 0.0F;
                        }
                    }
                    continue;
                case PlatformerEnemyType::Bowser:
                    enemy.stateMs = static_cast<uint16_t>(
                        enemy.stateMs + dtMs);
                    if (enemy.stateMs >= 2000U) {
                        enemy.stateMs = 0U;
                        enemy.actor.vx = enemy.actor.vx < 0.0F ? 38.0F
                                                               : -38.0F;
                        enemy.actor.vy = -185.0F;
                        const bool fire = (enemy.spawnOrder++ & 1U) == 0U;
                        spawnEnemyHazard(
                            fire ? PlatformerEnemyHazardKind::BowserFire
                                 : PlatformerEnemyHazardKind::Hammer,
                            enemy.actor.x,
                            enemy.actor.y + (fire ? 8.0F : 0.0F),
                            player_.x < enemy.actor.x ? -96.0F : 96.0F,
                            fire ? 0.0F : -210.0F);
                    }
                    break;
                default:
                    break;
            }
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
            continue;
        }
        if (campaignMode_) {
            const int32_t row = static_cast<int32_t>(std::floor(
                (nextY + enemy.height - EPSILON) / TILE_SIZE));
            enemy.actor.y = static_cast<float>(row * TILE_SIZE) - enemy.height;
            enemy.actor.vy =
                enemy.type == PlatformerEnemyType::KoopaParatroopa
                    ? -190.0F
                    : 0.0F;
            if (enemy.actor.x + enemy.width < cameraX_ - 64.0F ||
                enemy.actor.y > cameraY_ + VIEWPORT_WIDTH) {
                enemy.actor.active = false;
                enemy.motion = PlatformerEnemyMotion::Defeated;
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
    spiny.actor.x = lakitu.actor.x;
    spiny.actor.y = lakitu.actor.y + TILE_SIZE;
    spiny.actor.vx = player_.x < lakitu.actor.x ? -58.0F : 58.0F;
    spiny.actor.vy = 15.0F;
    spiny.originX = spiny.actor.x;
    spiny.originY = spiny.actor.y;
    spiny.actor.active = true;
    spiny.spawned = true;
}

void PlatformerEngine::spawnEnemyHazard(PlatformerEnemyHazardKind kind,
                                         float x, float y, float vx,
                                         float vy) {
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
        return;
    }
    PlatformerEnemyHazardState& hazard = enemyHazards_[slot];
    hazard = PlatformerEnemyHazardState{};
    hazard.kind = kind;
    hazard.x = x;
    hazard.y = y;
    hazard.vx = vx;
    hazard.vy = vy;
    hazard.sourceTileId =
        kind == PlatformerEnemyHazardKind::Hammer ? 60U : 470U;
    hazard.active = true;
}

void PlatformerEngine::updateEnemyHazards(float dt, uint16_t dtMs) {
    for (uint8_t index = 0; index < enemyHazardCount_; ++index) {
        PlatformerEnemyHazardState& hazard = enemyHazards_[index];
        if (!hazard.active) {
            continue;
        }
        hazard.ageMs = static_cast<uint16_t>(std::min<uint32_t>(
            65535U, static_cast<uint32_t>(hazard.ageMs) + dtMs));
        hazard.x += hazard.vx * dt;
        hazard.y += hazard.vy * dt;
        if (hazard.kind == PlatformerEnemyHazardKind::Hammer) {
            hazard.vy = std::min(MAX_FALL_SPEED,
                                 hazard.vy + GRAVITY * dt);
        }
        const float width =
            hazard.kind == PlatformerEnemyHazardKind::BowserFire ? 24.0F
                                                                 : 16.0F;
        const float height =
            hazard.kind == PlatformerEnemyHazardKind::BowserFire ? 8.0F
                                                                 : 16.0F;
        if (overlaps(player_.x, player_.y, playerWidth(), playerHeight(),
                     hazard.x, hazard.y, width, height)) {
            if (starInvincibleMs_ == 0U) {
                hurtPlayer();
            }
            hazard.active = false;
            continue;
        }
        if (hazard.ageMs >= 6000U || hazard.x < cameraX_ - 96.0F ||
            hazard.x > cameraX_ + VIEWPORT_WIDTH + 128.0F ||
            hazard.y > cameraY_ + VIEWPORT_WIDTH) {
            hazard.active = false;
        }
    }
}

void PlatformerEngine::activateCampaignEnemies() {
    const PlatformerCampaignLevel* level = levelRuntime_.level();
    if (level == nullptr) {
        return;
    }
    const int32_t firstColumn = std::max<int32_t>(
        0, static_cast<int32_t>(cameraX_ / TILE_SIZE) - 1);
    const int32_t lastColumn = std::min<int32_t>(
        level->width - 1,
        static_cast<int32_t>((cameraX_ + VIEWPORT_WIDTH + 32.0F) /
                             TILE_SIZE));
    const int32_t firstRow = std::max<int32_t>(
        0, static_cast<int32_t>(cameraY_ / TILE_SIZE) - 1);
    const int32_t lastRow = std::min<int32_t>(
        level->height - 1,
        static_cast<int32_t>((cameraY_ + VIEWPORT_WIDTH * 0.75F + 16.0F) /
                             TILE_SIZE));
    for (int32_t row = firstRow; row <= lastRow; ++row) {
        for (int32_t column = firstColumn; column <= lastColumn; ++column) {
            const uint16_t sourceId = platformerCampaignTileAt(
                *level, PlatformerMapLayer::Enemies,
                static_cast<uint16_t>(column), static_cast<uint8_t>(row));
            if (sourceId == PLATFORMER_EMPTY_TILE ||
                sourceId >= PLATFORMER_ENEMY_TILE_COUNT || sourceId == 73U ||
                sourceId == 79U || sourceId == 83U || sourceId == 85U ||
                sourceId == 91U || sourceId == 490U || sourceId == 492U ||
                sourceId == 496U) {
                continue;
            }
            const uint32_t spawnIndex =
                static_cast<uint32_t>(row) * level->width + column;
            const uint16_t wordIndex =
                static_cast<uint16_t>(spawnIndex >> 5U);
            const uint32_t mask = 1UL << (spawnIndex & 31U);
            if (wordIndex >= CAMPAIGN_SPAWN_WORDS ||
                (campaignEnemySpawned_[wordIndex] & mask) != 0U) {
                continue;
            }

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
                continue;
            }

            campaignEnemySpawned_[wordIndex] |= mask;
            EnemyActor& enemy = enemies_[slot];
            enemy = EnemyActor{};
            const uint16_t reference =
                PLATFORMER_ENEMY_REFERENCE_IDS[sourceId];
            enemy.type = campaignEnemyType(reference);
            enemy.sourceTileId = sourceId;
            enemy.actor.x = static_cast<float>(column * TILE_SIZE);
            enemy.actor.y = static_cast<float>(row * TILE_SIZE);
            enemy.actor.vx = -ENEMY_SPEED;
            enemy.actor.active = true;
            enemy.spawned = true;
            if (enemy.type == PlatformerEnemyType::Koopa ||
                enemy.type == PlatformerEnemyType::KoopaParatroopa) {
                enemy.actor.y += 8.0F;
                enemy.height = 24.0F;
                enemy.actor.vx = -KOOPA_SPEED;
            } else if (enemy.type == PlatformerEnemyType::HammerBro ||
                       enemy.type == PlatformerEnemyType::Bowser) {
                enemy.height = 32.0F;
                if (enemy.type == PlatformerEnemyType::Bowser) {
                    enemy.width = 32.0F;
                    enemy.health = 5U;
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
                enemy.actor.vx = -150.0F;
            } else if (enemy.type == PlatformerEnemyType::CheepCheep ||
                       enemy.type == PlatformerEnemyType::Blooper ||
                       enemy.type == PlatformerEnemyType::Lakitu) {
                enemy.actor.vx = -55.0F;
                enemy.actor.vy = enemy.type == PlatformerEnemyType::CheepCheep
                                     ? -35.0F
                                     : 0.0F;
                if (enemy.type != PlatformerEnemyType::CheepCheep) {
                    enemy.height = 32.0F;
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
            if (enemy.type == PlatformerEnemyType::BuzzyBeetle ||
                enemy.type == PlatformerEnemyType::LavaBubble ||
                enemy.type == PlatformerEnemyType::BulletBill) {
                projectile.exploding = true;
                projectile.ageMs = 0;
                break;
            }
            if (enemy.type == PlatformerEnemyType::Bowser && enemy.health > 1U) {
                --enemy.health;
                projectile.exploding = true;
                projectile.ageMs = 0;
                break;
            }
            defeatEnemy(enemy, enemy.type == PlatformerEnemyType::Bowser
                                   ? 5000U
                                   : 200U,
                        true);
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
            addScore(200, static_cast<float>(column * TILE_SIZE),
                     static_cast<float>(row * TILE_SIZE));
            queueEvent(PlatformerEventType::CoinBoxHit);
            if (coinsCollected_ >= 100U) {
                coinsCollected_ = 0U;
                lives_ = static_cast<uint8_t>(
                    std::min<uint16_t>(99U, static_cast<uint16_t>(lives_) + 1U));
                queueEvent(PlatformerEventType::OneUp, lives_);
            }
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
        const PlatformerBlockHitResult hit = levelRuntime_.hitBlock(
            column, row, playerPower_ != PlatformerPlayerPower::Small);
        if (!hit.accepted) {
            return;
        }
        player_.y = blockY + TILE_SIZE;
        player_.vy = 10.0F;
        PlatformerBox bumped;
        bumped.x = static_cast<int16_t>(blockX);
        bumped.y = static_cast<int16_t>(blockY);
        bumpEnemiesAbove(bumped);

        if (hit.broken) {
            for (int8_t xSign : {-1, 1}) {
                for (int8_t ySign : {-1, 1}) {
                    spawnEffect(PlatformerEffectKind::BrickPiece,
                                blockX + (xSign > 0 ? 8.0F : 0.0F),
                                blockY + (ySign > 0 ? 8.0F : 0.0F),
                                xSign * 66.0F,
                                ySign < 0 ? -235.0F : -155.0F);
                }
            }
            addScore(50, blockX, blockY);
            queueEvent(PlatformerEventType::BrickBroken, 50);
            return;
        }

        switch (hit.reward) {
            case PlatformerRuntimeReward::Coin:
                ++coinsCollected_;
                addScore(200, blockX, blockY - 8.0F);
                spawnEffect(PlatformerEffectKind::RisingCoin,
                            blockX + 4.0F, blockY - 16.0F, 0.0F, -220.0F);
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
        queueEvent(PlatformerEventType::CoinBoxHit);
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
    bool hasStompCandidate = false;
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        const EnemyActor& enemy = enemies_[index];
        if (!enemy.actor.active ||
            enemy.motion == PlatformerEnemyMotion::Squashed ||
            enemy.motion == PlatformerEnemyMotion::Defeated ||
            !overlaps(collisionX, collisionY, collisionWidth, collisionHeight,
                      enemy.actor.x, enemy.actor.y, enemy.width,
                      enemy.height)) {
            continue;
        }
        const bool stompable =
            enemy.type != PlatformerEnemyType::PiranhaPlant &&
            enemy.type != PlatformerEnemyType::Spiny &&
            enemy.type != PlatformerEnemyType::BulletBill &&
            enemy.type != PlatformerEnemyType::LavaBubble &&
            enemy.type != PlatformerEnemyType::Bowser;
        hasStompCandidate |= stompable && descending &&
                             previousBottom <= enemy.actor.y + 5.0F;
    }
    bool stomped = false;
    float bounceY = player_.y;
    for (uint8_t index = 0; index < enemyCount_; ++index) {
        EnemyActor& enemy = enemies_[index];
        if (!enemy.actor.active ||
            enemy.motion == PlatformerEnemyMotion::Squashed ||
            enemy.motion == PlatformerEnemyMotion::Defeated ||
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
            queueEvent(PlatformerEventType::ShellKicked);
            continue;
        }
        const bool stompable =
            enemy.type != PlatformerEnemyType::PiranhaPlant &&
            enemy.type != PlatformerEnemyType::Spiny &&
            enemy.type != PlatformerEnemyType::BulletBill &&
            enemy.type != PlatformerEnemyType::LavaBubble &&
            enemy.type != PlatformerEnemyType::Bowser;
        if (stompable && descending &&
            previousBottom <= enemy.actor.y + 5.0F) {
            if (enemy.motion == PlatformerEnemyMotion::Walking) {
                if (enemy.type == PlatformerEnemyType::KoopaParatroopa) {
                    enemy.type = PlatformerEnemyType::Koopa;
                    enemy.sourceTileId = enemy.sourceTileId >= 2U
                                             ? enemy.sourceTileId - 2U
                                             : enemy.sourceTileId;
                    enemy.actor.vy = 0.0F;
                } else if (enemy.type == PlatformerEnemyType::Koopa ||
                           enemy.type == PlatformerEnemyType::BuzzyBeetle) {
                    const float previousHeight = enemy.height;
                    enemy.motion = PlatformerEnemyMotion::ShellIdle;
                    enemy.height = 16.0F;
                    enemy.actor.y += previousHeight - enemy.height;
                    enemy.actor.vx = 0.0F;
                } else {
                    enemy.motion = PlatformerEnemyMotion::Squashed;
                    enemy.stateMs = 300;
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
                queueEvent(PlatformerEventType::ShellKicked);
            } else if (enemy.motion == PlatformerEnemyMotion::ShellSliding) {
                enemy.motion = PlatformerEnemyMotion::ShellIdle;
                enemy.actor.vx = 0.0F;
                queueEvent(PlatformerEventType::ShellKicked);
            }
            bounceY = stomped
                          ? std::min(bounceY,
                                     enemy.actor.y - collisionHeight)
                          : enemy.actor.y - collisionHeight;
            stomped = true;
            continue;
        }
        if (hasStompCandidate) {
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
    if (hurtInvincibleMs_ > 0 || starInvincibleMs_ > 0 ||
        phase_ != PlatformerPhase::Running) {
        return;
    }
    if (playerPower_ != PlatformerPlayerPower::Small) {
        const float previousBottom = player_.y + playerHeight();
        playerPower_ = PlatformerPlayerPower::Small;
        playerCrouching_ = false;
        player_.y = previousBottom - PLAYER_HEIGHT;
        powerTransitionMs_ = DAMAGE_CONTROL_LOCK_MS;
        hurtInvincibleMs_ = DAMAGE_INVINCIBLE_MS;
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
    player_.x = flagClimbPlayerX();
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

float PlatformerEngine::flagClimbPlayerX() const {
    return campaignMode_ ? flagX_ : goalX_ - playerWidth();
}

void PlatformerEngine::beginCastleClear() {
    if (phase_ != PlatformerPhase::Running) {
        return;
    }
    phase_ = PlatformerPhase::CastleBridge;
    phaseElapsedMs_ = 0;
    bridgeRemovedCount_ = 0;
    player_.vx = 0.0F;
    player_.vy = 0.0F;
    player_.grounded = true;
    queueEvent(PlatformerEventType::ReachedGoal);
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
    powerup.vx = kind == PlatformerPowerupKind::FireFlower
                     ? 0.0F
                     : kind == PlatformerPowerupKind::Star ? STAR_SPEED
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
