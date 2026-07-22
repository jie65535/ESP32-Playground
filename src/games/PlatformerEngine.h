#pragma once

#include "games/PlatformerAnimation.h"
#include "games/PlatformerLevel.h"
#include "games/PlatformerLevelRuntime.h"

#include <cstdint>

namespace pgos {

enum class PlatformerPhase : uint8_t {
    Title,
    Running,
    Paused,
    Dying,
    Warping,
    VineClimb,
    CastleBridge,
    Flagpole,
    CastleWalk,
    TimeBonus,
    LevelTransition,
    GameOver,
    Won,
};

enum class PlatformerDeathReason : uint8_t {
    None,
    Enemy,
    Fall,
    Time,
};

enum class PlatformerPlayerPower : uint8_t {
    Small,
    Big,
    Fire,
};

enum class PlatformerPowerTransition : uint8_t {
    None,
    Grow,
    Shrink,
    Fire,
};

enum class PlatformerEventType : uint8_t {
    Jumped,
    SwimStroke,
    BlockHit,
    CoinCollected,
    BrickBroken,
    PowerupAppeared,
    PowerupCollected,
    PlayerHurt,
    EnemyStomped,
    EnemyDefeated,
    FireballShot,
    FireballHit,
    CannonFired,
    TrampolineBounced,
    BowserFire,
    BowserFell,
    CastleClear,
    TimerTick,
    Paused,
    OneUp,
    PlayerDied,
    GameOver,
    LifeRestarted,
    ReachedGoal,
    CourseClear,
    WarpStarted,
    WarpCompleted,
};

struct PlatformerInput {
    float moveAxis = 0.0F;
    bool jumpPressed = false;
    bool jumpHeld = false;
    bool crouchHeld = false;
    bool actionPressed = false;
    bool actionHeld = false;
};

struct PlatformerEvent {
    PlatformerEventType type = PlatformerEventType::Jumped;
    uint32_t score = 0;
    uint16_t value = 0;
};

struct PlatformerBox {
    int16_t x = 0;
    int16_t y = 0;
    PlatformerBoxReward reward = PlatformerBoxReward::None;
    bool opened = false;
    PlatformerObjectType type = PlatformerObjectType::CoinBox;
    bool visible = true;
    int8_t bumpOffset = 0;
    uint8_t remainingUses = 0;
};

enum class PlatformerPowerupKind : uint8_t {
    Mushroom,
    FireFlower,
    Star,
    OneUp,
};

enum class PlatformerPowerupState : uint8_t {
    Emerging,
    Moving,
    Bouncing,
    Resting,
};

struct PlatformerPowerup {
    PlatformerPowerupKind kind = PlatformerPowerupKind::Mushroom;
    PlatformerPowerupState state = PlatformerPowerupState::Emerging;
    float x = 0.0F;
    float y = 0.0F;
    float vx = 0.0F;
    float vy = 0.0F;
    uint16_t ageMs = 0;
    uint16_t stateFrames = 0;
    uint32_t bornFrame = 0;
    uint8_t animationFrame = 0;
    uint8_t animationTimer = 0;
    bool active = false;
};

enum class PlatformerEnemyMotion : uint8_t {
    Walking,
    Squashed,
    ShellIdle,
    ShellSliding,
    FallingDefeated,
    Defeated,
};

struct PlatformerEnemyState {
    PlatformerEnemyType type = PlatformerEnemyType::Goomba;
    PlatformerEnemyMotion motion = PlatformerEnemyMotion::Walking;
    float x = 0.0F;
    float y = 0.0F;
    float vx = 0.0F;
    float vy = 0.0F;
    float width = 16.0F;
    float height = 16.0F;
    uint16_t stateMs = 0;
    uint16_t sourceTileId = 0;
    uint32_t bornFrame = 0;
    uint8_t animationFrame = 0;
    float heldHammerX = 0.0F;
    float heldHammerY = 0.0F;
    bool active = false;
    bool facingLeft = false;
    bool verticalFlipped = false;
    bool alternatePose = false;
    bool heldHammer = false;
    bool flyingCheep = false;
};

enum class PlatformerEffectKind : uint8_t {
    RisingCoin,
    BrickPiece,
    Score,
    OneUp,
};

struct PlatformerEffect {
    PlatformerEffectKind kind = PlatformerEffectKind::Score;
    float x = 0.0F;
    float y = 0.0F;
    float vx = 0.0F;
    float vy = 0.0F;
    uint16_t value = 0;
    uint16_t ageMs = 0;
    uint32_t bornFrame = 0;
    uint8_t animationFrame = 0;
    uint8_t animationTimer = 0;
    bool active = false;
};

struct PlatformerProjectile {
    float x = 0.0F;
    float y = 0.0F;
    float vx = 0.0F;
    float vy = 0.0F;
    uint16_t ageMs = 0;
    uint8_t explosionFrames = 0;
    uint32_t bornFrame = 0;
    bool active = false;
    bool exploding = false;
};

struct PlatformerMovingPlatformState {
    float x = 0.0F;
    float y = 0.0F;
    float vx = 0.0F;
    float vy = 0.0F;
    float accelerationY = 0.0F;
    float minimum = 0.0F;
    float maximum = 0.0F;
    float pulleyTop = 0.0F;
    uint16_t sourceTileId = PLATFORMER_EMPTY_TILE;
    PlatformerMotionType motion = PlatformerMotionType::None;
    PlatformerDirection direction = PlatformerDirection::None;
    uint8_t widthTiles = 0;
    int8_t pairIndex = -1;
    bool pulley = false;
    bool detachedFalling = false;
    bool cloudPlatform = false;
    bool triggered = false;
    bool active = false;
};

struct PlatformerFireBarState {
    float x = 0.0F;
    float y = 0.0F;
    float angleDegrees = 0.0F;
    uint8_t length = 0;
    uint8_t timerFrames = 0;
    uint8_t animationFrames[12] = {};
    uint8_t animationTimers[12] = {};
    PlatformerRotationDirection direction =
        PlatformerRotationDirection::None;
    bool active = false;
};

struct PlatformerVineState {
    float x = 0.0F;
    float baseY = 0.0F;
    float grownPixels = 0.0F;
    bool active = false;
};

enum class PlatformerEnemyHazardKind : uint8_t {
    Hammer,
    BowserFire,
};

struct PlatformerEnemyHazardState {
    PlatformerEnemyHazardKind kind = PlatformerEnemyHazardKind::Hammer;
    float x = 0.0F;
    float y = 0.0F;
    float vx = 0.0F;
    float vy = 0.0F;
    float accelerationY = 0.0F;
    uint16_t sourceTileId = 0;
    uint16_t ageMs = 0;
    uint32_t bornFrame = 0;
    uint8_t animationFrame = 0;
    uint8_t animationTimer = 0;
    bool active = false;
};

struct PlatformerSnapshot {
    PlatformerPhase phase = PlatformerPhase::Title;
    PlatformerDeathReason deathReason = PlatformerDeathReason::None;
    PlatformerPlayerPower playerPower = PlatformerPlayerPower::Small;
    float playerX = 0.0F;
    float playerY = 0.0F;
    float playerVx = 0.0F;
    float playerVy = 0.0F;
    float cameraX = 0.0F;
    float cameraY = 0.0F;
    float flagX = 0.0F;
    float flagY = 0.0F;
    uint16_t flagTileId = PLATFORMER_EMPTY_TILE;
    bool grounded = false;
    bool playerBig = false;
    bool playerFire = false;
    bool playerCrouching = false;
    bool playerFacingLeft = false;
    bool playerSkidding = false;
    bool playerRunning = false;
    bool playerWalking = false;
    bool underwater = false;
    uint8_t swimStrokeFrame = 0;
    uint8_t playerAnimationFrame = 0;
    bool playerVisible = true;
    bool playerInvincible = false;
    bool playerDamageBlinking = false;
    bool playerFireballPose = false;
    PlatformerPowerTransition playerPowerTransition =
        PlatformerPowerTransition::None;
    uint16_t playerPowerTransitionFrame = 0;
    bool goalReached = false;
    bool mapTestMode = false;
    bool campaignMode = false;
    uint32_t score = 0;
    uint32_t logicFrame = 0;
    uint32_t animationFrame = 0;
    uint16_t timeRemaining = 400;
    uint16_t phaseElapsedMs = 0;
    uint16_t phaseFrames = 0;
    uint16_t lives = 3;
    uint16_t coinsCollected = 0;
    uint8_t totalBoxes = 0;
    uint8_t enemyCount = 0;
    uint8_t powerupCount = 0;
    uint8_t effectCount = 0;
    uint8_t projectileCount = 0;
    uint8_t movingPlatformCount = 0;
    uint8_t fireBarCount = 0;
    bool vineActive = false;
    uint8_t enemyHazardCount = 0;
    uint8_t world = 1;
    uint8_t stage = 1;
};

class PlatformerEngine final {
public:
    static constexpr uint8_t TILE_SIZE = 16;
    static constexpr uint16_t MAP_WIDTH = 212;
    static constexpr uint8_t MAP_HEIGHT = 14;
    static constexpr uint16_t WORLD_WIDTH = MAP_WIDTH * TILE_SIZE;
    static constexpr uint16_t WORLD_HEIGHT = MAP_HEIGHT * TILE_SIZE;
    static constexpr uint16_t VIEWPORT_WIDTH = 320;
    static constexpr uint16_t VIEWPORT_HEIGHT = 240;
    static constexpr uint8_t MAX_BOXES = 48;
    // The reference creates every CSV enemy when a level is loaded. Campaign
    // data currently peaks at 57 static enemies (World 8-1); the remaining
    // slots are reserved for cannon bullets and Lakitu's spawned Spinies.
    static constexpr uint8_t MAX_LEVEL_ENEMIES = 57;
    static constexpr uint8_t MAX_DYNAMIC_ENEMIES = 23;
    static constexpr uint8_t MAX_ENEMIES =
        MAX_LEVEL_ENEMIES + MAX_DYNAMIC_ENEMIES;
    static constexpr uint8_t MAX_POWERUPS = 6;
    static constexpr uint8_t MAX_EFFECTS = 20;
    static constexpr uint8_t MAX_PROJECTILES = 2;
    static constexpr uint8_t MAX_MOVING_PLATFORMS = 20;
    static constexpr uint8_t MAX_FIRE_BARS = 12;
    static constexpr uint8_t MAX_ENEMY_HAZARDS = 12;

