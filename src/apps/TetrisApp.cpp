#include "apps/TetrisApp.h"

#include "services/AudioService.h"
#include "services/BleGamepadService.h"
#include "services/TetrisScoreService.h"
#include "ui/UiRuntime.h"

#include <algorithm>
#include <cstdlib>

namespace {

constexpr int16_t SURFACE_WIDTH = 320;
constexpr int16_t SURFACE_HEIGHT = 218;
constexpr int16_t HUD_HEIGHT = 25;

// Each rotation is four row masks; bit zero is the leftmost cell.
constexpr uint8_t SHAPES[7][4][4] = {
    {{0x0F, 0x00, 0x00, 0x00}, {0x02, 0x02, 0x02, 0x02},
     {0x0F, 0x00, 0x00, 0x00}, {0x02, 0x02, 0x02, 0x02}}, // I
    {{0x06, 0x06, 0x00, 0x00}, {0x06, 0x06, 0x00, 0x00},
     {0x06, 0x06, 0x00, 0x00}, {0x06, 0x06, 0x00, 0x00}}, // O
    {{0x02, 0x07, 0x00, 0x00}, {0x02, 0x06, 0x02, 0x00},
     {0x00, 0x07, 0x02, 0x00}, {0x02, 0x03, 0x02, 0x00}}, // T
    {{0x06, 0x03, 0x00, 0x00}, {0x02, 0x06, 0x04, 0x00},
     {0x06, 0x03, 0x00, 0x00}, {0x02, 0x06, 0x04, 0x00}}, // S
    {{0x03, 0x06, 0x00, 0x00}, {0x04, 0x06, 0x02, 0x00},
     {0x03, 0x06, 0x00, 0x00}, {0x04, 0x06, 0x02, 0x00}}, // Z
    {{0x01, 0x07, 0x00, 0x00}, {0x06, 0x02, 0x02, 0x00},
     {0x07, 0x04, 0x00, 0x00}, {0x02, 0x02, 0x03, 0x00}}, // J
    {{0x04, 0x07, 0x00, 0x00}, {0x02, 0x02, 0x06, 0x00},
     {0x07, 0x01, 0x00, 0x00}, {0x03, 0x02, 0x02, 0x00}}, // L
};

}  // namespace

AppId TetrisApp::id() const { return AppId::Tetris; }

const char* TetrisApp::name() const { return "Tetris"; }

void TetrisApp::onEnter(AppContext& context) {
    root_ = nullptr;
    surface_ = nullptr;
    phase_ = Phase::Title;
    analogXSign_ = 0;
    analogYSign_ = 0;
    loadLeaderboard(context);
    resetGame();
}

void TetrisApp::onExit(AppContext& context) {
    saveHighScore(context);
    root_ = nullptr;
    surface_ = nullptr;
    hudFont_ = nullptr;
    overlayFont_ = nullptr;
}

void TetrisApp::onCommand(const AppCommand& command, AppContext& context) {
    switch (command.type) {
        case AppCommandType::Previous:
            if (phase_ == Phase::Running) {
                tryRotate();
                invalidate();
            }
            break;
        case AppCommandType::Next:
            if (phase_ == Phase::Running) {
                const uint32_t nowMs = millis();
                dropOne(nowMs, context, true);
                nextSoftDropMs_ = nowMs + SOFT_DROP_INTERVAL_MS;
            }
            break;
        case AppCommandType::QuickDrop:
            if (phase_ == Phase::Running) {
                hardDrop(context);
            }
            break;
        case AppCommandType::Left:
            if (phase_ == Phase::Running) {
                tryMove(-1, 0);
                invalidate();
            }
            break;
        case AppCommandType::Right:
            if (phase_ == Phase::Running) {
                tryMove(1, 0);
                invalidate();
            }
            break;
        case AppCommandType::Activate:
            if (phase_ == Phase::Title || phase_ == Phase::GameOver) {
                startGame(millis());
            } else if (phase_ == Phase::Running) {
                phase_ = Phase::Paused;
                invalidate();
            } else {
                phase_ = Phase::Running;
                nextGravityMs_ = millis() + gravityIntervalMs_;
                invalidate();
            }
            break;
        default:
            break;
    }
}

