#pragma once

#include "core/App.h"
#include "games/MinesweeperEngine.h"
#include "games/MinesweeperPresentation.h"
#include "games/MinesweeperRecords.h"

class MinesweeperProfileService;

class MinesweeperApp final : public IApp {
public:
    AppId id() const override;
    const char* name() const override;
    void onEnter(AppContext& context) override;
    void onExit(AppContext& context) override;
    void onCommand(const AppCommand& command, AppContext& context) override;
    void onTick(uint32_t nowMs, AppContext& context) override;
    lv_obj_t* onCreateView(AppContext& context) override;
    void onUpdateView(AppContext& context) override;
    bool onBack(AppContext& context) override;

private:
    enum class ViewPhase : uint8_t {
        Title,
        CustomSetup,
        Statistics,
        Ready,
        Generating,
        Playing,
        Paused,
        Won,
        Lost,
    };

    enum class Direction : int8_t {
        None,
        Up,
        Down,
        Left,
        Right,
    };

    enum class AssistFeedback : uint8_t {
        None,
        FlaggedMines,
        RevealedSafe,
        NoCertainMove,
    };

    static constexpr int16_t SURFACE_WIDTH = 320;
    static constexpr int16_t SURFACE_HEIGHT = 218;
    static constexpr uint16_t MAX_CELLS = pgos::MinesweeperEngine::MAX_CELLS;
    static constexpr uint32_t REVEAL_STEP_MS = 14;
    static constexpr uint32_t REVEAL_MAX_DELAY_MS = 210;
    static constexpr uint32_t TILE_SETTLE_MS = 75;
    static constexpr uint32_t FLAG_ANIMATION_MS = 120;
    static constexpr uint32_t DOUBLE_TAP_WINDOW_MS = 260;
    static constexpr uint32_t ASSIST_FEEDBACK_MS = 700;
    static constexpr uint32_t RESULT_PANEL_DELAY_MS = 430;
    static constexpr uint32_t NAVIGATION_INITIAL_REPEAT_MS = 145;
    static constexpr uint32_t NAVIGATION_REPEAT_MS = 58;
    static constexpr uint32_t NAVIGATION_FAST_REPEAT_MS = 36;
    static constexpr uint32_t NAVIGATION_ACCELERATION_MS = 650;
    static constexpr int16_t ANALOG_THRESHOLD = 135;
    static constexpr uint16_t NAVIGATION_RATE_CELLS_PER_SECOND = 14;

    ViewPhase phase_ = ViewPhase::Title;
    ViewPhase pausedFrom_ = ViewPhase::Playing;
    pgos::MinesweeperDifficulty difficulty_ =
        pgos::MinesweeperDifficulty::Beginner;
    pgos::MinesweeperEngine engine_{};
    pgos::MinesweeperRecordUpdate recordUpdate_{};
    uint8_t difficultySelection_ = 0;
    uint8_t customSelection_ = 0;
    uint8_t resultSelection_ = 0;
    uint8_t customWidth_ = 12;
    uint8_t customHeight_ = 12;
    uint16_t customMines_ = 24;
    uint8_t cursorX_ = 0;
    uint8_t cursorY_ = 0;
    uint8_t firstX_ = 0;
    uint8_t firstY_ = 0;
    int16_t cellSize_ = 18;
    int16_t boardX_ = 0;
    int16_t boardY_ = 0;
    int16_t boardWidth_ = 0;
    int16_t boardHeight_ = 0;
    uint32_t boardSeed_ = 0;
    uint32_t gameStartedMs_ = 0;
    uint32_t pausedStartedMs_ = 0;
    uint32_t pausedAccumulatedMs_ = 0;
    uint32_t resultElapsedMs_ = 0;
    uint32_t generationStartedMs_ = 0;
    uint32_t resultStartedMs_ = 0;
    uint32_t cursorMovedMs_ = 0;
    uint32_t lastFlagStartedMs_ = 0;
    uint32_t lastPressStartedMs_ = 0;
    uint32_t animationsUntilMs_ = 0;
    uint32_t lastVisualTickMs_ = 0;
    uint32_t heldDirectionSinceMs_ = 0;
    uint32_t nextDirectionRepeatMs_ = 0;
    uint32_t lastDiscreteNavigationMs_ = 0;
    uint32_t lastNavigationSampleMs_ = 0;
    uint32_t revealAtMs_[MAX_CELLS] = {};
    int16_t lastFlagIndex_ = -1;
    int16_t lastPressIndex_ = -1;
    int8_t heldAxisX_ = 0;
    int8_t heldAxisY_ = 0;
    int32_t navigationRemainderX_ = 0;
    int32_t navigationRemainderY_ = 0;
    uint16_t sampledGamepadButtons_ = 0;
    uint32_t lastActivateCommandMs_ = 0;
    uint32_t lastFlagCommandMs_ = 0;
    uint32_t lastActivateTapMs_ = 0;
    uint32_t lastFlagTapMs_ = 0;
    uint32_t assistFeedbackStartedMs_ = 0;
    bool resultRecorded_ = false;
    bool exitArmed_ = false;
    bool gamepadButtonsInitialized_ = false;
    bool activateCommandHandled_ = false;
    bool flagCommandHandled_ = false;
    bool pendingFlagTapChanged_ = false;
    bool winCueSecondNote_ = false;
    bool winCueThirdNote_ = false;
    int16_t pendingFlagTapIndex_ = -1;
    pgos::MinesweeperVisibility pendingFlagTapPreviousVisibility_ =
        pgos::MinesweeperVisibility::Covered;
    AssistFeedback assistFeedback_ = AssistFeedback::None;
    uint16_t assistFeedbackCount_ = 0;
    const MinesweeperProfileService* profile_ = nullptr;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* surface_ = nullptr;
    const lv_font_t* tinyFont_ = nullptr;
    const lv_font_t* hudFont_ = nullptr;
    const lv_font_t* bodyFont_ = nullptr;
    const lv_font_t* strongBodyFont_ = nullptr;
    const lv_font_t* titleFont_ = nullptr;
    lv_color_t backgroundColor_{};
    lv_color_t headerColor_{};
    lv_color_t frameColor_{};
    lv_color_t coveredColor_{};
    lv_color_t coveredHighlightColor_{};
    lv_color_t coveredShadowColor_{};
    lv_color_t revealedColor_{};
    lv_color_t revealedGridColor_{};
    lv_color_t textColor_{};
    lv_color_t mutedColor_{};
    lv_color_t accentColor_{};
    lv_color_t dangerColor_{};
    lv_color_t successColor_{};
    lv_color_t numberColors_[8]{};