    static constexpr float PLAYER_WIDTH = 16.0F;
    static constexpr float PLAYER_HEIGHT = 16.0F;
    static constexpr float BIG_PLAYER_WIDTH = 16.0F;
    static constexpr float BIG_PLAYER_HEIGHT = 32.0F;
    // The reference keeps big Mario's two-tile render box and moves a
    // one-tile collision box into its lower half while crouching.
    static constexpr float CROUCH_PLAYER_HEIGHT = PLAYER_HEIGHT;
    static constexpr float TILE_COLLISION_ROUNDNESS = 2.0F;
    static constexpr float ENEMY_WIDTH = 16.0F;
    static constexpr float ENEMY_HEIGHT = 16.0F;
    PlatformerEngine();

    void reset();
    void start();
    bool startCampaign(uint8_t world = 1, uint8_t stage = 1);
    bool prepareCampaignTitle(uint8_t world = 1, uint8_t stage = 1);
    bool startPreparedCampaign();
    bool advanceCampaign();
    void startMapTest();
    void advanceMapTest();
    void togglePause();
    void step(float deltaSeconds, const PlatformerInput& input);

    PlatformerPhase phase() const;
    PlatformerSnapshot snapshot() const;
    PlatformerBox box(uint8_t index) const;
    PlatformerEnemyState enemy(uint8_t index) const;
    PlatformerPowerup powerup(uint8_t index) const;
    PlatformerEffect effect(uint8_t index) const;
    PlatformerProjectile projectile(uint8_t index) const;
    PlatformerMovingPlatformState movingPlatform(uint8_t index) const;
    PlatformerFireBarState fireBar(uint8_t index) const;
    PlatformerVineState vine() const;
    PlatformerEnemyHazardState enemyHazard(uint8_t index) const;
    float goalX() const;
    float castleX() const;
    const PlatformerLevelRuntime& levelRuntime() const;

