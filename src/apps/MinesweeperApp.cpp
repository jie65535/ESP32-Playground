#include "apps/MinesweeperApp.h"

#include "services/AudioService.h"
#include "services/BleGamepadService.h"
#include "services/MinesweeperProfileService.h"
#include "services/RgbService.h"
#include "games/MinesweeperPresentation.h"
#include "ui/CanvasDraw.h"
#include "ui/UiRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using pgos::drawRect;
using pgos::drawText;

namespace {

constexpr uint8_t NUMBER_PIXELS[8][7] = {
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},  // 2
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},  // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},  // 4
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},  // 5
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},  // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},  // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},  // 8
};

constexpr uint8_t SEVEN_SEGMENTS[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

constexpr const char* DIFFICULTY_LABELS[] = {
    "初级", "中级", "专家", "自定义",
};

constexpr const char* DIFFICULTY_DETAILS[] = {
    "9x9 / 10", "16x16 / 40", "30x16 / 99", "CUSTOM",
};

constexpr uint32_t clampTimeMs(uint32_t elapsedMs) {
    return elapsedMs > 999000U ? 999000U : elapsedMs;
}

void formatTime(uint32_t elapsedMs, char* output, size_t size,
                bool tenths = false) {
    elapsedMs = clampTimeMs(elapsedMs);
    const uint32_t seconds = elapsedMs / 1000U;
    if (tenths) {
        std::snprintf(output, size, "%lu.%01lu",
                      static_cast<unsigned long>(seconds),
                      static_cast<unsigned long>((elapsedMs / 100U) % 10U));
    } else {
        std::snprintf(output, size, "%03lu",
                      static_cast<unsigned long>(seconds));
    }
}

lv_area_t inset(lv_area_t area, int16_t amount) {
    area.x1 += amount;
    area.y1 += amount;
    area.x2 -= amount;
    area.y2 -= amount;
    return area;
}

bool areasIntersect(const lv_area_t& first, const lv_area_t& second) {
    return first.x1 <= second.x2 && first.x2 >= second.x1 &&
           first.y1 <= second.y2 && first.y2 >= second.y1;
}

}  // namespace

AppId MinesweeperApp::id() const { return AppId::Minesweeper; }

const char* MinesweeperApp::name() const { return "Minesweeper"; }

void MinesweeperApp::onEnter(AppContext& context) {
    root_ = nullptr;
    surface_ = nullptr;
    profile_ = &context.minesweeperProfile;
    difficulty_ = context.minesweeperProfile.lastDifficulty();
    difficultySelection_ = static_cast<uint8_t>(difficulty_);
    customWidth_ = context.minesweeperProfile.customWidth();
    customHeight_ = context.minesweeperProfile.customHeight();
    customMines_ = context.minesweeperProfile.customMines();
    phase_ = ViewPhase::Title;
    resetHeldNavigation();
    sampledGamepadButtons_ = 0;
    lastActivateCommandMs_ = 0;
    lastFlagCommandMs_ = 0;
    gamepadButtonsInitialized_ = false;
    activateCommandHandled_ = false;
    flagCommandHandled_ = false;
    resultRecorded_ = false;
    exitArmed_ = false;
    clearVisualState();
}

void MinesweeperApp::onExit(AppContext&) {
    root_ = nullptr;
    surface_ = nullptr;
    tinyFont_ = nullptr;
    hudFont_ = nullptr;
    bodyFont_ = nullptr;
    strongBodyFont_ = nullptr;
    titleFont_ = nullptr;
    profile_ = nullptr;
    resetHeldNavigation();
}

void MinesweeperApp::onCommand(const AppCommand& command,
                               AppContext& context) {
    const uint32_t nowMs = millis();
    switch (command.type) {
        case AppCommandType::Previous:
            handleDirection(Direction::Up, nowMs, context, true);
            break;
        case AppCommandType::Next:
            handleDirection(Direction::Down, nowMs, context, true);
            break;
        case AppCommandType::Left:
            handleDirection(Direction::Left, nowMs, context, true);
            break;
        case AppCommandType::Right:
            handleDirection(Direction::Right, nowMs, context, true);
            break;
        case AppCommandType::QuickDrop:
            flagCommandHandled_ = true;
            lastFlagCommandMs_ = nowMs;
            handleFlagPress(nowMs, context);
            break;
        case AppCommandType::Pause:
            if (phase_ == ViewPhase::Playing) {
                pause(nowMs, false);
            } else if (phase_ == ViewPhase::Paused) {
                resume(nowMs);
            }
            break;
        case AppCommandType::Activate:
            activateCommandHandled_ = true;
            lastActivateCommandMs_ = nowMs;
            handleActivatePress(nowMs, context);
            break;
        default:
            break;
    }
}

void MinesweeperApp::onTick(uint32_t nowMs, AppContext& context) {
    if (surface_ == nullptr) {
        return;
    }
    sampleGamepadButtons(nowMs, context);
    sampleHeldDirection(nowMs, context);

    if (assistFeedback_ != AssistFeedback::None &&
        nowMs - assistFeedbackStartedMs_ >= ASSIST_FEEDBACK_MS) {
        assistFeedback_ = AssistFeedback::None;
        assistFeedbackStartedMs_ = 0;
        invalidateArea(assistFeedbackArea());
    }

    if (phase_ == ViewPhase::Generating) {
        if (engine_.generateStep(1)) {
            phase_ = engine_.phase() == pgos::MinesweeperPhase::Won
                         ? ViewPhase::Won
                         : ViewPhase::Playing;
            gameStartedMs_ = nowMs;
            pausedAccumulatedMs_ = 0;
            scheduleNewReveals(firstX_, firstY_, nowMs);
            context.audio.playGameTone(560U, 54U);
            if (phase_ == ViewPhase::Won) {
                processOutcome(nowMs, context);
            }
        }
        invalidate();
        return;
    }

    if ((phase_ == ViewPhase::Won || phase_ == ViewPhase::Lost) &&
        resultStartedMs_ != 0) {
        const uint32_t resultAge = nowMs - resultStartedMs_;
        if (phase_ == ViewPhase::Won) {
            if (!winCueSecondNote_ && resultAge >= 115U) {
                context.audio.playGameTone(880U, 85U);
                winCueSecondNote_ = true;
            }
            if (!winCueThirdNote_ && resultAge >= 235U) {
                context.audio.playGameTone(1180U, 150U);
                winCueThirdNote_ = true;
            }
        }
    }

    const bool animationsActive =
        static_cast<int32_t>(nowMs - animationsUntilMs_) < 0;
    const uint32_t visualInterval = animationsActive ? 32U : 125U;
    if (nowMs - lastVisualTickMs_ >= visualInterval) {
        lastVisualTickMs_ = nowMs;
        if (phase_ == ViewPhase::Title) {
            invalidateTitleSpark();
        } else if (phase_ == ViewPhase::Ready ||
                   phase_ == ViewPhase::Playing) {
            // A normal game tick redraws only the timer and cursor. Reveal
            // waves are intentionally still a short full-board animation.
            if (animationsActive) {
                invalidate();
            } else {
                invalidateClock();
                invalidateCursor();
            }
        } else if ((phase_ == ViewPhase::Won || phase_ == ViewPhase::Lost) &&
                   animationsActive) {
            invalidate();
        }
    }
}

lv_obj_t* MinesweeperApp::onCreateView(AppContext& context) {
    root_ = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, SURFACE_WIDTH, SURFACE_HEIGHT);
    lv_obj_set_pos(root_, 0, 22);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    surface_ = lv_obj_create(root_);
    lv_obj_remove_style_all(surface_);
    lv_obj_set_size(surface_, SURFACE_WIDTH, SURFACE_HEIGHT);
    lv_obj_set_pos(surface_, 0, 0);
    lv_obj_clear_flag(surface_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(surface_, drawEvent, LV_EVENT_DRAW_MAIN, this);

    tinyFont_ = context.ui.font(10);
    hudFont_ = context.ui.font(12);
    bodyFont_ = context.ui.bitmapFont(BitmapFontSize::Small12);
    strongBodyFont_ = context.ui.bitmapFont(BitmapFontSize::Bold12);
    titleFont_ = context.ui.font(24);

    backgroundColor_ = lv_color_hex(0x14181A);
    headerColor_ = lv_color_hex(0x202629);
    frameColor_ = lv_color_hex(0x090B0C);
    coveredColor_ = lv_color_hex(0x7A8283);
    coveredHighlightColor_ = lv_color_hex(0xC6CDCB);
    coveredShadowColor_ = lv_color_hex(0x373D3F);
    revealedColor_ = lv_color_hex(0xB8B9B4);
    revealedGridColor_ = lv_color_hex(0x8A8E8C);
    textColor_ = lv_color_hex(0xF4F5F2);
    mutedColor_ = lv_color_hex(0x9FA9A8);
    accentColor_ = lv_color_hex(0xF2B84B);
    dangerColor_ = lv_color_hex(0xDF4D4D);
    successColor_ = lv_color_hex(0x42B982);
    numberColors_[0] = lv_color_hex(0x2559C7);
    numberColors_[1] = lv_color_hex(0x27834F);
    numberColors_[2] = lv_color_hex(0xC63E3E);
    numberColors_[3] = lv_color_hex(0x5A3D9D);
    numberColors_[4] = lv_color_hex(0x8A332D);
    numberColors_[5] = lv_color_hex(0x187D86);
    numberColors_[6] = lv_color_hex(0x222629);
    numberColors_[7] = lv_color_hex(0x6D7475);
    invalidate();
    return root_;
}

void MinesweeperApp::onUpdateView(AppContext&) {}