    static void drawEvent(lv_event_t* event);
    void draw(lv_event_t* event);
    void drawTitle(lv_layer_t* layer, const lv_area_t& area,
                   uint32_t nowMs) const;
    void drawCustomSetup(lv_layer_t* layer, const lv_area_t& area) const;
    void drawStatistics(lv_layer_t* layer, const lv_area_t& area) const;
    void drawGame(lv_layer_t* layer, const lv_area_t& area,
                  uint32_t nowMs) const;
    void drawHud(lv_layer_t* layer, const lv_area_t& area,
                 uint32_t nowMs) const;
    void drawBoard(lv_layer_t* layer, const lv_area_t& area,
                   const lv_area_t& clip, uint32_t nowMs) const;
    void drawTile(lv_layer_t* layer, const lv_area_t& tile,
                  const pgos::MinesweeperCell& cell, uint16_t index,
                  uint32_t nowMs) const;
    void drawOverlay(lv_layer_t* layer, const lv_area_t& area,
                     uint32_t nowMs) const;
    void drawMine(lv_layer_t* layer, const lv_area_t& area,
                  lv_color_t color, bool detonated = false) const;
    void drawFlag(lv_layer_t* layer, const lv_area_t& area,
                  uint32_t progressMs, bool wrong = false) const;
    void drawNumber(lv_layer_t* layer, const lv_area_t& area,
                    uint8_t number) const;
    void drawCursor(lv_layer_t* layer, const lv_area_t& tile,
                    uint32_t nowMs) const;
    void drawCounter(lv_layer_t* layer, int16_t x, int16_t y, int16_t value,
                     lv_color_t color) const;
    void drawSevenSegmentDigit(lv_layer_t* layer, int16_t x, int16_t y,
                               int8_t digit, lv_color_t color) const;

    void selectDifficulty(int8_t direction);
    void adjustCustom(int8_t direction);
    void startSelectedDifficulty(AppContext& context);
    void startNewBoard(AppContext& context, bool newSeed);
    void beginFirstReveal(uint32_t nowMs);
    void performReveal(uint32_t nowMs, AppContext& context);
    void performFlag(uint32_t nowMs, AppContext& context);
    void performKnownMineFlags(uint32_t nowMs, AppContext& context);
    void performKnownSafeReveal(uint32_t nowMs, AppContext& context);
    void handleActivate(uint32_t nowMs, AppContext& context);
    void handleActivatePress(uint32_t nowMs, AppContext& context);
    void handleFlagPress(uint32_t nowMs, AppContext& context);
    void sampleGamepadButtons(uint32_t nowMs, AppContext& context);
    void handleDirection(Direction direction, uint32_t nowMs,
                         AppContext& context, bool discrete);
    void sampleHeldDirection(uint32_t nowMs, AppContext& context);
    pgos::MinesweeperNavigationVector navigationVectorFromGamepad(
        const GamepadSnapshot& gamepad) const;
    void applyHeldNavigation(const pgos::MinesweeperNavigationVector& vector,
                             uint32_t nowMs, AppContext& context);
    void resetHeldNavigation();
    void moveCursor(Direction direction, uint32_t nowMs);
    void moveCursorBy(int8_t deltaX, int8_t deltaY, uint32_t nowMs);
    void integrateAnalogAxis(int16_t axis, uint32_t elapsedMs,
                             int8_t deltaX, int8_t deltaY,
                             int32_t& remainder,
                             uint32_t nowMs);
    void scheduleNewReveals(uint8_t originX, uint8_t originY,
                            uint32_t nowMs);
    void processOutcome(uint32_t nowMs, AppContext& context);
    void pause(uint32_t nowMs, bool exitArmed);
    void resume(uint32_t nowMs);
    void activateResultChoice(AppContext& context);
    void updateBoardLayout();
    void clearVisualState();
    void showAssistFeedback(AssistFeedback feedback, uint16_t count,
                            uint32_t nowMs);
    bool restorePendingFlagTap();
    uint32_t elapsedMs(uint32_t nowMs) const;
    uint16_t currentCellIndex() const;
    pgos::MinesweeperConfig selectedConfig() const;
    uint16_t maximumCustomMines() const;
    const char* difficultyName(pgos::MinesweeperDifficulty difficulty) const;
    lv_area_t surfaceArea() const;
    lv_area_t tileArea(uint8_t x, uint8_t y) const;
    lv_area_t assistFeedbackArea() const;
    void invalidateArea(const lv_area_t& area);
    void invalidateTile(uint8_t x, uint8_t y);
    void invalidateCursor();
    void invalidateClock();
    void invalidateTitleSpark();
    void invalidate();
};