    bool pollEvent(PlatformerEvent& event);

#if defined(PGOS_PLATFORMER_TESTING)
    void debugSetPlayer(float x, float y, float vx = 0.0F,
                        float vy = 0.0F, bool grounded = false);
    void debugSetCamera(float x, float y = 0.0F);
    void debugSetPlayerPower(PlatformerPlayerPower power);
    void debugHitBox(uint8_t index);
    void debugSpawnPowerup(PlatformerPowerupKind kind, float x, float y);
    void debugActivateEnemy(uint8_t index, float x, float y,
                            PlatformerEnemyMotion motion);
    void debugSpawnCampaignEnemy(PlatformerEnemyType type, float x, float y,
                                 uint16_t sourceTileId);
    void debugSetEnemyVelocity(uint8_t index, float vx, float vy,
                               float accelerationY = 0.0F);
    void debugSpawnProjectile(float x, float y, float vx, float vy);
    void debugSpawnEnemyHazard(PlatformerEnemyHazardKind kind, float x,
                               float y, float vx, float vy,
                               float accelerationY = 0.0F);
    void debugBeginGoal();
    void debugSetTimeRemaining(uint16_t value);
    void debugSetCoinsCollected(uint16_t value);
    void debugSetLives(uint16_t value);
    void debugSetRandomState(uint32_t value);
    void debugLoadCampaignEnemies();
    bool debugTileSolid(uint16_t column, uint8_t row) const;
#endif

private:
    struct Actor {
        float x = 0.0F;
        float y = 0.0F;
        float vx = 0.0F;
        float vy = 0.0F;
        bool grounded = false;
        bool active = true;
    };

