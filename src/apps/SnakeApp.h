#pragma once

#include "core/App.h"

class SnakeApp final : public IApp {
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
        GameOver,
    };

    enum class Direction : uint8_t {
        Up,
        Down,
        Left,
        Right,
    };

    struct Cell {
        int8_t x = 0;
        int8_t y = 0;
    };

    static constexpr uint8_t GRID_WIDTH = 20;
    static constexpr uint8_t GRID_HEIGHT = 12;
    static constexpr uint8_t MAX_LENGTH = GRID_WIDTH * GRID_HEIGHT;
    static constexpr uint8_t LEADERBOARD_COUNT = 5;
    static constexpr int16_t CELL_SIZE = 15;
    static constexpr int16_t BOARD_X = 10;
    static constexpr int16_t BOARD_Y = 30;
    static constexpr int16_t BOARD_WIDTH = GRID_WIDTH * CELL_SIZE;
    static constexpr int16_t BOARD_HEIGHT = GRID_HEIGHT * CELL_SIZE;
    static constexpr uint32_t INITIAL_STEP_MS = 220;
    static constexpr uint32_t FASTEST_STEP_MS = 90;
    static constexpr uint8_t SPEEDUP_FOOD_COUNT = 4;
    static constexpr uint32_t SPEEDUP_STEP_MS = 10;
    static constexpr int16_t ANALOG_DIRECTION_THRESHOLD = 128;
    static constexpr int16_t ANALOG_DIRECTION_MARGIN = 64;

    Phase phase_ = Phase::Title;
    Direction direction_ = Direction::Right;
    Direction queuedDirection_ = Direction::Right;
    Cell body_[MAX_LENGTH] = {};
    Cell food_ = {};
    uint8_t length_ = 4;
    uint16_t score_ = 0;
    uint16_t bestScore_ = 0;
    uint16_t leaderboard_[LEADERBOARD_COUNT] = {};
    uint32_t stepIntervalMs_ = INITIAL_STEP_MS;
    uint32_t nextStepMs_ = 0;
    uint32_t randomState_ = 0x51A7C0DEU;
    bool won_ = false;
    bool turnAcceptedForStep_ = false;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* surface_ = nullptr;
    const lv_font_t* hudFont_ = nullptr;
    const lv_font_t* overlayFont_ = nullptr;
    lv_color_t backgroundColor_{};
    lv_color_t panelColor_{};
    lv_color_t gridColor_{};
    lv_color_t snakeColor_{};
    lv_color_t snakeHeadColor_{};
    lv_color_t foodColor_{};
    lv_color_t textColor_{};
    lv_color_t mutedColor_{};
    lv_color_t accentColor_{};

    static void drawEvent(lv_event_t* event);
    void draw(lv_event_t* event);
    void resetGame();
    void startGame(uint32_t nowMs);
    void stepGame(uint32_t nowMs, AppContext& context);
    void finishGame(bool won, AppContext& context);
    void saveHighScore(AppContext& context);
    void loadLeaderboard(AppContext& context);
    void spawnFood();
    void queueDirection(Direction direction);
    void sampleAnalogDirection(const GamepadSnapshot& gamepad);
    bool occupied(Cell cell, uint8_t count) const;
    bool isOpposite(Direction first, Direction second) const;
    Cell nextHead() const;
    uint32_t nextRandom();
    void invalidate();
};