bool MinesweeperApp::onBack(AppContext&) {
    if (phase_ == ViewPhase::CustomSetup ||
        phase_ == ViewPhase::Statistics ||
        phase_ == ViewPhase::Won || phase_ == ViewPhase::Lost) {
        phase_ = ViewPhase::Title;
        resetHeldNavigation();
        invalidate();
        return true;
    }
    return false;
}

void MinesweeperApp::drawEvent(lv_event_t* event) {
    auto* app = static_cast<MinesweeperApp*>(lv_event_get_user_data(event));
    if (app != nullptr && lv_event_get_target_obj(event) == app->surface_) {
        app->draw(event);
    }
}

void MinesweeperApp::draw(lv_event_t* event) {
    lv_obj_t* object = lv_event_get_target_obj(event);
    lv_layer_t* layer = lv_event_get_layer(event);
    if (object == nullptr || layer == nullptr) {
        return;
    }
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    const uint32_t nowMs = millis();
    drawRect(layer, area, backgroundColor_);
    if (phase_ == ViewPhase::Title) {
        drawTitle(layer, area, nowMs);
    } else if (phase_ == ViewPhase::CustomSetup) {
        drawCustomSetup(layer, area);
    } else if (phase_ == ViewPhase::Statistics) {
        drawStatistics(layer, area);
    } else {
        drawGame(layer, area, nowMs);
    }
}

void MinesweeperApp::drawTitle(lv_layer_t* layer, const lv_area_t& area,
                               uint32_t nowMs) const {
    lv_area_t header = area;
    header.y2 = header.y1 + 27;
    drawRect(layer, header, headerColor_);
    header.x1 += 9;
    drawText(layer, "MINESWEEPER", header, textColor_, hudFont_,
             LV_TEXT_ALIGN_LEFT);

    lv_area_t mineArea = {static_cast<lv_coord_t>(area.x1 + 139),
                          static_cast<lv_coord_t>(area.y1 + 36),
                          static_cast<lv_coord_t>(area.x1 + 180),
                          static_cast<lv_coord_t>(area.y1 + 77)};
    drawMine(layer, mineArea, accentColor_);
    const uint8_t fuse = static_cast<uint8_t>((nowMs / 180U) % 4U);
    lv_area_t spark = {static_cast<lv_coord_t>(mineArea.x2 - 2 + fuse),
                       static_cast<lv_coord_t>(mineArea.y1 - 4 - fuse),
                       static_cast<lv_coord_t>(mineArea.x2 + fuse),
                       static_cast<lv_coord_t>(mineArea.y1 - 2 - fuse)};
    drawRect(layer, spark, fuse == 3 ? dangerColor_ : accentColor_);

    lv_area_t title = area;
    title.y1 += 78;
    title.y2 = title.y1 + 27;
    drawText(layer, "MINESWEEPER", title, textColor_, titleFont_);
    lv_area_t subtitle = title;
    subtitle.y1 = title.y2 + 2;
    subtitle.y2 = subtitle.y1 + 17;
    drawText(layer, "PGOS LOGIC FIELD", subtitle, mutedColor_, hudFont_);

    for (uint8_t index = 0; index < 4; ++index) {
        const int16_t x = area.x1 + 6 + index * 78;
        lv_area_t button = {static_cast<lv_coord_t>(x),
                            static_cast<lv_coord_t>(area.y1 + 127),
                            static_cast<lv_coord_t>(x + 72),
                            static_cast<lv_coord_t>(area.y1 + 170)};
        const bool selected = index == difficultySelection_;
        drawRect(layer, button, selected ? accentColor_ : frameColor_, 5);
        button = inset(button, 2);
        drawRect(layer, button, selected ? lv_color_hex(0x34302A)
                                         : headerColor_, 3);
        lv_area_t label = button;
        label.y2 = label.y1 + 22;
        drawText(layer, DIFFICULTY_LABELS[index], label,
                 selected ? accentColor_ : textColor_, strongBodyFont_);
        lv_area_t detail = button;
        detail.y1 += 22;
        drawText(layer, DIFFICULTY_DETAILS[index], detail, mutedColor_,
                 tinyFont_);
    }

    if (profile_ != nullptr) {
        const pgos::MinesweeperDifficulty selected = difficultySelection_ < 4
            ? static_cast<pgos::MinesweeperDifficulty>(difficultySelection_)
            : difficulty_;
        char summary[64];
        if (pgos::MinesweeperRecords::isStandard(selected)) {
            const auto& stats = profile_->records().level(selected);
            char best[16] = "---";
            if (stats.bestTimesMs[0] != 0) {
                formatTime(stats.bestTimesMs[0], best, sizeof(best), true);
            }
            std::snprintf(summary, sizeof(summary),
                          "%s s / %lu/%lu",
                          best, static_cast<unsigned long>(stats.wins),
                          static_cast<unsigned long>(stats.played));
        } else {
            const auto& data = profile_->records().data();
            std::snprintf(summary, sizeof(summary),
                          "%ux%u/%u / %lu/%lu",
                          static_cast<unsigned>(customWidth_),
                          static_cast<unsigned>(customHeight_),
                          static_cast<unsigned>(customMines_),
                          static_cast<unsigned long>(data.customWins),
                          static_cast<unsigned long>(data.customPlayed));
        }
        lv_area_t records = {static_cast<lv_coord_t>(area.x1 + 62),
                             static_cast<lv_coord_t>(area.y1 + 179),
                             static_cast<lv_coord_t>(area.x2 - 62),
                             static_cast<lv_coord_t>(area.y1 + 207)};
        const bool selectedRecords = difficultySelection_ == 4;
        drawRect(layer, records,
                 selectedRecords ? accentColor_ : frameColor_, 4);
        records = inset(records, 2);
        drawRect(layer, records, headerColor_, 3);
        lv_area_t label = records;
        label.x1 += 8;
        label.x2 = label.x1 + 66;
        drawText(layer, "RECORDS", label,
                 selectedRecords ? accentColor_ : textColor_, tinyFont_,
                 LV_TEXT_ALIGN_LEFT);
        lv_area_t value = records;
        value.x1 = label.x2 + 4;
        value.x2 -= 8;
        drawText(layer, summary, value, mutedColor_, tinyFont_,
                 LV_TEXT_ALIGN_RIGHT);
    }
}

void MinesweeperApp::drawCustomSetup(lv_layer_t* layer,
                                     const lv_area_t& area) const {
    lv_area_t header = area;
    header.y2 = header.y1 + 30;
    drawRect(layer, header, headerColor_);
    header.x1 += 10;
    drawText(layer, "CUSTOM FIELD", header, textColor_, hudFont_,
             LV_TEXT_ALIGN_LEFT);

    const char* labels[] = {"宽度", "高度", "地雷", "开始"};
    char values[3][16];
    std::snprintf(values[0], sizeof(values[0]), "%u",
                  static_cast<unsigned>(customWidth_));
    std::snprintf(values[1], sizeof(values[1]), "%u",
                  static_cast<unsigned>(customHeight_));
    std::snprintf(values[2], sizeof(values[2]), "%u",
                  static_cast<unsigned>(customMines_));

    for (uint8_t row = 0; row < 4; ++row) {
        lv_area_t line = {static_cast<lv_coord_t>(area.x1 + 34),
                          static_cast<lv_coord_t>(area.y1 + 46 + row * 39),
                          static_cast<lv_coord_t>(area.x2 - 34),
                          static_cast<lv_coord_t>(area.y1 + 78 + row * 39)};
        const bool selected = row == customSelection_;
        if (selected) {
            drawRect(layer, line, lv_color_hex(0x332E25), 4);
            lv_area_t marker = line;
            marker.x2 = marker.x1 + 3;
            drawRect(layer, marker, accentColor_, 2);
        }
        line.x1 += 12;
        lv_area_t label = line;
        label.x2 = label.x1 + 90;
        drawText(layer, labels[row], label,
                 selected ? textColor_ : mutedColor_, strongBodyFont_,
                 LV_TEXT_ALIGN_LEFT);
        if (row < 3) {
            lv_area_t value = line;
            value.x1 = value.x2 - 90;
            value.x2 -= 12;
            drawText(layer, values[row], value,
                     selected ? accentColor_ : textColor_, hudFont_,
                     LV_TEXT_ALIGN_RIGHT);
        } else {
            lv_area_t detail = line;
            detail.x1 = detail.x2 - 150;
            detail.x2 -= 12;
            char text[40];
            std::snprintf(text, sizeof(text), "%ux%u / %u",
                          static_cast<unsigned>(customWidth_),
                          static_cast<unsigned>(customHeight_),
                          static_cast<unsigned>(customMines_));
            drawText(layer, text, detail,
                     selected ? accentColor_ : textColor_, hudFont_,
                     LV_TEXT_ALIGN_RIGHT);
        }
    }
}