    struct EnemyActor {
        Actor actor{};
        PlatformerEnemyType type = PlatformerEnemyType::Goomba;
        PlatformerEnemyMotion motion = PlatformerEnemyMotion::Walking;
        float activationX = 0.0F;
        float originX = 0.0F;
        float originY = 0.0F;
        float width = 16.0F;
        float height = 16.0F;
        float accelerationY = 0.0F;
        float heldHammerY = 0.0F;
        uint16_t stateMs = 0;
        uint16_t moveFrames = 0;
        uint16_t attackFrames = 0;
        uint16_t jumpFrames = 0;
        uint16_t callbackFrames = 0;
        uint16_t fireCallbackFrames = 0;
        uint16_t sourceTileId = 0;
        uint32_t bornFrame = 0;
        uint8_t animationFrame = 0;
        uint8_t animationTimer = 0;
        uint8_t spawnOrder = 0;
        uint8_t health = 1;
        uint8_t behaviorState = 0;
        uint8_t hammerBurstRemaining = 0;
        uint8_t hammerBurstFrames = 0;
        int8_t bowserMoveDirection = 0;
        bool facingLeft = false;
        bool verticalFlipped = false;
        bool spawned = false;
        bool activationSoundPending = false;
        bool flyingCheep = false;
        bool enemyLeftCollision = false;
        bool enemyRightCollision = false;
    };

    struct PendingBrickBreak {
        uint16_t column = 0;
        uint8_t row = 0;
        uint8_t legacyBox = 0;
        uint32_t bornFrame = 0;
        bool campaign = false;
        bool active = false;
    };

    struct CannonState {
        uint16_t column = 0;
        uint8_t row = 0;
        uint16_t sourceTileId = PLATFORMER_EMPTY_TILE;
    };

