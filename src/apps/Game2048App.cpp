#include "apps/Game2048App.h"

#include "services/AudioService.h"
#include "services/BleGamepadService.h"
#include "services/Game2048ProfileService.h"
#include "services/RgbService.h"
#include "ui/CanvasDraw.h"
#include "ui/UiRuntime.h"

#include <algorithm>
#include <cstdio>

using pgos::drawRect;
using pgos::drawText;
using pgos::drawTextSingleLine;

AppId Game2048App::id() const { return AppId::Game2048; }

const char* Game2048App::name() const { return "2048"; }

void Game2048App::onEnter(AppContext& context) {
    root_ = nullptr;
    surface_ = nullptr;
    phase_ = Phase::Title;
    gameRecorded_ = false;
    bestScore_ = context.game2048Profile.bestScore();
    randomSeed_ ^= millis() + 0x9E3779B9U;
    engine_.reset(randomSeed_);
    activeMove_ = {};
    animationUntilMs_ = 0;
    activeAnimationMs_ = MOVE_ANIMATION_MS;
    bannerStartMs_ = 0;
    bannerUntilMs_ = 0;
    nextAiMoveMs_ = 0;
    pendingGameOver_ = false;
    inputPolicy_.reset();
    const GamepadSnapshot& gamepad = context.gamepad.snapshot();
    const bool yHeld = gamepad.connected &&
                       (gamepad.buttons & GamepadButtonY) != 0;
    aiHoldPolicy_.reset(yHeld);
    aiActive_ = false;
}

void Game2048App::onExit(AppContext& context) {
    if (phase_ == Phase::GameOver && !gameRecorded_) {
        context.game2048Profile.recordGame(engine_.score(), engine_.bestTile(),
                                           engine_.hasReachedTarget());
    } else {
        context.game2048Profile.observeBest(engine_.score(),
                                            engine_.bestTile());
    }
    root_ = nullptr;
    surface_ = nullptr;
    hudFont_ = nullptr;
    tinyFont_ = nullptr;
    titleFont_ = nullptr;
    tileFont_ = nullptr;
    tileMediumFont_ = nullptr;
    tileSmallFont_ = nullptr;
    tileCompactFont_ = nullptr;
    overlayFont_ = nullptr;
    aiHoldPolicy_.reset();
    aiActive_ = false;
    nextAiMoveMs_ = 0;
}

void Game2048App::onCommand(const AppCommand& command, AppContext& context) {
    const uint32_t nowMs = millis();
    switch (command.type) {
        case AppCommandType::Previous:
            if (!aiActive_) {
                applyMove(pgos::Game2048Direction::Up, context, nowMs);
            }
            break;
        case AppCommandType::Next:
            if (!aiActive_) {
                applyMove(pgos::Game2048Direction::Down, context, nowMs);
            }
            break;
        case AppCommandType::Left:
            if (!aiActive_) {
                applyMove(pgos::Game2048Direction::Left, context, nowMs);
            }
            break;
        case AppCommandType::Right:
            if (!aiActive_) {
                applyMove(pgos::Game2048Direction::Right, context, nowMs);
            }
            break;
        case AppCommandType::Activate:
            if (phase_ == Phase::Title || phase_ == Phase::GameOver) {
                startGame(nowMs);
            } else if (phase_ == Phase::Running) {
                phase_ = Phase::Paused;
                invalidate();
            } else {
                phase_ = Phase::Running;
                invalidate();
            }
            break;
        case AppCommandType::Pause:
            if (phase_ == Phase::Running) {
                phase_ = Phase::Paused;
            } else if (phase_ == Phase::Paused) {
                phase_ = Phase::Running;
            }
            invalidate();
            break;
        default:
            break;
    }
}