void MinesweeperApp::drawStatistics(lv_layer_t* layer,
                                    const lv_area_t& area) const {
    lv_area_t header = area;
    header.y2 = header.y1 + 30;
    drawRect(layer, header, headerColor_);
    header.x1 += 10;
    drawText(layer, "RECORDS", header, textColor_, hudFont_,
             LV_TEXT_ALIGN_LEFT);
    if (profile_ == nullptr) {
        return;
    }

    for (uint8_t level = 0; level < 3; ++level) {
        const auto difficulty = static_cast<pgos::MinesweeperDifficulty>(level);
        const auto& stats = profile_->records().level(difficulty);
        lv_area_t row = {static_cast<lv_coord_t>(area.x1 + 14),
                         static_cast<lv_coord_t>(area.y1 + 42 + level * 50),
                         static_cast<lv_coord_t>(area.x2 - 14),
                         static_cast<lv_coord_t>(area.y1 + 84 + level * 50)};
        drawRect(layer, row, level % 2 == 0 ? lv_color_hex(0x1B2022)
                                            : headerColor_, 3);
        lv_area_t name = row;
        name.x1 += 10;
        name.x2 = name.x1 + 60;
        drawText(layer, DIFFICULTY_LABELS[level], name, textColor_,
                 strongBodyFont_, LV_TEXT_ALIGN_LEFT);
        char best[16] = "---";
        if (stats.bestTimesMs[0] != 0) {
            formatTime(stats.bestTimesMs[0], best, sizeof(best), true);
        }
        char detail[80];
        std::snprintf(detail, sizeof(detail),
                      "BEST %s   %lu/%lu   STREAK %u",
                      best, static_cast<unsigned long>(stats.wins),
                      static_cast<unsigned long>(stats.played),
                      static_cast<unsigned>(stats.bestStreak));
        lv_area_t details = row;
        details.x1 += 78;
        details.x2 -= 8;
        drawText(layer, detail, details, mutedColor_, tinyFont_,
                 LV_TEXT_ALIGN_LEFT);
    }
    const auto& data = profile_->records().data();
    char custom[64];
    std::snprintf(custom, sizeof(custom), "自定义   %lu / %lu",
                  static_cast<unsigned long>(data.customWins),
                  static_cast<unsigned long>(data.customPlayed));
    lv_area_t footer = area;
    footer.y1 = area.y2 - 27;
    footer.y2 = area.y2 - 7;
    drawText(layer, custom, footer, mutedColor_, bodyFont_);
}

void MinesweeperApp::drawGame(lv_layer_t* layer, const lv_area_t& area,
                              uint32_t nowMs) const {
    drawHud(layer, area, nowMs);
    drawBoard(layer, area, layer->_clip_area, nowMs);
    drawOverlay(layer, area, nowMs);
}

void MinesweeperApp::drawHud(lv_layer_t* layer, const lv_area_t& area,
                             uint32_t nowMs) const {
    lv_area_t header = area;
    header.y2 = header.y1 + 33;
    drawRect(layer, header, headerColor_);
    lv_area_t leftDisplay = {static_cast<lv_coord_t>(area.x1 + 8),
                             static_cast<lv_coord_t>(area.y1 + 4),
                             static_cast<lv_coord_t>(area.x1 + 63),
                             static_cast<lv_coord_t>(area.y1 + 29)};
    lv_area_t rightDisplay = {static_cast<lv_coord_t>(area.x2 - 63),
                              static_cast<lv_coord_t>(area.y1 + 4),
                              static_cast<lv_coord_t>(area.x2 - 8),
                              static_cast<lv_coord_t>(area.y1 + 29)};
    drawRect(layer, leftDisplay, frameColor_, 3);
    drawRect(layer, rightDisplay, frameColor_, 3);
    const int16_t remaining = static_cast<int16_t>(engine_.config().mines) -
                              static_cast<int16_t>(engine_.flagCount());
    drawCounter(layer, leftDisplay.x1 + 8, leftDisplay.y1 + 4, remaining,
                dangerColor_);
    drawCounter(layer, rightDisplay.x1 + 8, rightDisplay.y1 + 4,
                static_cast<int16_t>(elapsedMs(nowMs) / 1000U),
                accentColor_);

    lv_area_t status = {static_cast<lv_coord_t>(area.x1 + 145),
                        static_cast<lv_coord_t>(area.y1 + 4),
                        static_cast<lv_coord_t>(area.x1 + 174),
                        static_cast<lv_coord_t>(area.y1 + 29)};
    drawRect(layer, status, frameColor_, 4);
    status = inset(status, 3);
    const lv_color_t stateColor =
        phase_ == ViewPhase::Lost ? dangerColor_
        : phase_ == ViewPhase::Won ? successColor_
                                   : accentColor_;
    drawMine(layer, status, stateColor, phase_ == ViewPhase::Lost);
}

void MinesweeperApp::drawBoard(lv_layer_t* layer, const lv_area_t& area,
                               const lv_area_t& clip, uint32_t nowMs) const {
    const pgos::MinesweeperConfig config = engine_.config();
    lv_area_t frame = {
        static_cast<lv_coord_t>(area.x1 + boardX_ - 2),
        static_cast<lv_coord_t>(area.y1 + boardY_ - 2),
        static_cast<lv_coord_t>(area.x1 + boardX_ + boardWidth_ + 1),
        static_cast<lv_coord_t>(area.y1 + boardY_ + boardHeight_ + 1),
    };
    if (areasIntersect(frame, clip)) {
        drawRect(layer, frame, frameColor_, 2);
    }
    for (uint8_t y = 0; y < config.height; ++y) {
        for (uint8_t x = 0; x < config.width; ++x) {
            const uint16_t index = static_cast<uint16_t>(y) * config.width + x;
            lv_area_t tile = {
                static_cast<lv_coord_t>(area.x1 + boardX_ + x * cellSize_),
                static_cast<lv_coord_t>(area.y1 + boardY_ + y * cellSize_),
                static_cast<lv_coord_t>(area.x1 + boardX_ +
                                        (x + 1) * cellSize_ - 1),
                static_cast<lv_coord_t>(area.y1 + boardY_ +
                                        (y + 1) * cellSize_ - 1),
            };
            if (!areasIntersect(tile, clip)) {
                continue;
            }
            drawTile(layer, tile, engine_.cell(x, y), index, nowMs);
            if (x == cursorX_ && y == cursorY_ &&
                phase_ != ViewPhase::Won && phase_ != ViewPhase::Lost) {
                drawCursor(layer, tile, nowMs);
            }
        }
    }
}

void MinesweeperApp::drawTile(lv_layer_t* layer, const lv_area_t& tile,
                              const pgos::MinesweeperCell& cell,
                              uint16_t index, uint32_t nowMs) const {
    const bool lost = phase_ == ViewPhase::Lost;
    const bool won = phase_ == ViewPhase::Won;
    const uint32_t resultAge = resultStartedMs_ == 0
                                   ? 0
                                   : nowMs - resultStartedMs_;
    const uint8_t x = static_cast<uint8_t>(index % engine_.config().width);
    const uint8_t y = static_cast<uint8_t>(index / engine_.config().width);
    const uint32_t resultWave = std::min<uint32_t>(
        REVEAL_MAX_DELAY_MS,
        (std::abs(static_cast<int16_t>(x) - cursorX_) +
         std::abs(static_cast<int16_t>(y) - cursorY_)) * REVEAL_STEP_MS);

    bool showMine = false;
    bool showAutoFlag = false;
    bool wrongFlag = false;
    if (lost) {
        showMine = cell.mine &&
                   cell.visibility != pgos::MinesweeperVisibility::Flagged &&
                   resultAge >= resultWave;
        wrongFlag = cell.visibility == pgos::MinesweeperVisibility::Flagged &&
                    !cell.mine && resultAge >= 260U;
    } else if (won && cell.mine) {
        showAutoFlag = resultAge >= resultWave;
    }

    bool revealed = cell.visibility == pgos::MinesweeperVisibility::Revealed;
    if (lost && cell.mine && !cell.detonated) {
        revealed = showMine;
    }
    if (revealed && revealAtMs_[index] != 0 &&
        static_cast<int32_t>(nowMs - revealAtMs_[index]) < 0) {
        revealed = false;
    }
    if (showMine) {
        revealed = true;
    }

    if (revealed) {
        drawRect(layer, tile, revealedGridColor_);
        lv_area_t inner = tile;
        inner.x2 -= 1;
        inner.y2 -= 1;
        drawRect(layer, inner,
                 cell.detonated ? dangerColor_ : revealedColor_);
        if (showMine || cell.mine) {
            drawMine(layer, inset(inner, 1),
                     cell.detonated ? textColor_ : frameColor_,
                     cell.detonated);
        } else if (cell.adjacentMines != 0) {
            drawNumber(layer, inner, cell.adjacentMines);
        }
        return;
    }

    const bool pressed = index == static_cast<uint16_t>(lastPressIndex_) &&
                         nowMs - lastPressStartedMs_ < 60U;
    drawRect(layer, tile, pressed ? coveredShadowColor_ : coveredColor_);
    const int16_t bevel = cellSize_ >= 15 ? 2 : 1;
    if (!pressed) {
        lv_area_t top = tile;
        top.y2 = top.y1 + bevel - 1;
        drawRect(layer, top, coveredHighlightColor_);
        lv_area_t left = tile;
        left.x2 = left.x1 + bevel - 1;
        drawRect(layer, left, coveredHighlightColor_);
        lv_area_t bottom = tile;
        bottom.y1 = bottom.y2 - bevel + 1;
        drawRect(layer, bottom, coveredShadowColor_);
        lv_area_t right = tile;
        right.x1 = right.x2 - bevel + 1;
        drawRect(layer, right, coveredShadowColor_);
    }

    if (wrongFlag) {
        drawFlag(layer, inset(tile, 1), FLAG_ANIMATION_MS, true);
    } else if (cell.visibility == pgos::MinesweeperVisibility::Flagged ||
               showAutoFlag) {
        const uint32_t progress =
            index == static_cast<uint16_t>(lastFlagIndex_)
                ? nowMs - lastFlagStartedMs_
                : FLAG_ANIMATION_MS;
        drawFlag(layer, inset(tile, 1), progress, false);
    }
}