void TetrisApp::onTick(uint32_t nowMs, AppContext& context) {
    if (surface_ == nullptr) {
        return;
    }
    const GamepadSnapshot gamepad = context.gamepad.snapshot();
    sampleAnalog(gamepad, nowMs);
    const bool softDropHeld = gamepad.connected &&
        ((gamepad.dpad & GamepadDpadDown) != 0 ||
         gamepad.axisY >= ANALOG_THRESHOLD);
    if (phase_ == Phase::Running && softDropHeld &&
        static_cast<int32_t>(nowMs - nextSoftDropMs_) >= 0) {
        dropOne(nowMs, context, true);
        nextSoftDropMs_ = nowMs + SOFT_DROP_INTERVAL_MS;
    }
    if (phase_ == Phase::Running &&
        static_cast<int32_t>(nowMs - nextGravityMs_) >= 0) {
        dropOne(nowMs, context, false);
    }
}

lv_obj_t* TetrisApp::onCreateView(AppContext& context) {
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
    overlayFont_ = context.ui.font(20);
    backgroundColor_ = context.ui.background();
    panelColor_ = context.ui.panel();
    gridColor_ = context.ui.panelRaised();
    textColor_ = context.ui.text();
    mutedColor_ = context.ui.muted();
    accentColor_ = context.ui.accent();
    pieceColors_[0] = backgroundColor_;
    pieceColors_[1] = lv_color_hex(0x35C9E8); // I
    pieceColors_[2] = lv_color_hex(0xF1C84B); // O
    pieceColors_[3] = lv_color_hex(0xB06CDE); // T
    pieceColors_[4] = lv_color_hex(0x54C878); // S
    pieceColors_[5] = lv_color_hex(0xE65C61); // Z
    pieceColors_[6] = lv_color_hex(0x5689E8); // J
    pieceColors_[7] = lv_color_hex(0xE99A42); // L
    invalidate();
    return root_;
}

void TetrisApp::onUpdateView(AppContext&) { invalidate(); }

void TetrisApp::drawEvent(lv_event_t* event) {
    auto* app = static_cast<TetrisApp*>(lv_event_get_user_data(event));
    if (app != nullptr && lv_event_get_target_obj(event) == app->surface_) {
        app->draw(event);
    }
}