    static constexpr uint8_t EVENT_QUEUE_SIZE = 24;
    static constexpr uint8_t MAX_CANNONS = 16;
    static constexpr uint16_t CANNON_TIMER_FRAMES = 5U * 60U;
    static constexpr float REFERENCE_TICKS_PER_SECOND = 60.0F;
    static constexpr float REFERENCE_VELOCITY_SCALE = 30.0F;
    static constexpr float REFERENCE_FRICTION = 0.94F;
    static constexpr float REFERENCE_ACCELERATION = 0.24F;
    static constexpr float REFERENCE_WALK_MULTIPLIER =
        0.7978723404255319148936F;
    static constexpr float REFERENCE_RUN_MULTIPLIER =
        1.3297872340425531914F;
    static constexpr float WALK_SPEED = 90.0F;
    static constexpr float RUN_SPEED = 150.0F;
    static constexpr float MAX_PLAYER_SPEED = 300.0F;
    static constexpr float REFERENCE_GRAVITY = 0.575F;
    static constexpr float REFERENCE_JUMP_VELOCITY = -7.3F;
    static constexpr float REFERENCE_JUMP_ACCELERATION = -0.412F;
    static constexpr float REFERENCE_RUNNING_JUMP_ACCELERATION = -0.414F;
    static constexpr float REFERENCE_MAX_FALL_SPEED = 7.5F;
    static constexpr float GRAVITY =
        REFERENCE_GRAVITY * REFERENCE_VELOCITY_SCALE *
        REFERENCE_TICKS_PER_SECOND;
    static constexpr float MAX_FALL_SPEED =
        REFERENCE_MAX_FALL_SPEED * REFERENCE_VELOCITY_SCALE;
    static constexpr float ENEMY_SPEED = 30.0F;
    static constexpr float KOOPA_SPEED = 30.0F;
    static constexpr float SHELL_SPEED = 180.0F;
    static constexpr float POWERUP_SPEED = 60.0F;
    static constexpr float ENEMY_GROUP_SPACING = 60.0F / 2.679F;
    // The reference game renders an 800px viewport over a 2.679x background;
    // keep its logical 299px device-width when advancing the camera and
    // placing checkpoint enemies, while the LCD still displays 320px.
    static constexpr float REFERENCE_VIEWPORT_WIDTH = 299.0F;
    static constexpr float MAP_TEST_CAMERA_STEP = 256.0F;
    static constexpr float STOMP_BOUNCE_SPEED = 105.0F;
    static constexpr uint8_t LEVEL_TICK_FRAMES = 30;

