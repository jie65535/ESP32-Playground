#pragma once

#include "core/App.h"

class TetrisApp final : public IApp {
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
    enum class Phase : uint8_t { Title, Running, Paused, GameOver };

    struct Piece {
        uint8_t type = 0;
        uint8_t rotation = 0;
        int8_t x = 3;
        int8_t y = 0;
    };

    static constexpr uint8_t BOARD_WIDTH = 10;
    static constexpr uint8_t BOARD_HEIGHT = 20;
    static constexpr uint8_t LEADERBOARD_COUNT = 5;
    static constexpr int16_t CELL_SIZE = 9;
    static constexpr int16_t BOARD_X = 115;
    static constexpr int16_t BOARD_Y = 6;
    static constexpr uint32_t INITIAL_GRAVITY_MS = 650;
    static constexpr uint32_t MIN_GRAVITY_MS = 90;
    static constexpr uint32_t SOFT_DROP_INTERVAL_MS = 45;
    static constexpr uint32_t HORIZONTAL_INITIAL_REPEAT_MS = 180;
    static constexpr uint32_t HORIZONTAL_REPEAT_INTERVAL_MS = 85;
    static constexpr uint32_t CLEAR_EFFECT_DURATION_MS = 320;
    static constexpr uint32_t CLEAR_EFFECT_STAGGER_MS = 14;
    static constexpr uint8_t CLEAR_EFFECT_FRAGMENT_COUNT = 3;
    static constexpr int16_t ANALOG_THRESHOLD = 128;

    Phase phase_ = Phase::Title;
    uint8_t board_[BOARD_HEIGHT][BOARD_WIDTH] = {};
    Piece current_{};
    uint8_t nextType_ = 0;
    uint8_t bag_[7] = {};
    uint8_t bagIndex_ = 7;
    uint32_t randomState_ = 0x7E57C0DEU;
    uint32_t nextGravityMs_ = 0;
    uint32_t nextSoftDropMs_ = 0;
    uint32_t nextHorizontalRepeatMs_ = 0;
    uint32_t gravityIntervalMs_ = INITIAL_GRAVITY_MS;
    uint16_t score_ = 0;
    uint16_t lines_ = 0;
    uint8_t level_ = 1;
    uint16_t bestScore_ = 0;
    uint16_t leaderboard_[LEADERBOARD_COUNT] = {};
    uint8_t clearEffectRows_[4] = {};
    uint8_t clearEffectCount_ = 0;
    uint8_t clearEffectAnchorX_ = BOARD_WIDTH / 2U;
    uint32_t clearEffectStartMs_ = 0;
    uint32_t clearEffectUntilMs_ = 0;
    int8_t analogXSign_ = 0;
    int8_t analogYSign_ = 0;

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
    lv_color_t pieceColors_[8]{};

    static void drawEvent(lv_event_t* event);
    void draw(lv_event_t* event);
    void drawRect(lv_layer_t* layer, const lv_area_t& area,
                  lv_color_t color, int32_t radius = 0,
                  lv_opa_t opacity = LV_OPA_COVER) const;
    void drawText(lv_layer_t* layer, const char* text, lv_area_t area,
                  lv_color_t color, const lv_font_t* font,
                  lv_text_align_t align = LV_TEXT_ALIGN_CENTER) const;
    void drawClearEffect(lv_layer_t* layer, const lv_area_t& board,
                         uint32_t elapsedMs) const;

    void resetGame();
    void startGame(uint32_t nowMs);
    void spawnPiece();
    void refillBag();
    void lockPiece(AppContext& context);
    void hardDrop(AppContext& context);
    void dropOne(uint32_t nowMs, AppContext& context, bool softDrop);
    uint8_t clearLines();
    void applyClearedLines();
    void finishClearEffect(AppContext& context);
    bool tryMove(int8_t dx, int8_t dy);
    bool tryRotate();
    bool collides(const Piece& piece) const;
    bool hasCell(uint8_t type, uint8_t rotation, uint8_t row,
                 uint8_t column) const;
    void sampleAnalog(const GamepadSnapshot& gamepad, uint32_t nowMs);
    void saveHighScore(AppContext& context);
    void loadLeaderboard(AppContext& context);
    uint32_t nextRandom();
    void invalidate();
};