void TetrisApp::draw(lv_event_t* event) {
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
    char hudText[64];
    lv_snprintf(hudText, sizeof(hudText), "TETRIS   SCORE %04u   LV %u",
                static_cast<unsigned>(score_), static_cast<unsigned>(level_));
    lv_area_t hudLabel = hud;
    hudLabel.x1 += 9;
    hudLabel.x2 -= 9;
    drawText(layer, hudText, hudLabel, textColor_, hudFont_,
             LV_TEXT_ALIGN_LEFT);

    lv_area_t board = {
        static_cast<lv_coord_t>(surfaceArea.x1 + BOARD_X),
        static_cast<lv_coord_t>(surfaceArea.y1 + BOARD_Y + HUD_HEIGHT),
        static_cast<lv_coord_t>(surfaceArea.x1 + BOARD_X + BOARD_WIDTH * CELL_SIZE - 1),
        static_cast<lv_coord_t>(surfaceArea.y1 + BOARD_Y + HUD_HEIGHT + BOARD_HEIGHT * CELL_SIZE - 1),
    };
    drawRect(layer, board, lv_color_hex(0x0B1320), 4);
    for (uint8_t row = 0; row < BOARD_HEIGHT; ++row) {
        for (uint8_t column = 0; column < BOARD_WIDTH; ++column) {
            const uint8_t value = board_[row][column];
            lv_area_t cell = {
                static_cast<lv_coord_t>(board.x1 + column * CELL_SIZE + 1),
                static_cast<lv_coord_t>(board.y1 + row * CELL_SIZE + 1),
                static_cast<lv_coord_t>(board.x1 + (column + 1) * CELL_SIZE - 2),
                static_cast<lv_coord_t>(board.y1 + (row + 1) * CELL_SIZE - 2),
            };
            drawRect(layer, cell, value == 0 ? gridColor_ : pieceColors_[value],
                     value == 0 ? 1 : 2);
        }
    }
    if (phase_ == Phase::Running || phase_ == Phase::Paused) {
        Piece ghost = current_;
        while (true) {
            Piece candidate = ghost;
            ++candidate.y;
            if (collides(candidate)) {
                break;
            }
            ghost = candidate;
        }
        for (uint8_t row = 0; row < 4; ++row) {
            for (uint8_t column = 0; column < 4; ++column) {
                if (!hasCell(ghost.type, ghost.rotation, row, column)) {
                    continue;
                }
                const int16_t x = ghost.x + column;
                const int16_t y = ghost.y + row;
                if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT) {
                    continue;
                }
                lv_area_t cell = {
                    static_cast<lv_coord_t>(board.x1 + x * CELL_SIZE + 1),
                    static_cast<lv_coord_t>(board.y1 + y * CELL_SIZE + 1),
                    static_cast<lv_coord_t>(board.x1 + (x + 1) * CELL_SIZE - 2),
                    static_cast<lv_coord_t>(board.y1 + (y + 1) * CELL_SIZE - 2),
                };
                drawRect(layer, cell, pieceColors_[ghost.type + 1], 2,
                         LV_OPA_40);
            }
        }
        for (uint8_t row = 0; row < 4; ++row) {
            for (uint8_t column = 0; column < 4; ++column) {
                if (!hasCell(current_.type, current_.rotation, row, column)) {
                    continue;
                }
                const int16_t x = current_.x + column;
                const int16_t y = current_.y + row;
                if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT) {
                    continue;
                }
                lv_area_t cell = {
                    static_cast<lv_coord_t>(board.x1 + x * CELL_SIZE + 1),
                    static_cast<lv_coord_t>(board.y1 + y * CELL_SIZE + 1),
                    static_cast<lv_coord_t>(board.x1 + (x + 1) * CELL_SIZE - 2),
                    static_cast<lv_coord_t>(board.y1 + (y + 1) * CELL_SIZE - 2),
                };
                drawRect(layer, cell, pieceColors_[current_.type + 1], 2);
            }
        }
    }

    lv_area_t leftPanel = {
        static_cast<lv_coord_t>(surfaceArea.x1 + 8),
        static_cast<lv_coord_t>(surfaceArea.y1 + 35),
        static_cast<lv_coord_t>(surfaceArea.x1 + BOARD_X - 9),
        static_cast<lv_coord_t>(surfaceArea.y2 - 5),
    };
    drawRect(layer, leftPanel, panelColor_, 6);
    char text[40];
    lv_area_t label = leftPanel;
    label.x1 += 9;
    label.x2 -= 9;
    label.y1 += 10;
    label.y2 = label.y1 + 16;
    drawText(layer, "TOP 5", label, mutedColor_, hudFont_, LV_TEXT_ALIGN_LEFT);
    for (uint8_t index = 0; index < LEADERBOARD_COUNT; ++index) {
        label.y1 += 22;
        label.y2 = label.y1 + 16;
        lv_snprintf(text, sizeof(text), "%u. %04u",
                    static_cast<unsigned>(index + 1U),
                    static_cast<unsigned>(leaderboard_[index]));
        drawText(layer, text, label, index == 0 ? textColor_ : mutedColor_,
                 hudFont_, LV_TEXT_ALIGN_LEFT);
    }

    lv_area_t panel = {
        static_cast<lv_coord_t>(surfaceArea.x1 + 214),
        static_cast<lv_coord_t>(surfaceArea.y1 + 35),
        static_cast<lv_coord_t>(surfaceArea.x2 - 8),
        static_cast<lv_coord_t>(surfaceArea.y2 - 5),
    };
    drawRect(layer, panel, panelColor_, 6);
    label = panel;
    label.x1 += 9;
    label.x2 -= 9;
    label.y1 += 10;
    label.y2 = label.y1 + 16;
    drawText(layer, "NEXT", label, mutedColor_, hudFont_, LV_TEXT_ALIGN_LEFT);
    lv_area_t preview = panel;
    preview.x1 += 48;
    preview.x2 = preview.x1 + 48;
    preview.y1 += 28;
    preview.y2 = preview.y1 + 42;
    drawRect(layer, preview, lv_color_hex(0x0B1320), 4);
    for (uint8_t row = 0; row < 4; ++row) {
        for (uint8_t column = 0; column < 4; ++column) {
            if (!hasCell(nextType_, 0, row, column)) {
                continue;
            }
            lv_area_t cell = {
                static_cast<lv_coord_t>(preview.x1 + 5 + column * 10),
                static_cast<lv_coord_t>(preview.y1 + 1 + row * 10),
                static_cast<lv_coord_t>(preview.x1 + 13 + column * 10),
                static_cast<lv_coord_t>(preview.y1 + 9 + row * 10),
            };
            drawRect(layer, cell, pieceColors_[nextType_ + 1], 2);
        }
    }
    label.y1 = panel.y1 + 82;
    label.y2 = label.y1 + 15;
    lv_snprintf(text, sizeof(text), "LINES  %03u", static_cast<unsigned>(lines_));
    drawText(layer, text, label, textColor_, hudFont_, LV_TEXT_ALIGN_LEFT);
    label.y1 += 17;
    label.y2 += 17;
    lv_snprintf(text, sizeof(text), "BEST   %04u", static_cast<unsigned>(bestScore_));
    drawText(layer, text, label, accentColor_, hudFont_, LV_TEXT_ALIGN_LEFT);

    if (phase_ != Phase::Running) {
        lv_area_t overlay = surfaceArea;
        overlay.x1 += 8;
        overlay.x2 -= 8;
        overlay.y1 += phase_ == Phase::GameOver ? 78 : 76;
        overlay.y2 = overlay.y1 + (phase_ == Phase::GameOver ? 76 : 58);
        drawRect(layer, overlay, backgroundColor_, 8, LV_OPA_90);
        const char* title = "PRESS A TO START";
        const char* subtitle = "D-PAD / LEFT STICK";
        if (phase_ == Phase::Paused) {
            title = "PAUSED";
            subtitle = "A RESUME   B EXIT";
        } else if (phase_ == Phase::GameOver) {
            title = "GAME OVER";
            subtitle = "A RESTART   B EXIT";
        }
        lv_area_t titleArea = overlay;
        titleArea.y1 += phase_ == Phase::GameOver ? 10 : 14;
        titleArea.y2 = titleArea.y1 + 25;
        drawText(layer, title, titleArea, textColor_, overlayFont_);
        lv_area_t subtitleArea = overlay;
        subtitleArea.y1 = overlay.y2 - 22;
        subtitleArea.y2 = overlay.y2 - 5;
        drawText(layer, subtitle, subtitleArea, mutedColor_, hudFont_);
    }
}

