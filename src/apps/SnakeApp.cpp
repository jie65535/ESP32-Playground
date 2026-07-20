#include "apps/SnakeApp.h"

#include "services/AudioService.h"
#include "services/BleGamepadService.h"
#include "services/GameScoreService.h"
#include "ui/UiRuntime.h"

#include <algorithm>
#include <cstdlib>

namespace {

constexpr int16_t GAME_SURFACE_WIDTH = 320;
constexpr int16_t GAME_SURFACE_HEIGHT = 218;
constexpr int16_t HUD_HEIGHT = 28;

}  // namespace

AppId SnakeApp::id() const {
    return AppId::Snake;
}

const char* SnakeApp::name() const {
    return "Snake";
}

void SnakeApp::onEnter(AppContext& context) {
    root_ = nullptr;
    surface_ = nullptr;
    phase_ = Phase::Title;
    loadLeaderboard(context);
    resetGame();
}

void SnakeApp::onExit(AppContext& context) {
    saveHighScore(context);
    root_ = nullptr;
    surface_ = nullptr;
    hudFont_ = nullptr;
    overlayFont_ = nullptr;
}

void SnakeApp::onCommand(const AppCommand& command, AppContext& context) {
    switch (command.type) {
        case AppCommandType::Previous:
            queueDirection(Direction::Up);
            break;
        case AppCommandType::Next:
            queueDirection(Direction::Down);
            break;
        case AppCommandType::Left:
            queueDirection(Direction::Left);
            break;
        case AppCommandType::Right:
            queueDirection(Direction::Right);
            break;
        case AppCommandType::Activate:
            if (phase_ == Phase::Title || phase_ == Phase::GameOver) {
                startGame(millis());
            } else if (phase_ == Phase::Running) {
                phase_ = Phase::Paused;
                invalidate();
            } else if (phase_ == Phase::Paused) {
                phase_ = Phase::Running;
                nextStepMs_ = millis() + stepIntervalMs_;
                invalidate();
            }
            break;
        default:
            (void)context;
            break;
    }
}

void SnakeApp::onTick(uint32_t nowMs, AppContext& context) {
    if (surface_ == nullptr) {
        return;
    }

    sampleAnalogDirection(context.gamepad.snapshot());
    if (phase_ != Phase::Running) {
        return;
    }

    if (static_cast<int32_t>(nowMs - nextStepMs_) >= 0) {
        stepGame(nowMs, context);
    }
}

lv_obj_t* SnakeApp::onCreateView(AppContext& context) {
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
    snakeColor_ = lv_color_hex(0x48C774);
    snakeHeadColor_ = lv_color_hex(0x9BE15D);
    foodColor_ = context.ui.accent();
    textColor_ = context.ui.text();
    mutedColor_ = context.ui.muted();
    accentColor_ = context.ui.accent();
    invalidate();
    return root_;
}

void SnakeApp::onUpdateView(AppContext&) {
    invalidate();
}

void SnakeApp::drawEvent(lv_event_t* event) {
    auto* app = static_cast<SnakeApp*>(lv_event_get_user_data(event));
    if (app != nullptr && lv_event_get_target_obj(event) == app->surface_) {
        app->draw(event);
    }
}

