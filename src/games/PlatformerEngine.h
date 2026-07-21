#pragma once

#include "games/PlatformerLevel.h"

#include <cstdint>

namespace pgos {

enum class PlatformerPhase : uint8_t {
    Title,
    Running,
    Paused,
    Dying,
    Flagpole,
    CastleWalk,
    TimeBonus,
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

enum class PlatformerEventType : uint8_t {
    Jumped,
    CoinBoxHit,
    BrickBroken,
    PowerupAppeared,
    PowerupCollected,
    PlayerHurt,
    EnemyStomped,
    ShellKicked,
    EnemyDefeated,
    FireballShot,
    OneUp,
    TimeWarning,
    PlayerDied,
    LifeRestarted,
    ReachedGoal,
    CourseClear,
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
    bool active = false;
};

enum class PlatformerEnemyMotion : uint8_t {
    Walking,
    Squashed,
    ShellIdle,
    ShellSliding,
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
    bool active = false;
    bool facingLeft = false;
};

enum class PlatformerEffectKind : uint8_t {
    RisingCoin,
    BrickPiece,
    Score,
};

struct PlatformerEffect {
    PlatformerEffectKind kind = PlatformerEffectKind::Score;
    float x = 0.0F;
    float y = 0.0F;
    float vx = 0.0F;
    float vy = 0.0F;
    uint16_t value = 0;
    uint16_t ageMs = 0;
    bool active = false;
};

struct PlatformerProjectile {
    float x = 0.0F;
    float y = 0.0F;
    float vx = 0.0F;
    float vy = 0.0F;
    uint16_t ageMs = 0;
    bool active = false;
    bool exploding = false;
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
    float flagY = 0.0F;
    bool grounded = false;
    bool playerBig = false;
    bool playerFire = false;
    bool playerCrouching = false;
    bool playerFacingLeft = false;
    bool playerSkidding = false;
    bool playerVisible = true;
    bool playerInvincible = false;
    bool goalReached = false;
    bool mapTestMode = false;
    uint32_t score = 0;
    uint16_t timeRemaining = 400;
    uint16_t phaseElapsedMs = 0;
    uint8_t lives = 3;
    uint8_t coinsCollected = 0;
    uint8_t totalBoxes = 0;
    uint8_t enemyCount = 0;
    uint8_t powerupCount = 0;
    uint8_t effectCount = 0;
    uint8_t projectileCount = 0;
};

class PlatformerEngine final {
public:
    static constexpr uint8_t TILE_SIZE = 16;
    static constexpr uint16_t MAP_WIDTH = 212;
    static constexpr uint8_t MAP_HEIGHT = 14;
    static constexpr uint16_t WORLD_WIDTH = MAP_WIDTH * TILE_SIZE;
    static constexpr uint16_t WORLD_HEIGHT = MAP_HEIGHT * TILE_SIZE;
    static constexpr uint16_t VIEWPORT_WIDTH = 320;
    static constexpr uint8_t MAX_BOXES = 48;
    static constexpr uint8_t MAX_ENEMIES = 20;
    static constexpr uint8_t MAX_POWERUPS = 6;
    static constexpr uint8_t MAX_EFFECTS = 20;
    static constexpr uint8_t MAX_PROJECTILES = 2;

    static constexpr float PLAYER_WIDTH = 16.0F;
    static constexpr float PLAYER_HEIGHT = 16.0F;
    static constexpr float BIG_PLAYER_WIDTH = 16.0F;
    static constexpr float BIG_PLAYER_HEIGHT = 32.0F;
    static constexpr float CROUCH_PLAYER_HEIGHT = 22.0F;
    static constexpr float ENEMY_WIDTH = 16.0F;
    static constexpr float ENEMY_HEIGHT = 16.0F;
    PlatformerEngine();

    void reset();
    void start();
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
    float goalX() const;
    float castleX() const;

    bool pollEvent(PlatformerEvent& event);

#if defined(PGOS_PLATFORMER_TESTING)
    void debugSetPlayer(float x, float y, float vx = 0.0F,
                        float vy = 0.0F, bool grounded = false);
    void debugSetPlayerPower(PlatformerPlayerPower power);
    void debugHitBox(uint8_t index);
    void debugSpawnPowerup(PlatformerPowerupKind kind, float x, float y);
    void debugActivateEnemy(uint8_t index, float x, float y,
                            PlatformerEnemyMotion motion);
    void debugBeginGoal();
    void debugSetTimeRemaining(uint16_t value);
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
        float width = 16.0F;
        float height = 16.0F;
        uint16_t stateMs = 0;
        uint8_t spawnOrder = 0;
        bool spawned = false;
    };

