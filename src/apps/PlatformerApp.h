#pragma once

#include "core/App.h"
#include "games/PlatformerCampaignData.h"
#include "games/PlatformerEngine.h"
#include "games/PlatformerTileRenderer.h"
#include "services/GamepadPolicy.h"
#include "services/PlatformerProgressService.h"
#include "ui/PixelSprite.h"
#include "ui/BitmapFont.h"
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
    static constexpr uint32_t MAX_FRAME_MS = 64;
    static constexpr uint32_t FRAME_RENDER_INTERVAL_MS = 32;
    static constexpr uint32_t COMMAND_MOVE_MS = 140;

    pgos::PlatformerEngine engine_;
    PlatformerProgressService progress_;
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
    bool campaignMapTest_ = false;
    bool directFrameReady_ = false;
    uint8_t selectedWorld_ = 1;
    uint8_t selectedStage_ = 1;
    uint8_t selectedLevelField_ = 0;
    uint8_t mapTestWorld_ = 1;
    uint8_t mapTestStage_ = 1;
    int32_t mapTestCameraX_ = 0;
    int32_t mapTestCameraY_ = 0;
    uint16_t* mapFrameBuffer_ = nullptr;
    uint8_t* blockTilePixels_ = nullptr;
    uint8_t* enemyTilePixels_ = nullptr;
    uint8_t* playerTilePixels_ = nullptr;
    pgos::PlatformerDecodedTileSheet blockTileCache_{};
    pgos::PlatformerDecodedTileSheet enemyTileCache_{};
    pgos::PlatformerDecodedTileSheet playerTileCache_{};
    pgos::PixelSprite mapFrameSprite_{};
    pgos::GamepadAxisFilter moveAxisFilter_;
    pgos::GamepadAxisFilter crouchAxisFilter_;
    BitmapFont canvasFont_;

    static void drawEvent(lv_event_t* event, void* context);
    void draw(lv_event_t* event);
    void drawCampaignMapTest(lv_layer_t* layer, const lv_area_t& area);
    void composeCampaignFrame(const pgos::PlatformerSnapshot& state,
                              uint32_t animationMs);
    void startCampaignMapTest(uint8_t world = 1, uint8_t stage = 1);
    void advanceCampaignMapTest();
    void panCampaignMapTest(int16_t deltaX, int16_t deltaY);
    void adjustLevelSelection(int8_t delta);
    float sampleMove(const GamepadSnapshot& gamepad, uint32_t nowMs);
    bool sampleJumpHeld(const GamepadSnapshot& gamepad,
                        uint32_t nowMs) const;
    bool sampleCrouchHeld(const GamepadSnapshot& gamepad, uint32_t nowMs);
    bool sampleActionHeld(const GamepadSnapshot& gamepad) const;
    void consumeEvents(AppContext& context);
    void allocateTileCaches();
    void freeTileCaches();
    void invalidate();
};
