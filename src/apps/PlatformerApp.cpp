#include "apps/PlatformerApp.h"

#include "games/PlatformerExtraAssets.generated.h"
#include "games/PlatformerSpriteAssets.generated.h"
#include "games/PlatformerTileRenderer.h"
#include "services/AudioService.h"
#include "services/BleGamepadService.h"
#include "services/DisplayService.h"
#include "ui/CanvasDraw.h"
#include "ui/PixelSpriteRenderer.h"
#include "ui/UiRuntime.h"

#include <algorithm>
#include <cmath>
#include <esp_heap_caps.h>

using pgos::drawRect;
using pgos::drawText;

namespace {

// Mario-Level-1's source strip is 224px high with the ground at y=201. The
// shell root starts below the 22px status bar; drawing the strip eight pixels
// above that root puts its original ground line at the same device coordinate
// as Mario's collision geometry (global y≈215).
constexpr int16_t WORLD_Y_OFFSET = -8;
constexpr int16_t TILE_SIZE = pgos::PlatformerEngine::TILE_SIZE;

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red * 31U / 255U) << 11U) |
                                 ((green * 63U / 255U) << 5U) |
                                 (blue * 31U / 255U));
}

void fillRgb565(uint16_t* target, int16_t width, int16_t height, int16_t x,
                int16_t y, int16_t rectWidth, int16_t rectHeight,
                uint16_t color) {
    const int16_t x1 = std::max<int16_t>(0, x);
    const int16_t y1 = std::max<int16_t>(0, y);
    const int16_t x2 = std::min<int16_t>(width, x + rectWidth);
    const int16_t y2 = std::min<int16_t>(height, y + rectHeight);
    for (int16_t row = y1; row < y2; ++row) {
        std::fill(target + static_cast<size_t>(row) * width + x1,
                  target + static_cast<size_t>(row) * width + x2, color);
    }
}

void blitSpriteRgb565(uint16_t* target, int16_t width, int16_t height,
                      const pgos::PixelSprite& sprite, int16_t x, int16_t y,
                      bool flipX = false) {
    if (target == nullptr || !sprite.valid()) {
        return;
    }
    for (uint16_t sourceY = 0; sourceY < sprite.height; ++sourceY) {
        const int16_t destinationY = static_cast<int16_t>(y + sourceY);
        if (destinationY < 0 || destinationY >= height) {
            continue;
        }
        for (uint16_t sourceX = 0; sourceX < sprite.width; ++sourceX) {
            const int16_t destinationX = static_cast<int16_t>(x + sourceX);
            if (destinationX < 0 || destinationX >= width) {
                continue;
            }
            const uint16_t sampleX = flipX
                                         ? static_cast<uint16_t>(
                                               sprite.width - 1U - sourceX)
                                         : sourceX;
            const uint32_t sourceIndex =
                static_cast<uint32_t>(sourceY) * sprite.width + sampleX;
            if (sprite.opacityAt(sourceIndex) > LV_OPA_MIN) {
                target[static_cast<size_t>(destinationY) * width +
                       destinationX] = sprite.pixelAt(sourceIndex);
            }
        }
    }
}

using Sprite = pgos::PixelSprite;

constexpr const Sprite* BOX_FRAMES[] = {
    &pgos::platformer_assets::CoinBox_1,
    &pgos::platformer_assets::CoinBox_2,
    &pgos::platformer_assets::CoinBox_3,
};

constexpr const Sprite* FIRE_FLOWER_FRAMES[] = {
    &pgos::platformer_extra_assets::fire_flower_1,
    &pgos::platformer_extra_assets::fire_flower_2,
    &pgos::platformer_extra_assets::fire_flower_3,
    &pgos::platformer_extra_assets::fire_flower_4,
};

constexpr const Sprite* STAR_FRAMES[] = {
    &pgos::platformer_extra_assets::star_1,
    &pgos::platformer_extra_assets::star_2,
    &pgos::platformer_extra_assets::star_3,
    &pgos::platformer_extra_assets::star_4,
};

constexpr const Sprite* COIN_FRAMES[] = {
    &pgos::platformer_assets::coin_item_1,
    &pgos::platformer_assets::coin_item_2,
    &pgos::platformer_assets::coin_item_3,
    &pgos::platformer_assets::coin_item_4,
};

constexpr const Sprite* FIREBALL_FRAMES[] = {
    &pgos::platformer_extra_assets::fireball_1,
    &pgos::platformer_extra_assets::fireball_2,
    &pgos::platformer_extra_assets::fireball_3,
    &pgos::platformer_extra_assets::fireball_4,
};

constexpr const Sprite* FIREBALL_EXPLOSION_FRAMES[] = {
    &pgos::platformer_extra_assets::fireball_explode1,
    &pgos::platformer_extra_assets::fireball_explode2,
    &pgos::platformer_extra_assets::fireball_explode3,
};

uint16_t campaignPlayerTile(const pgos::PlatformerSnapshot& state,
                            uint32_t animationMs) {
    if (state.phase == pgos::PlatformerPhase::Dying) {
        return 1U;
    }
    const uint16_t powerOffset = state.playerFire ? 225U
                                 : state.playerBig ? 25U
                                                   : 0U;
    if (state.phase == pgos::PlatformerPhase::Flagpole ||
        state.phase == pgos::PlatformerPhase::VineClimb) {
        return static_cast<uint16_t>(powerOffset + 13U +
                                     ((animationMs / 125U) & 1U));
    }
    if (state.playerCrouching && state.playerBig) {
        return static_cast<uint16_t>(powerOffset + 1U);
    }
    if (state.playerSkidding) {
        return static_cast<uint16_t>(powerOffset + 5U);
    }
    if (!state.grounded && state.phase != pgos::PlatformerPhase::CastleWalk) {
        return static_cast<uint16_t>(powerOffset + 6U);
    }
    if (std::abs(state.playerVx) > 8.0F) {
        return static_cast<uint16_t>(powerOffset + 2U +
                                     (animationMs / 90U) % 3U);
    }
    return powerOffset;
}

bool directCampaignPhase(pgos::PlatformerPhase phase) {
    return phase == pgos::PlatformerPhase::Running ||
           phase == pgos::PlatformerPhase::Dying ||
           phase == pgos::PlatformerPhase::Warping ||
           phase == pgos::PlatformerPhase::VineClimb ||
           phase == pgos::PlatformerPhase::CastleBridge ||
           phase == pgos::PlatformerPhase::Flagpole ||
           phase == pgos::PlatformerPhase::CastleWalk ||
           phase == pgos::PlatformerPhase::TimeBonus;
}

bool enemySpriteFacesMovement(pgos::PlatformerEnemyType type) {
    return type == pgos::PlatformerEnemyType::Goomba ||
           type == pgos::PlatformerEnemyType::Koopa ||
           type == pgos::PlatformerEnemyType::KoopaParatroopa ||
           type == pgos::PlatformerEnemyType::BuzzyBeetle ||
           type == pgos::PlatformerEnemyType::Spiny;
}

float playerTileVisualY(const pgos::PlatformerSnapshot& state) {
    return state.playerY -
           (state.playerCrouching && state.playerBig
                ? pgos::PlatformerEngine::BIG_PLAYER_HEIGHT -
                      pgos::PlatformerEngine::CROUCH_PLAYER_HEIGHT
                : 0.0F);
}

bool playerFlickerHidden(const pgos::PlatformerSnapshot& state,
                         uint32_t animationMs) {
    if (state.playerDamageBlinking) {
        // Reference EndingBlinkComponent(10, 150) at 60 FPS.
        return ((animationMs / 167U) & 1U) != 0U;
    }
    if (state.playerInvincible) {
        // Reference star blink toggles every five frames at 60 FPS.
        return ((animationMs / 83U) & 1U) != 0U;
    }
    return false;
}

}  // namespace

AppId PlatformerApp::id() const {
    return AppId::Platformer;
}

const char* PlatformerApp::name() const {
    return "Super Mario";
}