void SnakeApp::draw(lv_event_t* event) {
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

    char hudText[48];
    lv_snprintf(hudText, sizeof(hudText), "SNAKE   SCORE %03u   BEST %03u",
                static_cast<unsigned>(score_),
                static_cast<unsigned>(bestScore_));
    lv_area_t hudTextArea = hud;
    hudTextArea.x1 += 10;
    hudTextArea.x2 -= 10;
    drawText(layer, hudText, hudTextArea, textColor_, hudFont_,
             LV_TEXT_ALIGN_LEFT);

    lv_area_t board = {
        static_cast<lv_coord_t>(surfaceArea.x1 + BOARD_X),
        static_cast<lv_coord_t>(surfaceArea.y1 + BOARD_Y),
        static_cast<lv_coord_t>(surfaceArea.x1 + BOARD_X + BOARD_WIDTH - 1),
        static_cast<lv_coord_t>(surfaceArea.y1 + BOARD_Y + BOARD_HEIGHT - 1),
    };
    drawRect(layer, board, lv_color_hex(0x0D1524), 5);

    for (uint8_t column = 0; column < GRID_WIDTH; ++column) {
        lv_area_t line = board;
        line.x1 = board.x1 + column * CELL_SIZE;
        line.x2 = line.x1;
        line.y2 = board.y2;
        drawRect(layer, line, gridColor_);
    }
    for (uint8_t row = 0; row < GRID_HEIGHT; ++row) {
        lv_area_t line = board;
        line.y1 = board.y1 + row * CELL_SIZE;
        line.y2 = line.y1;
        line.x2 = board.x2;
        drawRect(layer, line, gridColor_);
    }

    lv_area_t foodArea = {
        static_cast<lv_coord_t>(board.x1 + food_.x * CELL_SIZE + 3),
        static_cast<lv_coord_t>(board.y1 + food_.y * CELL_SIZE + 3),
        static_cast<lv_coord_t>(board.x1 + food_.x * CELL_SIZE + CELL_SIZE - 4),
        static_cast<lv_coord_t>(board.y1 + food_.y * CELL_SIZE + CELL_SIZE - 4),
    };
    drawRect(layer, foodArea, foodColor_, LV_RADIUS_CIRCLE);

    for (uint8_t index = 0; index < length_; ++index) {
        const Cell cell = body_[index];
        lv_area_t segment = {
            static_cast<lv_coord_t>(board.x1 + cell.x * CELL_SIZE + 2),
            static_cast<lv_coord_t>(board.y1 + cell.y * CELL_SIZE + 2),
            static_cast<lv_coord_t>(board.x1 + cell.x * CELL_SIZE + CELL_SIZE - 3),
            static_cast<lv_coord_t>(board.y1 + cell.y * CELL_SIZE + CELL_SIZE - 3),
        };
        drawRect(layer, segment, index == 0 ? snakeHeadColor_ : snakeColor_,
                 index == 0 ? 5 : 3);
    }

    if (phase_ != Phase::Running) {
        lv_area_t overlay = board;
        overlay.x1 += 20;
        overlay.x2 -= 20;
        overlay.y1 += phase_ == Phase::GameOver ? 20 : 60;
        overlay.y2 -= phase_ == Phase::GameOver ? 20 : 60;
        drawRect(layer, overlay, backgroundColor_, 10, LV_OPA_90);

        const char* title = "PRESS A TO START";
        const char* subtitle = "D-PAD / LEFT STICK";
        if (phase_ == Phase::Paused) {
            title = "PAUSED";
            subtitle = "PRESS A TO RESUME";
        } else if (phase_ == Phase::GameOver) {
            title = won_ ? "YOU WIN" : "GAME OVER";
            subtitle = "A RESTART   B EXIT";
        }

        lv_area_t titleArea = overlay;
        titleArea.y1 += phase_ == Phase::GameOver ? 8 : 28;
        titleArea.y2 = titleArea.y1 + 28;
        drawText(layer, title, titleArea, textColor_, overlayFont_);
        if (phase_ == Phase::GameOver) {
            lv_area_t scoreArea = overlay;
            scoreArea.y1 += 38;
            scoreArea.y2 = scoreArea.y1 + 18;
            char scoreText[24];
            lv_snprintf(scoreText, sizeof(scoreText), "SCORE %03u",
                        static_cast<unsigned>(score_));
            drawText(layer, scoreText, scoreArea, accentColor_, hudFont_);

            for (uint8_t index = 0; index < LEADERBOARD_COUNT; ++index) {
                lv_area_t rowArea = overlay;
                rowArea.x1 += 34;
                rowArea.x2 -= 34;
                rowArea.y1 += 59 + index * 14;
                rowArea.y2 = rowArea.y1 + 14;
                char rowText[24];
                lv_snprintf(rowText, sizeof(rowText), "%u. %03u",
                            static_cast<unsigned>(index + 1U),
                            static_cast<unsigned>(leaderboard_[index]));
                drawText(layer, rowText, rowArea,
                         index == 0 ? textColor_ : mutedColor_, hudFont_,
                         LV_TEXT_ALIGN_LEFT);
            }
            lv_area_t subtitleArea = overlay;
            subtitleArea.y1 = overlay.y2 - 20;
            subtitleArea.y2 = overlay.y2 - 4;
            drawText(layer, subtitle, subtitleArea, mutedColor_, hudFont_);
        } else {
            lv_area_t subtitleArea = overlay;
            subtitleArea.y1 += 68;
            subtitleArea.y2 = subtitleArea.y1 + 18;
            drawText(layer, subtitle, subtitleArea, mutedColor_, hudFont_);
        }
    }
}