void MinesweeperApp::drawOverlay(lv_layer_t* layer, const lv_area_t& area,
                                 uint32_t nowMs) const {
    if (phase_ == ViewPhase::Generating) {
        lv_area_t panel = {static_cast<lv_coord_t>(area.x1 + 88),
                           static_cast<lv_coord_t>(area.y1 + 88),
                           static_cast<lv_coord_t>(area.x2 - 88),
                           static_cast<lv_coord_t>(area.y1 + 132)};
        drawRect(layer, panel, frameColor_, 5, LV_OPA_90);
        char text[32];
        std::snprintf(text, sizeof(text), "LOGIC CHECK  %lu",
                      static_cast<unsigned long>(engine_.generationAttempts()));
        drawText(layer, text, panel, accentColor_, hudFont_);
        return;
    }
    if (phase_ == ViewPhase::Paused) {
        lv_area_t shade = area;
        shade.y1 += 34;
        drawRect(layer, shade, frameColor_, 0, LV_OPA_70);
        lv_area_t title = shade;
        title.y1 += 63;
        title.y2 = title.y1 + 30;
        drawText(layer, "PAUSED", title, textColor_, titleFont_);
        lv_area_t subtitle = title;
        subtitle.y1 += 33;
        subtitle.y2 = subtitle.y1 + 18;
        drawText(layer, "游戏已暂停", subtitle, mutedColor_, bodyFont_);
        return;
    }
    if ((phase_ == ViewPhase::Ready || phase_ == ViewPhase::Playing) &&
        assistFeedback_ != AssistFeedback::None) {
        const lv_area_t panel = assistFeedbackArea();
        const lv_color_t color =
            assistFeedback_ == AssistFeedback::FlaggedMines ? accentColor_
            : assistFeedback_ == AssistFeedback::RevealedSafe ? successColor_
                                                             : mutedColor_;
        drawRect(layer, panel, frameColor_, 3, static_cast<lv_opa_t>(242));
        const lv_area_t inner = inset(panel, 2);
        drawRect(layer, inner, headerColor_, 2, static_cast<lv_opa_t>(248));
        char message[32];
        switch (assistFeedback_) {
            case AssistFeedback::FlaggedMines:
                std::snprintf(message, sizeof(message), "AUTO FLAG +%u",
                              static_cast<unsigned>(assistFeedbackCount_));
                break;
            case AssistFeedback::RevealedSafe:
                std::snprintf(message, sizeof(message), "SAFE OPEN +%u",
                              static_cast<unsigned>(assistFeedbackCount_));
                break;
            case AssistFeedback::NoCertainMove:
                std::snprintf(message, sizeof(message), "NO CERTAIN MOVE");
                break;
            case AssistFeedback::None:
                message[0] = '\0';
                break;
        }
        drawText(layer, message, inner, color, hudFont_);
    }
    if ((phase_ != ViewPhase::Won && phase_ != ViewPhase::Lost) ||
        resultStartedMs_ == 0 ||
        nowMs - resultStartedMs_ < RESULT_PANEL_DELAY_MS) {
        return;
    }

    lv_area_t panel = {static_cast<lv_coord_t>(area.x1 + 22),
                       static_cast<lv_coord_t>(area.y1 + 56),
                       static_cast<lv_coord_t>(area.x2 - 22),
                       static_cast<lv_coord_t>(area.y2 - 12)};
    drawRect(layer, panel, frameColor_, 7, static_cast<lv_opa_t>(235));
    panel = inset(panel, 2);
    drawRect(layer, panel, headerColor_, 5, static_cast<lv_opa_t>(242));

    lv_area_t title = panel;
    title.y1 += 9;
    title.y2 = title.y1 + 28;
    drawText(layer, phase_ == ViewPhase::Won ? "FIELD CLEARED"
                                             : "MINE TRIGGERED",
             title, phase_ == ViewPhase::Won ? successColor_ : dangerColor_,
             titleFont_);
    char result[64];
    char time[16];
    formatTime(resultElapsedMs_, time, sizeof(time), true);
    if (phase_ == ViewPhase::Won && recordUpdate_.newBest) {
        std::snprintf(result, sizeof(result), "%s 秒 / NEW BEST", time);
    } else if (phase_ == ViewPhase::Won && recordUpdate_.rank != 0xFF) {
        std::snprintf(result, sizeof(result), "%s 秒 / TOP %u", time,
                      static_cast<unsigned>(recordUpdate_.rank + 1U));
    } else {
        std::snprintf(result, sizeof(result), "%s 秒", time);
    }
    lv_area_t detail = title;
    detail.y1 += 30;
    detail.y2 = detail.y1 + 18;
    drawText(layer, result, detail, textColor_, bodyFont_);

    const char* actions[] = {"再来一局", "难度"};
    for (uint8_t index = 0; index < 2; ++index) {
        const int16_t x = panel.x1 + 16 + index * 123;
        lv_area_t button = {static_cast<lv_coord_t>(x),
                            static_cast<lv_coord_t>(panel.y2 - 43),
                            static_cast<lv_coord_t>(x + 118),
                            static_cast<lv_coord_t>(panel.y2 - 10)};
        const bool selected = index == resultSelection_;
        drawRect(layer, button, selected ? accentColor_ : frameColor_, 4);
        button = inset(button, 2);
        drawRect(layer, button, headerColor_, 3);
        drawText(layer, actions[index], button,
                 selected ? accentColor_ : textColor_, strongBodyFont_);
    }
}

void MinesweeperApp::drawMine(lv_layer_t* layer, const lv_area_t& area,
                              lv_color_t color, bool detonated) const {
    const int16_t width = area.x2 - area.x1 + 1;
    const int16_t height = area.y2 - area.y1 + 1;
    const int16_t size = std::max<int16_t>(5, std::min(width, height));
    const int16_t unit = std::max<int16_t>(1, size / 9);
    const int16_t cx = (area.x1 + area.x2) / 2;
    const int16_t cy = (area.y1 + area.y2) / 2;
    lv_area_t horizontal = {static_cast<lv_coord_t>(cx - 4 * unit),
                            static_cast<lv_coord_t>(cy - unit / 2),
                            static_cast<lv_coord_t>(cx + 4 * unit),
                            static_cast<lv_coord_t>(cy + unit / 2)};
    lv_area_t vertical = {static_cast<lv_coord_t>(cx - unit / 2),
                          static_cast<lv_coord_t>(cy - 4 * unit),
                          static_cast<lv_coord_t>(cx + unit / 2),
                          static_cast<lv_coord_t>(cy + 4 * unit)};
    drawRect(layer, horizontal, color);
    drawRect(layer, vertical, color);
    lv_area_t core = {static_cast<lv_coord_t>(cx - 3 * unit),
                      static_cast<lv_coord_t>(cy - 3 * unit),
                      static_cast<lv_coord_t>(cx + 3 * unit),
                      static_cast<lv_coord_t>(cy + 3 * unit)};
    drawRect(layer, core, color, LV_RADIUS_CIRCLE);
    if (detonated && size >= 9) {
        lv_area_t flash = {static_cast<lv_coord_t>(cx - unit),
                           static_cast<lv_coord_t>(cy - unit),
                           static_cast<lv_coord_t>(cx + unit),
                           static_cast<lv_coord_t>(cy + unit)};
        drawRect(layer, flash, textColor_, LV_RADIUS_CIRCLE);
    }
}

void MinesweeperApp::drawFlag(lv_layer_t* layer, const lv_area_t& area,
                              uint32_t progressMs, bool wrong) const {
    const int16_t width = area.x2 - area.x1 + 1;
    const int16_t height = area.y2 - area.y1 + 1;
    const int16_t unit = std::max<int16_t>(1, std::min(width, height) / 9);
    const int16_t cx = (area.x1 + area.x2) / 2;
    const int16_t top = area.y1 + unit;
    const int16_t bottom = area.y2 - unit;
    const uint32_t clamped = std::min<uint32_t>(progressMs,
                                                FLAG_ANIMATION_MS);
    const int16_t poleHeight = static_cast<int16_t>(
        (bottom - top) * std::min<uint32_t>(clamped * 2U,
                                            FLAG_ANIMATION_MS) /
        FLAG_ANIMATION_MS);
    lv_area_t pole = {static_cast<lv_coord_t>(cx),
                      static_cast<lv_coord_t>(bottom - poleHeight),
                      static_cast<lv_coord_t>(cx + unit - 1),
                      static_cast<lv_coord_t>(bottom)};
    drawRect(layer, pole, frameColor_);
    lv_area_t base = {static_cast<lv_coord_t>(cx - 3 * unit),
                      static_cast<lv_coord_t>(bottom - unit + 1),
                      static_cast<lv_coord_t>(cx + 3 * unit),
                      static_cast<lv_coord_t>(bottom)};
    drawRect(layer, base, frameColor_);
    if (clamped >= FLAG_ANIMATION_MS / 3U) {
        const int16_t clothWidth = static_cast<int16_t>(
            4 * unit * (clamped - FLAG_ANIMATION_MS / 3U) /
            (FLAG_ANIMATION_MS - FLAG_ANIMATION_MS / 3U));
        lv_area_t cloth = {static_cast<lv_coord_t>(cx - clothWidth),
                           static_cast<lv_coord_t>(top),
                           static_cast<lv_coord_t>(cx),
                           static_cast<lv_coord_t>(top + 3 * unit)};
        drawRect(layer, cloth, wrong ? dangerColor_ : lv_color_hex(0xE84C4C));
    }
    if (wrong) {
        const int16_t thickness = std::max<int16_t>(1, unit);
        for (int16_t offset = -3 * unit; offset <= 3 * unit; ++offset) {
            const int16_t x1 = cx + offset;
            const int16_t y1 = (area.y1 + area.y2) / 2 + offset;
            const int16_t y2 = (area.y1 + area.y2) / 2 - offset;
            if (x1 >= area.x1 && x1 <= area.x2 && y1 >= area.y1 &&
                y1 <= area.y2) {
                drawRect(layer, {static_cast<lv_coord_t>(x1),
                                 static_cast<lv_coord_t>(y1),
                                 static_cast<lv_coord_t>(x1 + thickness - 1),
                                 static_cast<lv_coord_t>(y1 + thickness - 1)},
                         dangerColor_);
            }
            if (x1 >= area.x1 && x1 <= area.x2 && y2 >= area.y1 &&
                y2 <= area.y2) {
                drawRect(layer, {static_cast<lv_coord_t>(x1),
                                 static_cast<lv_coord_t>(y2),
                                 static_cast<lv_coord_t>(x1 + thickness - 1),
                                 static_cast<lv_coord_t>(y2 + thickness - 1)},
                         dangerColor_);
            }
        }
    }
}

