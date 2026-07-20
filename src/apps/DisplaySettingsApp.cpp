#include "apps/DisplaySettingsApp.h"

#include "services/DisplayService.h"
#include "ui/UiRuntime.h"

constexpr uint8_t DisplaySettingsApp::BRIGHTNESS_VALUES[];
constexpr uint32_t DisplaySettingsApp::TIMEOUT_VALUES[];

namespace {

String settingValue(uint8_t percent) {
    return String("< ") + percent + "% >";
}

}  // namespace

AppId DisplaySettingsApp::id() const {
    return AppId::DisplaySettings;
}

const char* DisplaySettingsApp::name() const {
    return "Display Settings";
}

void DisplaySettingsApp::onEnter(AppContext&) {
    selected_ = 0;
    renderedSelected_ = -1;
    renderedBrightness_ = 0;
    renderedTimeout_ = UINT32_MAX;
}

void DisplaySettingsApp::onExit(AppContext&) {
    root_ = nullptr;
    memset(rows_, 0, sizeof(rows_));
    memset(valueLabels_, 0, sizeof(valueLabels_));
    brightnessBar_ = nullptr;
}

void DisplaySettingsApp::onCommand(const AppCommand& command,
                                   AppContext& context) {
    switch (command.type) {
        case AppCommandType::Previous:
        case AppCommandType::Left:
            if (command.type == AppCommandType::Previous) {
                selected_ = selected_ == 0 ? SETTING_COUNT - 1U : selected_ - 1U;
            } else {
                adjustSelected(-1, context);
            }
            break;
        case AppCommandType::Next:
        case AppCommandType::Right:
            if (command.type == AppCommandType::Next) {
                selected_ = static_cast<uint8_t>((selected_ + 1U) % SETTING_COUNT);
            } else {
                adjustSelected(1, context);
            }
            break;
        case AppCommandType::Activate:
            adjustSelected(1, context);
            break;
        default:
            break;
    }
}

void DisplaySettingsApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* DisplaySettingsApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot(nullptr, "Display");

    rows_[0] = context.ui.createCard(root_, UiRuntime::CARD_START_Y, UiIcon::Display,
                                     "亮度", nullptr);
    rows_[1] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + UiRuntime::CARD_STEP_Y, UiIcon::Display,
                                     "自动息屏", nullptr);

    for (uint8_t index = 0; index < SETTING_COUNT; ++index) {
        valueLabels_[index] = context.ui.createLabel(
            rows_[index].root, index == 0 ? "< 100% >" : "< 永不 >",
            200, 10, 12, context.ui.text());
        context.ui.applyBodyFont(valueLabels_[index]);
        lv_obj_set_width(valueLabels_[index], 72);
        lv_label_set_long_mode(valueLabels_[index], LV_LABEL_LONG_CLIP);
        lv_obj_align(valueLabels_[index], LV_ALIGN_RIGHT_MID, -9, -5);
    }

    brightnessBar_ = lv_bar_create(rows_[0].root);
    lv_obj_set_size(brightnessBar_, 72, 5);
    lv_obj_align(brightnessBar_, LV_ALIGN_RIGHT_MID, -9, 12);
    lv_obj_set_style_radius(brightnessBar_, 3, 0);
    lv_obj_set_style_bg_color(brightnessBar_, context.ui.panelRaised(), 0);
    lv_obj_set_style_bg_opa(brightnessBar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(brightnessBar_, context.ui.accent(),
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(brightnessBar_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_bar_set_range(brightnessBar_, 0, 100);

    return root_;
}

void DisplaySettingsApp::onUpdateView(AppContext& context) {
    if (root_ == nullptr) {
        return;
    }

    DisplayService& display = context.display;
    const uint8_t brightness = display.brightnessPercent();
    const uint32_t timeout = display.screenTimeoutSeconds();

    if (renderedSelected_ != static_cast<int8_t>(selected_)) {
        for (uint8_t index = 0; index < SETTING_COUNT; ++index) {
            context.ui.setCardFocused(rows_[index], index == selected_);
        }
        context.ui.centerFocused(root_, rows_[selected_].root);
        renderedSelected_ = static_cast<int8_t>(selected_);
    }

    if (renderedBrightness_ != brightness) {
        const String value = settingValue(brightness);
        lv_label_set_text(valueLabels_[0], value.c_str());
        lv_bar_set_value(brightnessBar_, brightness, LV_ANIM_OFF);
        renderedBrightness_ = brightness;
    }

    if (renderedTimeout_ != timeout) {
        const String value = String("< ") + timeoutText(timeout) + " >";
        lv_label_set_text(valueLabels_[1], value.c_str());
        renderedTimeout_ = timeout;
    }
}

void DisplaySettingsApp::adjustSelected(int8_t delta, AppContext& context) {
    if (selected_ == 0) {
        cycleBrightness(delta, context);
    } else {
        cycleTimeout(delta, context);
    }
}

void DisplaySettingsApp::cycleBrightness(int8_t delta, AppContext& context) {
    DisplayService& display = context.display;
    const int16_t count = static_cast<int16_t>(
        sizeof(BRIGHTNESS_VALUES) / sizeof(BRIGHTNESS_VALUES[0]));
    int16_t index = brightnessIndex(display.brightnessPercent());
    index = delta < 0 ? (index == 0 ? count - 1 : index - 1)
                      : (index + 1) % count;
    display.setBrightnessPercent(BRIGHTNESS_VALUES[index]);
}

void DisplaySettingsApp::cycleTimeout(int8_t delta, AppContext& context) {
    DisplayService& display = context.display;
    const int16_t count = static_cast<int16_t>(
        sizeof(TIMEOUT_VALUES) / sizeof(TIMEOUT_VALUES[0]));
    int16_t index = timeoutIndex(display.screenTimeoutSeconds());
    index = delta < 0 ? (index == 0 ? count - 1 : index - 1)
                      : (index + 1) % count;
    display.setScreenTimeoutSeconds(TIMEOUT_VALUES[index]);
}

uint8_t DisplaySettingsApp::brightnessIndex(uint8_t value) const {
    for (uint8_t index = 0;
         index < sizeof(BRIGHTNESS_VALUES) / sizeof(BRIGHTNESS_VALUES[0]);
         ++index) {
        if (value <= BRIGHTNESS_VALUES[index]) {
            return index;
        }
    }
    return static_cast<uint8_t>(
        sizeof(BRIGHTNESS_VALUES) / sizeof(BRIGHTNESS_VALUES[0]) - 1U);
}

uint8_t DisplaySettingsApp::timeoutIndex(uint32_t seconds) const {
    for (uint8_t index = 0;
         index < sizeof(TIMEOUT_VALUES) / sizeof(TIMEOUT_VALUES[0]);
         ++index) {
        if (seconds == TIMEOUT_VALUES[index]) {
            return index;
        }
    }
    return 0;
}

String DisplaySettingsApp::timeoutText(uint32_t seconds) const {
    if (seconds == 0) {
        return "永不";
    }
    if (seconds < 60) {
        return String(seconds) + " 秒";
    }
    if (seconds % 60 == 0) {
        return String(seconds / 60U) + " 分钟";
    }
    return String(seconds) + " 秒";
}