    static constexpr uint8_t EVENT_QUEUE_SIZE = 24;
    static constexpr float WALK_SPEED = 134.0F;
    static constexpr float RUN_SPEED = 194.0F;
    static constexpr float WALK_ACCELERATION = 205.0F;
    static constexpr float RUN_ACCELERATION = 760.0F;
    static constexpr float TURN_ACCELERATION = 490.0F;
    static constexpr float AIR_ACCELERATION = 255.0F;
    static constexpr float JUMP_SPEED = 244.0F;
    static constexpr float FAST_JUMP_SPEED = 264.0F;
    static constexpr float GRAVITY = 1360.0F;
    static constexpr float JUMP_HOLD_GRAVITY = 417.0F;
    static constexpr float MAX_FALL_SPEED = 246.0F;
    static constexpr float ENEMY_SPEED = 45.0F;
    static constexpr float KOOPA_SPEED = 45.0F;
    static constexpr float SHELL_SPEED = 190.0F;
    static constexpr float POWERUP_SPEED = 68.0F;
    static constexpr float STAR_SPEED = 112.0F;
    static constexpr float ENEMY_GROUP_SPACING = 60.0F / 2.679F;
    // The reference game renders an 800px viewport over a 2.679x background;
    // keep its logical 299px device-width when advancing the camera and
    // placing checkpoint enemies, while the LCD still displays 320px.
    static constexpr float REFERENCE_VIEWPORT_WIDTH = 299.0F;
    static constexpr float MAP_TEST_CAMERA_STEP = 256.0F;
    static constexpr float STOMP_BOUNCE_SPEED = 157.0F;
    static constexpr uint16_t MAX_JUMP_HOLD_MS = 533;
    static constexpr uint16_t MIN_JUMP_HOLD_MS = 120;
    static constexpr uint16_t COYOTE_TIME_MS = 72;
    static constexpr uint16_t JUMP_BUFFER_MS = 96;
    static constexpr uint16_t LEVEL_TICK_MS = 400;

    PlatformerPhase phase_ = PlatformerPhase::Title;
    PlatformerDeathReason deathReason_ = PlatformerDeathReason::None;
    Actor player_{};
    EnemyActor enemies_[MAX_ENEMIES] = {};
    PlatformerPowerup powerups_[MAX_POWERUPS] = {};
    PlatformerEffect effects_[MAX_EFFECTS] = {};
    PlatformerProjectile projectiles_[MAX_PROJECTILES] = {};
    PlatformerBox boxes_[MAX_BOXES] = {};
    uint16_t boxBumpMs_[MAX_BOXES] = {};
    uint8_t boxCount_ = 0;
    uint8_t enemyCount_ = 0;
    uint8_t powerupCount_ = 0;
    uint8_t effectCount_ = 0;
    uint8_t projectileCount_ = 0;
    uint8_t coinsCollected_ = 0;
    uint8_t lives_ = 3;
    uint32_t score_ = 0;
    uint16_t timeRemaining_ = 400;
    uint16_t levelClockMs_ = 0;
    uint16_t phaseElapsedMs_ = 0;
    uint16_t jumpHoldMs_ = 0;
    uint16_t coyoteMs_ = 0;
    uint16_t jumpBufferMs_ = 0;
    uint16_t powerTransitionMs_ = 0;
    uint16_t hurtInvincibleMs_ = 0;
    uint16_t starInvincibleMs_ = 0;
    uint16_t fireCooldownMs_ = 0;
    uint8_t stompChain_ = 0;
    float cameraX_ = 0.0F;
    float flagY_ = 0.0F;
    float goalX_ = 0.0F;
    float castleX_ = 0.0F;
    PlatformerPlayerPower playerPower_ = PlatformerPlayerPower::Small;
    bool jumpActive_ = false;
    bool playerCrouching_ = false;
    bool playerFacingLeft_ = false;
    bool playerSkidding_ = false;
    bool timeWarningSent_ = false;
    bool mapTestMode_ = false;
    PlatformerEvent events_[EVENT_QUEUE_SIZE] = {};
    uint8_t eventRead_ = 0;
    uint8_t eventWrite_ = 0;
    uint8_t eventCount_ = 0;

    void buildLevel();
    void resetActors();
    void resetLife();
    void updateRunning(float dt, uint16_t dtMs, const PlatformerInput& input);
    void updateMapTest(float dt, uint16_t dtMs);
    void updateScriptedPhase(float dt, uint16_t dtMs);
    void updateTimers(uint16_t dtMs);
    void updateCamera();
    void updatePlayerHorizontal(float dt, const PlatformerInput& input);
    void updatePlayerVertical(float dt, uint16_t dtMs,
                              const PlatformerInput& input);
    void moveHorizontal(float distance);
    void moveVertical(float distance);
    bool rectHitsSolid(float x, float y, float width, float height,
                       bool includeHidden = false) const;
    void updateBoxes(uint16_t dtMs);
    void updateEnemies(float dt, uint16_t dtMs);
    void placeEnemyAtSpawn(EnemyActor& enemy);
    void updatePowerups(float dt, uint16_t dtMs);
    void updateEffects(float dt, uint16_t dtMs);
    void updateProjectiles(float dt, uint16_t dtMs);
    void collectPowerups();
    void checkBoxCollision(float previousY, float verticalVelocity);
    void hitBox(uint8_t index);
    void breakBrick(uint8_t index);
    void bumpEnemiesAbove(const PlatformerBox& box);
    void checkEnemyCollisions(float previousBottom);
    void checkEnemyPairCollisions();
    void hurtPlayer();
    void beginDeath(PlatformerDeathReason reason);
    void beginGoal();
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