void SnakeApp::drawRect(lv_layer_t* layer, const lv_area_t& area,
                        lv_color_t color, int32_t radius, lv_opa_t opacity) const {
    lv_draw_rect_dsc_t descriptor;
    lv_draw_rect_dsc_init(&descriptor);
    descriptor.bg_color = color;
    descriptor.bg_opa = opacity;
    descriptor.radius = radius;
    lv_draw_rect(layer, &descriptor, &area);
}

void SnakeApp::drawText(lv_layer_t* layer, const char* text, lv_area_t area,
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

void SnakeApp::resetGame() {
    randomState_ ^= millis() + 0x9E3779B9U;
    if (randomState_ == 0) {
        randomState_ = 0x51A7C0DEU;
    }

    phase_ = Phase::Title;
    direction_ = Direction::Right;
    queuedDirection_ = Direction::Right;
    length_ = 4;
    score_ = 0;
    stepIntervalMs_ = INITIAL_STEP_MS;
    nextStepMs_ = 0;
    won_ = false;
    turnAcceptedForStep_ = false;
    body_[0] = {10, 6};
    body_[1] = {9, 6};
    body_[2] = {8, 6};
    body_[3] = {7, 6};
    for (uint8_t index = 4; index < MAX_LENGTH; ++index) {
        body_[index] = {};
    }
    spawnFood();
    invalidate();
}

void SnakeApp::startGame(uint32_t nowMs) {
    resetGame();
    phase_ = Phase::Running;
    nextStepMs_ = nowMs + stepIntervalMs_;
    invalidate();
}

void SnakeApp::stepGame(uint32_t nowMs, AppContext& context) {
    direction_ = queuedDirection_;
    // A snake cell may accept at most one turn.  This prevents a single
    // analog stick flick (which can briefly report several dominant axes)
    // from replacing the queued turn multiple times before the next move.
    turnAcceptedForStep_ = false;
    const Cell head = nextHead();
    const bool ateFood = head.x == food_.x && head.y == food_.y;

    if (head.x < 0 || head.x >= GRID_WIDTH || head.y < 0 ||
        head.y >= GRID_HEIGHT || occupied(head, ateFood ? length_ : length_ - 1U)) {
        finishGame(false, context);
        return;
    }

    if (ateFood) {
        if (length_ < MAX_LENGTH) {
            ++length_;
        }
        ++score_;
        bestScore_ = std::max(bestScore_, score_);
        const uint32_t speedReduction =
            (score_ / SPEEDUP_FOOD_COUNT) * SPEEDUP_STEP_MS;
        stepIntervalMs_ = speedReduction >=
                                  (INITIAL_STEP_MS - FASTEST_STEP_MS)
                              ? FASTEST_STEP_MS
                              : INITIAL_STEP_MS - speedReduction;
        context.audio.playFeedback();
        if (context.gamepad.snapshot().connected) {
            context.gamepad.requestRumble(70, 70, 135);
        }
    }

    for (uint8_t index = length_ - 1U; index > 0; --index) {
        body_[index] = body_[index - 1U];
    }
    body_[0] = head;

    if (ateFood) {
        if (length_ >= MAX_LENGTH) {
            finishGame(true, context);
            return;
        }
        spawnFood();
    }

    nextStepMs_ = nowMs + stepIntervalMs_;
    invalidate();
}

void SnakeApp::finishGame(bool won, AppContext& context) {
    phase_ = Phase::GameOver;
    won_ = won;
    saveHighScore(context);
    context.audio.playFeedback();
    if (context.gamepad.snapshot().connected) {
        context.gamepad.requestRumble(won ? 180 : 260, won ? 150 : 210,
                                      won ? 220 : 255);
    }
    invalidate();
}

void SnakeApp::saveHighScore(AppContext& context) {
    if (score_ > bestScore_) {
        bestScore_ = score_;
    }
    context.snakeScore.recordScore(bestScore_);
    loadLeaderboard(context);
}

void SnakeApp::loadLeaderboard(AppContext& context) {
    bestScore_ = context.snakeScore.bestScore();
    for (uint8_t index = 0; index < LEADERBOARD_COUNT; ++index) {
        leaderboard_[index] = context.snakeScore.scoreAt(index);
    }
}

void SnakeApp::spawnFood() {
    for (uint16_t attempt = 0; attempt < MAX_LENGTH * 2U; ++attempt) {
        Cell candidate = {
            static_cast<int8_t>(nextRandom() % GRID_WIDTH),
            static_cast<int8_t>(nextRandom() % GRID_HEIGHT),
        };
        if (!occupied(candidate, length_)) {
            food_ = candidate;
            return;
        }
    }

    for (int8_t y = 0; y < GRID_HEIGHT; ++y) {
        for (int8_t x = 0; x < GRID_WIDTH; ++x) {
            Cell candidate = {x, y};
            if (!occupied(candidate, length_)) {
                food_ = candidate;
                return;
            }
        }
    }
}

void SnakeApp::queueDirection(Direction direction) {
    if (turnAcceptedForStep_ || direction == queuedDirection_ ||
        isOpposite(direction, direction_) ||
        isOpposite(direction, queuedDirection_)) {
        return;
    }
    queuedDirection_ = direction;
    turnAcceptedForStep_ = true;
    invalidate();
}

void SnakeApp::sampleAnalogDirection(const GamepadSnapshot& gamepad) {
    if (!gamepad.connected) {
        return;
    }

    const int16_t x = gamepad.axisX;
    const int16_t y = gamepad.axisY;
    if (std::abs(static_cast<int32_t>(x)) < ANALOG_DIRECTION_THRESHOLD &&
        std::abs(static_cast<int32_t>(y)) < ANALOG_DIRECTION_THRESHOLD) {
        return;
    }

    const int32_t absX = std::abs(static_cast<int32_t>(x));
    const int32_t absY = std::abs(static_cast<int32_t>(y));
    if (absX >= ANALOG_DIRECTION_THRESHOLD &&
        absX >= absY + ANALOG_DIRECTION_MARGIN) {
        queueDirection(x < 0 ? Direction::Left : Direction::Right);
    } else if (absY >= ANALOG_DIRECTION_THRESHOLD &&
               absY >= absX + ANALOG_DIRECTION_MARGIN) {
        queueDirection(y < 0 ? Direction::Up : Direction::Down);
    }
}

bool SnakeApp::occupied(Cell cell, uint8_t count) const {
    for (uint8_t index = 0; index < count; ++index) {
        if (body_[index].x == cell.x && body_[index].y == cell.y) {
            return true;
        }
    }
    return false;
}

bool SnakeApp::isOpposite(Direction first, Direction second) const {
    return (first == Direction::Up && second == Direction::Down) ||
           (first == Direction::Down && second == Direction::Up) ||
           (first == Direction::Left && second == Direction::Right) ||
           (first == Direction::Right && second == Direction::Left);
}

SnakeApp::Cell SnakeApp::nextHead() const {
    Cell next = body_[0];
    switch (direction_) {
        case Direction::Up: --next.y; break;
        case Direction::Down: ++next.y; break;
        case Direction::Left: --next.x; break;
        case Direction::Right: ++next.x; break;
    }
    return next;
}

uint32_t SnakeApp::nextRandom() {
    uint32_t value = randomState_;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    randomState_ = value == 0 ? 0x51A7C0DEU : value;
    return randomState_;
}

void SnakeApp::invalidate() {
    if (surface_ != nullptr) {
        lv_obj_invalidate(surface_);
    }
}
