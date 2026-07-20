#include "apps/BreakoutApp.h"

#include "services/AudioService.h"
#include "services/BleGamepadService.h"
#include "services/BreakoutScoreService.h"
#include "ui/UiRuntime.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int16_t GAME_SURFACE_WIDTH = 320;
constexpr int16_t GAME_SURFACE_HEIGHT = 218;
constexpr int16_t HUD_HEIGHT = 25;

float clampFloat(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(value, maximum));
}

}  // namespace

AppId BreakoutApp::id() const {
    return AppId::Breakout;
}

const char* BreakoutApp::name() const {
    return "Breakout";
}

void BreakoutApp::onEnter(AppContext& context) {
    root_ = nullptr;
    surface_ = nullptr;
    loadLeaderboard(context);
    resetGame();
}

void BreakoutApp::onExit(AppContext& context) {
    saveHighScore(context);
    root_ = nullptr;
    surface_ = nullptr;
    hudFont_ = nullptr;
    overlayFont_ = nullptr;
}

void BreakoutApp::onCommand(const AppCommand& command, AppContext& context) {
    bool changed = false;
    const bool paddleActive = phase_ == Phase::Title ||
                              phase_ == Phase::Running ||
                              phase_ == Phase::Serve;
    switch (command.type) {
        case AppCommandType::Left:
            changed = paddleActive && movePaddle(-14.0F);
            break;
        case AppCommandType::Right:
            changed = paddleActive && movePaddle(14.0F);
            break;
        case AppCommandType::Activate:
            if (phase_ == Phase::Title || phase_ == Phase::GameOver ||
                phase_ == Phase::Victory) {
                startGame(millis());
            } else if (phase_ == Phase::Running) {
                phase_ = Phase::Paused;
                physicsAccumulatorMs_ = 0;
                invalidate();
            } else if (phase_ == Phase::Paused) {
                phase_ = Phase::Running;
                lastTickMs_ = millis();
                physicsAccumulatorMs_ = 0;
                invalidate();
            } else if (phase_ == Phase::Serve) {
                serveBall(millis());
            }
            break;
        default:
            (void)context;
            break;
    }

    if (changed) {
        if (phase_ == Phase::Title || phase_ == Phase::Serve) {
            resetBall();
        }
        invalidate();
    }
}

void BreakoutApp::onTick(uint32_t nowMs, AppContext& context) {
    if (surface_ == nullptr) {
        return;
    }

    const uint32_t elapsedMs = lastTickMs_ == 0
                                   ? 0
                                   : std::min<uint32_t>(nowMs - lastTickMs_,
                                                        MAX_FRAME_MS);
    lastTickMs_ = nowMs;

    if (phase_ == Phase::LevelClear &&
        static_cast<int32_t>(nowMs - phaseUntilMs_) >= 0) {
        advanceLevel(nowMs);
    }

    samplePaddle(context.gamepad.snapshot());
    bool changed = false;
    if (elapsedMs != 0 && paddleDirection_ != 0.0F &&
        (phase_ == Phase::Running || phase_ == Phase::Serve ||
         phase_ == Phase::Title)) {
        changed = movePaddle(paddleDirection_ * PADDLE_SPEED *
                             (static_cast<float>(elapsedMs) / 1000.0F));
        if (changed && (phase_ == Phase::Title || phase_ == Phase::Serve)) {
            resetBall();
        }
    }

    if (phase_ == Phase::Running) {
        physicsAccumulatorMs_ = std::min<uint32_t>(
            physicsAccumulatorMs_ + elapsedMs,
            PHYSICS_STEP_MS * 6U);
        while (physicsAccumulatorMs_ >= PHYSICS_STEP_MS &&
               phase_ == Phase::Running) {
            stepPhysics(static_cast<float>(PHYSICS_STEP_MS) / 1000.0F,
                        nowMs, context);
            physicsAccumulatorMs_ -= PHYSICS_STEP_MS;
            changed = true;
        }
    } else {
        physicsAccumulatorMs_ = 0;
    }

    changed = updateParticles(nowMs,
                              static_cast<float>(elapsedMs) / 1000.0F) ||
              changed;
    if (changed || phase_ == Phase::LevelClear) {
        invalidate();
    }
}

