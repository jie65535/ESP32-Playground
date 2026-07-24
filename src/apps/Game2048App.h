#pragma once

#include "core/App.h"
#include "games/Game2048Ai.h"
#include "games/Game2048AiHoldPolicy.h"
#include "games/Game2048Engine.h"
#include "games/Game2048InputPolicy.h"

class Game2048App final : public IApp {
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

    static constexpr int16_t SURFACE_WIDTH = 320;
    static constexpr int16_t SURFACE_HEIGHT = 218;
    static constexpr int16_t BOARD_X = 67;
    static constexpr int16_t BOARD_Y = 32;
    static constexpr int16_t BOARD_PADDING = 5;
    static constexpr int16_t TILE_SIZE = 40;
    static constexpr int16_t TILE_GAP = 5;
    static constexpr int16_t BOARD_SIZE = BOARD_PADDING * 2 +
                                          TILE_SIZE * 4 + TILE_GAP * 3;
    static constexpr uint32_t MOVE_ANIMATION_MS = 145;
    static constexpr uint32_t TARGET_CELEBRATION_MS = 1800;
    pgos::Game2048Ai ai_;
    pgos::Game2048AiHoldPolicy aiHoldPolicy_;
    pgos::Game2048Engine engine_;
    pgos::Game2048InputPolicy inputPolicy_;
    pgos::Game2048MoveResult activeMove_{};
    Phase phase_ = Phase::Title;
    uint32_t bestScore_ = 0;
    uint32_t animationStartMs_ = 0;
    uint32_t animationUntilMs_ = 0;
    uint32_t activeAnimationMs_ = MOVE_ANIMATION_MS;
    uint32_t bannerStartMs_ = 0;
    uint32_t bannerUntilMs_ = 0;
    uint32_t nextAiMoveMs_ = 0;
    uint32_t randomSeed_ = 0x2048C0DEU;
    bool gameRecorded_ = false;
    bool pendingGameOver_ = false;
    bool aiActive_ = false;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* surface_ = nullptr;
    const lv_font_t* hudFont_ = nullptr;
    const lv_font_t* tinyFont_ = nullptr;
    const lv_font_t* titleFont_ = nullptr;
    const lv_font_t* tileFont_ = nullptr;
    const lv_font_t* tileMediumFont_ = nullptr;
    const lv_font_t* tileSmallFont_ = nullptr;
    const lv_font_t* tileCompactFont_ = nullptr;
    const lv_font_t* overlayFont_ = nullptr;
    lv_color_t backgroundColor_{};
    lv_color_t boardColor_{};
    lv_color_t gapColor_{};
    lv_color_t textColor_{};
    lv_color_t mutedColor_{};
    lv_color_t accentColor_{};

    static void drawEvent(lv_event_t* event);
    void draw(lv_event_t* event);
    void drawTile(lv_layer_t* layer, uint8_t index, uint32_t value,
                  uint16_t scale = 100, bool moving = false) const;
    void drawTileAt(lv_layer_t* layer, int16_t x, int16_t y, uint32_t value,
                    uint16_t scale = 100) const;
    void drawTargetCelebration(lv_layer_t* layer, const lv_area_t& surface,
                               uint32_t nowMs) const;
    void drawOverlay(lv_layer_t* layer, const lv_area_t& surface,
                     const char* title, const char* subtitle) const;
    void startGame(uint32_t nowMs);
    void applyMove(pgos::Game2048Direction direction, AppContext& context,
                   uint32_t nowMs,
                   uint32_t animationMs = MOVE_ANIMATION_MS);
    void finishGame(AppContext& context);
    void sampleAi(const GamepadSnapshot& gamepad, uint32_t nowMs,
                  AppContext& context);
    void sampleAnalog(const GamepadSnapshot& gamepad, uint32_t nowMs,
                      AppContext& context);
    void invalidate();
    const lv_font_t* tileNumberFont(const char* text,
                                    int16_t availableSize) const;
    static lv_color_t tileColor(uint32_t value);
    static lv_color_t tileTextColor(uint32_t value);
    static uint32_t easeOut(uint32_t progress);
};