void MinesweeperApp::drawNumber(lv_layer_t* layer, const lv_area_t& area,
                                uint8_t number) const {
    if (number == 0 || number > 8) {
        return;
    }
    const int16_t width = area.x2 - area.x1 + 1;
    const int16_t height = area.y2 - area.y1 + 1;
    const int16_t scale = width >= 16 && height >= 16 ? 2 : 1;
    const int16_t patternWidth = 5 * scale;
    const int16_t patternHeight = 7 * scale;
    const int16_t startX = area.x1 + (width - patternWidth) / 2;
    const int16_t startY = area.y1 + (height - patternHeight) / 2;
    for (uint8_t row = 0; row < 7; ++row) {
        for (uint8_t column = 0; column < 5; ++column) {
            if ((NUMBER_PIXELS[number - 1U][row] &
                 (1U << (4U - column))) == 0) {
                continue;
            }
            lv_area_t pixel = {
                static_cast<lv_coord_t>(startX + column * scale),
                static_cast<lv_coord_t>(startY + row * scale),
                static_cast<lv_coord_t>(startX + (column + 1) * scale - 1),
                static_cast<lv_coord_t>(startY + (row + 1) * scale - 1),
            };
            drawRect(layer, pixel, numberColors_[number - 1U]);
        }
    }
}

void MinesweeperApp::drawCursor(lv_layer_t* layer, const lv_area_t& tile,
                                uint32_t nowMs) const {
    const bool bright = ((nowMs - cursorMovedMs_) / 180U) % 2U == 0;
    const lv_color_t color = bright ? lv_color_hex(0xFFF0A6)
                                    : lv_color_hex(0xFFD15A);
    const int16_t thickness = cellSize_ >= 15 ? 2 : 1;
    const int16_t left = tile.x1;
    const int16_t right = tile.x2;
    const int16_t top = tile.y1;
    const int16_t bottom = tile.y2;
    // A filled tint and a complete perimeter remain legible on the 10px
    // expert cells, while leaving the center available for the number glyph.
    drawRect(layer, tile, color, 0, LV_OPA_20);
    const lv_area_t edges[] = {
        {static_cast<lv_coord_t>(left), static_cast<lv_coord_t>(top),
         static_cast<lv_coord_t>(right),
         static_cast<lv_coord_t>(top + thickness - 1)},
        {static_cast<lv_coord_t>(left),
         static_cast<lv_coord_t>(bottom - thickness + 1),
         static_cast<lv_coord_t>(right), static_cast<lv_coord_t>(bottom)},
        {static_cast<lv_coord_t>(left), static_cast<lv_coord_t>(top),
         static_cast<lv_coord_t>(left + thickness - 1),
         static_cast<lv_coord_t>(bottom)},
        {static_cast<lv_coord_t>(right - thickness + 1),
         static_cast<lv_coord_t>(top), static_cast<lv_coord_t>(right),
         static_cast<lv_coord_t>(bottom)},
    };
    for (const lv_area_t& edge : edges) {
        drawRect(layer, edge, color);
    }
}

void MinesweeperApp::drawCounter(lv_layer_t* layer, int16_t x, int16_t y,
                                 int16_t value, lv_color_t color) const {
    value = std::clamp<int16_t>(value, -99, 999);
    int8_t digits[3] = {};
    if (value < 0) {
        digits[0] = -1;
        value = static_cast<int16_t>(-value);
        digits[1] = static_cast<int8_t>((value / 10) % 10);
        digits[2] = static_cast<int8_t>(value % 10);
    } else {
        digits[0] = static_cast<int8_t>((value / 100) % 10);
        digits[1] = static_cast<int8_t>((value / 10) % 10);
        digits[2] = static_cast<int8_t>(value % 10);
    }
    for (uint8_t index = 0; index < 3; ++index) {
        drawSevenSegmentDigit(layer, x + index * 14, y, digits[index], color);
    }
}

void MinesweeperApp::drawSevenSegmentDigit(lv_layer_t* layer, int16_t x,
                                           int16_t y, int8_t digit,
                                           lv_color_t color) const {
    const uint8_t segments = digit >= 0 && digit <= 9
                                 ? SEVEN_SEGMENTS[digit]
                                 : digit == -1 ? 0x40U : 0U;
    const lv_area_t areas[7] = {
        {static_cast<lv_coord_t>(x + 2), static_cast<lv_coord_t>(y),
         static_cast<lv_coord_t>(x + 8), static_cast<lv_coord_t>(y + 1)},
        {static_cast<lv_coord_t>(x + 8), static_cast<lv_coord_t>(y + 2),
         static_cast<lv_coord_t>(x + 9), static_cast<lv_coord_t>(y + 7)},
        {static_cast<lv_coord_t>(x + 8), static_cast<lv_coord_t>(y + 9),
         static_cast<lv_coord_t>(x + 9), static_cast<lv_coord_t>(y + 14)},
        {static_cast<lv_coord_t>(x + 2), static_cast<lv_coord_t>(y + 15),
         static_cast<lv_coord_t>(x + 8), static_cast<lv_coord_t>(y + 16)},
        {static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(y + 9),
         static_cast<lv_coord_t>(x + 1), static_cast<lv_coord_t>(y + 14)},
        {static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(y + 2),
         static_cast<lv_coord_t>(x + 1), static_cast<lv_coord_t>(y + 7)},
        {static_cast<lv_coord_t>(x + 2), static_cast<lv_coord_t>(y + 8),
         static_cast<lv_coord_t>(x + 8), static_cast<lv_coord_t>(y + 9)},
    };
    for (uint8_t index = 0; index < 7; ++index) {
        if ((segments & (1U << index)) != 0) {
            drawRect(layer, areas[index], color, 1);
        }
    }
}

void MinesweeperApp::selectDifficulty(int8_t direction) {
    difficultySelection_ = static_cast<uint8_t>(
        (difficultySelection_ + 5 + direction) % 5);
    if (difficultySelection_ < 4) {
        difficulty_ =
            static_cast<pgos::MinesweeperDifficulty>(difficultySelection_);
    }
    invalidate();
}

void MinesweeperApp::adjustCustom(int8_t direction) {
    if (customSelection_ == 0) {
        customWidth_ = static_cast<uint8_t>(
            std::clamp<int16_t>(customWidth_ + direction, 8, 30));
    } else if (customSelection_ == 1) {
        customHeight_ = static_cast<uint8_t>(
            std::clamp<int16_t>(customHeight_ + direction, 8, 16));
    } else if (customSelection_ == 2) {
        customMines_ = static_cast<uint16_t>(std::clamp<int32_t>(
            static_cast<int32_t>(customMines_) + direction, 1,
            maximumCustomMines()));
    }
    customMines_ = std::min<uint16_t>(customMines_, maximumCustomMines());
    invalidate();
}

void MinesweeperApp::startSelectedDifficulty(AppContext& context) {
    if (difficultySelection_ == 4) {
        phase_ = ViewPhase::Statistics;
        invalidate();
        return;
    }
    difficulty_ =
        static_cast<pgos::MinesweeperDifficulty>(difficultySelection_);
    if (difficulty_ == pgos::MinesweeperDifficulty::Custom) {
        customSelection_ = 0;
        phase_ = ViewPhase::CustomSetup;
        invalidate();
        return;
    }
    startNewBoard(context, true);
}

void MinesweeperApp::startNewBoard(AppContext& context, bool newSeed) {
    if (newSeed || boardSeed_ == 0) {
        boardSeed_ ^= static_cast<uint32_t>(micros()) + 0x9E3779B9U +
                      (boardSeed_ << 6U) + (boardSeed_ >> 2U);
        if (boardSeed_ == 0) {
            boardSeed_ = 0x4D494E45U;
        }
    }
    const pgos::MinesweeperConfig config = selectedConfig();
    if (!engine_.reset(config, boardSeed_)) {
        phase_ = ViewPhase::Title;
        return;
    }
    context.minesweeperProfile.saveSetup(difficulty_, customWidth_,
                                         customHeight_, customMines_);
    cursorX_ = config.width / 2U;
    cursorY_ = config.height / 2U;
    firstX_ = cursorX_;
    firstY_ = cursorY_;
    phase_ = ViewPhase::Ready;
    resultSelection_ = 0;
    resultRecorded_ = false;
    recordUpdate_ = {};
    gameStartedMs_ = 0;
    pausedAccumulatedMs_ = 0;
    resultElapsedMs_ = 0;
    resultStartedMs_ = 0;
    cursorMovedMs_ = millis();
    updateBoardLayout();
    clearVisualState();
    invalidate();
}

void MinesweeperApp::beginFirstReveal(uint32_t nowMs) {
    firstX_ = cursorX_;
    firstY_ = cursorY_;
    if (!engine_.beginGeneration(firstX_, firstY_)) {
        return;
    }
    phase_ = ViewPhase::Generating;
    generationStartedMs_ = nowMs;
    lastPressIndex_ = currentCellIndex();
    lastPressStartedMs_ = nowMs;
    animationsUntilMs_ = nowMs + 180U;
    invalidate();
}