    PlatformerPhase phase_ = PlatformerPhase::Title;
    PlatformerDeathReason deathReason_ = PlatformerDeathReason::None;
    Actor player_{};
    EnemyActor enemies_[MAX_ENEMIES] = {};
    PlatformerPowerup powerups_[MAX_POWERUPS] = {};
    PlatformerEffect effects_[MAX_EFFECTS] = {};
    PlatformerProjectile projectiles_[MAX_PROJECTILES] = {};
    PlatformerMovingPlatformState movingPlatforms_[MAX_MOVING_PLATFORMS] = {};
    PlatformerFireBarState fireBars_[MAX_FIRE_BARS] = {};
    CannonState cannons_[MAX_CANNONS] = {};
    PlatformerVineState vine_{};
    PlatformerEnemyHazardState enemyHazards_[MAX_ENEMY_HAZARDS] = {};
    PlatformerBox boxes_[MAX_BOXES] = {};
    uint8_t boxBumpFrames_[MAX_BOXES] = {};
    PendingBrickBreak pendingBrickBreak_{};
    uint8_t boxCount_ = 0;
    uint8_t enemyCount_ = 0;
    uint8_t powerupCount_ = 0;
    uint8_t effectCount_ = 0;
    uint8_t projectileCount_ = 0;
    uint8_t movingPlatformCount_ = 0;
    uint8_t fireBarCount_ = 0;
    uint8_t cannonCount_ = 0;
    uint16_t cannonTimerFrames_ = CANNON_TIMER_FRAMES;
    uint8_t enemyHazardCount_ = 0;
    uint16_t coinsCollected_ = 0;
    uint16_t lives_ = 3;
    uint32_t score_ = 0;
    uint16_t timeRemaining_ = 400;
    uint8_t levelClockFrames_ = 0;
    uint16_t phaseElapsedMs_ = 0;
    uint16_t phaseFrames_ = 0;
    uint16_t powerTransitionFrames_ = 0;
    uint16_t powerTransitionElapsedFrames_ = 0;
    uint16_t hurtInvincibleFrames_ = 0;
    uint16_t starInvincibleFrames_ = 0;
    uint16_t starBlinkFrames_ = 0;
    uint8_t fireballPoseFrames_ = 0;
    PlatformerPowerTransition powerTransition_ =
        PlatformerPowerTransition::None;
    bool starProtectedThisFrame_ = false;
    bool hurtProtectedThisFrame_ = false;
    uint8_t stompChain_ = 0;
    float playerAccelerationX_ = 0.0F;
    float playerAccelerationY_ = 0.0F;
    float cameraAdvanceX_ = 0.0F;
    float cameraX_ = 0.0F;
    float cameraY_ = 0.0F;
    float flagX_ = 0.0F;
    float flagY_ = 0.0F;
    uint16_t flagTileId_ = PLATFORMER_EMPTY_TILE;
    float goalX_ = 0.0F;
    float castleX_ = 0.0F;
    PlatformerPlayerPower playerPower_ = PlatformerPlayerPower::Small;
    bool jumpActive_ = false;
    bool playerCrouching_ = false;
    bool playerFacingLeft_ = false;
    bool playerSkidding_ = false;
    bool playerRunning_ = false;
    bool trampolineCollided_ = false;
    uint8_t swimStrokeFrames_ = 0;
    bool mapTestMode_ = false;
    bool campaignMode_ = false;
    bool campaignEnemiesLoaded_ = false;
    bool cameraFrozen_ = false;
    uint8_t warpState_ = 0;
    bool startIntro_ = false;
    bool vineReturnActive_ = false;
    uint8_t vineSequenceState_ = 0;
    uint8_t vineReturnFrames_ = 0;
    bool flagLanded_ = false;
    bool flagShifted_ = false;
    bool timeBonusReady_ = false;
    uint16_t timeBonusCompletionFrames_ = 0;
    uint8_t activeWarpIndex_ = 0;
    uint8_t activeVineIndex_ = 0;
    int16_t bridgeStartColumn_ = -1;
    int16_t bridgeEndColumn_ = -1;
    uint8_t bridgeRow_ = 0;
    uint8_t bridgeRemovedCount_ = 0;
    uint8_t bridgeSequenceState_ = 0;
    uint8_t bridgeStepFrames_ = 0;
    uint16_t bridgeDelayFrames_ = 0;
    uint8_t castleClearFrames_ = 0;
    PlatformerLevelType vinePreviousLevelType_ = PlatformerLevelType::None;
    PlatformerBackgroundColor vinePreviousBackground_ =
        PlatformerBackgroundColor::Black;
    PlatformerLevelRuntime levelRuntime_{};
    PlatformerEvent events_[EVENT_QUEUE_SIZE] = {};
    uint8_t eventRead_ = 0;
    uint8_t eventWrite_ = 0;
    uint8_t eventCount_ = 0;
    uint32_t logicFrame_ = 0;
    uint32_t animationFrame_ = 0;
    uint32_t randomState_ = 0x6D2B79F5UL;
    uint8_t playerAnimationMode_ = 0;
    uint8_t playerAnimationFrame_ = 0;
    uint8_t playerAnimationTimer_ = 0;

