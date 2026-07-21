#pragma once

#include "core/App.h"

class BreakoutApp final : public IApp {
public:
    AppId id() const override;
    const char* name() const override;
    void onEnter(AppContext& context) override;
    void onExit(AppContext& context) override;
    void onCommand(const AppCommand& command, AppContext& context) override;
    void onTick(uint32_t nowMs, AppContext& context) override;
    lv_obj_t* onCreateView(AppContext& context) override;
    void onUpdateView(AppContext& context) override;

private:
    enum class Phase : uint8_t {
        Title,
        Running,
        Paused,
        Serve,
        LevelClear,
        GameOver,
        Victory,
    };

    struct Particle {
        float x = 0.0F;
        float y = 0.0F;
        float vx = 0.0F;
        float vy = 0.0F;
        uint32_t bornMs = 0;
        lv_color_t color{};
        bool active = false;
    };

    static constexpr uint8_t BRICK_ROWS = 6;
    static constexpr uint8_t BRICK_COLUMNS = 12;
    static constexpr uint8_t LEVEL_COUNT = 3;
    static constexpr uint8_t LEADERBOARD_COUNT = 5;
    static constexpr uint8_t PARTICLE_COUNT = 30;
    static constexpr int16_t PLAY_X = 8;
    static constexpr int16_t PLAY_Y = 28;
    static constexpr int16_t PLAY_WIDTH = 304;
    static constexpr int16_t PLAY_HEIGHT = 186;
    static constexpr int16_t BRICK_X = 10;
    static constexpr int16_t BRICK_Y = 40;
    static constexpr int16_t BRICK_WIDTH = 23;
    static constexpr int16_t BRICK_HEIGHT = 12;
    static constexpr int16_t BRICK_GAP_X = 2;
    static constexpr int16_t BRICK_GAP_Y = 3;
    static constexpr int16_t PADDLE_Y = 199;
    static constexpr int16_t PADDLE_HEIGHT = 7;
    static constexpr float BALL_RADIUS = 3.5F;
    static constexpr float PADDLE_SPEED = 210.0F;
    static constexpr float BASE_BALL_SPEED = 112.0F;
    static constexpr float MAX_BALL_SPEED = 178.0F;
    static constexpr uint32_t PHYSICS_STEP_MS = 8;
    static constexpr uint32_t MAX_FRAME_MS = 48;
    static constexpr uint32_t LEVEL_CLEAR_MS = 900;
    static constexpr uint32_t PARTICLE_LIFETIME_MS = 360;
    static constexpr int16_t ANALOG_THRESHOLD = 128;

    Phase phase_ = Phase::Title;
    uint8_t bricks_[BRICK_ROWS][BRICK_COLUMNS] = {};
    uint8_t remainingBricks_ = 0;
    uint8_t level_ = 1;
    uint8_t lives_ = 3;
    uint8_t combo_ = 0;
    uint16_t score_ = 0;
    uint16_t bestScore_ = 0;
    uint16_t leaderboard_[LEADERBOARD_COUNT] = {};
    bool scoreSaved_ = false;
    float paddleX_ = 134.0F;
    float paddleWidth_ = 52.0F;
    float paddleDirection_ = 0.0F;
    float ballX_ = 160.0F;
    float ballY_ = 193.0F;
    float ballVx_ = 0.0F;
    float ballVy_ = 0.0F;
    uint32_t lastTickMs_ = 0;
    uint32_t physicsAccumulatorMs_ = 0;
    uint32_t phaseUntilMs_ = 0;
    uint32_t randomState_ = 0xB4EA4C01U;
    Particle particles_[PARTICLE_COUNT] = {};
    uint8_t nextParticle_ = 0;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* surface_ = nullptr;
    const lv_font_t* hudFont_ = nullptr;
    const lv_font_t* overlayFont_ = nullptr;
    lv_color_t backgroundColor_{};
    lv_color_t panelColor_{};
    lv_color_t gridColor_{};
    lv_color_t textColor_{};
    lv_color_t mutedColor_{};
    lv_color_t accentColor_{};
    lv_color_t brickColors_[BRICK_ROWS]{};

    static void drawEvent(lv_event_t* event);
    void draw(lv_event_t* event);
    void resetGame();
    void startGame(uint32_t nowMs);
    void buildLevel();
    void advanceLevel(uint32_t nowMs);
    void serveBall(uint32_t nowMs);
    void resetBall();
    void samplePaddle(const GamepadSnapshot& gamepad);
    bool movePaddle(float distance);
    void stepPhysics(float deltaSeconds, uint32_t nowMs, AppContext& context);
    bool hitBrick(float previousX, float previousY, uint32_t nowMs,
                  AppContext& context);
    void bounceFromPaddle(AppContext& context);
    void speedBall(float factor);
    void loseLife(AppContext& context);
    void finishLevel(uint32_t nowMs, AppContext& context);
    void spawnBrickParticles(uint8_t row, uint8_t column, uint32_t nowMs);
    bool updateParticles(uint32_t nowMs, float deltaSeconds);
    void saveHighScore(AppContext& context);
    void loadLeaderboard(AppContext& context);
    uint32_t nextRandom();
    void invalidate();
};