void MinesweeperApp::performReveal(uint32_t nowMs, AppContext& context) {
    if (phase_ == ViewPhase::Ready) {
        beginFirstReveal(nowMs);
        return;
    }
    if (phase_ != ViewPhase::Playing) {
        return;
    }
    lastPressIndex_ = currentCellIndex();
    lastPressStartedMs_ = nowMs;
    const bool chordAttempt =
        engine_.cell(cursorX_, cursorY_).visibility ==
        pgos::MinesweeperVisibility::Revealed;
    const pgos::MinesweeperActionResult result =
        engine_.reveal(cursorX_, cursorY_);
    if (!result.accepted) {
        animationsUntilMs_ = nowMs + 70U;
        invalidate();
        return;
    }
    if (result.revealed != 0) {
        // Show a successful chord almost immediately. Covered-cell reveals
        // keep a very short anticipation beat while retaining the wave.
        scheduleNewReveals(cursorX_, cursorY_,
                           nowMs + (chordAttempt ? 0U : 12U));
        context.audio.playGameTone(result.revealed > 1 ? 660U : 520U,
                                   result.revealed > 1 ? 74U : 42U);
    }
    if (result.hitMine || result.won) {
        processOutcome(nowMs, context);
    }
    invalidate();
}

void MinesweeperApp::handleActivate(uint32_t nowMs, AppContext& context) {
    if (phase_ == ViewPhase::Title) {
        startSelectedDifficulty(context);
    } else if (phase_ == ViewPhase::CustomSetup) {
        if (customSelection_ == 3) {
            difficulty_ = pgos::MinesweeperDifficulty::Custom;
            startNewBoard(context, true);
        } else {
            adjustCustom(1);
        }
    } else if (phase_ == ViewPhase::Statistics) {
        phase_ = ViewPhase::Title;
        invalidate();
    } else if (phase_ == ViewPhase::Ready ||
               phase_ == ViewPhase::Playing) {
        performReveal(nowMs, context);
    } else if (phase_ == ViewPhase::Paused) {
        resume(nowMs);
    } else if (phase_ == ViewPhase::Won || phase_ == ViewPhase::Lost) {
        if (nowMs - resultStartedMs_ >= RESULT_PANEL_DELAY_MS) {
            activateResultChoice(context);
        }
    }
}

void MinesweeperApp::handleActivatePress(uint32_t nowMs,
                                          AppContext& context) {
    const bool doubleTap =
        phase_ == ViewPhase::Playing && lastActivateTapMs_ != 0 &&
        static_cast<int32_t>(nowMs - lastActivateTapMs_) <=
            static_cast<int32_t>(DOUBLE_TAP_WINDOW_MS);
    if (doubleTap) {
        lastActivateTapMs_ = 0;
        performKnownSafeReveal(nowMs, context);
        return;
    }

    lastActivateTapMs_ = phase_ == ViewPhase::Playing ? nowMs : 0;
    handleActivate(nowMs, context);
    if (phase_ != ViewPhase::Playing) {
        lastActivateTapMs_ = 0;
    }
}

void MinesweeperApp::handleFlagPress(uint32_t nowMs, AppContext& context) {
    if (phase_ == ViewPhase::Title) {
        lastFlagTapMs_ = 0;
        pendingFlagTapChanged_ = false;
        pendingFlagTapIndex_ = -1;
        phase_ = ViewPhase::Statistics;
        invalidate();
        return;
    }
    if (phase_ == ViewPhase::Won || phase_ == ViewPhase::Lost) {
        lastFlagTapMs_ = 0;
        pendingFlagTapChanged_ = false;
        pendingFlagTapIndex_ = -1;
        if (resultStartedMs_ != 0 &&
            nowMs - resultStartedMs_ >= RESULT_PANEL_DELAY_MS) {
            phase_ = ViewPhase::Statistics;
            invalidate();
        }
        return;
    }
    if (phase_ != ViewPhase::Ready && phase_ != ViewPhase::Playing) {
        return;
    }

    const bool doubleTap =
        phase_ == ViewPhase::Playing && lastFlagTapMs_ != 0 &&
        static_cast<int32_t>(nowMs - lastFlagTapMs_) <=
            static_cast<int32_t>(DOUBLE_TAP_WINDOW_MS);
    if (doubleTap) {
        lastFlagTapMs_ = 0;
        restorePendingFlagTap();
        performKnownMineFlags(nowMs, context);
        return;
    }

    lastFlagTapMs_ = phase_ == ViewPhase::Playing ? nowMs : 0;
    pendingFlagTapChanged_ = false;
    pendingFlagTapIndex_ = -1;
    const uint16_t index = currentCellIndex();
    const pgos::MinesweeperVisibility before =
        engine_.cell(cursorX_, cursorY_).visibility;
    performFlag(nowMs, context);
    if (phase_ == ViewPhase::Playing &&
        engine_.cell(cursorX_, cursorY_).visibility != before) {
        pendingFlagTapChanged_ = true;
        pendingFlagTapIndex_ = static_cast<int16_t>(index);
        pendingFlagTapPreviousVisibility_ = before;
    }
}

void MinesweeperApp::sampleGamepadButtons(uint32_t nowMs,
                                          AppContext& context) {
    const GamepadSnapshot& gamepad = context.gamepad.snapshot();
    if (!gamepad.connected) {
        sampledGamepadButtons_ = 0;
        gamepadButtonsInitialized_ = false;
        activateCommandHandled_ = false;
        flagCommandHandled_ = false;
        return;
    }

    const uint16_t buttons = gamepad.buttons;
    const bool activatePressed = (buttons & GamepadButtonA) != 0;
    const bool flagPressed = (buttons & GamepadButtonX) != 0;
    if (!gamepadButtonsInitialized_) {
        // Do not let the button that entered Minesweeper also alter a board.
        sampledGamepadButtons_ = buttons;
        gamepadButtonsInitialized_ = true;
        if (!activatePressed) {
            activateCommandHandled_ = false;
        }
        if (!flagPressed) {
            flagCommandHandled_ = false;
        }
        return;
    }

    const bool activateWasPressed =
        (sampledGamepadButtons_ & GamepadButtonA) != 0;
    const bool flagWasPressed =
        (sampledGamepadButtons_ & GamepadButtonX) != 0;
    sampledGamepadButtons_ = buttons;
    if (!activatePressed) {
        activateCommandHandled_ = false;
    }
    if (!flagPressed) {
        flagCommandHandled_ = false;
    }

    // The normal path is the latched BLE ButtonDown event routed through the
    // kernel. If an edge was dropped at a queue boundary, current-state
    // sampling gives the corresponding action exactly one fallback chance.
    const bool activateCommandAlreadyHandled =
        activateCommandHandled_ &&
        static_cast<int32_t>(nowMs - lastActivateCommandMs_) <= 25;
    if (activatePressed && !activateWasPressed &&
        !activateCommandAlreadyHandled) {
        activateCommandHandled_ = true;
        lastActivateCommandMs_ = nowMs;
        handleActivatePress(nowMs, context);
    }

    const bool flagCommandAlreadyHandled =
        flagCommandHandled_ &&
        static_cast<int32_t>(nowMs - lastFlagCommandMs_) <= 25;
    if (flagPressed && !flagWasPressed && !flagCommandAlreadyHandled) {
        flagCommandHandled_ = true;
        lastFlagCommandMs_ = nowMs;
        handleFlagPress(nowMs, context);
    }
}

void MinesweeperApp::performFlag(uint32_t nowMs, AppContext& context) {
    const pgos::MinesweeperActionResult result =
        engine_.toggleFlag(cursorX_, cursorY_);
    if (!result.accepted) {
        return;
    }
    lastFlagIndex_ = currentCellIndex();
    lastFlagStartedMs_ = nowMs;
    animationsUntilMs_ = nowMs + FLAG_ANIMATION_MS;
    context.audio.playGameTone(920U, 36U);
    if (context.gamepad.snapshot().connected) {
        context.gamepad.requestRumble(28, 28, 48);
    }
    invalidate();
}

void MinesweeperApp::performKnownMineFlags(uint32_t nowMs,
                                            AppContext& context) {
    const pgos::MinesweeperActionResult result = engine_.flagKnownMines();
    if (!result.accepted) {
        context.audio.playGameTone(260U, 26U);
        showAssistFeedback(AssistFeedback::NoCertainMove, 0, nowMs);
        return;
    }

    lastFlagIndex_ = -1;
    lastFlagStartedMs_ = 0;
    context.audio.playGameTone(1040U, 62U);
    if (context.gamepad.snapshot().connected) {
        context.gamepad.requestRumble(64, 44, 72);
    }
    context.rgb.flashFeedback(255, 185, 45, 220);
    showAssistFeedback(AssistFeedback::FlaggedMines, result.flagged, nowMs);
    invalidate();
}

void MinesweeperApp::performKnownSafeReveal(uint32_t nowMs,
                                             AppContext& context) {
    const pgos::MinesweeperActionResult result = engine_.revealKnownSafe();
    if (!result.accepted || (result.revealed == 0 && !result.hitMine)) {
        context.audio.playGameTone(260U, 26U);
        showAssistFeedback(AssistFeedback::NoCertainMove, 0, nowMs);
        return;
    }

    if (result.revealed != 0) {
        scheduleNewReveals(cursorX_, cursorY_, nowMs);
    }
    if (result.hitMine || result.won) {
        processOutcome(nowMs, context);
    } else {
        context.audio.playGameTone(720U, 70U);
        if (context.gamepad.snapshot().connected) {
            context.gamepad.requestRumble(48, 24, 48);
        }
        context.rgb.flashFeedback(48, 166, 204, 180);
        showAssistFeedback(AssistFeedback::RevealedSafe, result.revealed,
                           nowMs);
    }
    invalidate();
}