lv_obj_t* BreakoutApp::onCreateView(AppContext& context) {
    root_ = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT);
    lv_obj_set_pos(root_, 0, 22);
    lv_obj_set_style_bg_color(root_, context.ui.background(), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    surface_ = lv_obj_create(root_);
    lv_obj_remove_style_all(surface_);
    lv_obj_set_size(surface_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT);
    lv_obj_set_pos(surface_, 0, 0);
    lv_obj_set_style_bg_color(surface_, context.ui.background(), 0);
    lv_obj_set_style_bg_opa(surface_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(surface_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(surface_, drawEvent, LV_EVENT_DRAW_MAIN, this);

    hudFont_ = context.ui.font(12);
    overlayFont_ = context.ui.font(20);
    backgroundColor_ = context.ui.background();
    panelColor_ = context.ui.panel();
    gridColor_ = context.ui.panelRaised();
    textColor_ = context.ui.text();
    mutedColor_ = context.ui.muted();
    accentColor_ = context.ui.accent();
    brickColors_[0] = lv_color_hex(0xFF5C62);
    brickColors_[1] = lv_color_hex(0xFF9F43);
    brickColors_[2] = lv_color_hex(0xF7D154);
    brickColors_[3] = lv_color_hex(0x48C774);
    brickColors_[4] = lv_color_hex(0x38BDF8);
    brickColors_[5] = lv_color_hex(0xB983FF);
    lastTickMs_ = millis();
    invalidate();
    return root_;
}

void BreakoutApp::onUpdateView(AppContext&) {
    invalidate();
}

void BreakoutApp::drawEvent(lv_event_t* event) {
    auto* app = static_cast<BreakoutApp*>(lv_event_get_user_data(event));
    if (app != nullptr && lv_event_get_target_obj(event) == app->surface_) {
        app->draw(event);
    }
}

void BreakoutApp::draw(lv_event_t* event) {
    lv_obj_t* object = lv_event_get_target_obj(event);
    lv_layer_t* layer = lv_event_get_layer(event);
    if (object == nullptr || layer == nullptr) {
        return;
    }

    lv_area_t surfaceArea;
    lv_obj_get_coords(object, &surfaceArea);
    drawRect(layer, surfaceArea, backgroundColor_);

    lv_area_t hud = surfaceArea;
    hud.y2 = hud.y1 + HUD_HEIGHT - 1;
    drawRect(layer, hud, panelColor_);
    char text[64];
    lv_snprintf(text, sizeof(text),
                "BREAKOUT  %05u  BEST %05u  L%u  LIFE %u",
                static_cast<unsigned>(score_),
                static_cast<unsigned>(bestScore_),
                static_cast<unsigned>(level_),
                static_cast<unsigned>(lives_));
    lv_area_t hudText = hud;
    hudText.x1 += 8;
    hudText.x2 -= 8;
    drawText(layer, text, hudText, textColor_, hudFont_, LV_TEXT_ALIGN_LEFT);

    lv_area_t playfield = {
        static_cast<lv_coord_t>(surfaceArea.x1 + PLAY_X),
        static_cast<lv_coord_t>(surfaceArea.y1 + PLAY_Y),
        static_cast<lv_coord_t>(surfaceArea.x1 + PLAY_X + PLAY_WIDTH - 1),
        static_cast<lv_coord_t>(surfaceArea.y1 + PLAY_Y + PLAY_HEIGHT - 1),
    };
    drawRect(layer, playfield, lv_color_hex(0x09111D), 5);
    lv_area_t topRule = playfield;
    topRule.y2 = topRule.y1;
    drawRect(layer, topRule, gridColor_);

    for (uint8_t row = 0; row < BRICK_ROWS; ++row) {
        for (uint8_t column = 0; column < BRICK_COLUMNS; ++column) {
            const uint8_t durability = bricks_[row][column];
            if (durability == 0) {
                continue;
            }
            const int16_t x = surfaceArea.x1 + BRICK_X +
                              column * (BRICK_WIDTH + BRICK_GAP_X);
            const int16_t y = surfaceArea.y1 + BRICK_Y +
                              row * (BRICK_HEIGHT + BRICK_GAP_Y);
            lv_area_t shadow = {
                static_cast<lv_coord_t>(x + 1),
                static_cast<lv_coord_t>(y + 2),
                static_cast<lv_coord_t>(x + BRICK_WIDTH),
                static_cast<lv_coord_t>(y + BRICK_HEIGHT + 1),
            };
            drawRect(layer, shadow, backgroundColor_, 2, LV_OPA_70);
            lv_area_t brick = {
                static_cast<lv_coord_t>(x),
                static_cast<lv_coord_t>(y),
                static_cast<lv_coord_t>(x + BRICK_WIDTH - 1),
                static_cast<lv_coord_t>(y + BRICK_HEIGHT - 1),
            };
            drawRect(layer, brick, brickColors_[row], 2,
                     durability > 1 ? LV_OPA_COVER : LV_OPA_80);
            brick.x1 += 2;
            brick.x2 -= 2;
            brick.y1 += 2;
            brick.y2 = brick.y1 + 1;
            drawRect(layer, brick, lv_color_hex(0xFFFFFF), 1,
                     durability > 1 ? LV_OPA_50 : LV_OPA_20);
        }
    }

    const uint32_t nowMs = millis();
    for (const Particle& particle : particles_) {
        if (!particle.active) {
            continue;
        }
        const uint32_t ageMs = nowMs - particle.bornMs;
        if (ageMs >= PARTICLE_LIFETIME_MS) {
            continue;
        }
        const lv_opa_t opacity = static_cast<lv_opa_t>(
            ((PARTICLE_LIFETIME_MS - ageMs) * LV_OPA_COVER) /
            PARTICLE_LIFETIME_MS);
        const int16_t x = surfaceArea.x1 +
                          static_cast<int16_t>(std::lround(particle.x));
        const int16_t y = surfaceArea.y1 +
                          static_cast<int16_t>(std::lround(particle.y));
        lv_area_t fragment = {
            static_cast<lv_coord_t>(x - 1),
            static_cast<lv_coord_t>(y - 1),
            static_cast<lv_coord_t>(x + 2),
            static_cast<lv_coord_t>(y + 1),
        };
        drawRect(layer, fragment, particle.color, 1, opacity);
    }

    const int16_t paddleX = surfaceArea.x1 +
                            static_cast<int16_t>(std::lround(paddleX_));
    const int16_t paddleWidth = static_cast<int16_t>(std::lround(paddleWidth_));
    lv_area_t paddleShadow = {
        static_cast<lv_coord_t>(paddleX + 1),
        static_cast<lv_coord_t>(surfaceArea.y1 + PADDLE_Y + 2),
        static_cast<lv_coord_t>(paddleX + paddleWidth),
        static_cast<lv_coord_t>(surfaceArea.y1 + PADDLE_Y + PADDLE_HEIGHT + 1),
    };
    drawRect(layer, paddleShadow, backgroundColor_, 4, LV_OPA_80);
    lv_area_t paddle = {
        static_cast<lv_coord_t>(paddleX),
        static_cast<lv_coord_t>(surfaceArea.y1 + PADDLE_Y),
        static_cast<lv_coord_t>(paddleX + paddleWidth - 1),
        static_cast<lv_coord_t>(surfaceArea.y1 + PADDLE_Y + PADDLE_HEIGHT - 1),
    };
    drawRect(layer, paddle, accentColor_, 4);
    lv_area_t paddleCore = paddle;
    paddleCore.x1 += paddleWidth / 3;
    paddleCore.x2 -= paddleWidth / 3;
    paddleCore.y1 += 1;
    paddleCore.y2 -= 1;
    drawRect(layer, paddleCore, lv_color_hex(0xFFFFFF), 3, LV_OPA_50);

    const int16_t ballX = surfaceArea.x1 +
                          static_cast<int16_t>(std::lround(ballX_));
    const int16_t ballY = surfaceArea.y1 +
                          static_cast<int16_t>(std::lround(ballY_));
    const int16_t radius = static_cast<int16_t>(std::ceil(BALL_RADIUS));
    lv_area_t ball = {
        static_cast<lv_coord_t>(ballX - radius),
        static_cast<lv_coord_t>(ballY - radius),
        static_cast<lv_coord_t>(ballX + radius),
        static_cast<lv_coord_t>(ballY + radius),
    };
    drawRect(layer, ball, lv_color_hex(0xF8FAFC), LV_RADIUS_CIRCLE);
    lv_area_t highlight = {
        static_cast<lv_coord_t>(ballX - 2),
        static_cast<lv_coord_t>(ballY - 2),
        static_cast<lv_coord_t>(ballX),
        static_cast<lv_coord_t>(ballY),
    };
    drawRect(layer, highlight, lv_color_hex(0xFFFFFF), LV_RADIUS_CIRCLE);

    if (phase_ == Phase::Running) {
        if (combo_ > 1) {
            lv_area_t comboArea = playfield;
            comboArea.x1 = comboArea.x2 - 80;
            comboArea.y1 = playfield.y2 - 22;
            comboArea.y2 = playfield.y2 - 6;
            lv_snprintf(text, sizeof(text), "CHAIN x%u",
                        static_cast<unsigned>(combo_));
            drawText(layer, text, comboArea, mutedColor_, hudFont_,
                     LV_TEXT_ALIGN_RIGHT);
        }
        return;
    }

    lv_area_t overlay = playfield;
    overlay.x1 += 27;
    overlay.x2 -= 27;
    overlay.y1 += 42;
    overlay.y2 = overlay.y1 + 78;
    if (phase_ == Phase::Serve) {
        overlay.y1 += 30;
        overlay.y2 = overlay.y1 + 56;
    }
    drawRect(layer, overlay, backgroundColor_, 8, LV_OPA_90);

    const char* title = "BREAKOUT";
    const char* subtitle = "A START   LEFT / RIGHT";
    switch (phase_) {
        case Phase::Paused:
            title = "PAUSED";
            subtitle = "A RESUME   B EXIT";
            break;
        case Phase::Serve:
            title = "BALL READY";
            subtitle = "A SERVE   LEFT / RIGHT";
            break;
        case Phase::LevelClear:
            title = "LEVEL CLEAR";
            subtitle = "GET READY FOR NEXT ROUND";
            break;
        case Phase::GameOver:
            title = "GAME OVER";
            subtitle = "A RESTART   B EXIT";
            break;
        case Phase::Victory:
            title = "ALL CLEAR";
            subtitle = "A RESTART   B EXIT";
            break;
        default:
            break;
    }

    lv_area_t titleArea = overlay;
    titleArea.y1 += phase_ == Phase::Serve ? 7 : 11;
    titleArea.y2 = titleArea.y1 + 25;
    drawText(layer, title, titleArea,
             phase_ == Phase::Victory ? accentColor_ : textColor_,
             overlayFont_);
    lv_area_t subtitleArea = overlay;
    subtitleArea.y1 = overlay.y2 - 25;
    subtitleArea.y2 = overlay.y2 - 7;
    drawText(layer, subtitle, subtitleArea, mutedColor_, hudFont_);

    if (phase_ == Phase::Title || phase_ == Phase::GameOver ||
        phase_ == Phase::Victory) {
        lv_area_t scores = playfield;
        scores.x1 += 18;
        scores.x2 -= 18;
        scores.y1 = overlay.y2 + 11;
        scores.y2 = scores.y1 + 18;
        lv_snprintf(text, sizeof(text), "TOP  %u  %u  %u  %u  %u",
                    static_cast<unsigned>(leaderboard_[0]),
                    static_cast<unsigned>(leaderboard_[1]),
                    static_cast<unsigned>(leaderboard_[2]),
                    static_cast<unsigned>(leaderboard_[3]),
                    static_cast<unsigned>(leaderboard_[4]));
        drawText(layer, text, scores, mutedColor_, hudFont_);
    }
}

void BreakoutApp::drawRect(lv_layer_t* layer, const lv_area_t& area,
                           lv_color_t color, int32_t radius,
                           lv_opa_t opacity) const {
    lv_draw_rect_dsc_t descriptor;
    lv_draw_rect_dsc_init(&descriptor);
    descriptor.bg_color = color;
    descriptor.bg_opa = opacity;
    descriptor.radius = radius;
    lv_draw_rect(layer, &descriptor, &area);
}

void BreakoutApp::drawText(lv_layer_t* layer, const char* text, lv_area_t area,
                           lv_color_t color, const lv_font_t* font,
                           lv_text_align_t align) const {
    if (text == nullptr || font == nullptr) {
        return;
    }
    lv_draw_label_dsc_t descriptor;
    lv_draw_label_dsc_init(&descriptor);
    descriptor.color = color;
    descriptor.font = font;
    descriptor.text = text;
    descriptor.text_local = true;
    descriptor.align = align;
    lv_draw_label(layer, &descriptor, &area);
}

void BreakoutApp::resetGame() {
    randomState_ ^= millis() + 0x9E3779B9U;
    if (randomState_ == 0) {
        randomState_ = 0xB4EA4C01U;
    }
    level_ = 1;
    lives_ = 3;
    combo_ = 0;
    score_ = 0;
    scoreSaved_ = false;
    paddleWidth_ = 52.0F;
    paddleX_ = (GAME_SURFACE_WIDTH - paddleWidth_) * 0.5F;
    paddleDirection_ = 0.0F;
    lastTickMs_ = millis();
    physicsAccumulatorMs_ = 0;
    phaseUntilMs_ = 0;
    for (Particle& particle : particles_) {
        particle.active = false;
    }
    nextParticle_ = 0;
    buildLevel();
    resetBall();
    phase_ = Phase::Title;
    invalidate();
}

void BreakoutApp::startGame(uint32_t nowMs) {
    resetGame();
    serveBall(nowMs);
}

void BreakoutApp::buildLevel() {
    remainingBricks_ = 0;
    paddleWidth_ = 52.0F - static_cast<float>(level_ - 1U) * 4.0F;
    paddleX_ = clampFloat(paddleX_, static_cast<float>(PLAY_X + 3),
                          static_cast<float>(PLAY_X + PLAY_WIDTH - 3) -
                              paddleWidth_);
    for (uint8_t row = 0; row < BRICK_ROWS; ++row) {
        for (uint8_t column = 0; column < BRICK_COLUMNS; ++column) {
            uint8_t durability = 1;
            if (level_ == 2) {
                const bool sideGap = row >= 2 && row <= 3 &&
                                     (column == 0 ||
                                      column == BRICK_COLUMNS - 1U);
                durability = sideGap ? 0 : (row < 2 ? 2 : 1);
            } else if (level_ == 3) {
                const uint8_t edgeDistance = std::min<uint8_t>(
                    column, BRICK_COLUMNS - 1U - column);
                const bool active = row < 4 || edgeDistance >= row - 3U;
                durability = active &&
                                     (row < 2 || (row + column) % 4U == 0)
                                 ? 2
                                 : (active ? 1 : 0);
            }
            bricks_[row][column] = durability;
            if (durability != 0) {
                ++remainingBricks_;
            }
        }
    }
}

void BreakoutApp::advanceLevel(uint32_t nowMs) {
    if (level_ >= LEVEL_COUNT) {
        return;
    }
    ++level_;
    combo_ = 0;
    buildLevel();
    resetBall();
    phase_ = Phase::Serve;
    lastTickMs_ = nowMs;
    phaseUntilMs_ = 0;
    invalidate();
}

void BreakoutApp::serveBall(uint32_t nowMs) {
    resetBall();
    const float speed = BASE_BALL_SPEED + static_cast<float>(level_ - 1U) * 12.0F;
    const float horizontal = 0.32F +
                             static_cast<float>(nextRandom() % 18U) / 100.0F;
    ballVx_ = ((nextRandom() & 1U) == 0U ? -1.0F : 1.0F) *
              speed * horizontal;
    ballVy_ = -std::sqrt(std::max(1.0F, speed * speed - ballVx_ * ballVx_));
    phase_ = Phase::Running;
    lastTickMs_ = nowMs;
    physicsAccumulatorMs_ = 0;
    invalidate();
}

void BreakoutApp::resetBall() {
    ballX_ = paddleX_ + paddleWidth_ * 0.5F;
    ballY_ = static_cast<float>(PADDLE_Y) - BALL_RADIUS - 2.0F;
    ballVx_ = 0.0F;
    ballVy_ = 0.0F;
}

void BreakoutApp::samplePaddle(const GamepadSnapshot& gamepad) {
    paddleDirection_ = 0.0F;
    if (!gamepad.connected) {
        return;
    }
    if ((gamepad.dpad & GamepadDpadLeft) != 0) {
        paddleDirection_ = -1.0F;
    } else if ((gamepad.dpad & GamepadDpadRight) != 0) {
        paddleDirection_ = 1.0F;
    } else if (std::abs(static_cast<int32_t>(gamepad.axisX)) >=
               ANALOG_THRESHOLD) {
        paddleDirection_ = clampFloat(
            static_cast<float>(gamepad.axisX) / 512.0F, -1.0F, 1.0F);
    }
}

bool BreakoutApp::movePaddle(float distance) {
    const float minimum = static_cast<float>(PLAY_X + 3);
    const float maximum = static_cast<float>(PLAY_X + PLAY_WIDTH - 3) -
                          paddleWidth_;
    const float next = clampFloat(paddleX_ + distance, minimum, maximum);
    if (std::abs(next - paddleX_) < 0.01F) {
        return false;
    }
    paddleX_ = next;
    return true;
}

void BreakoutApp::stepPhysics(float deltaSeconds, uint32_t nowMs,
                              AppContext& context) {
    const float previousX = ballX_;
    const float previousY = ballY_;
    ballX_ += ballVx_ * deltaSeconds;
    ballY_ += ballVy_ * deltaSeconds;

    const float left = static_cast<float>(PLAY_X) + BALL_RADIUS;
    const float right = static_cast<float>(PLAY_X + PLAY_WIDTH - 1) -
                        BALL_RADIUS;
    const float top = static_cast<float>(PLAY_Y) + BALL_RADIUS;
    if (ballX_ < left) {
        ballX_ = left;
        ballVx_ = std::abs(ballVx_);
    } else if (ballX_ > right) {
        ballX_ = right;
        ballVx_ = -std::abs(ballVx_);
    }
    if (ballY_ < top) {
        ballY_ = top;
        ballVy_ = std::abs(ballVy_);
    }

    if (ballVy_ > 0.0F && previousY + BALL_RADIUS <= PADDLE_Y &&
        ballY_ + BALL_RADIUS >= PADDLE_Y &&
        ballX_ + BALL_RADIUS >= paddleX_ &&
        ballX_ - BALL_RADIUS <= paddleX_ + paddleWidth_) {
        ballY_ = static_cast<float>(PADDLE_Y) - BALL_RADIUS;
        bounceFromPaddle(context);
    }

    if (hitBrick(previousX, previousY, nowMs, context) &&
        phase_ != Phase::Running) {
        return;
    }

    if (ballY_ - BALL_RADIUS > PLAY_Y + PLAY_HEIGHT) {
        loseLife(context);
    }
}

bool BreakoutApp::hitBrick(float previousX, float previousY, uint32_t nowMs,
                           AppContext& context) {
    for (uint8_t row = 0; row < BRICK_ROWS; ++row) {
        for (uint8_t column = 0; column < BRICK_COLUMNS; ++column) {
            if (bricks_[row][column] == 0) {
                continue;
            }
            const float x1 = static_cast<float>(
                BRICK_X + column * (BRICK_WIDTH + BRICK_GAP_X));
            const float y1 = static_cast<float>(
                BRICK_Y + row * (BRICK_HEIGHT + BRICK_GAP_Y));
            const float x2 = x1 + BRICK_WIDTH - 1.0F;
            const float y2 = y1 + BRICK_HEIGHT - 1.0F;
            const float closestX = clampFloat(ballX_, x1, x2);
            const float closestY = clampFloat(ballY_, y1, y2);
            const float dx = ballX_ - closestX;
            const float dy = ballY_ - closestY;
            if (dx * dx + dy * dy > BALL_RADIUS * BALL_RADIUS) {
                continue;
            }

            const bool fromTop = previousY + BALL_RADIUS <= y1;
            const bool fromBottom = previousY - BALL_RADIUS >= y2;
            const bool fromLeft = previousX + BALL_RADIUS <= x1;
            const bool fromRight = previousX - BALL_RADIUS >= x2;
            if ((fromTop || fromBottom) && !(fromLeft || fromRight)) {
                ballVy_ = -ballVy_;
                ballY_ = fromTop ? y1 - BALL_RADIUS : y2 + BALL_RADIUS;
            } else if (fromLeft || fromRight) {
                ballVx_ = -ballVx_;
                ballX_ = fromLeft ? x1 - BALL_RADIUS : x2 + BALL_RADIUS;
            } else {
                const float horizontalOverlap = std::min(
                    ballX_ + BALL_RADIUS - x1,
                    x2 - (ballX_ - BALL_RADIUS));
                const float verticalOverlap = std::min(
                    ballY_ + BALL_RADIUS - y1,
                    y2 - (ballY_ - BALL_RADIUS));
                if (horizontalOverlap < verticalOverlap) {
                    ballVx_ = -ballVx_;
                    ballX_ = previousX < (x1 + x2) * 0.5F
                                 ? x1 - BALL_RADIUS
                                 : x2 + BALL_RADIUS;
                } else {
                    ballVy_ = -ballVy_;
                    ballY_ = previousY < (y1 + y2) * 0.5F
                                 ? y1 - BALL_RADIUS
                                 : y2 + BALL_RADIUS;
                }
            }

            --bricks_[row][column];
            const bool destroyed = bricks_[row][column] == 0;
            if (destroyed) {
                --remainingBricks_;
                combo_ = std::min<uint8_t>(10U, combo_ + 1U);
                spawnBrickParticles(row, column, nowMs);
            }
            const uint16_t base = static_cast<uint16_t>(
                (BRICK_ROWS - row) * 10U * level_);
            const uint16_t bonus = destroyed
                                       ? static_cast<uint16_t>(combo_ * 5U)
                                       : 5U;
            score_ = static_cast<uint16_t>(std::min<uint32_t>(
                65535U, static_cast<uint32_t>(score_) + base + bonus));
            bestScore_ = std::max(bestScore_, score_);
            speedBall(1.005F);
            context.audio.playGameTone(destroyed ? 24U : 12U);

            if (remainingBricks_ == 0) {
                finishLevel(nowMs, context);
            }
            return true;
        }
    }
    return false;
}

void BreakoutApp::bounceFromPaddle(AppContext& context) {
    combo_ = 0;
    const float incomingVx = ballVx_;
    float speed = std::sqrt(ballVx_ * ballVx_ + ballVy_ * ballVy_);
    speed = clampFloat(speed * 1.015F, BASE_BALL_SPEED, MAX_BALL_SPEED);
    const float hit = clampFloat(
        (ballX_ - (paddleX_ + paddleWidth_ * 0.5F)) /
            (paddleWidth_ * 0.5F),
        -1.0F, 1.0F);
    ballVx_ = hit * speed * 0.82F + paddleDirection_ * 12.0F;
    ballVx_ = clampFloat(ballVx_, -speed * 0.88F, speed * 0.88F);
    if (std::abs(ballVx_) < 28.0F) {
        const float direction = hit < -0.01F
                                    ? -1.0F
                                    : hit > 0.01F
                                          ? 1.0F
                                          : (incomingVx < 0.0F ? -1.0F : 1.0F);
        ballVx_ = direction * 28.0F;
    }
    ballVy_ = -std::sqrt(std::max(1.0F,
                                   speed * speed - ballVx_ * ballVx_));
    context.audio.playGameTone(18U);
}

void BreakoutApp::speedBall(float factor) {
    const float speed = std::sqrt(ballVx_ * ballVx_ + ballVy_ * ballVy_);
    if (speed < 1.0F) {
        return;
    }
    const float nextSpeed = std::min(MAX_BALL_SPEED, speed * factor);
    const float scale = nextSpeed / speed;
    ballVx_ *= scale;
    ballVy_ *= scale;
}

void BreakoutApp::loseLife(AppContext& context) {
    combo_ = 0;
    if (lives_ > 0) {
        --lives_;
    }
    context.audio.playGameTone(120U);
    if (context.gamepad.snapshot().connected) {
        context.gamepad.requestRumble(260, 180, 255);
    }
    if (lives_ == 0) {
        phase_ = Phase::GameOver;
        saveHighScore(context);
    } else {
        phase_ = Phase::Serve;
        resetBall();
    }
    physicsAccumulatorMs_ = 0;
    invalidate();
}

void BreakoutApp::finishLevel(uint32_t nowMs, AppContext& context) {
    const uint32_t bonus = 400U * level_ + 100U * lives_;
    score_ = static_cast<uint16_t>(std::min<uint32_t>(
        65535U, static_cast<uint32_t>(score_) + bonus));
    bestScore_ = std::max(bestScore_, score_);
    ballVx_ = 0.0F;
    ballVy_ = 0.0F;
    context.audio.playGameTone(220U);
    if (context.gamepad.snapshot().connected) {
        context.gamepad.requestRumble(260, 255, 220);
    }
    if (level_ >= LEVEL_COUNT) {
        phase_ = Phase::Victory;
        saveHighScore(context);
    } else {
        phase_ = Phase::LevelClear;
        phaseUntilMs_ = nowMs + LEVEL_CLEAR_MS;
    }
    physicsAccumulatorMs_ = 0;
    invalidate();
}

void BreakoutApp::spawnBrickParticles(uint8_t row, uint8_t column,
                                      uint32_t nowMs) {
    const float centerX = BRICK_X + column * (BRICK_WIDTH + BRICK_GAP_X) +
                          BRICK_WIDTH * 0.5F;
    const float centerY = BRICK_Y + row * (BRICK_HEIGHT + BRICK_GAP_Y) +
                          BRICK_HEIGHT * 0.5F;
    for (uint8_t index = 0; index < 5U; ++index) {
        Particle& particle = particles_[nextParticle_];
        nextParticle_ = static_cast<uint8_t>(
            (nextParticle_ + 1U) % PARTICLE_COUNT);
        particle.x = centerX + static_cast<int8_t>(nextRandom() % 7U) - 3.0F;
        particle.y = centerY + static_cast<int8_t>(nextRandom() % 5U) - 2.0F;
        particle.vx = static_cast<float>(
            static_cast<int16_t>(nextRandom() % 91U) - 45);
        particle.vy = -20.0F - static_cast<float>(nextRandom() % 46U);
        particle.bornMs = nowMs;
        particle.color = brickColors_[row];
        particle.active = true;
    }
}

bool BreakoutApp::updateParticles(uint32_t nowMs, float deltaSeconds) {
    bool active = false;
    for (Particle& particle : particles_) {
        if (!particle.active) {
            continue;
        }
        if (nowMs - particle.bornMs >= PARTICLE_LIFETIME_MS) {
            particle.active = false;
            continue;
        }
        particle.x += particle.vx * deltaSeconds;
        particle.y += particle.vy * deltaSeconds;
        particle.vy += 95.0F * deltaSeconds;
        active = true;
    }
    return active;
}

void BreakoutApp::saveHighScore(AppContext& context) {
    if (!scoreSaved_ && score_ != 0) {
        context.breakoutScore.recordScore(score_);
        scoreSaved_ = true;
    }
    loadLeaderboard(context);
    bestScore_ = std::max(bestScore_, score_);
}

void BreakoutApp::loadLeaderboard(AppContext& context) {
    bestScore_ = context.breakoutScore.bestScore();
    for (uint8_t index = 0; index < LEADERBOARD_COUNT; ++index) {
        leaderboard_[index] = context.breakoutScore.scoreAt(index);
    }
}

uint32_t BreakoutApp::nextRandom() {
    uint32_t value = randomState_;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    randomState_ = value == 0 ? 0xB4EA4C01U : value;
    return randomState_;
}

void BreakoutApp::invalidate() {
    if (surface_ != nullptr) {
        lv_obj_invalidate(surface_);
    }
}