    void buildLevel();
    void resetActors();
    void resetCampaignLevel(bool resetPower, bool queueRestartEvent = true);
    void resetLife();
    void completeCourse();
    bool beginLevelTransition(uint8_t world, uint8_t stage,
                              bool resetPower = false);
    void updateRunning(float dt, uint16_t dtMs, const PlatformerInput& input);
    void updateMapTest(float dt, uint16_t dtMs);
    void updateScriptedPhase(float dt, uint16_t dtMs,
                             const PlatformerInput& input);
    void updateWarp(float dt);
    void updateVineClimb(float dt, const PlatformerInput& input);
    void updateCastleBridge(float dt);
    void updatePlayerStateTimers();
    void updatePlayerAnimation();
    void updateLevelTimer();
    void updateCamera();
    void updatePlayerHorizontal(float dt, const PlatformerInput& input);
    void updatePlayerVertical(float dt, uint16_t dtMs,
                              const PlatformerInput& input);
    void moveHorizontal(float distance);
    void moveVertical(float distance);
    bool rectHitsSolid(float x, float y, float width, float height,
                       bool includeHidden = false) const;
    bool rectHitsPlayerTiles(float x, float y, float width, float height,
                             float horizontalInset, float bottomTrim,
                             float tileHorizontalInset) const;
    bool rectHitsPlayerVertical(float x, float y, float width, float height,
                                bool rising) const;
    bool rectHitsPlayerHorizontal(float x, float y, float width,
                                  float height) const;
    void updateBoxes();
    void updateEnemies(float dt, uint16_t dtMs);
    void updateEnemyHazards(float dt, uint16_t dtMs);
    void loadCampaignEnemies();
    void updateEnemyActivationCallbacks();
    void spawnSpiny(const EnemyActor& lakitu);
    void updateHammerBroBehavior(EnemyActor& enemy);
    void updateHammerBroCallbacks(EnemyActor& enemy);
    void updateQueuedEnemyCommands(EnemyActor& enemy);
    void updatePausedCommands();
    void updateBowserBehavior(EnemyActor& enemy, float dt);
    void updateBowserCallbacks(EnemyActor& enemy);
    bool spawnEnemyHazard(PlatformerEnemyHazardKind kind, float x, float y,
                          float vx, float vy,
                          float accelerationY = 0.0F);
    uint32_t nextRandom();
    void placeEnemyAtSpawn(EnemyActor& enemy);
    void updatePowerups(float dt, uint16_t dtMs);
    void updateEffects(float dt, uint16_t dtMs);
    void updateProjectiles(float dt, uint16_t dtMs);
    void resetCampaignDynamics();
    void updateMovingPlatforms(float dt, bool carryPlayer = true);
    void updateCloudPlatformCallbacks();
    void updateTrampolines();
    void updateFireBars(float dt);
    void checkFireBarCollisions();
    void updateCannonTimers();
    bool spawnCannonBullet(const CannonState& cannon, bool movingRight);
    void updateVine(float dt, const PlatformerInput& input);
    void spawnVine(uint16_t column, uint8_t row);
    void checkVineReturn();
    void completeVineReturn();
    int8_t standingPlatform() const;
    int8_t landingPlatform(float previousBottom, float nextBottom) const;
    void collectPowerups();
    void collectMapCoins();
    bool tryEnterWarp(const PlatformerInput& input);
    void applyTeleportPoints(float previousX);
    const PlatformerWarpData* activeWarp() const;
    void resetPiranhasForWarp();
    void checkBoxCollision(float previousY, float verticalVelocity);
    void hitBox(uint8_t index);
    void breakBrick(uint8_t index);
    void queueCampaignBrickBreak(uint16_t column, uint8_t row);
    void completePendingBrickBreak();
    void bumpEnemiesAbove(const PlatformerBox& box);
    void checkEnemyCollisions(float previousBottom);
    void checkEnemyPairCollisions();
    void enemyCollisionBounds(const EnemyActor& enemy, float& x, float& y,
                              float& width, float& height) const;
    bool enemyIsCrushable(const EnemyActor& enemy) const;
    void crushEnemy(EnemyActor& enemy);
    void hurtPlayer();
    void beginDeath(PlatformerDeathReason reason);
    void beginGoal();
    float flagClimbPlayerX() const;
    void beginCastleClear();
    void spawnPowerup(PlatformerPowerupKind kind, float x, float y);
    void spawnEffect(PlatformerEffectKind kind, float x, float y,
                     float vx, float vy, uint16_t value = 0);
    void shootFireball();
    void defeatEnemy(EnemyActor& enemy, uint16_t points, bool launch);
    void addScore(uint16_t points, float x, float y);
    void queueEvent(PlatformerEventType type, uint16_t value = 0);
    void updateCrouch(bool crouchHeld);
    float playerWidth() const;
    float playerHeight() const;
    static bool overlaps(float ax, float ay, float aw, float ah,
                         float bx, float by, float bw, float bh);
};

}  // namespace pgos
