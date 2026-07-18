#include "ui/UiRuntime.h"

#include "services/DisplayService.h"

#include <esp_heap_caps.h>

namespace {

constexpr uint32_t STATUS_UPDATE_INTERVAL_MS = 250;

const char* wifiStateName(WifiState state) {
    switch (state) {
        case WifiState::Disabled: return "OFF";
        case WifiState::NoCredentials: return "SETUP";
        case WifiState::Ready: return "READY";
        case WifiState::Scanning: return "SCAN";
        case WifiState::Connecting: return "LINK";
        case WifiState::Connected: return "ONLINE";
        case WifiState::Backoff: return "RETRY";
        default: return "?";
    }
}

const char* serverStateName(ServerState state) {
    switch (state) {
        case ServerState::Disabled: return "OFF";
        case ServerState::NoTarget: return "TARGET";
        case ServerState::WaitingWifi: return "WIFI";
        case ServerState::Connecting: return "LINK";
        case ServerState::Connected: return "ONLINE";
        case ServerState::Backoff: return "RETRY";
        default: return "?";
    }
}

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
        bufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (drawBuffer_ == nullptr) {
        drawBuffer_ = static_cast<uint8_t*>(malloc(bufferBytes));
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
    lv_display_set_buffers(lvDisplay_, drawBuffer_, nullptr, bufferBytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

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
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    createLabel(root, eyebrow, 16, 10, 12, accent());
    createLabel(root, title, 16, 27, 24, text());
    if (subtitle != nullptr) {
        createLabel(root, subtitle, 17, 57, 12, muted());
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

void UiRuntime::replacePage(lv_obj_t* page) {
    if (page == nullptr) {
        return;
    }

    if (previousPage_ != nullptr) {
        lv_obj_delete(previousPage_);
        previousPage_ = nullptr;
    }

    previousPage_ = activePage_;
    activePage_ = page;

    if (previousPage_ == nullptr) {
        lv_obj_set_x(page, 0);
        return;
    }

    lv_obj_set_x(page, DisplayService::SCREEN_WIDTH);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, page);
    lv_anim_set_values(&animation, DisplayService::SCREEN_WIDTH, 0);
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
                             const ServerSnapshot& server, uint32_t nowMs) {
    if (!ready_ || statusWifi_ == nullptr ||
        nowMs - lastStatusMs_ < STATUS_UPDATE_INTERVAL_MS) {
        return;
    }
    lastStatusMs_ = nowMs;

    String wifiText = "WiFi ";
    if (wifi.state == WifiState::Connected) {
        wifiText += wifi.rssi;
    } else {
        wifiText += wifiStateName(wifi.state);
    }
    lv_label_set_text(statusWifi_, wifiText.c_str());
    lv_obj_set_style_text_color(statusWifi_,
                                wifi.state == WifiState::Connected ? text()
                                                                  : muted(),
                                0);

    String serverText = server.state == ServerState::Connected
                            ? String("TCP ON")
                            : String("TCP ") + serverStateName(server.state);
    lv_label_set_text(statusServer_, serverText.c_str());
    lv_obj_set_style_text_color(statusServer_,
                                server.state == ServerState::Connected ? accent()
                                                                       : muted(),
                                0);

    String memoryText = String(ESP.getFreeHeap() / 1024U) + "K";
    lv_label_set_text(statusMemory_, memoryText.c_str());
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
        display->flush(*area, pixels);
    }
    lv_display_flush_ready(lvDisplay);
}

void UiRuntime::setObjX(void* object, int32_t value) {
    lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
}

void UiRuntime::setObjWidth(void* object, int32_t value) {
    lv_obj_set_width(static_cast<lv_obj_t*>(object), value);
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
    statusWifi_ = createLabel(statusBar_, "WiFi SETUP", 78, 4, 11, muted());
    statusServer_ = createLabel(statusBar_, "TCP OFF", 166, 4, 11, muted());
    statusMemory_ = createLabel(statusBar_, "--", 252, 4, 10, dim());
    lv_obj_set_width(statusMemory_, 58);
    lv_label_set_long_mode(statusMemory_, LV_LABEL_LONG_CLIP);
    lv_obj_align(statusMemory_, LV_ALIGN_RIGHT_MID, -8, -1);
}
