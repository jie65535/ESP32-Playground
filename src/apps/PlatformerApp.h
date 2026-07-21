#pragma once

#include "core/App.h"
#include "games/PlatformerEngine.h"
#include "services/GamepadPolicy.h"
#include "ui/RenderSurface.h"

class PlatformerApp final : public IApp {
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
    static constexpr int16_t GAME_SURFACE_WIDTH = 320;
    static constexpr int16_t GAME_SURFACE_HEIGHT = 218;
    static constexpr int16_t HUD_HEIGHT = 26;
    static constexpr uint32_t PHYSICS_STEP_MS = 8;
    static constexpr uint32_t MAX_FRAME_MS = 48;
    static constexpr uint32_t FRAME_RENDER_INTERVAL_MS = 24;
    static constexpr uint32_t COMMAND_MOVE_MS = 140;

    pgos::PlatformerEngine engine_;
    RenderSurface surface_;
    lv_obj_t* root_ = nullptr;
    const lv_font_t* hudFont_ = nullptr;
    const lv_font_t* overlayFont_ = nullptr;
    lv_color_t skyColor_{};
    lv_color_t skyDarkColor_{};
    lv_color_t textColor_{};
    lv_color_t mutedColor_{};
    lv_color_t accentColor_{};
    uint32_t lastTickMs_ = 0;
    uint32_t lastRenderMs_ = 0;
    uint32_t physicsAccumulatorMs_ = 0;
    uint32_t commandMoveUntilMs_ = 0;
    uint32_t commandJumpHoldUntilMs_ = 0;
    uint32_t commandCrouchUntilMs_ = 0;
    int8_t commandMoveDirection_ = 0;
    bool jumpPending_ = false;
    bool actionHeldLast_ = false;
    bool gamepadConnectedLast_ = false;
    pgos::GamepadAxisFilter moveAxisFilter_;
    pgos::GamepadAxisFilter crouchAxisFilter_;

    static void drawEvent(lv_event_t* event, void* context);
    void draw(lv_event_t* event);
    float sampleMove(const GamepadSnapshot& gamepad, uint32_t nowMs);
    bool sampleJumpHeld(const GamepadSnapshot& gamepad,
                        uint32_t nowMs) const;
    bool sampleCrouchHeld(const GamepadSnapshot& gamepad, uint32_t nowMs);
    bool sampleActionHeld(const GamepadSnapshot& gamepad) const;
    void consumeEvents(AppContext& context);
    void invalidate();
};