void MinesweeperApp::handleDirection(Direction direction, uint32_t nowMs,
                                     AppContext& context, bool discrete) {
    if (direction == Direction::None) {
        return;
    }
    if (discrete) {
        lastDiscreteNavigationMs_ = nowMs;
    }
    if (phase_ == ViewPhase::Title) {
        selectDifficulty(direction == Direction::Left ||
                                 direction == Direction::Up
                             ? -1
                             : 1);
    } else if (phase_ == ViewPhase::CustomSetup) {
        if (direction == Direction::Up || direction == Direction::Down) {
            customSelection_ = static_cast<uint8_t>(
                (customSelection_ + 4 +
                 (direction == Direction::Up ? -1 : 1)) % 4);
            invalidate();
        } else {
            adjustCustom(direction == Direction::Left ? -1 : 1);
        }
    } else if (phase_ == ViewPhase::Ready ||
               phase_ == ViewPhase::Playing) {
        moveCursor(direction, nowMs);
    } else if (phase_ == ViewPhase::Won || phase_ == ViewPhase::Lost) {
        if (resultStartedMs_ == 0 ||
            nowMs - resultStartedMs_ < RESULT_PANEL_DELAY_MS) {
            return;
        }
        resultSelection_ =
            static_cast<uint8_t>((resultSelection_ + 1U) % 2U);
        invalidate();
    }
    (void)context;
}

void MinesweeperApp::sampleHeldDirection(uint32_t nowMs,
                                         AppContext& context) {
    const pgos::MinesweeperNavigationVector vector =
        navigationVectorFromGamepad(context.gamepad.snapshot());
    const int8_t signX = vector.x < 0 ? -1 : vector.x > 0 ? 1 : 0;
    const int8_t signY = vector.y < 0 ? -1 : vector.y > 0 ? 1 : 0;

    if (phase_ == ViewPhase::Ready || phase_ == ViewPhase::Playing) {
        if (signX == 0 && signY == 0) {
            resetHeldNavigation();
            return;
        }

        const bool firstSample = lastNavigationSampleMs_ == 0;
        const uint32_t elapsedMs = firstSample
            ? 0
            : std::min<uint32_t>(100U, nowMs - lastNavigationSampleMs_);
        lastNavigationSampleMs_ = nowMs;

        const bool xChanged = signX != heldAxisX_;
        const bool yChanged = signY != heldAxisY_;
        if (firstSample) {
            heldAxisX_ = signX;
            heldAxisY_ = signY;
            if (pgos::minesweeperHeldDirectionShouldRepeat(
                    nowMs, lastDiscreteNavigationMs_)) {
                moveCursorBy(signX, signY, nowMs);
            }
            return;
        }

        if (xChanged) {
            heldAxisX_ = signX;
            navigationRemainderX_ = 0;
        }
        if (yChanged) {
            heldAxisY_ = signY;
            navigationRemainderY_ = 0;
        }
        if ((xChanged || yChanged) &&
            pgos::minesweeperHeldDirectionShouldRepeat(
                nowMs, lastDiscreteNavigationMs_)) {
            moveCursorBy(xChanged ? signX : 0, yChanged ? signY : 0,
                         nowMs);
        }
        if (!xChanged && signX != 0) {
            integrateAnalogAxis(vector.x, elapsedMs, signX, 0,
                                navigationRemainderX_, nowMs);
        }
        if (!yChanged && signY != 0) {
            integrateAnalogAxis(vector.y, elapsedMs, 0, signY,
                                navigationRemainderY_, nowMs);
        }
        return;
    }

    if (signX == 0 && signY == 0) {
        resetHeldNavigation();
        return;
    }
    if (signX != heldAxisX_ || signY != heldAxisY_) {
        heldAxisX_ = signX;
        heldAxisY_ = signY;
        heldDirectionSinceMs_ = nowMs;
        nextDirectionRepeatMs_ = nowMs + NAVIGATION_INITIAL_REPEAT_MS;
        if (pgos::minesweeperHeldDirectionShouldRepeat(
                nowMs, lastDiscreteNavigationMs_)) {
            applyHeldNavigation(vector, nowMs, context);
        }
        return;
    }
    if (nextDirectionRepeatMs_ != 0 &&
        static_cast<int32_t>(nowMs - nextDirectionRepeatMs_) >= 0) {
        applyHeldNavigation(vector, nowMs, context);
        nextDirectionRepeatMs_ = nowMs +
            (nowMs - heldDirectionSinceMs_ >= NAVIGATION_ACCELERATION_MS
                 ? NAVIGATION_FAST_REPEAT_MS
                 : NAVIGATION_REPEAT_MS);
    }
}

pgos::MinesweeperNavigationVector
MinesweeperApp::navigationVectorFromGamepad(
    const GamepadSnapshot& gamepad) const {
    if (!gamepad.connected) {
        return {};
    }
    pgos::MinesweeperNavigationVector vector =
        pgos::minesweeperAnalogNavigationVector(gamepad.axisX,
                                                 gamepad.axisY,
                                                 ANALOG_THRESHOLD);
    const bool left = (gamepad.dpad & GamepadDpadLeft) != 0;
    const bool right = (gamepad.dpad & GamepadDpadRight) != 0;
    const bool up = (gamepad.dpad & GamepadDpadUp) != 0;
    const bool down = (gamepad.dpad & GamepadDpadDown) != 0;
    if (left != right) {
        vector.x = left ? -pgos::MINESWEEPER_NAVIGATION_FULL_SCALE
                        : pgos::MINESWEEPER_NAVIGATION_FULL_SCALE;
    }
    if (up != down) {
        vector.y = up ? -pgos::MINESWEEPER_NAVIGATION_FULL_SCALE
                      : pgos::MINESWEEPER_NAVIGATION_FULL_SCALE;
    }
    return vector;
}

void MinesweeperApp::applyHeldNavigation(
    const pgos::MinesweeperNavigationVector& vector, uint32_t nowMs,
    AppContext& context) {
    if (phase_ == ViewPhase::Ready || phase_ == ViewPhase::Playing) {
        const int8_t deltaX = vector.x < 0 ? -1 : vector.x > 0 ? 1 : 0;
        const int8_t deltaY = vector.y < 0 ? -1 : vector.y > 0 ? 1 : 0;
        moveCursorBy(deltaX, deltaY, nowMs);
        return;
    }
    const Direction direction = vector.x < 0   ? Direction::Left
                              : vector.x > 0   ? Direction::Right
                              : vector.y < 0   ? Direction::Up
                                               : Direction::Down;
    handleDirection(direction, nowMs, context, false);
}

void MinesweeperApp::resetHeldNavigation() {
    heldAxisX_ = 0;
    heldAxisY_ = 0;
    heldDirectionSinceMs_ = 0;
    nextDirectionRepeatMs_ = 0;
    lastNavigationSampleMs_ = 0;
    navigationRemainderX_ = 0;
    navigationRemainderY_ = 0;
}

void MinesweeperApp::moveCursor(Direction direction, uint32_t nowMs) {
    const int8_t deltaX = direction == Direction::Left  ? -1
                          : direction == Direction::Right ? 1
                                                         : 0;
    const int8_t deltaY = direction == Direction::Up    ? -1
                          : direction == Direction::Down ? 1
                                                         : 0;
    moveCursorBy(deltaX, deltaY, nowMs);
}

void MinesweeperApp::moveCursorBy(int8_t deltaX, int8_t deltaY,
                                   uint32_t nowMs) {
    const pgos::MinesweeperConfig config = engine_.config();
    const uint8_t previousX = cursorX_;
    const uint8_t previousY = cursorY_;
    uint8_t nextX = cursorX_;
    uint8_t nextY = cursorY_;
    if (deltaX < 0 && nextX > 0) --nextX;
    if (deltaX > 0 && nextX + 1U < config.width) ++nextX;
    if (deltaY < 0 && nextY > 0) --nextY;
    if (deltaY > 0 && nextY + 1U < config.height) ++nextY;
    if (nextX == cursorX_ && nextY == cursorY_) {
        return;
    }
    cursorX_ = nextX;
    cursorY_ = nextY;
    cursorMovedMs_ = nowMs;
    invalidateTile(previousX, previousY);
    invalidateTile(cursorX_, cursorY_);
}

void MinesweeperApp::integrateAnalogAxis(int16_t axis, uint32_t elapsedMs,
                                          int8_t deltaX, int8_t deltaY,
                                          int32_t& remainder,
                                          uint32_t nowMs) {
    if (axis == 0 || (deltaX == 0 && deltaY == 0) || elapsedMs == 0) {
        return;
    }
    const int32_t steps = pgos::minesweeperIntegratedNavigationSteps(
        axis, elapsedMs, NAVIGATION_RATE_CELLS_PER_SECOND, remainder);
    for (int32_t index = 0; index < steps; ++index) {
        moveCursorBy(deltaX, deltaY, nowMs);
    }
}

void MinesweeperApp::scheduleNewReveals(uint8_t originX, uint8_t originY,
                                        uint32_t nowMs) {
    const pgos::MinesweeperConfig config = engine_.config();
    uint32_t latest = nowMs + TILE_SETTLE_MS;
    for (uint8_t y = 0; y < config.height; ++y) {
        for (uint8_t x = 0; x < config.width; ++x) {
            const uint16_t index = static_cast<uint16_t>(y) * config.width + x;
            if (revealAtMs_[index] != 0 ||
                engine_.cell(x, y).visibility !=
                    pgos::MinesweeperVisibility::Revealed) {
                continue;
            }
            const uint32_t delay =
                pgos::minesweeperRevealDelay(x, y, originX, originY);
            revealAtMs_[index] = std::max<uint32_t>(1U, nowMs + delay);
            latest = std::max(latest,
                              revealAtMs_[index] + TILE_SETTLE_MS);
        }
    }
    animationsUntilMs_ = std::max(animationsUntilMs_, latest);
}

