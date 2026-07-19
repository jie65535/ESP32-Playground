#include "ui/UiRuntime.h"

#include "services/DisplayService.h"
#include "services/TimeService.h"

#include <esp_heap_caps.h>

namespace {

constexpr uint32_t STATUS_UPDATE_INTERVAL_MS = 250;

}  // namespace

UiRuntime::UiRuntime(DisplayService& display) : display_(display) {}

bool UiRuntime::begin() {
    if (!display_.ready()) {
        return false;
    }

    lv_init();

    const size_t pixelCount = static_cast<size_t>(DisplayService::SCREEN_WIDTH) *
                              DRAW_BUFFER_LINES;
    const size_t bufferBytes = pixelCount * sizeof(uint16_t);
    drawBuffer_ = static_cast<uint8_t*>(heap_caps_malloc(
        bufferBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    drawBuffer2_ = static_cast<uint8_t*>(heap_caps_malloc(
        bufferBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    if (drawBuffer_ != nullptr && drawBuffer2_ != nullptr) {
        asyncFlush_ = display_.initDma();
    }
    if (!asyncFlush_) {
        if (drawBuffer2_ != nullptr) {
            heap_caps_free(drawBuffer2_);
            drawBuffer2_ = nullptr;
        }
        if (drawBuffer_ != nullptr) {
            heap_caps_free(drawBuffer_);
            drawBuffer_ = nullptr;
        }
        drawBuffer_ = static_cast<uint8_t*>(heap_caps_malloc(
            bufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (drawBuffer_ == nullptr) {
            drawBuffer_ = static_cast<uint8_t*>(malloc(bufferBytes));
        }
    }
    if (drawBuffer_ == nullptr) {
        Serial.println(F("[ui] LVGL draw buffer allocation failed"));
        return false;
    }

    lvDisplay_ = lv_display_create(DisplayService::SCREEN_WIDTH,
                                   DisplayService::SCREEN_HEIGHT);
    if (lvDisplay_ == nullptr) {
        Serial.println(F("[ui] LVGL display allocation failed"));
        return false;
    }
    lv_display_set_color_format(lvDisplay_, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(lvDisplay_, &display_);
    lv_display_set_flush_cb(lvDisplay_, displayFlush);
    display_.setAsyncFlushEnabled(asyncFlush_);
    lv_display_set_buffers(lvDisplay_, drawBuffer_, drawBuffer2_, bufferBytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    if (asyncFlush_) {
        lv_display_set_flush_wait_cb(lvDisplay_, displayFlushWait);
    }

    lv_obj_t* screen = lv_screen_active();
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, background(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    createStatusBar();
    lastTickMs_ = millis();
    ready_ = true;
    return true;
}

void UiRuntime::tick() {
    if (!ready_) {
        return;
    }

    const uint32_t now = millis();
    lv_tick_inc(now - lastTickMs_);
    lastTickMs_ = now;
    lv_timer_handler();
}

void UiRuntime::refreshNow() {
    if (!ready_ || lvDisplay_ == nullptr) {
        return;
    }
    lv_refr_now(lvDisplay_);
}

void UiRuntime::pollDisplayFlush() {
    if (!ready_ || !asyncFlush_ || lvDisplay_ == nullptr) {
        return;
    }
    if (display_.finishDmaIfReady()) {
        lv_display_flush_ready(lvDisplay_);
    }
}

bool UiRuntime::ready() const {
    return ready_;
}

lv_obj_t* UiRuntime::createPageRoot(const char* eyebrow, const char* title,
                                    const char* subtitle) {
    lv_obj_t* root = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, DisplayService::SCREEN_WIDTH,
                    DisplayService::SCREEN_HEIGHT - 22);
    lv_obj_set_pos(root, 0, 22);
    lv_obj_set_style_bg_color(root, background(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(root, 72, 0);

    (void)eyebrow;
    createLabel(root, title, 16, 12, 24, text());
    if (subtitle != nullptr) {
        createLabel(root, subtitle, 17, 43, 12, muted());
    }
    return root;
}

UiCard UiRuntime::createCard(lv_obj_t* parent, int16_t y,
                             const char* symbol, const char* title,
                             const char* subtitle) {
    UiCard card;
    card.root = lv_obj_create(parent);
    lv_obj_remove_style_all(card.root);
    lv_obj_set_size(card.root, card.normalWidth, 40);
    lv_obj_set_pos(card.root, card.normalX, y);
    lv_obj_set_style_radius(card.root, 10, 0);
    lv_obj_set_style_bg_color(card.root, panel(), 0);
    lv_obj_set_style_bg_opa(card.root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card.root, 1, 0);
    lv_obj_set_style_border_color(card.root, panelRaised(), 0);
    lv_obj_set_style_pad_all(card.root, 0, 0);
    lv_obj_clear_flag(card.root, LV_OBJ_FLAG_SCROLLABLE);

    card.icon = lv_obj_create(card.root);
    lv_obj_remove_style_all(card.icon);
    lv_obj_set_size(card.icon, 30, 30);
    lv_obj_set_pos(card.icon, 6, 5);
    lv_obj_set_style_radius(card.icon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(card.icon, panelRaised(), 0);
    lv_obj_set_style_bg_opa(card.icon, LV_OPA_COVER, 0);

    card.iconLabel = lv_label_create(card.icon);
    lv_obj_set_style_text_font(card.iconLabel, font(20), 0);
    lv_obj_set_style_text_color(card.iconLabel, muted(), 0);
    lv_label_set_text(card.iconLabel, symbol);
    lv_obj_center(card.iconLabel);

    card.title = createLabel(card.root, title, 48, 5, 16, text());
    card.subtitle = createLabel(card.root, subtitle, 49, 24, 11, muted());
    return card;
}

void UiRuntime::setCardFocused(UiCard& card, bool focused, bool animated) {
    if (card.root == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(card.root, focused ? panelRaised() : panel(), 0);
    lv_obj_set_style_border_color(card.root, focused ? accent() : panelRaised(), 0);
    lv_obj_set_style_border_width(card.root, focused ? 2 : 1, 0);
    lv_obj_set_style_bg_color(card.icon, focused ? accent() : panelRaised(), 0);
    lv_obj_set_style_text_color(card.iconLabel, focused ? background() : muted(), 0);
    lv_obj_set_style_text_color(card.title, text(), 0);
    lv_obj_set_style_text_color(card.subtitle, focused ? text() : muted(), 0);
    if (animated) {
        animateCard(card, focused);
    } else {
        lv_anim_delete(card.root, setObjX);
        lv_anim_delete(card.root, setObjWidth);
        lv_obj_set_x(card.root, focused ? card.focusedX : card.normalX);
        lv_obj_set_width(card.root,
                         focused ? card.focusedWidth : card.normalWidth);
    }
}

void UiRuntime::centerFocused(lv_obj_t* scrollable, lv_obj_t* focused,
                              bool animated) {
    if (scrollable == nullptr || focused == nullptr) {
        return;
    }

    lv_obj_update_layout(scrollable);
    lv_area_t viewportArea;
    lv_area_t focusedArea;
    lv_obj_get_coords(scrollable, &viewportArea);
    lv_obj_get_coords(focused, &focusedArea);

    const int32_t current = lv_obj_get_scroll_y(scrollable);
    const int32_t viewportCenter =
        (viewportArea.y1 + viewportArea.y2) / 2 + 18;
    const int32_t focusedCenter = (focusedArea.y1 + focusedArea.y2) / 2;
    int32_t target = current + focusedCenter - viewportCenter;
    const int32_t minimum = current - lv_obj_get_scroll_top(scrollable);
    const int32_t maximum = current + lv_obj_get_scroll_bottom(scrollable);
    target = max(minimum, min(target, maximum));

    lv_anim_delete(scrollable, setObjScrollY);
    if (!animated || target == current) {
        lv_obj_scroll_to_y(scrollable, target, LV_ANIM_OFF);
        return;
    }

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, scrollable);
    lv_anim_set_values(&animation, current, target);
    lv_anim_set_duration(&animation, 240);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, setObjScrollY);
    lv_anim_start(&animation);
}

lv_obj_t* UiRuntime::createLabel(lv_obj_t* parent, const char* textValue,
                                 int16_t x, int16_t y, uint16_t fontSize,
                                 lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font(fontSize), 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_pos(label, x, y);
    lv_label_set_text(label, textValue == nullptr ? "" : textValue);
    return label;
}

lv_obj_t* UiRuntime::createValueRow(lv_obj_t* parent, const char* labelText,
                                    const char* valueText, int16_t y,
                                    lv_obj_t** valueLabel) {
    createLabel(parent, labelText, 18, y, 12, muted());
    lv_obj_t* value = createLabel(parent, valueText, 132, y, 14, text());
    if (valueLabel != nullptr) {
        *valueLabel = value;
    }
    return value;
}

void UiRuntime::replacePage(lv_obj_t* page, UiPageTransition transition) {
    if (page == nullptr) {
        return;
    }

    if (previousPage_ != nullptr) {
        lv_obj_delete(previousPage_);
        previousPage_ = nullptr;
    }

    previousPage_ = activePage_;
    activePage_ = page;

    if (previousPage_ == nullptr || transition == UiPageTransition::None) {
        lv_obj_set_x(page, 0);
        if (transition == UiPageTransition::None && previousPage_ != nullptr) {
            lv_obj_delete(previousPage_);
            previousPage_ = nullptr;
        }
        return;
    }

    const bool backward = transition == UiPageTransition::Backward;
    if (backward) {
        // A hierarchical back action reveals the page underneath while the
        // current page exits to the right.  Put the freshly rebuilt parent
        // below the current page instead of sliding it in from the left.
        const int32_t oldIndex = lv_obj_get_index(previousPage_);
        lv_obj_set_x(page, 0);
        lv_obj_move_to_index(page, oldIndex);

        lv_anim_delete(previousPage_, setObjX);
        lv_anim_t popAnimation;
        lv_anim_init(&popAnimation);
        lv_anim_set_var(&popAnimation, previousPage_);
        lv_anim_set_values(&popAnimation, 0, DisplayService::SCREEN_WIDTH);
        lv_anim_set_duration(&popAnimation, 220);
        lv_anim_set_path_cb(&popAnimation, lv_anim_path_ease_in);
        lv_anim_set_exec_cb(&popAnimation, setObjX);
        lv_anim_set_completed_cb(&popAnimation, onPageTransitionFinished);
        lv_anim_set_user_data(&popAnimation, this);
        lv_anim_start(&popAnimation);
        return;
    }

    lv_obj_set_x(page, DisplayService::SCREEN_WIDTH);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, page);
    lv_anim_set_values(&animation,
                       DisplayService::SCREEN_WIDTH,
                       0);
    lv_anim_set_duration(&animation, 220);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, setObjX);
    lv_anim_set_completed_cb(&animation, onPageTransitionFinished);
    lv_anim_set_user_data(&animation, this);
    lv_anim_start(&animation);

    lv_anim_t oldAnimation;
    lv_anim_init(&oldAnimation);
    lv_anim_set_var(&oldAnimation, previousPage_);
    lv_anim_set_values(&oldAnimation, 0, -24);
    lv_anim_set_duration(&oldAnimation, 220);
    lv_anim_set_path_cb(&oldAnimation, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&oldAnimation, setObjX);
    lv_anim_start(&oldAnimation);
}

lv_obj_t* UiRuntime::currentPage() const {
    return activePage_;
}

void UiRuntime::updateStatus(const WifiSnapshot& wifi,
                             const ServerSnapshot& server,
                             const TimeSnapshot& time, uint32_t nowMs) {
    if (!ready_ || statusWifiIcon_ == nullptr ||
        nowMs - lastStatusMs_ < STATUS_UPDATE_INTERVAL_MS) {
        return;
    }
    lastStatusMs_ = nowMs;

    const bool wifiVisible = wifi.state != WifiState::Disabled &&
                             wifi.state != WifiState::NoCredentials;
    if (wifiVisible) {
        lv_obj_clear_flag(statusWifiIcon_, LV_OBJ_FLAG_HIDDEN);
        lv_color_t wifiColor = muted();
        if (wifi.state == WifiState::Connected) {
            wifiColor = wifi.rssi >= -60 ? text()
                        : wifi.rssi >= -75 ? muted()
                                           : dim();
        }
        lv_obj_set_style_text_color(
            statusWifiIcon_, wifiColor, 0);
    } else {
        lv_obj_add_flag(statusWifiIcon_, LV_OBJ_FLAG_HIDDEN);
    }

    const bool serverVisible = server.state != ServerState::Disabled;
    if (serverVisible) {
        lv_obj_clear_flag(statusServerIcon_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(
            statusServerIcon_,
            server.state == ServerState::Connected ? accent() : muted(), 0);
    } else {
        lv_obj_add_flag(statusServerIcon_, LV_OBJ_FLAG_HIDDEN);
    }

    char timeText[6] = "--:--";
    const bool timeReady = time.effectiveTimeValid;
    if (timeReady) {
        snprintf(timeText, sizeof(timeText), "%02u:%02u",
                 time.effectiveDateTime.hour,
                 time.effectiveDateTime.minute);
    }
    lv_label_set_text(statusTimeLabel_, timeText);
    lv_obj_set_style_text_color(statusTimeLabel_,
                                timeReady ? text() : dim(), 0);
}

lv_color_t UiRuntime::background() const {
    return lv_color_hex(0x080B10);
}

lv_color_t UiRuntime::panel() const {
    return lv_color_hex(0x111722);
}

lv_color_t UiRuntime::panelRaised() const {
    return lv_color_hex(0x202A3A);
}

lv_color_t UiRuntime::accent() const {
    return lv_color_hex(0xFF654D);
}

lv_color_t UiRuntime::accentSoft() const {
    return lv_color_hex(0x3C2525);
}

lv_color_t UiRuntime::text() const {
    return lv_color_hex(0xF5F7FA);
}

lv_color_t UiRuntime::muted() const {
    return lv_color_hex(0x9AA5B5);
}

lv_color_t UiRuntime::dim() const {
    return lv_color_hex(0x586274);
}

const lv_font_t* UiRuntime::font(uint16_t size) const {
    switch (size) {
        case 10:
        case 11: return &lv_font_montserrat_10;
        case 12: return &lv_font_montserrat_12;
        case 16: return &lv_font_montserrat_16;
        case 20: return &lv_font_montserrat_20;
        case 24: return &lv_font_montserrat_24;
        case 28: return &lv_font_montserrat_28;
        case 14:
        default: return &lv_font_montserrat_14;
    }
}

void UiRuntime::displayFlush(lv_display_t* lvDisplay, const lv_area_t* area,
                             uint8_t* pixels) {
    auto* display = static_cast<DisplayService*>(
        lv_display_get_user_data(lvDisplay));
    if (display != nullptr && pixels != nullptr) {
        if (!display->flush(*area, pixels,
                            lv_display_flush_is_last(lvDisplay))) {
            lv_display_flush_ready(lvDisplay);
        }
    } else {
        lv_display_flush_ready(lvDisplay);
    }
}

void UiRuntime::displayFlushWait(lv_display_t* lvDisplay) {
    auto* display = static_cast<DisplayService*>(
        lv_display_get_user_data(lvDisplay));
    if (display != nullptr) {
        display->waitForDma();
    }
}

void UiRuntime::setObjX(void* object, int32_t value) {
    lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
}

void UiRuntime::setObjWidth(void* object, int32_t value) {
    lv_obj_set_width(static_cast<lv_obj_t*>(object), value);
}

void UiRuntime::setObjScrollY(void* object, int32_t value) {
    lv_obj_scroll_to_y(static_cast<lv_obj_t*>(object), value, LV_ANIM_OFF);
}

void UiRuntime::onPageTransitionFinished(lv_anim_t* animation) {
    auto* runtime = static_cast<UiRuntime*>(animation->user_data);
    if (runtime != nullptr) {
        runtime->finishPageTransition();
    }
}

void UiRuntime::finishPageTransition() {
    if (previousPage_ != nullptr) {
        lv_obj_delete(previousPage_);
        previousPage_ = nullptr;
    }
}

void UiRuntime::animateCard(UiCard& card, bool focused) {
    lv_anim_delete(card.root, setObjX);
    lv_anim_delete(card.root, setObjWidth);

    lv_anim_t xAnimation;
    lv_anim_init(&xAnimation);
    lv_anim_set_var(&xAnimation, card.root);
    lv_anim_set_values(&xAnimation, lv_obj_get_x(card.root),
                       focused ? card.focusedX : card.normalX);
    lv_anim_set_duration(&xAnimation, 180);
    lv_anim_set_path_cb(&xAnimation, lv_anim_path_overshoot);
    lv_anim_set_exec_cb(&xAnimation, setObjX);
    lv_anim_start(&xAnimation);

    lv_anim_t widthAnimation;
    lv_anim_init(&widthAnimation);
    lv_anim_set_var(&widthAnimation, card.root);
    lv_anim_set_values(&widthAnimation, lv_obj_get_width(card.root),
                       focused ? card.focusedWidth : card.normalWidth);
    lv_anim_set_duration(&widthAnimation, 180);
    lv_anim_set_path_cb(&widthAnimation, lv_anim_path_overshoot);
    lv_anim_set_exec_cb(&widthAnimation, setObjWidth);
    lv_anim_start(&widthAnimation);
}

void UiRuntime::createStatusBar() {
    statusBar_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(statusBar_);
    lv_obj_set_size(statusBar_, DisplayService::SCREEN_WIDTH, 22);
    lv_obj_set_pos(statusBar_, 0, 0);
    lv_obj_set_style_bg_color(statusBar_, lv_color_hex(0x0D1118), 0);
    lv_obj_set_style_bg_opa(statusBar_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(statusBar_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* line = lv_obj_create(statusBar_);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, DisplayService::SCREEN_WIDTH, 1);
    lv_obj_set_pos(line, 0, 21);
    lv_obj_set_style_bg_color(line, accent(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);

    createLabel(statusBar_, "PGOS", 10, 3, 14, text());
    statusWifiIcon_ = createLabel(statusBar_, LV_SYMBOL_WIFI, 80, 2, 16,
                                  muted());
    lv_obj_add_flag(statusWifiIcon_, LV_OBJ_FLAG_HIDDEN);
    statusServerIcon_ = createLabel(statusBar_, LV_SYMBOL_UPLOAD, 116, 2, 16,
                                    muted());
    statusTimeLabel_ = createLabel(statusBar_, "--:--", 247, 3, 14, dim());
    lv_obj_set_width(statusTimeLabel_, 63);
    lv_obj_set_style_text_align(statusTimeLabel_, LV_TEXT_ALIGN_RIGHT, 0);
}