void TetrisApp::drawRect(lv_layer_t* layer, const lv_area_t& area,
                         lv_color_t color, int32_t radius, lv_opa_t opacity) const {
    lv_draw_rect_dsc_t descriptor;
    lv_draw_rect_dsc_init(&descriptor);
    descriptor.bg_color = color;
    descriptor.bg_opa = opacity;
    descriptor.radius = radius;
    lv_draw_rect(layer, &descriptor, &area);
}

void TetrisApp::drawText(lv_layer_t* layer, const char* text, lv_area_t area,
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

void TetrisApp::resetGame() {
    randomState_ ^= millis() + 0x9E3779B9U;
    if (randomState_ == 0) {
        randomState_ = 0x7E57C0DEU;
    }
    for (auto& row : board_) {
        for (auto& cell : row) {
            cell = 0;
        }
    }
    bagIndex_ = 7;
    score_ = 0;
    lines_ = 0;
    level_ = 1;
    gravityIntervalMs_ = INITIAL_GRAVITY_MS;
    nextSoftDropMs_ = 0;
    nextHorizontalRepeatMs_ = 0;
    current_ = {};
    refillBag();
    nextType_ = bag_[bagIndex_++];
    spawnPiece();
    phase_ = Phase::Title;
    invalidate();
}

void TetrisApp::startGame(uint32_t nowMs) {
    resetGame();
    phase_ = Phase::Running;
    nextGravityMs_ = nowMs + gravityIntervalMs_;
    invalidate();
}

void TetrisApp::spawnPiece() {
    current_.type = nextType_;
    current_.rotation = 0;
    current_.x = 3;
    current_.y = 0;
    if (bagIndex_ >= 7) {
        refillBag();
    }
    nextType_ = bag_[bagIndex_++];
    if (collides(current_)) {
        phase_ = Phase::GameOver;
    }
}

void TetrisApp::refillBag() {
    for (uint8_t index = 0; index < 7; ++index) {
        bag_[index] = index;
    }
    for (int index = 6; index > 0; --index) {
        const uint8_t swapIndex = nextRandom() % static_cast<uint32_t>(index + 1);
        std::swap(bag_[index], bag_[swapIndex]);
    }
    bagIndex_ = 0;
}

void TetrisApp::lockPiece(AppContext& context) {
    for (uint8_t row = 0; row < 4; ++row) {
        for (uint8_t column = 0; column < 4; ++column) {
            if (!hasCell(current_.type, current_.rotation, row, column)) {
                continue;
            }
            const int8_t x = current_.x + column;
            const int8_t y = current_.y + row;
            if (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT) {
                board_[y][x] = current_.type + 1U;
            }
        }
    }
    const uint8_t cleared = clearLines();
    context.audio.playGameTone(cleared == 0 ? 35U : 110U + cleared * 30U);
    if (context.gamepad.snapshot().connected) {
        if (cleared == 0) {
            context.gamepad.requestRumble(45, 55, 70);
        } else {
            context.gamepad.requestRumble(
                120U + cleared * 35U, 120U + cleared * 15U,
                100U + cleared * 30U);
        }
    }
    spawnPiece();
    nextGravityMs_ = millis() + gravityIntervalMs_;
    if (phase_ == Phase::GameOver) {
        saveHighScore(context);
        context.audio.playFeedback();
        if (context.gamepad.snapshot().connected) {
            context.gamepad.requestRumble(260, 200, 255);
        }
    }
}

void TetrisApp::hardDrop(AppContext& context) {
    uint8_t dropped = 0;
    while (tryMove(0, 1)) {
        if (dropped < 255U) {
            ++dropped;
        }
    }
    score_ = std::min<uint32_t>(65535U,
                                score_ + static_cast<uint16_t>(dropped) * 2U);
    bestScore_ = std::max(bestScore_, score_);
    lockPiece(context);
    invalidate();
}

void TetrisApp::dropOne(uint32_t nowMs, AppContext& context, bool softDrop) {
    if (tryMove(0, 1)) {
        if (softDrop) {
            score_ = std::min<uint16_t>(65535U, score_ + 1U);
            bestScore_ = std::max(bestScore_, score_);
        }
        nextGravityMs_ = nowMs + gravityIntervalMs_;
    } else {
        lockPiece(context);
    }
    invalidate();
}

uint8_t TetrisApp::clearLines() {
    uint8_t cleared = 0;
    for (int row = BOARD_HEIGHT - 1; row >= 0; --row) {
        bool full = true;
        for (uint8_t column = 0; column < BOARD_WIDTH; ++column) {
            if (board_[row][column] == 0) {
                full = false;
                break;
            }
        }
        if (!full) {
            continue;
        }
        ++cleared;
        for (int copyRow = row; copyRow > 0; --copyRow) {
            for (uint8_t column = 0; column < BOARD_WIDTH; ++column) {
                board_[copyRow][column] = board_[copyRow - 1][column];
            }
        }
        for (uint8_t column = 0; column < BOARD_WIDTH; ++column) {
            board_[0][column] = 0;
        }
        ++row;
    }
    if (cleared == 0) {
        return 0;
    }
    static constexpr uint16_t lineScores[] = {0, 100, 300, 500, 800};
    score_ = std::min<uint32_t>(65535U,
                                score_ + lineScores[cleared] * level_);
    lines_ = std::min<uint16_t>(999U, lines_ + cleared);
    level_ = static_cast<uint8_t>(1U + lines_ / 10U);
    gravityIntervalMs_ = std::max<uint32_t>(MIN_GRAVITY_MS,
        INITIAL_GRAVITY_MS - static_cast<uint32_t>(level_ - 1U) * 55U);
    bestScore_ = std::max(bestScore_, score_);
    return cleared;
}

bool TetrisApp::tryMove(int8_t dx, int8_t dy) {
    Piece candidate = current_;
    candidate.x += dx;
    candidate.y += dy;
    if (collides(candidate)) {
        return false;
    }
    current_ = candidate;
    return true;
}

bool TetrisApp::tryRotate() {
    Piece candidate = current_;
    candidate.rotation = (candidate.rotation + 1U) % 4U;
    constexpr int8_t KICKS[] = {0, -1, 1, -2, 2};
    for (int8_t kick : KICKS) {
        candidate.x = current_.x + kick;
        if (!collides(candidate)) {
            current_ = candidate;
            return true;
        }
    }
    return false;
}

bool TetrisApp::collides(const Piece& piece) const {
    for (uint8_t row = 0; row < 4; ++row) {
        for (uint8_t column = 0; column < 4; ++column) {
            if (!hasCell(piece.type, piece.rotation, row, column)) {
                continue;
            }
            const int16_t x = piece.x + column;
            const int16_t y = piece.y + row;
            if (x < 0 || x >= BOARD_WIDTH || y >= BOARD_HEIGHT) {
                return true;
            }
            if (y >= 0 && board_[y][x] != 0) {
                return true;
            }
        }
    }
    return false;
}

bool TetrisApp::hasCell(uint8_t type, uint8_t rotation, uint8_t row,
                        uint8_t column) const {
    return type < 7 && row < 4 && column < 4 &&
           (SHAPES[type][rotation % 4U][row] & (1U << column)) != 0;
}

void TetrisApp::sampleAnalog(const GamepadSnapshot& gamepad, uint32_t nowMs) {
    if (!gamepad.connected || phase_ != Phase::Running) {
        analogXSign_ = 0;
        analogYSign_ = 0;
        nextHorizontalRepeatMs_ = 0;
        return;
    }
    const int8_t xSign = std::abs(static_cast<int32_t>(gamepad.axisX)) >= ANALOG_THRESHOLD
                             ? (gamepad.axisX < 0 ? -1 : 1) : 0;
    const int8_t ySign = std::abs(static_cast<int32_t>(gamepad.axisY)) >= ANALOG_THRESHOLD
                             ? (gamepad.axisY < 0 ? -1 : 1) : 0;
    bool changed = false;
    if (xSign != analogXSign_) {
        if (xSign < 0) {
            changed = tryMove(-1, 0) || changed;
        } else if (xSign > 0) {
            changed = tryMove(1, 0) || changed;
        }
        analogXSign_ = xSign;
        nextHorizontalRepeatMs_ = xSign == 0
            ? 0 : nowMs + HORIZONTAL_INITIAL_REPEAT_MS;
    } else if (xSign != 0 &&
               static_cast<int32_t>(nowMs - nextHorizontalRepeatMs_) >= 0) {
        changed = (xSign < 0 ? tryMove(-1, 0) : tryMove(1, 0)) || changed;
        nextHorizontalRepeatMs_ = nowMs + HORIZONTAL_REPEAT_INTERVAL_MS;
    }
    if (ySign != analogYSign_) {
        if (ySign < 0) {
            changed = tryRotate() || changed;
        }
        analogYSign_ = ySign;
    }
    if (changed) {
        invalidate();
    }
}

void TetrisApp::saveHighScore(AppContext& context) {
    if (score_ > bestScore_) {
        bestScore_ = score_;
    }
    context.tetrisScore.recordScore(bestScore_);
    loadLeaderboard(context);
}

void TetrisApp::loadLeaderboard(AppContext& context) {
    bestScore_ = context.tetrisScore.bestScore();
    for (uint8_t index = 0; index < LEADERBOARD_COUNT; ++index) {
        leaderboard_[index] = context.tetrisScore.scoreAt(index);
    }
}

uint32_t TetrisApp::nextRandom() {
    uint32_t value = randomState_;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    randomState_ = value == 0 ? 0x7E57C0DEU : value;
    return randomState_;
}

void TetrisApp::invalidate() {
    if (surface_ != nullptr) {
        lv_obj_invalidate(surface_);
    }
}