void MinesweeperApp::processOutcome(uint32_t nowMs, AppContext& context) {
    const bool won = engine_.phase() == pgos::MinesweeperPhase::Won;
    const bool lost = engine_.phase() == pgos::MinesweeperPhase::Lost;
    if (!won && !lost) {
        return;
    }
    resultElapsedMs_ = gameStartedMs_ == 0
                           ? 0
                           : nowMs - gameStartedMs_ - pausedAccumulatedMs_;
    resultStartedMs_ = nowMs;
    animationsUntilMs_ = nowMs + RESULT_PANEL_DELAY_MS + 160U;
    phase_ = won ? ViewPhase::Won : ViewPhase::Lost;
    resultSelection_ = 0;
    if (!resultRecorded_) {
        recordUpdate_ = context.minesweeperProfile.recordResult(
            difficulty_, won, resultElapsedMs_);
        resultRecorded_ = true;
    }
    winCueSecondNote_ = false;
    winCueThirdNote_ = false;
    if (won) {
        context.audio.playGameTone(660U, 80U);
        context.rgb.flashFeedback(40, 190, 120, 520);
        if (context.gamepad.snapshot().connected) {
            context.gamepad.requestRumble(210, 85, 145);
        }
    } else {
        context.audio.playGameTone(180U, 260U);
        context.rgb.flashFeedback(220, 42, 48, 460);
        if (context.gamepad.snapshot().connected) {
            context.gamepad.requestRumble(300, 210, 255);
        }
    }
}

void MinesweeperApp::pause(uint32_t nowMs, bool exitArmed) {
    if (phase_ != ViewPhase::Playing) {
        return;
    }
    pausedFrom_ = phase_;
    phase_ = ViewPhase::Paused;
    pausedStartedMs_ = nowMs;
    exitArmed_ = exitArmed;
    resetHeldNavigation();
    invalidate();
}

void MinesweeperApp::resume(uint32_t nowMs) {
    if (phase_ != ViewPhase::Paused) {
        return;
    }
    pausedAccumulatedMs_ += nowMs - pausedStartedMs_;
    phase_ = pausedFrom_;
    exitArmed_ = false;
    resetHeldNavigation();
    invalidate();
}

void MinesweeperApp::activateResultChoice(AppContext& context) {
    if (resultSelection_ == 0) {
        startNewBoard(context, true);
    } else {
        phase_ = ViewPhase::Title;
        invalidate();
    }
}

void MinesweeperApp::updateBoardLayout() {
    const pgos::MinesweeperBoardLayout layout =
        pgos::minesweeperBoardLayout(engine_.config());
    cellSize_ = layout.cellSize;
    boardWidth_ = layout.width;
    boardHeight_ = layout.height;
    boardX_ = layout.x;
    boardY_ = layout.y;
}

void MinesweeperApp::clearVisualState() {
    std::fill_n(revealAtMs_, MAX_CELLS, 0U);
    lastFlagIndex_ = -1;
    lastPressIndex_ = -1;
    lastFlagStartedMs_ = 0;
    lastPressStartedMs_ = 0;
    animationsUntilMs_ = 0;
    resultStartedMs_ = 0;
    lastActivateTapMs_ = 0;
    lastFlagTapMs_ = 0;
    pendingFlagTapChanged_ = false;
    pendingFlagTapIndex_ = -1;
    assistFeedback_ = AssistFeedback::None;
    assistFeedbackCount_ = 0;
    assistFeedbackStartedMs_ = 0;
    winCueSecondNote_ = false;
    winCueThirdNote_ = false;
}

void MinesweeperApp::showAssistFeedback(AssistFeedback feedback,
                                        uint16_t count, uint32_t nowMs) {
    assistFeedback_ = feedback;
    assistFeedbackCount_ = count;
    assistFeedbackStartedMs_ = nowMs;
    invalidateArea(assistFeedbackArea());
}

bool MinesweeperApp::restorePendingFlagTap() {
    const int16_t index = pendingFlagTapIndex_;
    const bool changed = pendingFlagTapChanged_;
    const pgos::MinesweeperVisibility previous =
        pendingFlagTapPreviousVisibility_;
    pendingFlagTapChanged_ = false;
    pendingFlagTapIndex_ = -1;
    if (!changed || index < 0 || phase_ != ViewPhase::Playing) {
        return false;
    }

    const pgos::MinesweeperConfig config = engine_.config();
    const uint8_t x = static_cast<uint8_t>(index % config.width);
    const uint8_t y = static_cast<uint8_t>(index / config.width);
    const pgos::MinesweeperVisibility expected =
        previous == pgos::MinesweeperVisibility::Flagged
            ? pgos::MinesweeperVisibility::Covered
            : pgos::MinesweeperVisibility::Flagged;
    if (engine_.cell(x, y).visibility != expected) {
        return false;
    }
    const pgos::MinesweeperActionResult result = engine_.toggleFlag(x, y);
    if (!result.accepted) {
        return false;
    }
    if (lastFlagIndex_ == index) {
        lastFlagIndex_ = -1;
        lastFlagStartedMs_ = 0;
    }
    invalidateTile(x, y);
    return true;
}

uint32_t MinesweeperApp::elapsedMs(uint32_t nowMs) const {
    if (gameStartedMs_ == 0) {
        return 0;
    }
    if (phase_ == ViewPhase::Won || phase_ == ViewPhase::Lost) {
        return resultElapsedMs_;
    }
    const uint32_t endpoint = phase_ == ViewPhase::Paused
                                  ? pausedStartedMs_
                                  : nowMs;
    return endpoint - gameStartedMs_ - pausedAccumulatedMs_;
}

uint16_t MinesweeperApp::currentCellIndex() const {
    return static_cast<uint16_t>(cursorY_) * engine_.config().width + cursorX_;
}

pgos::MinesweeperConfig MinesweeperApp::selectedConfig() const {
    switch (difficulty_) {
        case pgos::MinesweeperDifficulty::Beginner:
            return pgos::MinesweeperEngine::BEGINNER;
        case pgos::MinesweeperDifficulty::Intermediate:
            return pgos::MinesweeperEngine::INTERMEDIATE;
        case pgos::MinesweeperDifficulty::Expert:
            return pgos::MinesweeperEngine::EXPERT;
        case pgos::MinesweeperDifficulty::Custom:
        default:
            return {customWidth_, customHeight_, customMines_};
    }
}

uint16_t MinesweeperApp::maximumCustomMines() const {
    const uint16_t cells = static_cast<uint16_t>(customWidth_) * customHeight_;
    return std::max<uint16_t>(1U, std::min<uint16_t>(
        static_cast<uint16_t>(cells - 9U),
        static_cast<uint16_t>(cells * 24U / 100U)));
}

const char* MinesweeperApp::difficultyName(
    pgos::MinesweeperDifficulty difficulty) const {
    const uint8_t index = static_cast<uint8_t>(difficulty);
    return DIFFICULTY_LABELS[index < 4 ? index : 0];
}

lv_area_t MinesweeperApp::surfaceArea() const {
    lv_area_t area = {0, 0, -1, -1};
    if (surface_ != nullptr) {
        lv_obj_get_coords(surface_, &area);
    }
    return area;
}

lv_area_t MinesweeperApp::tileArea(uint8_t x, uint8_t y) const {
    const lv_area_t area = surfaceArea();
    return {
        static_cast<lv_coord_t>(area.x1 + boardX_ + x * cellSize_),
        static_cast<lv_coord_t>(area.y1 + boardY_ + y * cellSize_),
        static_cast<lv_coord_t>(area.x1 + boardX_ +
                                (x + 1) * cellSize_ - 1),
        static_cast<lv_coord_t>(area.y1 + boardY_ +
                                (y + 1) * cellSize_ - 1),
    };
}

lv_area_t MinesweeperApp::assistFeedbackArea() const {
    const lv_area_t area = surfaceArea();
    return {
        static_cast<lv_coord_t>(area.x1 + 71),
        static_cast<lv_coord_t>(area.y1 + 3),
        static_cast<lv_coord_t>(area.x2 - 71),
        static_cast<lv_coord_t>(area.y1 + 30),
    };
}

void MinesweeperApp::invalidateArea(const lv_area_t& area) {
    if (surface_ != nullptr) {
        lv_obj_invalidate_area(surface_, &area);
    }
}

void MinesweeperApp::invalidateTile(uint8_t x, uint8_t y) {
    const pgos::MinesweeperConfig config = engine_.config();
    if (x < config.width && y < config.height) {
        invalidateArea(tileArea(x, y));
    }
}

void MinesweeperApp::invalidateCursor() {
    if (phase_ == ViewPhase::Ready || phase_ == ViewPhase::Playing) {
        invalidateTile(cursorX_, cursorY_);
    }
}

void MinesweeperApp::invalidateClock() {
    const lv_area_t area = surfaceArea();
    const lv_area_t clock = {
        static_cast<lv_coord_t>(area.x1 + SURFACE_WIDTH - 64),
        static_cast<lv_coord_t>(area.y1 + 4),
        static_cast<lv_coord_t>(area.x2 - 8),
        static_cast<lv_coord_t>(area.y1 + 29),
    };
    invalidateArea(clock);
}

void MinesweeperApp::invalidateTitleSpark() {
    const lv_area_t area = surfaceArea();
    const lv_area_t spark = {
        static_cast<lv_coord_t>(area.x1 + 178),
        static_cast<lv_coord_t>(area.y1 + 29),
        static_cast<lv_coord_t>(area.x1 + 183),
        static_cast<lv_coord_t>(area.y1 + 34),
    };
    invalidateArea(spark);
}

void MinesweeperApp::invalidate() {
    if (surface_ != nullptr) {
        lv_obj_invalidate(surface_);
    }
}