void Game2048App::onTick(uint32_t nowMs, AppContext& context) {
    if (surface_ == nullptr) {
        return;
    }
    const GamepadSnapshot& gamepad = context.gamepad.snapshot();
    sampleAi(gamepad, nowMs, context);
    if (animationUntilMs_ != 0 &&
        static_cast<int32_t>(nowMs - animationUntilMs_) >= 0) {
        animationUntilMs_ = 0;
        activeMove_ = {};
        if (pendingGameOver_) {
            pendingGameOver_ = false;
            finishGame(context);
        }
        invalidate();
    }
    if (bannerUntilMs_ != 0 &&
        static_cast<int32_t>(nowMs - bannerUntilMs_) >= 0) {
        bannerUntilMs_ = 0;
        invalidate();
    }
    if (!aiActive_) {
        sampleAnalog(gamepad, nowMs, context);
    }
    if (animationUntilMs_ != 0 || bannerUntilMs_ != 0) {
        invalidate();
    }
}

lv_obj_t* Game2048App::onCreateView(AppContext& context) {
    root_ = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, SURFACE_WIDTH, SURFACE_HEIGHT);
    lv_obj_set_pos(root_, 0, 22);
    lv_obj_set_style_bg_color(root_, context.ui.background(), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    surface_ = lv_obj_create(root_);
    lv_obj_remove_style_all(surface_);
    lv_obj_set_size(surface_, SURFACE_WIDTH, SURFACE_HEIGHT);
    lv_obj_set_pos(surface_, 0, 0);
    lv_obj_set_style_bg_color(surface_, context.ui.background(), 0);
    lv_obj_set_style_bg_opa(surface_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(surface_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(surface_, drawEvent, LV_EVENT_DRAW_MAIN, this);

    hudFont_ = context.ui.font(12);
    tinyFont_ = context.ui.font(10);
    titleFont_ = context.ui.font(28);
    tileFont_ = context.ui.font(24);
    tileMediumFont_ = context.ui.font(20);
    tileSmallFont_ = context.ui.font(16);
    tileCompactFont_ = context.ui.font(14);
    overlayFont_ = context.ui.font(20);
    backgroundColor_ = lv_color_hex(0x111827);
    boardColor_ = lv_color_hex(0x1E293B);
    gapColor_ = lv_color_hex(0x334155);
    textColor_ = lv_color_hex(0xF8FAFC);
    mutedColor_ = lv_color_hex(0x94A3B8);
    accentColor_ = lv_color_hex(0xA78BFA);
    invalidate();
    return root_;
}

void Game2048App::onUpdateView(AppContext&) { invalidate(); }

void Game2048App::drawEvent(lv_event_t* event) {
    auto* app = static_cast<Game2048App*>(lv_event_get_user_data(event));
    if (app != nullptr && lv_event_get_target_obj(event) == app->surface_) {
        app->draw(event);
    }
}

void Game2048App::draw(lv_event_t* event) {
    lv_obj_t* object = lv_event_get_target_obj(event);
    lv_layer_t* layer = lv_event_get_layer(event);
    if (object == nullptr || layer == nullptr) {
        return;
    }
    lv_area_t surface;
    lv_obj_get_coords(object, &surface);
    drawRect(layer, surface, backgroundColor_);

    lv_area_t titleArea = surface;
    titleArea.x1 += 9;
    titleArea.x2 = titleArea.x1 + 90;
    titleArea.y1 += 1;
    titleArea.y2 = titleArea.y1 + 31;
    drawText(layer, "2048", titleArea, textColor_, titleFont_,
             LV_TEXT_ALIGN_LEFT);

    lv_area_t scoreBox = {static_cast<lv_coord_t>(surface.x1 + 143),
                          static_cast<lv_coord_t>(surface.y1 + 4),
                          static_cast<lv_coord_t>(surface.x1 + 221),
                          static_cast<lv_coord_t>(surface.y1 + 28)};
    drawRect(layer, scoreBox, boardColor_, 5);
    lv_area_t bestBox = scoreBox;
    bestBox.x1 += 85;
    bestBox.x2 += 85;
    drawRect(layer, bestBox, boardColor_, 5);
    char text[32];
    lv_area_t statLabel = scoreBox;
    statLabel.y1 += 2;
    statLabel.y2 = statLabel.y1 + 10;
    drawText(layer, "SCORE", statLabel, mutedColor_, tinyFont_);
    lv_area_t statValue = scoreBox;
    statValue.y1 += 11;
    statValue.y2 = statValue.y1 + 13;
    lv_snprintf(text, sizeof(text), "%lu",
                static_cast<unsigned long>(engine_.score()));
    drawText(layer, text, statValue, textColor_, tinyFont_);
    statLabel = bestBox;
    statLabel.y1 += 2;
    statLabel.y2 = statLabel.y1 + 10;
    drawText(layer, "BEST", statLabel, mutedColor_, tinyFont_);
    statValue = bestBox;
    statValue.y1 += 11;
    statValue.y2 = statValue.y1 + 13;
    lv_snprintf(text, sizeof(text), "%lu",
                static_cast<unsigned long>(bestScore_));
    drawText(layer, text, statValue, accentColor_, tinyFont_);

    lv_area_t board = {static_cast<lv_coord_t>(surface.x1 + BOARD_X),
                       static_cast<lv_coord_t>(surface.y1 + BOARD_Y),
                       static_cast<lv_coord_t>(surface.x1 + BOARD_X + BOARD_SIZE - 1),
                       static_cast<lv_coord_t>(surface.y1 + BOARD_Y + BOARD_SIZE - 1)};
    drawRect(layer, board, boardColor_, 10);
    for (uint8_t index = 0; index < pgos::Game2048Engine::CELL_COUNT; ++index) {
        const uint8_t x = index % 4U;
        const uint8_t y = index / 4U;
        const int16_t px = board.x1 + BOARD_PADDING + x * (TILE_SIZE + TILE_GAP);
        const int16_t py = board.y1 + BOARD_PADDING + y * (TILE_SIZE + TILE_GAP);
        drawRect(layer, {px, py, static_cast<lv_coord_t>(px + TILE_SIZE - 1),
                         static_cast<lv_coord_t>(py + TILE_SIZE - 1)},
                 gapColor_, 7);
    }

    const uint32_t nowMs = millis();
    const bool animating = animationUntilMs_ != 0;
    bool animatedTarget[pgos::Game2048Engine::CELL_COUNT] = {};
    if (animating) {
        for (uint8_t i = 0; i < activeMove_.motionCount; ++i) {
            animatedTarget[activeMove_.motions[i].to] = true;
        }
    }
    for (uint8_t index = 0; index < pgos::Game2048Engine::CELL_COUNT; ++index) {
        if (engine_.tileAt(index) == 0 ||
            (animating && (animatedTarget[index] ||
                           activeMove_.spawnIndex == index))) {
            continue;
        }
        drawTile(layer, index, engine_.tileAt(index));
    }
    if (animating) {
        const uint32_t elapsed = std::min<uint32_t>(activeAnimationMs_,
                                                     nowMs - animationStartMs_);
        const uint32_t progress = (elapsed * 1000U) / activeAnimationMs_;
        const uint32_t eased = easeOut(progress);
        bool drawnMerged[pgos::Game2048Engine::CELL_COUNT] = {};
        for (uint8_t i = 0; i < activeMove_.motionCount; ++i) {
            const auto& motion = activeMove_.motions[i];
            const uint8_t fromX = motion.from % 4U;
            const uint8_t fromY = motion.from / 4U;
            const uint8_t toX = motion.to % 4U;
            const uint8_t toY = motion.to / 4U;
            const int16_t fromXpx = BOARD_X + BOARD_PADDING +
                                     fromX * (TILE_SIZE + TILE_GAP);
            const int16_t fromYpx = BOARD_Y + BOARD_PADDING +
                                     fromY * (TILE_SIZE + TILE_GAP);
            const int16_t toXpx = BOARD_X + BOARD_PADDING +
                                   toX * (TILE_SIZE + TILE_GAP);
            const int16_t toYpx = BOARD_Y + BOARD_PADDING +
                                  toY * (TILE_SIZE + TILE_GAP);
            if (motion.merged && progress >= 760U) {
                if (!drawnMerged[motion.to]) {
                    const uint32_t value = engine_.tileAt(motion.to);
                    const uint32_t local = progress - 760U;
                    const uint32_t bump = local <= 120U
                        ? local * 18U / 120U
                        : (240U - local) * 18U / 120U;
                    const uint16_t scale = static_cast<uint16_t>(100U + bump);
                    drawTileAt(layer, surface.x1 + toXpx, surface.y1 + toYpx,
                               value, scale);
                    drawnMerged[motion.to] = true;
                }
                continue;
            }
            const int16_t px = static_cast<int16_t>(fromXpx +
                ((toXpx - fromXpx) * static_cast<int32_t>(eased)) / 1000);
            const int16_t py = static_cast<int16_t>(fromYpx +
                ((toYpx - fromYpx) * static_cast<int32_t>(eased)) / 1000);
            drawTileAt(layer, surface.x1 + px, surface.y1 + py, motion.value);
        }
        if (activeMove_.spawnIndex != 0xFF && progress > 550U) {
            const uint16_t scale = static_cast<uint16_t>(
                ((progress - 550U) * 100U) / 450U);
            drawTile(layer, activeMove_.spawnIndex, activeMove_.spawnValue,
                     std::max<uint16_t>(12U, scale));
        }
    }

    if (bannerUntilMs_ != 0 && phase_ == Phase::Running) {
        drawTargetCelebration(layer, surface, nowMs);
    }
    if (phase_ == Phase::Title) {
        drawOverlay(layer, surface, "BUILD 2048", "PRESS A TO START");
    } else if (phase_ == Phase::Paused) {
        drawOverlay(layer, surface, "PAUSED", "A RESUME   B EXIT");
    } else if (phase_ == Phase::GameOver) {
        char subtitle[40];
        lv_snprintf(subtitle, sizeof(subtitle), "SCORE %lu   A RESTART",
                    static_cast<unsigned long>(engine_.score()));
        drawOverlay(layer, surface, "NO MOVES", subtitle);
    }
}

void Game2048App::drawTile(lv_layer_t* layer, uint8_t index, uint32_t value,
                           uint16_t scale, bool) const {
    lv_area_t surface{};
    if (surface_ != nullptr) {
        lv_obj_get_coords(surface_, &surface);
    }
    const uint8_t x = index % 4U;
    const uint8_t y = index / 4U;
    drawTileAt(layer, surface.x1 + BOARD_X + BOARD_PADDING +
                         x * (TILE_SIZE + TILE_GAP),
               surface.y1 + BOARD_Y + BOARD_PADDING +
                         y * (TILE_SIZE + TILE_GAP), value,
               scale);
}

void Game2048App::drawTileAt(lv_layer_t* layer, int16_t x, int16_t y,
                             uint32_t value, uint16_t scale) const {
    const int16_t size = static_cast<int16_t>(TILE_SIZE * scale / 100U);
    const int16_t inset = static_cast<int16_t>((TILE_SIZE - size) / 2);
    const lv_area_t tile = {static_cast<lv_coord_t>(x + inset),
                            static_cast<lv_coord_t>(y + inset),
                            static_cast<lv_coord_t>(x + inset + size - 1),
                            static_cast<lv_coord_t>(y + inset + size - 1)};
    drawRect(layer, tile, tileColor(value), 7);
    char text[16];
    lv_snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(value));
    const lv_font_t* font = tileNumberFont(text, size - 4);
    if (font != nullptr) {
        drawTextSingleLine(layer, text, tile, tileTextColor(value), font);
    }
}

void Game2048App::drawTargetCelebration(lv_layer_t* layer,
                                         const lv_area_t& surface,
                                         uint32_t nowMs) const {
    const uint32_t elapsed = nowMs - bannerStartMs_;
    const uint32_t revealProgress = std::min<uint32_t>(
        1000U, (elapsed * 1000U) / 280U);
    const uint32_t reveal = easeOut(revealProgress);
    const int16_t centerX = static_cast<int16_t>(surface.x1 + 160);
    const int16_t centerY = static_cast<int16_t>(surface.y1 + 109);

    drawRect(layer, surface, backgroundColor_, 0, LV_OPA_70);

    struct Particle {
        int16_t dx;
        int16_t dy;
    };
    constexpr Particle PARTICLES[] = {
        {-142, -75}, {-119, 65}, {-88, -92}, {-64, 87},
        {66, -88}, {91, 91}, {120, -61}, {142, 69},
        {-150, 3}, {-105, -22}, {108, 19}, {151, -4},
    };
    const uint32_t burst = easeOut(std::min<uint32_t>(
        1000U, (elapsed * 1000U) / 720U));
    for (uint8_t i = 0; i < sizeof(PARTICLES) / sizeof(PARTICLES[0]); ++i) {
        if (((elapsed / 110U) + i) % 4U == 0U) {
            continue;
        }
        const int16_t x = static_cast<int16_t>(
            centerX + (PARTICLES[i].dx * static_cast<int32_t>(burst)) / 1000);
        const int16_t y = static_cast<int16_t>(
            centerY + (PARTICLES[i].dy * static_cast<int32_t>(burst)) / 1000);
        const lv_color_t color = i % 4U == 0U ? lv_color_hex(0xF4C542)
                               : i % 4U == 1U ? lv_color_hex(0x22D3EE)
                               : i % 4U == 2U ? lv_color_hex(0xFB7185)
                                              : lv_color_hex(0xA78BFA);
        const lv_area_t particle = {
            static_cast<lv_coord_t>(x - 2),
            static_cast<lv_coord_t>(y - 4),
            static_cast<lv_coord_t>(x + 2),
            static_cast<lv_coord_t>(y + 4),
        };
        drawRect(layer, particle, color, 1, LV_OPA_90);
    }

    const int16_t halfWidth = static_cast<int16_t>(72 + (48 * reveal) / 1000U);
    const int16_t halfHeight = static_cast<int16_t>(38 + (30 * reveal) / 1000U);
    const uint32_t pulsePhase = elapsed % 480U;
    const int16_t pulse = static_cast<int16_t>(
        (pulsePhase <= 240U ? pulsePhase : 480U - pulsePhase) / 80U);
    lv_area_t halo = {
        static_cast<lv_coord_t>(centerX - halfWidth - pulse),
        static_cast<lv_coord_t>(centerY - halfHeight - pulse),
        static_cast<lv_coord_t>(centerX + halfWidth + pulse),
        static_cast<lv_coord_t>(centerY + halfHeight + pulse),
    };
    drawRect(layer, halo, lv_color_hex(0xF4C542), 14, LV_OPA_30);
    lv_area_t panel = {
        static_cast<lv_coord_t>(centerX - halfWidth),
        static_cast<lv_coord_t>(centerY - halfHeight),
        static_cast<lv_coord_t>(centerX + halfWidth),
        static_cast<lv_coord_t>(centerY + halfHeight),
    };
    drawRect(layer, panel, lv_color_hex(0xF4C542), 12);
    panel.x1 += 2;
    panel.y1 += 2;
    panel.x2 -= 2;
    panel.y2 -= 2;
    drawRect(layer, panel, lv_color_hex(0x171B26), 10, LV_OPA_COVER);

    if (revealProgress < 650U) {
        return;
    }
    lv_area_t line = {static_cast<lv_coord_t>(centerX - 46),
                      static_cast<lv_coord_t>(surface.y1 + 104),
                      static_cast<lv_coord_t>(centerX + 46),
                      static_cast<lv_coord_t>(surface.y1 + 106)};
    drawRect(layer, line, lv_color_hex(0xF4C542), 1);
    lv_area_t textArea = {
        static_cast<lv_coord_t>(surface.x1 + 60),
        static_cast<lv_coord_t>(surface.y1 + 48),
        static_cast<lv_coord_t>(surface.x2 - 60),
        static_cast<lv_coord_t>(surface.y1 + 65),
    };
    drawTextSingleLine(layer, "TILE UNLOCKED", textArea,
                       lv_color_hex(0xF4C542), hudFont_);
    textArea.y1 = surface.y1 + 66;
    textArea.y2 = surface.y1 + 101;
    drawTextSingleLine(layer, "2048", textArea, textColor_, titleFont_);
    textArea.y1 = surface.y1 + 109;
    textArea.y2 = surface.y1 + 135;
    drawTextSingleLine(layer, "YOU MADE IT", textArea, textColor_,
                       overlayFont_);
    textArea.y1 = surface.y1 + 140;
    textArea.y2 = surface.y1 + 157;
    drawTextSingleLine(layer, "KEEP BUILDING", textArea, mutedColor_,
                       tinyFont_);
}

void Game2048App::drawOverlay(lv_layer_t* layer, const lv_area_t& surface,
                              const char* title, const char* subtitle) const {
    lv_area_t overlay = surface;
    overlay.x1 += 30;
    overlay.x2 -= 30;
    overlay.y1 += 75;
    overlay.y2 = overlay.y1 + 68;
    drawRect(layer, overlay, backgroundColor_, 12, LV_OPA_90);
    lv_area_t titleArea = overlay;
    titleArea.y1 += 8;
    titleArea.y2 = titleArea.y1 + 27;
    drawText(layer, title, titleArea, textColor_, overlayFont_);
    lv_area_t subtitleArea = overlay;
    subtitleArea.y1 = overlay.y2 - 24;
    subtitleArea.y2 = overlay.y2 - 6;
    drawText(layer, subtitle, subtitleArea, mutedColor_, hudFont_);
}

void Game2048App::startGame(uint32_t nowMs) {
    randomSeed_ ^= nowMs + 0x9E3779B9U;
    engine_.reset(randomSeed_);
    engine_.start();
    phase_ = Phase::Running;
    gameRecorded_ = false;
    activeMove_ = {};
    animationUntilMs_ = 0;
    activeAnimationMs_ = MOVE_ANIMATION_MS;
    bannerStartMs_ = 0;
    bannerUntilMs_ = 0;
    nextAiMoveMs_ = 0;
    pendingGameOver_ = false;
    invalidate();
}

void Game2048App::applyMove(pgos::Game2048Direction direction,
                            AppContext& context, uint32_t nowMs,
                            uint32_t animationMs) {
    if (phase_ != Phase::Running || animationUntilMs_ != 0 ||
        bannerUntilMs_ != 0) {
        return;
    }
    const pgos::Game2048MoveResult result = engine_.move(direction);
    if (!result.moved) {
        if (result.gameOver) {
            finishGame(context);
        }
        return;
    }
    activeMove_ = result;
    animationStartMs_ = nowMs;
    activeAnimationMs_ = std::max<uint32_t>(1U, animationMs);
    animationUntilMs_ = nowMs + activeAnimationMs_;
    bestScore_ = std::max(bestScore_, engine_.score());
    if (result.reachedTarget) {
        context.audio.playGameTone(1120U, 150U);
        context.rgb.flashFeedback(237, 194, 46, 620);
    } else if (result.scoreDelta != 0) {
        const uint16_t frequency = static_cast<uint16_t>(
            480U + std::min<uint32_t>(420U, result.scoreDelta * 2U));
        context.audio.playGameTone(frequency, 54U);
    } else {
        context.audio.playGameTone(360U, 22U);
    }
    if (context.gamepad.snapshot().connected) {
        context.gamepad.requestRumble(result.scoreDelta == 0 ? 70U : 105U,
                                      result.scoreDelta == 0 ? 120U : 170U,
                                      result.reachedTarget ? 255U : 150U);
    }
    if (result.reachedTarget) {
        bannerStartMs_ = nowMs;
        bannerUntilMs_ = nowMs + TARGET_CELEBRATION_MS;
    }
    if (result.gameOver) {
        pendingGameOver_ = true;
    }
    invalidate();
}

void Game2048App::finishGame(AppContext& context) {
    if (phase_ == Phase::GameOver && gameRecorded_) {
        return;
    }
    phase_ = Phase::GameOver;
    const GamepadSnapshot& gamepad = context.gamepad.snapshot();
    aiHoldPolicy_.reset(gamepad.connected &&
                        (gamepad.buttons & GamepadButtonY) != 0);
    aiActive_ = false;
    nextAiMoveMs_ = 0;
    if (!gameRecorded_) {
        context.game2048Profile.recordGame(engine_.score(), engine_.bestTile(),
                                           engine_.hasReachedTarget());
        gameRecorded_ = true;
    }
    context.audio.playFeedback();
    context.rgb.flashFeedback(210, 48, 70, 420);
    if (context.gamepad.snapshot().connected) {
        context.gamepad.requestRumble(300, 210, 230);
    }
    invalidate();
}

void Game2048App::sampleAi(const GamepadSnapshot& gamepad, uint32_t nowMs,
                           AppContext& context) {
    const bool yHeld = gamepad.connected &&
                       (gamepad.buttons & GamepadButtonY) != 0;
    const bool wasActive = aiActive_;
    aiActive_ = aiHoldPolicy_.sample(yHeld, nowMs,
                                     phase_ == Phase::Running);
    if (aiActive_ != wasActive) {
        inputPolicy_.reset();
        nextAiMoveMs_ = aiActive_ ? nowMs : 0;
        invalidate();
    }
    if (!aiActive_ || animationUntilMs_ != 0 || bannerUntilMs_ != 0 ||
        static_cast<int32_t>(nowMs - nextAiMoveMs_) < 0) {
        return;
    }

    const pgos::Game2048AiDecision decision = ai_.chooseMove(engine_.board());
    const uint32_t decisionMs = millis();
    const uint32_t activeDurationMs =
        aiHoldPolicy_.activeDurationMs(decisionMs);
    nextAiMoveMs_ = decisionMs +
        pgos::Game2048AiPacing::moveIntervalMs(activeDurationMs);
    if (decision.valid) {
        applyMove(decision.direction, context, decisionMs,
                  pgos::Game2048AiPacing::animationMs(activeDurationMs));
    } else if (engine_.isGameOver()) {
        finishGame(context);
    }
}

void Game2048App::sampleAnalog(const GamepadSnapshot& gamepad, uint32_t nowMs,
                               AppContext& context) {
    if (!gamepad.connected || phase_ != Phase::Running) {
        inputPolicy_.reset();
        return;
    }
    const pgos::Game2048AnalogDecision decision = inputPolicy_.sample(
        gamepad.axisX, gamepad.axisY, nowMs, animationUntilMs_ == 0);
    if (decision.triggered) {
        applyMove(decision.direction, context, nowMs);
    }
}

void Game2048App::invalidate() {
    if (surface_ != nullptr) {
        lv_obj_invalidate(surface_);
    }
}

const lv_font_t* Game2048App::tileNumberFont(
    const char* text, int16_t availableSize) const {
    if (text == nullptr || availableSize <= 0) {
        return nullptr;
    }
    const lv_font_t* candidates[] = {
        tileFont_, tileMediumFont_, tileSmallFont_, tileCompactFont_, tinyFont_,
    };
    for (const lv_font_t* font : candidates) {
        if (font == nullptr) {
            continue;
        }
        lv_point_t size{};
        lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX,
                         LV_TEXT_FLAG_EXPAND);
        if (size.x <= availableSize && size.y <= availableSize) {
            return font;
        }
    }
    return nullptr;
}

lv_color_t Game2048App::tileColor(uint32_t value) {
    switch (value) {
        case 0: return lv_color_hex(0x334155);
        case 2: return lv_color_hex(0xEEE8DE);
        case 4: return lv_color_hex(0xEDE0C8);
        case 8: return lv_color_hex(0xF2B179);
        case 16: return lv_color_hex(0xF59563);
        case 32: return lv_color_hex(0xF67C5F);
        case 64: return lv_color_hex(0xF65E3B);
        case 128: return lv_color_hex(0xEDCF72);
        case 256: return lv_color_hex(0xEDCC61);
        case 512: return lv_color_hex(0xEDC850);
        case 1024: return lv_color_hex(0xEDC53F);
        case 2048: return lv_color_hex(0xEDC22E);
        default: return lv_color_hex(0xA855F7);
    }
}

lv_color_t Game2048App::tileTextColor(uint32_t value) {
    return value <= 4 ? lv_color_hex(0x334155) : lv_color_hex(0xFFFFFF);
}

uint32_t Game2048App::easeOut(uint32_t progress) {
    progress = std::min<uint32_t>(1000U, progress);
    return progress * (2000U - progress) / 1000U;
}
