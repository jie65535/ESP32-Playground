#pragma once

#include "core/App.h"
#include "games/BlackjackEngine.h"

class BlackjackApp final : public IApp {
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
    enum class ViewPhase : uint8_t {
        Intro,
        Betting,
        Animating,
        PlayerTurn,
        RoundOver,
        Bailout,
    };

    enum class PlayerAction : uint8_t {
        Hit,
        Stand,
        Double,
    };

    static constexpr int16_t SURFACE_WIDTH = 320;
    static constexpr int16_t SURFACE_HEIGHT = 218;
    static constexpr int16_t HUD_HEIGHT = 24;
    static constexpr int16_t CARD_WIDTH = 42;
    static constexpr int16_t CARD_HEIGHT = 60;
    static constexpr int16_t DEALER_CARD_Y = 34;
    static constexpr int16_t PLAYER_CARD_Y = 104;
    static constexpr int16_t ACTION_BAR_Y = 176;
    static constexpr int16_t ANALOG_THRESHOLD = 128;
    static constexpr uint32_t SHUFFLE_ANIMATION_MS = 420;
    static constexpr uint32_t DEAL_ANIMATION_MS = 220;
    static constexpr uint32_t FLIP_ANIMATION_MS = 260;
    static constexpr uint32_t SETTLE_ANIMATION_MS = 620;
    static constexpr uint32_t BAILOUT_ANIMATION_MS = 720;

    ViewPhase phase_ = ViewPhase::Intro;
    pgos::BlackjackEngine engine_;
    pgos::BlackjackEvent activeEvent_{};
    bool eventActive_ = false;
    bool wagerActive_ = false;
    bool bailoutAnimating_ = false;
    bool holeRevealed_ = false;
    bool doubleAvailable_ = false;
    uint8_t visiblePlayerCards_ = 0;
    uint8_t visibleDealerCards_ = 0;
    uint8_t betSelection_ = 0;
    uint8_t actionSelection_ = 0;
    int8_t analogXSign_ = 0;
    uint32_t eventStartedMs_ = 0;
    uint32_t eventDurationMs_ = 0;
    uint32_t bailoutStartedMs_ = 0;
    uint32_t wagerDebited_ = 0;
    uint32_t settlementFromBankroll_ = 0;
    uint32_t settlementToBankroll_ = 0;
    uint32_t bailoutFromBankroll_ = 0;
    uint32_t profileWins_ = 0;
    uint32_t profileBlackjacks_ = 0;
    uint16_t profileBestStreak_ = 0;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* surface_ = nullptr;
    const lv_font_t* hudFont_ = nullptr;
    const lv_font_t* bodyFont_ = nullptr;
    const lv_font_t* strongBodyFont_ = nullptr;
    const lv_font_t* rankFont_ = nullptr;
    const lv_font_t* titleFont_ = nullptr;
    lv_color_t feltColor_{};
    lv_color_t feltRaisedColor_{};
    lv_color_t headerColor_{};
    lv_color_t cardColor_{};
    lv_color_t cardInkColor_{};
    lv_color_t redSuitColor_{};
    lv_color_t goldColor_{};
    lv_color_t mutedColor_{};
    lv_color_t panelColor_{};
    lv_color_t textColor_{};

    static void drawEvent(lv_event_t* event);
    void draw(lv_event_t* event);
    void drawTable(lv_layer_t* layer, const lv_area_t& surfaceArea,
                   uint32_t nowMs);
    void drawIntro(lv_layer_t* layer, const lv_area_t& surfaceArea) const;
    void drawBetting(lv_layer_t* layer, const lv_area_t& surfaceArea) const;
    void drawBailout(lv_layer_t* layer, const lv_area_t& surfaceArea,
                     uint32_t nowMs) const;
    void drawActions(lv_layer_t* layer, const lv_area_t& surfaceArea) const;
    void drawOutcome(lv_layer_t* layer, const lv_area_t& surfaceArea,
                     uint32_t nowMs) const;
    void drawShoe(lv_layer_t* layer, const lv_area_t& surfaceArea) const;
    void drawHand(lv_layer_t* layer, const lv_area_t& surfaceArea,
                  pgos::BlackjackHandOwner owner, uint32_t nowMs) const;
    void drawCard(lv_layer_t* layer, const pgos::BlackjackCard& card,
                  int16_t x, int16_t y, bool faceUp,
                  int16_t width = CARD_WIDTH) const;
    void drawChip(lv_layer_t* layer, int16_t x, int16_t y,
                  lv_color_t color) const;
    void drawRect(lv_layer_t* layer, const lv_area_t& area,
                  lv_color_t color, int32_t radius = 0,
                  lv_opa_t opacity = LV_OPA_COVER) const;
    void drawText(lv_layer_t* layer, const char* text, lv_area_t area,
                  lv_color_t color, const lv_font_t* font,
                  lv_text_align_t align = LV_TEXT_ALIGN_CENTER) const;

    void enterBetting(AppContext& context);
    void startRound(AppContext& context, uint32_t nowMs);
    void executeAction(PlayerAction action, AppContext& context,
                       uint32_t nowMs);
    void startNextEvent(AppContext& context, uint32_t nowMs);
    void finishActiveEvent(AppContext& context, uint32_t nowMs);
    void beginEvent(const pgos::BlackjackEvent& event, AppContext& context,
                    uint32_t nowMs);
    void settleProfile(AppContext& context);
    void playOutcomeFeedback(AppContext& context) const;
    void claimBailout(AppContext& context, uint32_t nowMs);
    void sampleAnalog(const GamepadSnapshot& gamepad, AppContext& context);
    void moveSelection(int8_t direction, AppContext& context);

    uint8_t availableActionCount(const AppContext& context) const;
    PlayerAction selectedAction(const AppContext& context) const;
    uint32_t selectedBet(const AppContext& context) const;
    uint32_t displayedBankroll(uint32_t nowMs) const;
    float activeProgress(uint32_t nowMs) const;
    int16_t cardX(uint8_t count, uint8_t index) const;
    void invalidate();
};