void PlatformerApp::onEnter(AppContext& context) {
    root_ = nullptr;
    surface_.reset();
    progress_.begin(context.console);
    selectedWorld_ = progress_.continueWorld();
    selectedStage_ = progress_.continueStage();
    selectedLevelField_ = 0;
    if (!engine_.prepareCampaignTitle(selectedWorld_, selectedStage_)) {
        selectedWorld_ = 1;
        selectedStage_ = 1;
        engine_.prepareCampaignTitle(1, 1);
    }
    lastTickMs_ = millis();
    lastRenderMs_ = 0;
    physicsAccumulatorMs_ = 0;
    commandMoveUntilMs_ = 0;
    commandJumpHoldUntilMs_ = 0;
    commandCrouchUntilMs_ = 0;
    commandMoveDirection_ = 0;
    jumpPending_ = false;
    actionHeldLast_ = false;
    gamepadConnectedLast_ = false;
    campaignMapTest_ = false;
    directFrameReady_ = false;
    mapTestWorld_ = 1;
    mapTestStage_ = 1;
    mapTestCameraX_ = 0;
    mapTestCameraY_ = 0;
    if (mapFrameBuffer_ == nullptr) {
        mapFrameBuffer_ = static_cast<uint16_t*>(heap_caps_malloc(
            static_cast<size_t>(GAME_SURFACE_WIDTH) * GAME_SURFACE_HEIGHT *
                sizeof(uint16_t),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    mapFrameSprite_ = {
        "campaign-map",
        static_cast<uint16_t>(GAME_SURFACE_WIDTH),
        static_cast<uint16_t>(GAME_SURFACE_HEIGHT),
        reinterpret_cast<const uint8_t*>(mapFrameBuffer_),
        static_cast<uint32_t>(GAME_SURFACE_WIDTH) * GAME_SURFACE_HEIGHT *
            sizeof(uint16_t),
        true,
    };
    allocateTileCaches();
    moveAxisFilter_.reset();
    crouchAxisFilter_.reset();
}

void PlatformerApp::onExit(AppContext&) {
    surface_.reset();
    directFrameReady_ = false;
    root_ = nullptr;
    hudFont_ = nullptr;
    overlayFont_ = nullptr;
    if (mapFrameBuffer_ != nullptr) {
        heap_caps_free(mapFrameBuffer_);
        mapFrameBuffer_ = nullptr;
    }
    freeTileCaches();
    mapFrameSprite_ = {};
}

void PlatformerApp::allocateTileCaches() {
    freeTileCaches();
    const auto allocate = [](const pgos::PlatformerPackedTileSheet& source,
                             uint8_t*& pixels,
                             pgos::PlatformerDecodedTileSheet& cache) {
        const size_t size =
            pgos::PlatformerTileRenderer::decodedPixelCount(source);
        pixels = static_cast<uint8_t*>(heap_caps_malloc(
            size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (pixels == nullptr ||
            !pgos::PlatformerTileRenderer::decode(source, pixels, size,
                                                   cache)) {
            if (pixels != nullptr) {
                heap_caps_free(pixels);
                pixels = nullptr;
            }
            cache = {};
        }
    };
    allocate(pgos::PLATFORMER_BLOCK_TILES, blockTilePixels_, blockTileCache_);
    allocate(pgos::PLATFORMER_ENEMY_TILES, enemyTilePixels_, enemyTileCache_);
    allocate(pgos::PLATFORMER_PLAYER_TILES, playerTilePixels_,
             playerTileCache_);
}

void PlatformerApp::freeTileCaches() {
    for (uint8_t** pixels :
         {&blockTilePixels_, &enemyTilePixels_, &playerTilePixels_}) {
        if (*pixels != nullptr) {
            heap_caps_free(*pixels);
            *pixels = nullptr;
        }
    }
    blockTileCache_ = {};
    enemyTileCache_ = {};
    playerTileCache_ = {};
}

void PlatformerApp::onCommand(const AppCommand& command, AppContext&) {
    const uint32_t nowMs = millis();
    switch (command.type) {
        case AppCommandType::Previous:
            if (campaignMapTest_) {
                panCampaignMapTest(0, -GAME_SURFACE_HEIGHT);
                invalidate();
                break;
            }
            if (engine_.phase() == pgos::PlatformerPhase::Title) {
                adjustLevelSelection(1);
                invalidate();
                break;
            }
            if (engine_.phase() == pgos::PlatformerPhase::Running) {
                jumpPending_ = true;
                commandJumpHoldUntilMs_ = nowMs + 160U;
                invalidate();
            }
            break;
        case AppCommandType::Next:
            if (campaignMapTest_) {
                panCampaignMapTest(0, GAME_SURFACE_HEIGHT);
                invalidate();
                break;
            }
            if (engine_.phase() == pgos::PlatformerPhase::Title) {
                adjustLevelSelection(-1);
                invalidate();
                break;
            }
            if (engine_.phase() == pgos::PlatformerPhase::Running) {
                commandCrouchUntilMs_ = nowMs + 180U;
                invalidate();
            }
            break;
        case AppCommandType::Left:
            if (campaignMapTest_) {
                panCampaignMapTest(-256, 0);
                invalidate();
                break;
            }
            if (engine_.phase() == pgos::PlatformerPhase::Title) {
                selectedLevelField_ = 0;
                invalidate();
                break;
            }
            commandMoveDirection_ = -1;
            commandMoveUntilMs_ = nowMs + COMMAND_MOVE_MS;
            invalidate();
            break;
        case AppCommandType::Right:
            if (campaignMapTest_) {
                panCampaignMapTest(256, 0);
                invalidate();
                break;
            }
            if (engine_.phase() == pgos::PlatformerPhase::Title) {
                selectedLevelField_ = 1;
                invalidate();
                break;
            }
            commandMoveDirection_ = 1;
            commandMoveUntilMs_ = nowMs + COMMAND_MOVE_MS;
            invalidate();
            break;
        case AppCommandType::Activate:
            if (campaignMapTest_) {
                advanceCampaignMapTest();
                invalidate();
                break;
            }
            if (engine_.phase() == pgos::PlatformerPhase::Title) {
                engine_.startCampaign(selectedWorld_, selectedStage_);
                physicsAccumulatorMs_ = 0;
                lastTickMs_ = nowMs;
                commandJumpHoldUntilMs_ = 0;
                commandCrouchUntilMs_ = 0;
            } else if (engine_.phase() == pgos::PlatformerPhase::GameOver) {
                const pgos::PlatformerSnapshot state = engine_.snapshot();
                engine_.startCampaign(state.world, state.stage);
                physicsAccumulatorMs_ = 0;
                lastTickMs_ = nowMs;
                commandJumpHoldUntilMs_ = 0;
                commandCrouchUntilMs_ = 0;
            } else if (engine_.phase() == pgos::PlatformerPhase::Won) {
                if (!engine_.advanceCampaign()) {
                    engine_.startCampaign(1, 1);
                }
                physicsAccumulatorMs_ = 0;
                lastTickMs_ = nowMs;
            } else if (engine_.phase() == pgos::PlatformerPhase::Running) {
                jumpPending_ = true;
                commandJumpHoldUntilMs_ = nowMs + 160U;
            } else if (engine_.phase() == pgos::PlatformerPhase::Paused) {
                engine_.togglePause();
                lastTickMs_ = nowMs;
                physicsAccumulatorMs_ = 0;
                commandJumpHoldUntilMs_ = 0;
                commandCrouchUntilMs_ = 0;
            }
            invalidate();
            break;
        case AppCommandType::QuickDrop:
            if (campaignMapTest_) {
                campaignMapTest_ = false;
                engine_.prepareCampaignTitle(selectedWorld_, selectedStage_);
                invalidate();
                break;
            }
            if (engine_.phase() == pgos::PlatformerPhase::Running ||
                engine_.phase() == pgos::PlatformerPhase::Paused) {
                engine_.togglePause();
                lastTickMs_ = nowMs;
                physicsAccumulatorMs_ = 0;
                jumpPending_ = false;
                commandJumpHoldUntilMs_ = 0;
                commandCrouchUntilMs_ = 0;
                invalidate();
            }
            break;
        case AppCommandType::PlatformerMapTest:
            if (command.number >= 0 && command.number < 32) {
                startCampaignMapTest(
                    static_cast<uint8_t>(command.number / 4 + 1),
                    static_cast<uint8_t>(command.number % 4 + 1));
            } else {
                startCampaignMapTest();
            }
            physicsAccumulatorMs_ = 0;
            lastTickMs_ = nowMs;
            jumpPending_ = false;
            actionHeldLast_ = false;
            invalidate();
            break;
        case AppCommandType::PlatformerMapTestNext:
            if (!campaignMapTest_) {
                startCampaignMapTest();
            } else {
                advanceCampaignMapTest();
            }
            invalidate();
            break;
        default:
            break;
    }
}

void PlatformerApp::onTick(uint32_t nowMs, AppContext& context) {
    if (surface_.object() == nullptr) {
        return;
    }

    const uint32_t elapsedMs =
        lastTickMs_ == 0 ? 0 : std::min<uint32_t>(nowMs - lastTickMs_,
                                                   MAX_FRAME_MS);
    lastTickMs_ = nowMs;

    if (campaignMapTest_) {
        return;
    }

    const pgos::PlatformerPhase phase = engine_.phase();
    const bool simulationActive =
        phase == pgos::PlatformerPhase::Running ||
        phase == pgos::PlatformerPhase::Dying ||
        phase == pgos::PlatformerPhase::Warping ||
        phase == pgos::PlatformerPhase::VineClimb ||
        phase == pgos::PlatformerPhase::CastleBridge ||
        phase == pgos::PlatformerPhase::Flagpole ||
        phase == pgos::PlatformerPhase::CastleWalk ||
        phase == pgos::PlatformerPhase::TimeBonus;
    bool simulationChanged = false;
    if (simulationActive && elapsedMs != 0) {
        physicsAccumulatorMs_ = std::min<uint32_t>(
            physicsAccumulatorMs_ + elapsedMs, MAX_FRAME_MS);
        const GamepadSnapshot gamepad = context.gamepad.snapshot();
        if (gamepad.connected != gamepadConnectedLast_) {
            moveAxisFilter_.reset();
            crouchAxisFilter_.reset();
            gamepadConnectedLast_ = gamepad.connected;
        }
        while (physicsAccumulatorMs_ >= PHYSICS_STEP_MS &&
               engine_.phase() != pgos::PlatformerPhase::Paused &&
               engine_.phase() != pgos::PlatformerPhase::Title &&
               engine_.phase() != pgos::PlatformerPhase::GameOver &&
               engine_.phase() != pgos::PlatformerPhase::Won) {
            pgos::PlatformerInput input;
            input.moveAxis = sampleMove(gamepad, nowMs);
            input.jumpPressed = jumpPending_;
            input.jumpHeld = sampleJumpHeld(gamepad, nowMs);
            input.crouchHeld = sampleCrouchHeld(gamepad, nowMs);
            const bool actionHeld = sampleActionHeld(gamepad);
            input.actionPressed = actionHeld && !actionHeldLast_;
            input.actionHeld = actionHeld;
            jumpPending_ = false;
            actionHeldLast_ = actionHeld;
            engine_.step(static_cast<float>(PHYSICS_STEP_MS) / 1000.0F,
                         input);
            physicsAccumulatorMs_ -= PHYSICS_STEP_MS;
            simulationChanged = true;
        }
    } else if (!simulationActive) {
        physicsAccumulatorMs_ = 0;
        actionHeldLast_ = false;
    }

    if (simulationChanged &&
        (lastRenderMs_ == 0 ||
         nowMs - lastRenderMs_ >= FRAME_RENDER_INTERVAL_MS)) {
        const pgos::PlatformerSnapshot state = engine_.snapshot();
        if (state.campaignMode && directCampaignPhase(state.phase) &&
            mapFrameBuffer_ != nullptr) {
            context.ui.pollDisplayFlush();
            if (!context.display.dmaPending()) {
                composeCampaignFrame(state, nowMs);
                if (context.display.presentRgb565(
                        0, 22, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                        mapFrameBuffer_, GAME_SURFACE_WIDTH)) {
                    directFrameReady_ = true;
                    lastRenderMs_ = nowMs;
                }
            }
        } else {
            directFrameReady_ = false;
            lastRenderMs_ = nowMs;
            invalidate();
        }
    }

    consumeEvents(context);
}

lv_obj_t* PlatformerApp::onCreateView(AppContext& context) {
    root_ = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT);
    lv_obj_set_pos(root_, 0, 22);
    lv_obj_set_style_bg_color(root_, context.ui.background(), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    skyColor_ = lv_color_hex(0x72C8F4);
    skyDarkColor_ = lv_color_hex(0x4B92C5);
    textColor_ = context.ui.text();
    mutedColor_ = context.ui.muted();
    accentColor_ = context.ui.accent();
    hudFont_ = context.ui.font(12);
    overlayFont_ = context.ui.font(20);

    surface_.attach(root_, 0, 0, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                     context.ui.background(), drawEvent, this);
    invalidate();
    return root_;
}

void PlatformerApp::onUpdateView(AppContext&) {
    if (!directFrameReady_ || !directCampaignPhase(engine_.phase())) {
        invalidate();
    }
}

void PlatformerApp::drawEvent(lv_event_t* event, void* context) {
    auto* app = static_cast<PlatformerApp*>(context);
    if (app != nullptr) {
        app->draw(event);
    }
}

void PlatformerApp::startCampaignMapTest(uint8_t world, uint8_t stage) {
    campaignMapTest_ = true;
    directFrameReady_ = false;
    mapTestWorld_ = world;
    mapTestStage_ = stage;
    const pgos::PlatformerCampaignLevel* level =
        pgos::platformerCampaignLevel(mapTestWorld_, mapTestStage_);
    if (level == nullptr) {
        campaignMapTest_ = false;
        return;
    }
    mapTestCameraX_ = static_cast<int32_t>(level->cameraStart.x) * TILE_SIZE;
    mapTestCameraY_ = static_cast<int32_t>(level->cameraStart.y) * TILE_SIZE;
}

void PlatformerApp::advanceCampaignMapTest() {
    const pgos::PlatformerCampaignLevel* level =
        pgos::platformerCampaignLevel(mapTestWorld_, mapTestStage_);
    if (level == nullptr) {
        startCampaignMapTest();
        return;
    }
    const int32_t maximumX = std::max<int32_t>(
        0, static_cast<int32_t>(level->width) * TILE_SIZE -
               GAME_SURFACE_WIDTH);
    if (mapTestCameraX_ < maximumX) {
        mapTestCameraX_ = std::min<int32_t>(maximumX, mapTestCameraX_ + 256);
        return;
    }

    if (++mapTestStage_ > 4U) {
        mapTestStage_ = 1;
        if (++mapTestWorld_ > 8U) {
            mapTestWorld_ = 1;
        }
    }
    level = pgos::platformerCampaignLevel(mapTestWorld_, mapTestStage_);
    if (level != nullptr) {
        mapTestCameraX_ = static_cast<int32_t>(level->cameraStart.x) * TILE_SIZE;
        mapTestCameraY_ = static_cast<int32_t>(level->cameraStart.y) * TILE_SIZE;
    }
}

void PlatformerApp::panCampaignMapTest(int16_t deltaX, int16_t deltaY) {
    const pgos::PlatformerCampaignLevel* level =
        pgos::platformerCampaignLevel(mapTestWorld_, mapTestStage_);
    if (level == nullptr) {
        return;
    }
    const int32_t maximumX = std::max<int32_t>(
        0, static_cast<int32_t>(level->width) * TILE_SIZE -
               GAME_SURFACE_WIDTH);
    const int32_t maximumY = std::max<int32_t>(
        0, static_cast<int32_t>(level->height) * TILE_SIZE -
               GAME_SURFACE_HEIGHT);
    mapTestCameraX_ = std::clamp<int32_t>(mapTestCameraX_ + deltaX, 0,
                                         maximumX);
    mapTestCameraY_ = std::clamp<int32_t>(mapTestCameraY_ + deltaY, 0,
                                         maximumY);
}

void PlatformerApp::drawCampaignMapTest(lv_layer_t* layer,
                                        const lv_area_t& area) {
    const pgos::PlatformerCampaignLevel* level =
        pgos::platformerCampaignLevel(mapTestWorld_, mapTestStage_);
    if (level == nullptr || mapFrameBuffer_ == nullptr) {
        return;
    }
    pgos::PlatformerTileRenderer::render(
        *level, mapTestCameraX_, mapTestCameraY_, mapFrameBuffer_,
        GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT, GAME_SURFACE_WIDTH, true,
        &blockTileCache_, &enemyTileCache_);
    pgos::PixelSpriteRenderer::draw(layer, mapFrameSprite_, area.x1, area.y1);

    lv_area_t hud = area;
    hud.y2 = static_cast<lv_coord_t>(hud.y1 + 21);
    drawRect(layer, hud, lv_color_hex(0x000000), 0, LV_OPA_80);
    char status[48];
    lv_snprintf(status, sizeof(status), "MAP %u-%u  X%03ld  Y%02ld",
                static_cast<unsigned>(mapTestWorld_),
                static_cast<unsigned>(mapTestStage_),
                static_cast<long>(mapTestCameraX_ / TILE_SIZE),
                static_cast<long>(mapTestCameraY_ / TILE_SIZE));
    hud.x1 = static_cast<lv_coord_t>(hud.x1 + 6);
    drawText(layer, status, hud, lv_color_hex(0xFFFFFF), hudFont_,
             LV_TEXT_ALIGN_LEFT);
}

void PlatformerApp::composeCampaignFrame(
    const pgos::PlatformerSnapshot& state, uint32_t animationMs) {
    if (mapFrameBuffer_ == nullptr || engine_.levelRuntime().level() == nullptr) {
        return;
    }
    const int32_t cameraX =
        static_cast<int32_t>(std::lround(state.cameraX));
    const int32_t cameraY =
        static_cast<int32_t>(std::lround(state.cameraY));
    pgos::PlatformerTileRenderer::renderBase(
        engine_.levelRuntime(), cameraX, cameraY, mapFrameBuffer_,
        GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT, GAME_SURFACE_WIDTH,
        &blockTileCache_);

    if (state.vineActive) {
        const pgos::PlatformerVineState vine = engine_.vine();
        const uint8_t pieces = static_cast<uint8_t>(std::min<float>(
            6.0F, std::ceil(vine.grownPixels / TILE_SIZE)));
        const float topY = vine.baseY - vine.grownPixels;
        for (uint8_t piece = 0; piece < pieces; ++piece) {
            pgos::PlatformerTileRenderer::drawTile(
                blockTileCache_, piece == 0U ? 100U : 148U,
                static_cast<int16_t>(std::lround(vine.x - state.cameraX)),
                static_cast<int16_t>(std::lround(
                    topY + piece * TILE_SIZE - state.cameraY)),
                mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                GAME_SURFACE_WIDTH);
        }
    }

    for (uint8_t index = 0; index < state.movingPlatformCount; ++index) {
        const pgos::PlatformerMovingPlatformState platform =
            engine_.movingPlatform(index);
        if (!platform.active) {
            continue;
        }
        if (platform.pulley && platform.pairIndex >= 0 &&
            index < static_cast<uint8_t>(platform.pairIndex)) {
            const pgos::PlatformerMovingPlatformState pair =
                engine_.movingPlatform(static_cast<uint8_t>(platform.pairIndex));
            for (const pgos::PlatformerMovingPlatformState* side :
                 {&platform, &pair}) {
                const int16_t ropeX = static_cast<int16_t>(std::lround(
                    side->x + TILE_SIZE - state.cameraX));
                const int16_t ropeTop = static_cast<int16_t>(std::lround(
                    side->pulleyTop - TILE_SIZE - state.cameraY));
                const int16_t ropeBottom = static_cast<int16_t>(std::lround(
                    side->y - state.cameraY));
                fillRgb565(mapFrameBuffer_, GAME_SURFACE_WIDTH,
                           GAME_SURFACE_HEIGHT, ropeX, ropeTop, 1,
                           ropeBottom - ropeTop, 0x94B2U);
            }
        }
        if (platform.sourceTileId >= pgos::PLATFORMER_BLOCK_TILE_COUNT) {
            continue;
        }
        const int16_t x = static_cast<int16_t>(
            std::lround(platform.x - state.cameraX));
        const int16_t y = static_cast<int16_t>(
            std::lround(platform.y - state.cameraY));
        for (uint8_t tile = 0; tile < platform.widthTiles; ++tile) {
            pgos::PlatformerTileRenderer::drawTile(
                blockTileCache_,
                static_cast<uint16_t>(platform.sourceTileId + tile),
                static_cast<int16_t>(x + tile * TILE_SIZE), y,
                mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                GAME_SURFACE_WIDTH);
        }
    }

    for (uint8_t index = 0; index < state.fireBarCount; ++index) {
        const pgos::PlatformerFireBarState fireBar = engine_.fireBar(index);
        if (!fireBar.active) {
            continue;
        }
        constexpr float FIRE_BAR_PI = 3.14159265358979323846F;
        const float radians = fireBar.angleDegrees * FIRE_BAR_PI / 180.0F;
        const uint16_t sourceId =
            static_cast<uint16_t>(611U + (animationMs / 100U) % 4U);
        for (uint8_t element = 0; element < fireBar.length; ++element) {
            const float distance = static_cast<float>(element * TILE_SIZE);
            pgos::PlatformerTileRenderer::drawTile(
                blockTileCache_, sourceId,
                static_cast<int16_t>(std::lround(
                    fireBar.x + std::cos(radians) * distance - state.cameraX)),
                static_cast<int16_t>(std::lround(
                    fireBar.y - std::sin(radians) * distance - state.cameraY)),
                mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                GAME_SURFACE_WIDTH);
        }
    }

    for (uint8_t index = 0; index < state.projectileCount; ++index) {
        const pgos::PlatformerProjectile projectile = engine_.projectile(index);
        if (!projectile.active) {
            continue;
        }
        const pgos::PixelSprite* sprite =
            projectile.exploding
                ? FIREBALL_EXPLOSION_FRAMES[std::min<uint8_t>(
                      2U, static_cast<uint8_t>(projectile.ageMs / 40U))]
                : FIREBALL_FRAMES[(animationMs / 80U) % 4U];
        blitSpriteRgb565(
            mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT, *sprite,
            static_cast<int16_t>(std::lround(projectile.x - state.cameraX)),
            static_cast<int16_t>(std::lround(projectile.y - state.cameraY)));
    }

    for (uint8_t index = 0; index < state.powerupCount; ++index) {
        const pgos::PlatformerPowerup powerup = engine_.powerup(index);
        if (!powerup.active) {
            continue;
        }
        const pgos::PixelSprite* sprite = &pgos::platformer_assets::mushroom;
        if (powerup.kind == pgos::PlatformerPowerupKind::OneUp) {
            sprite = &pgos::platformer_extra_assets::one_up;
        } else if (powerup.kind == pgos::PlatformerPowerupKind::FireFlower) {
            sprite = FIRE_FLOWER_FRAMES[(animationMs / 70U) % 4U];
        } else if (powerup.kind == pgos::PlatformerPowerupKind::Star) {
            sprite = STAR_FRAMES[(animationMs / 70U) % 4U];
        }
        blitSpriteRgb565(
            mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT, *sprite,
            static_cast<int16_t>(std::lround(powerup.x - state.cameraX)),
            static_cast<int16_t>(std::lround(powerup.y - state.cameraY)));
    }

    for (uint8_t index = 0; index < state.enemyCount; ++index) {
        const pgos::PlatformerEnemyState enemy = engine_.enemy(index);
        if (!enemy.active ||
            enemy.sourceTileId >= pgos::PLATFORMER_ENEMY_TILE_COUNT) {
            continue;
        }
        uint16_t sourceId = enemy.sourceTileId;
        const uint16_t reference =
            pgos::PLATFORMER_ENEMY_REFERENCE_IDS[sourceId];
        const bool squashed =
            enemy.motion == pgos::PlatformerEnemyMotion::Squashed;
        const bool shell =
            enemy.motion == pgos::PlatformerEnemyMotion::ShellIdle ||
            enemy.motion == pgos::PlatformerEnemyMotion::ShellSliding;
        if (squashed && (reference == 70U || reference == 71U)) {
            sourceId = static_cast<uint16_t>(
                sourceId + (reference == 70U ? 2U : 1U));
        } else if (shell) {
            sourceId = enemy.type == pgos::PlatformerEnemyType::BuzzyBeetle
                           ? 89U
                           : reference == 455U ? 494U : 77U;
        } else if (reference == 39U || reference == 71U) {
            --sourceId;
        }
        if (!squashed && !shell &&
            (reference == 38U || reference == 39U || reference == 70U ||
             reference == 71U || reference == 87U) &&
            ((animationMs / 180U) & 1U) != 0U) {
            ++sourceId;
        }
        const bool tall =
            !shell && (enemy.type == pgos::PlatformerEnemyType::Koopa ||
                       enemy.type ==
                           pgos::PlatformerEnemyType::KoopaParatroopa ||
                       enemy.type == pgos::PlatformerEnemyType::PiranhaPlant ||
                       enemy.type == pgos::PlatformerEnemyType::Blooper ||
                       enemy.type == pgos::PlatformerEnemyType::Lakitu ||
                       enemy.type == pgos::PlatformerEnemyType::HammerBro ||
                       enemy.type == pgos::PlatformerEnemyType::Bowser);
        const bool wide = enemy.type == pgos::PlatformerEnemyType::Bowser;
        const bool flipX =
            !shell && enemySpriteFacesMovement(enemy.type) &&
            !enemy.facingLeft;
        const int16_t x = static_cast<int16_t>(
            std::lround(enemy.x - state.cameraX));
        const int16_t y = static_cast<int16_t>(std::lround(
            enemy.y - state.cameraY -
            ((!shell &&
              (enemy.type == pgos::PlatformerEnemyType::Koopa ||
               enemy.type == pgos::PlatformerEnemyType::KoopaParatroopa))
                 ? 8.0F
                 : 0.0F)));
        for (uint8_t tileY = 0; tileY < (tall ? 2U : 1U); ++tileY) {
            for (uint8_t tileX = 0; tileX < (wide ? 2U : 1U); ++tileX) {
                pgos::PlatformerTileRenderer::drawTile(
                    enemyTileCache_,
                    static_cast<uint16_t>(sourceId + tileY * 35U + tileX),
                    static_cast<int16_t>(x + tileX * TILE_SIZE),
                    static_cast<int16_t>(y + tileY * TILE_SIZE),
                    mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                    GAME_SURFACE_WIDTH, flipX);
            }
        }
    }

    for (uint8_t index = 0; index < state.enemyHazardCount; ++index) {
        const pgos::PlatformerEnemyHazardState hazard =
            engine_.enemyHazard(index);
        if (hazard.active &&
            hazard.sourceTileId < pgos::PLATFORMER_ENEMY_TILE_COUNT) {
            pgos::PlatformerTileRenderer::drawTile(
                enemyTileCache_, hazard.sourceTileId,
                static_cast<int16_t>(
                    std::lround(hazard.x - state.cameraX)),
                static_cast<int16_t>(
                    std::lround(hazard.y - state.cameraY)),
                mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                GAME_SURFACE_WIDTH);
        }
    }

    BitmapCanvas canvas{mapFrameBuffer_, GAME_SURFACE_WIDTH,
                        GAME_SURFACE_HEIGHT, GAME_SURFACE_WIDTH};
    for (uint8_t index = 0; index < state.effectCount; ++index) {
        const pgos::PlatformerEffect effect = engine_.effect(index);
        if (!effect.active) {
            continue;
        }
        const int16_t x = static_cast<int16_t>(
            std::lround(effect.x - state.cameraX));
        const int16_t y = static_cast<int16_t>(
            std::lround(effect.y - state.cameraY));
        if (effect.kind == pgos::PlatformerEffectKind::RisingCoin) {
            blitSpriteRgb565(mapFrameBuffer_, GAME_SURFACE_WIDTH,
                             GAME_SURFACE_HEIGHT,
                             *COIN_FRAMES[(animationMs / 120U) % 4U], x, y);
        } else if (effect.kind == pgos::PlatformerEffectKind::BrickPiece) {
            blitSpriteRgb565(mapFrameBuffer_, GAME_SURFACE_WIDTH,
                             GAME_SURFACE_HEIGHT,
                             pgos::platformer_extra_assets::brick_piece, x, y,
                             effect.vx < 0.0F);
        } else if (effect.kind == pgos::PlatformerEffectKind::Score) {
            char points[8];
            snprintf(points, sizeof(points), "%u",
                     static_cast<unsigned>(effect.value));
            canvasFont_.draw(canvas, points, x + 8, y, rgb565(245, 247, 250),
                             BitmapFontSize::Small12,
                             BitmapTextAlign::Center);
        }
    }

    if (state.flagTileId < pgos::PLATFORMER_BLOCK_TILE_COUNT) {
        pgos::PlatformerTileRenderer::drawTile(
            blockTileCache_, state.flagTileId,
            static_cast<int16_t>(std::lround(state.flagX - state.cameraX)),
            static_cast<int16_t>(std::lround(state.flagY - state.cameraY)),
            mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
            GAME_SURFACE_WIDTH);
    }
    const bool flickerHidden = playerFlickerHidden(state, animationMs);
    if (state.playerVisible && !flickerHidden) {
        const uint16_t playerTile = campaignPlayerTile(state, animationMs);
        const int16_t x = static_cast<int16_t>(
            std::lround(state.playerX - state.cameraX));
        const int16_t y = static_cast<int16_t>(
            std::lround(playerTileVisualY(state) - state.cameraY));
        pgos::PlatformerTileRenderer::drawTile(
            playerTileCache_, playerTile, x, y, mapFrameBuffer_,
            GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT, GAME_SURFACE_WIDTH,
            state.playerFacingLeft);
        if (state.playerBig && state.phase != pgos::PlatformerPhase::Dying &&
            playerTile + 25U < pgos::PLATFORMER_PLAYER_TILE_COUNT) {
            pgos::PlatformerTileRenderer::drawTile(
                playerTileCache_, static_cast<uint16_t>(playerTile + 25U), x,
                static_cast<int16_t>(y + TILE_SIZE), mapFrameBuffer_,
                GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT, GAME_SURFACE_WIDTH,
                state.playerFacingLeft);
        }
    }
    pgos::PlatformerTileRenderer::drawAboveForeground(
        engine_.levelRuntime(), cameraX, cameraY, mapFrameBuffer_,
        GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT, GAME_SURFACE_WIDTH,
        &blockTileCache_);

    fillRgb565(mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT, 0, 0,
               GAME_SURFACE_WIDTH, HUD_HEIGHT, rgb565(75, 146, 197));
    char hudText[64];
    snprintf(hudText, sizeof(hudText), "M%u %06lu x%02u %u-%u T%03u",
             static_cast<unsigned>(state.lives),
             static_cast<unsigned long>(state.score % 1000000UL),
             static_cast<unsigned>(state.coinsCollected % 100U),
             static_cast<unsigned>(state.world),
             static_cast<unsigned>(state.stage),
             static_cast<unsigned>(state.timeRemaining));
    canvasFont_.draw(canvas, hudText, 6, 5, rgb565(245, 247, 250),
                     BitmapFontSize::Small12);
}

void PlatformerApp::draw(lv_event_t* event) {
    lv_obj_t* object = lv_event_get_target_obj(event);
    lv_layer_t* layer = lv_event_get_layer(event);
    if (object == nullptr || layer == nullptr) {
        return;
    }

    lv_area_t surfaceArea;
    lv_obj_get_coords(object, &surfaceArea);
    if (campaignMapTest_ && mapFrameBuffer_ != nullptr) {
        drawCampaignMapTest(layer, surfaceArea);
        return;
    }
    const pgos::PlatformerSnapshot state = engine_.snapshot();
    const uint32_t animationMs = millis();
    const bool flickerHidden = playerFlickerHidden(state, animationMs);
    const bool campaignRendered =
        state.campaignMode && engine_.levelRuntime().level() != nullptr &&
        mapFrameBuffer_ != nullptr;
    if (campaignRendered && directFrameReady_) {
        pgos::PixelSpriteRenderer::draw(layer, mapFrameSprite_, surfaceArea.x1,
                                        surfaceArea.y1);
    } else if (campaignRendered) {
        const int32_t cameraX =
            static_cast<int32_t>(std::lround(state.cameraX));
        const int32_t cameraY =
            static_cast<int32_t>(std::lround(state.cameraY));
        pgos::PlatformerTileRenderer::renderBase(
            engine_.levelRuntime(),
            cameraX, cameraY, mapFrameBuffer_,
            GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT, GAME_SURFACE_WIDTH,
            &blockTileCache_);
        if (state.vineActive) {
            const pgos::PlatformerVineState vine = engine_.vine();
            const uint8_t pieces = static_cast<uint8_t>(std::min<float>(
                6.0F, std::ceil(vine.grownPixels / TILE_SIZE)));
            const float topY = vine.baseY - vine.grownPixels;
            for (uint8_t piece = 0; piece < pieces; ++piece) {
                pgos::PlatformerTileRenderer::drawTile(
                    blockTileCache_,
                    piece == 0U ? 100U : 148U,
                    static_cast<int16_t>(std::lround(vine.x - state.cameraX)),
                    static_cast<int16_t>(std::lround(
                        topY + piece * TILE_SIZE - state.cameraY)),
                    mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                    GAME_SURFACE_WIDTH);
            }
        }
        for (uint8_t index = 0; index < state.movingPlatformCount; ++index) {
            const pgos::PlatformerMovingPlatformState platform =
                engine_.movingPlatform(index);
            if (!platform.active) {
                continue;
            }
            if (platform.pulley && platform.pairIndex >= 0 &&
                index < static_cast<uint8_t>(platform.pairIndex)) {
                const pgos::PlatformerMovingPlatformState pair =
                    engine_.movingPlatform(
                        static_cast<uint8_t>(platform.pairIndex));
                for (const pgos::PlatformerMovingPlatformState* side :
                     {&platform, &pair}) {
                    const int16_t ropeX = static_cast<int16_t>(std::lround(
                        side->x + TILE_SIZE - state.cameraX));
                    const int16_t ropeTop = static_cast<int16_t>(std::lround(
                        side->pulleyTop - TILE_SIZE - state.cameraY));
                    const int16_t ropeBottom = static_cast<int16_t>(std::lround(
                        side->y - state.cameraY));
                    if (ropeX < 0 || ropeX >= GAME_SURFACE_WIDTH) {
                        continue;
                    }
                    for (int16_t y = std::max<int16_t>(0, ropeTop);
                         y < std::min<int16_t>(GAME_SURFACE_HEIGHT,
                                               ropeBottom);
                         ++y) {
                        mapFrameBuffer_[y * GAME_SURFACE_WIDTH + ropeX] =
                            0x94B2U;
                    }
                }
            }
            if (platform.sourceTileId >=
                pgos::PLATFORMER_BLOCK_TILE_COUNT) {
                continue;
            }
            const int16_t screenX = static_cast<int16_t>(std::lround(
                platform.x - state.cameraX));
            const int16_t screenY = static_cast<int16_t>(std::lround(
                platform.y - state.cameraY));
            for (uint8_t tile = 0; tile < platform.widthTiles; ++tile) {
                pgos::PlatformerTileRenderer::drawTile(
                    blockTileCache_,
                    static_cast<uint16_t>(platform.sourceTileId + tile),
                    static_cast<int16_t>(screenX + tile * TILE_SIZE), screenY,
                    mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                    GAME_SURFACE_WIDTH);
            }
        }
        for (uint8_t index = 0; index < state.fireBarCount; ++index) {
            const pgos::PlatformerFireBarState fireBar = engine_.fireBar(index);
            if (!fireBar.active) {
                continue;
            }
            constexpr float FIRE_BAR_PI = 3.14159265358979323846F;
            const float radians =
                fireBar.angleDegrees * FIRE_BAR_PI / 180.0F;
            const uint16_t sourceId =
                static_cast<uint16_t>(611U + (animationMs / 100U) % 4U);
            for (uint8_t element = 0; element < fireBar.length; ++element) {
                const float distance = static_cast<float>(element * TILE_SIZE);
                pgos::PlatformerTileRenderer::drawTile(
                    blockTileCache_, sourceId,
                    static_cast<int16_t>(std::lround(
                        fireBar.x + std::cos(radians) * distance -
                        state.cameraX)),
                    static_cast<int16_t>(std::lround(
                        fireBar.y - std::sin(radians) * distance -
                        state.cameraY)),
                    mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                    GAME_SURFACE_WIDTH);
            }
        }
        for (uint8_t index = 0; index < state.enemyCount; ++index) {
            const pgos::PlatformerEnemyState enemy = engine_.enemy(index);
            if (!enemy.active ||
                enemy.sourceTileId >= pgos::PLATFORMER_ENEMY_TILE_COUNT) {
                continue;
            }
            uint16_t sourceId = enemy.sourceTileId;
            const uint16_t reference =
                pgos::PLATFORMER_ENEMY_REFERENCE_IDS[sourceId];
            if (enemy.motion == pgos::PlatformerEnemyMotion::Squashed &&
                (reference == 70U || reference == 71U)) {
                sourceId = static_cast<uint16_t>(
                    sourceId + (reference == 70U ? 2U : 1U));
            } else if (enemy.motion == pgos::PlatformerEnemyMotion::ShellIdle ||
                enemy.motion == pgos::PlatformerEnemyMotion::ShellSliding) {
                sourceId = enemy.type == pgos::PlatformerEnemyType::BuzzyBeetle
                               ? 89U
                               : reference == 455U ? 494U : 77U;
            }
            if (enemy.motion != pgos::PlatformerEnemyMotion::Squashed &&
                (reference == 39U || reference == 71U)) {
                --sourceId;
            }
            if (enemy.motion != pgos::PlatformerEnemyMotion::Squashed &&
                (reference == 38U || reference == 39U || reference == 70U ||
                 reference == 71U || reference == 87U) &&
                ((animationMs / 180U) & 1U) != 0U) {
                ++sourceId;
            }
            const bool shell =
                enemy.motion == pgos::PlatformerEnemyMotion::ShellIdle ||
                enemy.motion == pgos::PlatformerEnemyMotion::ShellSliding;
            const bool tall = !shell &&
                              (enemy.type == pgos::PlatformerEnemyType::Koopa ||
                              enemy.type ==
                                  pgos::PlatformerEnemyType::KoopaParatroopa ||
                              enemy.type ==
                                  pgos::PlatformerEnemyType::PiranhaPlant ||
                              enemy.type == pgos::PlatformerEnemyType::Blooper ||
                              enemy.type == pgos::PlatformerEnemyType::Lakitu ||
                              enemy.type ==
                                  pgos::PlatformerEnemyType::HammerBro ||
                              enemy.type == pgos::PlatformerEnemyType::Bowser);
            const bool wide = enemy.type == pgos::PlatformerEnemyType::Bowser;
            const bool flipX =
                !shell && enemySpriteFacesMovement(enemy.type) &&
                !enemy.facingLeft;
            const int16_t screenX = static_cast<int16_t>(std::lround(
                enemy.x - state.cameraX));
            const int16_t screenY = static_cast<int16_t>(std::lround(
                enemy.y - state.cameraY -
                ((!shell &&
                  (enemy.type == pgos::PlatformerEnemyType::Koopa ||
                   enemy.type ==
                       pgos::PlatformerEnemyType::KoopaParatroopa))
                     ? 8.0F
                     : 0.0F)));
            for (uint8_t tileY = 0; tileY < (tall ? 2U : 1U); ++tileY) {
                for (uint8_t tileX = 0; tileX < (wide ? 2U : 1U); ++tileX) {
                    pgos::PlatformerTileRenderer::drawTile(
                        enemyTileCache_,
                        static_cast<uint16_t>(sourceId + tileY * 35U + tileX),
                        static_cast<int16_t>(screenX + tileX * TILE_SIZE),
                        static_cast<int16_t>(screenY + tileY * TILE_SIZE),
                        mapFrameBuffer_, GAME_SURFACE_WIDTH,
                        GAME_SURFACE_HEIGHT, GAME_SURFACE_WIDTH, flipX);
                }
            }
        }
        for (uint8_t index = 0; index < state.enemyHazardCount; ++index) {
            const pgos::PlatformerEnemyHazardState hazard =
                engine_.enemyHazard(index);
            if (!hazard.active ||
                hazard.sourceTileId >= pgos::PLATFORMER_ENEMY_TILE_COUNT) {
                continue;
            }
            pgos::PlatformerTileRenderer::drawTile(
                enemyTileCache_, hazard.sourceTileId,
                static_cast<int16_t>(std::lround(hazard.x - state.cameraX)),
                static_cast<int16_t>(std::lround(hazard.y - state.cameraY)),
                mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                GAME_SURFACE_WIDTH);
        }
        if (state.flagTileId < pgos::PLATFORMER_BLOCK_TILE_COUNT) {
            pgos::PlatformerTileRenderer::drawTile(
                blockTileCache_, state.flagTileId,
                static_cast<int16_t>(
                    std::lround(state.flagX - state.cameraX)),
                static_cast<int16_t>(
                    std::lround(state.flagY - state.cameraY)),
                mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                GAME_SURFACE_WIDTH);
        }
        if (state.playerVisible && !flickerHidden) {
            const uint16_t playerTile = campaignPlayerTile(state, animationMs);
            const int16_t screenX = static_cast<int16_t>(
                std::lround(state.playerX - state.cameraX));
            const int16_t screenY = static_cast<int16_t>(
                std::lround(playerTileVisualY(state) - state.cameraY));
            pgos::PlatformerTileRenderer::drawTile(
                playerTileCache_, playerTile, screenX, screenY,
                mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                GAME_SURFACE_WIDTH, state.playerFacingLeft);
            if (state.playerBig &&
                state.phase != pgos::PlatformerPhase::Dying &&
                playerTile + 25U <
                                       pgos::PLATFORMER_PLAYER_TILE_COUNT) {
                pgos::PlatformerTileRenderer::drawTile(
                    playerTileCache_, static_cast<uint16_t>(playerTile + 25U),
                    screenX, static_cast<int16_t>(screenY + TILE_SIZE),
                    mapFrameBuffer_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                    GAME_SURFACE_WIDTH, state.playerFacingLeft);
            }
        }
        pgos::PlatformerTileRenderer::drawAboveForeground(
            engine_.levelRuntime(), cameraX, cameraY, mapFrameBuffer_,
            GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT, GAME_SURFACE_WIDTH,
            &blockTileCache_);
        pgos::PixelSpriteRenderer::draw(layer, mapFrameSprite_, surfaceArea.x1,
                                        surfaceArea.y1);
    } else {
        drawRect(layer, surfaceArea, skyColor_);
    }
    const auto isVisibleHorizontally = [&surfaceArea](int32_t x,
                                                      int32_t width) {
        return x + width > surfaceArea.x1 && x <= surfaceArea.x2;
    };

    lv_area_t world = surfaceArea;
    world.y1 += campaignRendered
                    ? -static_cast<int16_t>(std::lround(state.cameraY))
                    : WORLD_Y_OFFSET;
    if (!campaignRendered) {
        drawRect(layer, world, skyColor_);
    }

    const int16_t camera = static_cast<int16_t>(std::lround(state.cameraX));

    for (uint8_t index = 0; index < state.totalBoxes; ++index) {
        const pgos::PlatformerBox box = engine_.box(index);
        const int16_t boxX = world.x1 + box.x - camera;
        if (boxX + TILE_SIZE < surfaceArea.x1 || boxX > surfaceArea.x2) {
            continue;
        }
        if (!box.visible) {
            if (box.opened &&
                box.type == pgos::PlatformerObjectType::CoinBrick) {
                const int16_t boxY = world.y1 + box.y;
                lv_area_t clearedBrick = {
                    static_cast<lv_coord_t>(boxX),
                    static_cast<lv_coord_t>(boxY),
                    static_cast<lv_coord_t>(boxX + TILE_SIZE - 1),
                    static_cast<lv_coord_t>(boxY + TILE_SIZE - 1),
                };
                drawRect(layer, clearedBrick, lv_color_hex(0x6380E8));
            }
            continue;
        }
        const pgos::PixelSprite* boxSprite = nullptr;
        if (box.opened) {
            boxSprite = &pgos::platformer_assets::empty;
        } else if (box.type == pgos::PlatformerObjectType::CoinBrick) {
            boxSprite = &pgos::platformer_assets::bricks;
        } else {
            boxSprite = BOX_FRAMES[(animationMs / 160U) % 3U];
        }
        pgos::PixelSpriteRenderer::draw(
            layer, *boxSprite, boxX, world.y1 + box.y + box.bumpOffset);
    }

    for (uint8_t index = 0; index < state.powerupCount; ++index) {
        const pgos::PlatformerPowerup powerup = engine_.powerup(index);
        if (!powerup.active) {
            continue;
        }
        const pgos::PixelSprite* sprite = &pgos::platformer_assets::mushroom;
        switch (powerup.kind) {
            case pgos::PlatformerPowerupKind::Mushroom:
                break;
            case pgos::PlatformerPowerupKind::OneUp:
                sprite = &pgos::platformer_extra_assets::one_up;
                break;
            case pgos::PlatformerPowerupKind::FireFlower: {
                sprite = FIRE_FLOWER_FRAMES[(animationMs / 70U) % 4U];
                break;
            }
            case pgos::PlatformerPowerupKind::Star: {
                sprite = STAR_FRAMES[(animationMs / 70U) % 4U];
                break;
            }
        }
        const int16_t x = world.x1 +
                          static_cast<int16_t>(std::lround(powerup.x)) -
                          camera;
        if (!isVisibleHorizontally(x, sprite->width)) {
            continue;
        }
        pgos::PixelSpriteRenderer::draw(
            layer, *sprite, x,
            world.y1 + static_cast<int16_t>(std::lround(powerup.y)));
    }

    const pgos::PixelSprite& coinSprite =
        *COIN_FRAMES[(animationMs / 120U) % 4U];

    const int16_t goalX = world.x1 +
                          static_cast<int16_t>(std::lround(engine_.goalX())) -
                          camera;
    if (!campaignRendered) {
        lv_area_t pole = {
            static_cast<lv_coord_t>(goalX),
            static_cast<lv_coord_t>(world.y1 +
                                    pgos::PLATFORMER_LEVEL_1_1.flagTopY),
            static_cast<lv_coord_t>(goalX + 2),
            static_cast<lv_coord_t>(
                world.y1 + pgos::PLATFORMER_LEVEL_1_1.flagPoleBottomY),
        };
        drawRect(layer, pole, textColor_);
        pgos::PixelSpriteRenderer::draw(
            layer, pgos::platformer_extra_assets::pole_flag, goalX - 16,
            world.y1 + static_cast<int16_t>(std::lround(state.flagY)));
    }

    if (!campaignRendered &&
        (state.phase == pgos::PlatformerPhase::TimeBonus ||
         state.phase == pgos::PlatformerPhase::Won)) {
        const int16_t castleFlagX = world.x1 +
                                    static_cast<int16_t>(std::lround(engine_.castleX())) -
                                    camera - 11;
        const int16_t castleFlagRise =
            state.phase == pgos::PlatformerPhase::Won
                ? 14
                : std::min<int16_t>(14, state.phaseElapsedMs / 35U);
        const int16_t castleFlagY = world.y1 + 120 - castleFlagRise;
        pgos::PixelSpriteRenderer::draw(
            layer, pgos::platformer_extra_assets::castle_flag, castleFlagX,
            castleFlagY);
    }

    for (uint8_t index = 0; !campaignRendered && index < state.enemyCount;
         ++index) {
        const pgos::PlatformerEnemyState enemy = engine_.enemy(index);
        if (!enemy.active) {
            continue;
        }
        const int16_t x = world.x1 +
                          static_cast<int16_t>(std::lround(enemy.x)) - camera;
        const int16_t y = world.y1 +
                          static_cast<int16_t>(std::lround(enemy.y));
        if (x + static_cast<int16_t>(std::ceil(enemy.width)) < surfaceArea.x1 ||
            x > surfaceArea.x2) {
            continue;
        }
        if (enemy.motion == pgos::PlatformerEnemyMotion::ShellIdle ||
            enemy.motion == pgos::PlatformerEnemyMotion::ShellSliding) {
            const pgos::PixelSprite& shell =
                pgos::platformer_extra_assets::koopa_shell;
            pgos::PixelSpriteRenderer::draw(
                layer, shell, x,
                y + static_cast<int16_t>(std::ceil(enemy.height)) -
                    static_cast<int16_t>(shell.height),
                1, enemy.facingLeft);
        } else if (enemy.type == pgos::PlatformerEnemyType::Koopa) {
            const pgos::PixelSprite& koopaSprite =
                ((animationMs / 180U) & 1U) == 0U
                    ? pgos::platformer_assets::koopa_1
                    : pgos::platformer_assets::koopa_2;
            // The converted Koopa frames face left by default, just like the
            // reference game's sprite sheet.  Mirror only while travelling
            // right; passing facingLeft here reverses the visual direction.
            // Enemy coordinates are collision-box tops.  Keep the legacy
            // 17x32 converted Koopa art aligned by its feet; the reference
            // sprite itself is 16x24, so this also remains correct if the
            // asset is regenerated at its native size.
            const int16_t koopaYOffset = static_cast<int16_t>(
                koopaSprite.height > static_cast<uint16_t>(std::ceil(enemy.height))
                    ? koopaSprite.height -
                          static_cast<uint16_t>(std::ceil(enemy.height))
                    : 0U);
            pgos::PixelSpriteRenderer::draw(layer, koopaSprite, x,
                                            y - koopaYOffset, 1,
                                            !enemy.facingLeft);
        } else {
            const pgos::PixelSprite& goombaSprite =
                enemy.motion == pgos::PlatformerEnemyMotion::Squashed
                    ? pgos::platformer_assets::goomba_flat
                    : ((animationMs / 180U) & 1U) == 0U
                          ? pgos::platformer_assets::goomba_1
                          : pgos::platformer_assets::goomba_2;
            pgos::PixelSpriteRenderer::draw(layer, goombaSprite, x, y,
                                            1, enemy.facingLeft);
        }
    }

    for (uint8_t index = 0; index < state.projectileCount; ++index) {
        const pgos::PlatformerProjectile projectile = engine_.projectile(index);
        if (!projectile.active) {
            continue;
        }
        const pgos::PixelSprite* projectileSprite = nullptr;
        if (projectile.exploding) {
            projectileSprite = FIREBALL_EXPLOSION_FRAMES[std::min<uint8_t>(
                2, static_cast<uint8_t>(projectile.ageMs / 40U))];
        } else {
            projectileSprite = FIREBALL_FRAMES[(animationMs / 80U) % 4U];
        }
        const int16_t x = world.x1 +
                          static_cast<int16_t>(std::lround(projectile.x)) -
                          camera;
        if (!isVisibleHorizontally(x, projectileSprite->width)) {
            continue;
        }
        pgos::PixelSpriteRenderer::draw(
            layer, *projectileSprite, x,
            world.y1 + static_cast<int16_t>(std::lround(projectile.y)));
    }

    for (uint8_t index = 0; index < state.effectCount; ++index) {
        const pgos::PlatformerEffect effect = engine_.effect(index);
        if (!effect.active) {
            continue;
        }
        const int16_t x = world.x1 +
                          static_cast<int16_t>(std::lround(effect.x)) - camera;
        const int16_t y = world.y1 +
                          static_cast<int16_t>(std::lround(effect.y));
        if (!isVisibleHorizontally(x, 32)) {
            continue;
        }
        if (effect.kind == pgos::PlatformerEffectKind::RisingCoin) {
            pgos::PixelSpriteRenderer::draw(layer, coinSprite, x, y);
        } else if (effect.kind == pgos::PlatformerEffectKind::BrickPiece) {
            pgos::PixelSpriteRenderer::draw(
                layer, pgos::platformer_extra_assets::brick_piece, x, y, 1,
                effect.vx < 0.0F);
        } else if (effect.kind == pgos::PlatformerEffectKind::Score) {
            char points[8];
            lv_snprintf(points, sizeof(points), "%u",
                        static_cast<unsigned>(effect.value));
            lv_area_t scoreArea = {
                static_cast<lv_coord_t>(x - 12),
                static_cast<lv_coord_t>(y - 2),
                static_cast<lv_coord_t>(x + 28),
                static_cast<lv_coord_t>(y + 12),
            };
            drawText(layer, points, scoreArea, textColor_, hudFont_);
        }
    }

    const int16_t playerX = world.x1 +
                            static_cast<int16_t>(std::lround(state.playerX)) -
                            camera;
    const pgos::PixelSprite* playerSprite = nullptr;
    if (state.phase == pgos::PlatformerPhase::Dying) {
        playerSprite = &pgos::platformer_extra_assets::mario_death;
    } else if (state.phase == pgos::PlatformerPhase::Flagpole) {
        const bool secondFrame = ((animationMs / 70U) & 1U) != 0U;
        if (state.playerBig) {
            playerSprite = secondFrame
                               ? &pgos::platformer_extra_assets::mario_big_flag2
                               : &pgos::platformer_extra_assets::mario_big_flag1;
        } else {
            playerSprite = secondFrame
                               ? &pgos::platformer_extra_assets::mario_flag2
                               : &pgos::platformer_extra_assets::mario_flag1;
        }
    } else if (state.playerFire) {
        if (state.playerCrouching) {
            playerSprite = &pgos::platformer_extra_assets::mario_fire_crouch;
        } else if (!state.grounded) {
            playerSprite = &pgos::platformer_extra_assets::mario_fire_jump;
        } else if (state.playerSkidding) {
            playerSprite = &pgos::platformer_extra_assets::mario_fire_skid;
        } else if (std::abs(state.playerVx) > 8.0F) {
            const uint8_t runFrame = static_cast<uint8_t>((animationMs / 90U) % 3U);
            playerSprite = runFrame == 0U
                               ? &pgos::platformer_extra_assets::mario_fire_run1
                               : runFrame == 1U
                                     ? &pgos::platformer_extra_assets::mario_fire_run2
                                     : &pgos::platformer_extra_assets::mario_fire_run3;
        } else {
            playerSprite = &pgos::platformer_extra_assets::mario_fire_idle;
        }
    } else if (state.playerBig) {
        if (state.playerCrouching) {
            playerSprite = &pgos::platformer_extra_assets::mario_big_crouch;
        } else if (state.playerSkidding) {
            playerSprite = &pgos::platformer_extra_assets::mario_big_skid;
        } else if (!state.grounded) {
            playerSprite = &pgos::platformer_assets::mario_big_jump;
        } else if (std::abs(state.playerVx) > 8.0F) {
            const uint8_t runFrame = static_cast<uint8_t>((animationMs / 90U) % 3U);
            playerSprite = runFrame == 0U
                               ? &pgos::platformer_assets::mario_big_run1
                               : runFrame == 1U
                                     ? &pgos::platformer_assets::mario_big_run2
                                     : &pgos::platformer_assets::mario_big_run3;
        } else {
            playerSprite = &pgos::platformer_assets::mario_big_idle;
        }
    } else if (state.playerSkidding) {
        playerSprite = &pgos::platformer_extra_assets::mario_skid;
    } else if (!state.grounded) {
        playerSprite = &pgos::platformer_assets::mario_jump;
    } else if (std::abs(state.playerVx) > 8.0F) {
        const uint8_t runFrame = static_cast<uint8_t>((animationMs / 90U) % 3U);
        playerSprite = runFrame == 0U
                           ? &pgos::platformer_assets::mario_run1
                           : runFrame == 1U
                                 ? &pgos::platformer_assets::mario_run2
                                 : &pgos::platformer_assets::mario_run3;
    } else {
        playerSprite = &pgos::platformer_assets::mario_idle;
    }
    if (!campaignRendered && state.playerVisible && !flickerHidden &&
        playerSprite != nullptr) {
        const float spriteWorldY =
            state.playerCrouching && state.playerBig
                ? state.playerY + pgos::PlatformerEngine::CROUCH_PLAYER_HEIGHT -
                      playerSprite->height
                : state.playerY;
        const int16_t playerY = world.y1 +
                                static_cast<int16_t>(std::lround(spriteWorldY));
        pgos::PixelSpriteRenderer::draw(
            layer, *playerSprite, playerX, playerY, 1,
            state.playerFacingLeft);
    }

    lv_area_t hud = surfaceArea;
    hud.y2 = hud.y1 + HUD_HEIGHT - 1;
    drawRect(layer, hud, skyDarkColor_);
    char hudText[64];
    lv_snprintf(hudText, sizeof(hudText),
                "M%u %06lu  x%02u  %u-%u  T%03u",
                static_cast<unsigned>(state.lives),
                static_cast<unsigned long>(state.score % 1000000UL),
                static_cast<unsigned>(state.coinsCollected % 100U),
                static_cast<unsigned>(state.world),
                static_cast<unsigned>(state.stage),
                static_cast<unsigned>(state.timeRemaining));
    lv_area_t hudTextArea = hud;
    hudTextArea.x1 += 7;
    hudTextArea.x2 -= 7;
    drawText(layer, hudText, hudTextArea, textColor_, hudFont_,
             LV_TEXT_ALIGN_LEFT);

    if (state.phase == pgos::PlatformerPhase::Dying) {
        const char* reason = "MISS";
        switch (state.deathReason) {
            case pgos::PlatformerDeathReason::Time:
                reason = "TIME UP";
                break;
            case pgos::PlatformerDeathReason::Enemy:
                reason = "ENEMY HIT";
                break;
            case pgos::PlatformerDeathReason::Fall:
                reason = "FELL";
                break;
            case pgos::PlatformerDeathReason::None:
                break;
        }
        lv_area_t reasonArea = surfaceArea;
        reasonArea.y1 += HUD_HEIGHT + 5;
        reasonArea.y2 = reasonArea.y1 + 22;
        drawText(layer, reason, reasonArea, textColor_, overlayFont_);
    }

    const bool showOverlay = state.phase == pgos::PlatformerPhase::Title ||
                             state.phase == pgos::PlatformerPhase::Paused ||
                             state.phase == pgos::PlatformerPhase::GameOver ||
                             state.phase == pgos::PlatformerPhase::Won;
    if (showOverlay) {
        const bool selectingLevel =
            state.phase == pgos::PlatformerPhase::Title;
        lv_area_t overlay = surfaceArea;
        overlay.x1 += 28;
        overlay.x2 -= 28;
        overlay.y1 += selectingLevel ? 38 : 46;
        overlay.y2 = overlay.y1 + (selectingLevel ? 104 : 86);
        drawRect(layer, overlay, lv_color_hex(0x101722), 10, LV_OPA_90);

        const char* title = progress_.completed() ? "ALL WORLDS CLEAR"
                                                  : "SUPER MARIO BROS.";
        char subtitle[48];
        lv_snprintf(subtitle, sizeof(subtitle), "WORLD %u-%u",
                    static_cast<unsigned>(selectedWorld_),
                    static_cast<unsigned>(selectedStage_));
        if (state.phase == pgos::PlatformerPhase::Paused) {
            title = "PAUSED";
            lv_snprintf(subtitle, sizeof(subtitle), "WORLD %u-%u",
                        static_cast<unsigned>(state.world),
                        static_cast<unsigned>(state.stage));
        } else if (state.phase == pgos::PlatformerPhase::GameOver) {
            title = "GAME OVER";
            lv_snprintf(subtitle, sizeof(subtitle), "WORLD %u-%u",
                        static_cast<unsigned>(state.world),
                        static_cast<unsigned>(state.stage));
        } else if (state.phase == pgos::PlatformerPhase::Won) {
            title = state.world == 8U && state.stage == 4U ? "THANK YOU MARIO"
                                                           : "STAGE CLEAR";
            lv_snprintf(subtitle, sizeof(subtitle), "WORLD %u-%u  %06lu",
                        static_cast<unsigned>(state.world),
                        static_cast<unsigned>(state.stage),
                        static_cast<unsigned long>(state.score % 1000000UL));
        }

        lv_area_t titleArea = overlay;
        titleArea.y1 += 11;
        titleArea.y2 = titleArea.y1 + 26;
        drawText(layer, title, titleArea,
                 state.phase == pgos::PlatformerPhase::Won ? accentColor_
                                                           : textColor_,
                 overlayFont_);
        if (selectingLevel) {
            char worldText[20];
            char stageText[20];
            lv_snprintf(worldText, sizeof(worldText),
                        selectedLevelField_ == 0 ? "WORLD [%u]" : "WORLD %u",
                        static_cast<unsigned>(selectedWorld_));
            lv_snprintf(stageText, sizeof(stageText),
                        selectedLevelField_ == 1 ? "STAGE [%u]" : "STAGE %u",
                        static_cast<unsigned>(selectedStage_));
            lv_area_t worldArea = overlay;
            worldArea.x1 += 16;
            worldArea.x2 = (overlay.x1 + overlay.x2) / 2 - 2;
            worldArea.y1 += 43;
            worldArea.y2 = worldArea.y1 + 18;
            lv_area_t stageArea = worldArea;
            stageArea.x1 = worldArea.x2 + 5;
            stageArea.x2 = overlay.x2 - 16;
            drawText(layer, worldText, worldArea,
                     selectedLevelField_ == 0 ? accentColor_ : mutedColor_,
                     hudFont_);
            drawText(layer, stageText, stageArea,
                     selectedLevelField_ == 1 ? accentColor_ : mutedColor_,
                     hudFont_);

            char bestText[24];
            lv_snprintf(bestText, sizeof(bestText), "BEST %06lu",
                        static_cast<unsigned long>(progress_.bestScore() %
                                                   1000000UL));
            lv_area_t bestArea = overlay;
            bestArea.y1 = overlay.y2 - 28;
            bestArea.y2 = overlay.y2 - 10;
            drawText(layer, bestText, bestArea, mutedColor_, hudFont_);
        } else {
            lv_area_t subtitleArea = overlay;
            subtitleArea.y1 = overlay.y2 - 25;
            subtitleArea.y2 = overlay.y2 - 7;
            drawText(layer, subtitle, subtitleArea, mutedColor_, hudFont_);
        }
    }
}

void PlatformerApp::adjustLevelSelection(int8_t delta) {
    uint8_t& value =
        selectedLevelField_ == 0 ? selectedWorld_ : selectedStage_;
    const uint8_t maximum = selectedLevelField_ == 0 ? 8U : 4U;
    const int16_t next = std::max<int16_t>(
        1, std::min<int16_t>(maximum, static_cast<int16_t>(value) + delta));
    if (value == static_cast<uint8_t>(next)) {
        return;
    }
    value = static_cast<uint8_t>(next);
    engine_.prepareCampaignTitle(selectedWorld_, selectedStage_);
}

float PlatformerApp::sampleMove(const GamepadSnapshot& gamepad,
                                uint32_t nowMs) {
    if (gamepad.connected) {
        if ((gamepad.dpad & GamepadDpadLeft) != 0) {
            return -1.0F;
        }
        if ((gamepad.dpad & GamepadDpadRight) != 0) {
            return 1.0F;
        }
        const int16_t axis = moveAxisFilter_.update(gamepad.axisX);
        if (axis != 0) {
            return std::clamp(static_cast<float>(axis) / 512.0F, -1.0F,
                              1.0F);
        }
    }
    if (static_cast<int32_t>(nowMs - commandMoveUntilMs_) < 0) {
        return static_cast<float>(commandMoveDirection_);
    }
    return 0.0F;
}

bool PlatformerApp::sampleJumpHeld(const GamepadSnapshot& gamepad,
                                    uint32_t nowMs) const {
    if (gamepad.connected &&
        ((gamepad.buttons & GamepadButtonA) != 0 ||
         (gamepad.dpad & GamepadDpadUp) != 0)) {
        return true;
    }
    return static_cast<int32_t>(nowMs - commandJumpHoldUntilMs_) < 0;
}

bool PlatformerApp::sampleCrouchHeld(const GamepadSnapshot& gamepad,
                                     uint32_t nowMs) {
    if (gamepad.connected) {
        if ((gamepad.dpad & GamepadDpadDown) != 0) {
            return true;
        }
        if (crouchAxisFilter_.update(gamepad.axisY) > 0) {
            return true;
        }
    }
    return static_cast<int32_t>(nowMs - commandCrouchUntilMs_) < 0;
}

bool PlatformerApp::sampleActionHeld(const GamepadSnapshot& gamepad) const {
    if (!gamepad.connected) {
        return false;
    }
    constexpr uint16_t ACTION_BUTTONS =
        GamepadButtonY | GamepadButtonShoulderR | GamepadButtonTriggerR;
    return (gamepad.buttons & ACTION_BUTTONS) != 0 ||
           gamepad.throttle >= 96U;
}

void PlatformerApp::consumeEvents(AppContext& context) {
    pgos::PlatformerEvent event;
    const bool gamepadConnected = context.gamepad.snapshot().connected;
    while (engine_.pollEvent(event)) {
        switch (event.type) {
            case pgos::PlatformerEventType::CoinBoxHit:
                context.audio.playGameTone(520);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(55, 40, 60);
                }
                break;
            case pgos::PlatformerEventType::BrickBroken:
                context.audio.playGameTone(110);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(95, 115, 85);
                }
                break;
            case pgos::PlatformerEventType::PowerupAppeared:
                context.audio.playGameTone(120);
                break;
            case pgos::PlatformerEventType::PowerupCollected:
                context.audio.playGameTone(680);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(90, 70, 100);
                }
                break;
            case pgos::PlatformerEventType::PlayerHurt:
                context.audio.playGameTone(180);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(170, 135, 180);
                }
                break;
            case pgos::PlatformerEventType::EnemyStomped:
                context.audio.playGameTone(70);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(70, 95, 90);
                }
                break;
            case pgos::PlatformerEventType::ShellKicked:
            case pgos::PlatformerEventType::EnemyDefeated:
                context.audio.playGameTone(80);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(90, 110, 75);
                }
                break;
            case pgos::PlatformerEventType::FireballShot:
                context.audio.playGameTone(55);
                break;
            case pgos::PlatformerEventType::OneUp:
                context.audio.playGameTone(420);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(120, 80, 150);
                }
                break;
            case pgos::PlatformerEventType::TimeWarning:
                context.audio.playGameTone(650);
                break;
            case pgos::PlatformerEventType::PlayerDied:
                context.audio.playGameTone(240);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(220, 160, 240);
                }
                break;
            case pgos::PlatformerEventType::LifeRestarted:
                break;
            case pgos::PlatformerEventType::ReachedGoal:
                context.audio.playGameTone(360);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(260, 220, 255);
                }
                break;
            case pgos::PlatformerEventType::CourseClear:
                context.audio.playGameTone(700);
                if (const pgos::PlatformerCampaignLevel* level =
                        engine_.levelRuntime().level();
                    level != nullptr) {
                    progress_.recordCourseClear(
                        level->world, level->stage, level->nextWorld,
                        level->nextStage, engine_.snapshot().score);
                }
                break;
            case pgos::PlatformerEventType::WarpStarted:
                context.audio.playGameTone(95);
                break;
            case pgos::PlatformerEventType::WarpCompleted:
                context.audio.playGameTone(145);
                break;
            case pgos::PlatformerEventType::Jumped:
                break;
        }
    }
}

void PlatformerApp::invalidate() {
    if (!directFrameReady_ || !directCampaignPhase(engine_.phase())) {
        surface_.invalidate();
    }
}
