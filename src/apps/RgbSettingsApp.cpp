#include "apps/RgbSettingsApp.h"

#include "services/RgbService.h"
#include "ui/UiRuntime.h"

constexpr uint8_t RgbSettingsApp::BRIGHTNESS_VALUES[];

namespace {

String choiceText(const char* value) {
    return String("< ") + value + " >";
}

}  // namespace

AppId RgbSettingsApp::id() const {
    return AppId::RgbSettings;
}

const char* RgbSettingsApp::name() const {
    return "RGB Light";
}

void RgbSettingsApp::onEnter(AppContext&) {
    selected_ = 0;
    renderedSelected_ = -1;
    renderedValid_ = false;
}

void RgbSettingsApp::onExit(AppContext&) {
    root_ = nullptr;
    memset(rows_, 0, sizeof(rows_));
    memset(valueLabels_, 0, sizeof(valueLabels_));
    brightnessBar_ = nullptr;
}

void RgbSettingsApp::onCommand(const AppCommand& command,
                               AppContext& context) {
    switch (command.type) {
        case AppCommandType::Previous:
            selected_ = selected_ == 0 ? SETTING_COUNT - 1U : selected_ - 1U;
            break;
        case AppCommandType::Next:
            selected_ = static_cast<uint8_t>((selected_ + 1U) % SETTING_COUNT);
            break;
        case AppCommandType::Left:
            adjustSelected(-1, context);
            break;
        case AppCommandType::Right:
        case AppCommandType::Activate:
            adjustSelected(1, context);
            break;
        default:
            break;
    }
}

void RgbSettingsApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* RgbSettingsApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot("LIGHT / GPIO42", "RGB Light");

    rows_[0] = context.ui.createCard(root_, 58, LV_SYMBOL_POWER,
                                     "Power", "Onboard WS2812 output");
    rows_[1] = context.ui.createCard(root_, 108, LV_SYMBOL_LOOP,
                                     "Effect", "Non-blocking light scene");
    rows_[2] = context.ui.createCard(root_, 158, LV_SYMBOL_TINT,
                                     "Palette", "Base color for effects");
    rows_[3] = context.ui.createCard(root_, 208, LV_SYMBOL_EYE_OPEN,
                                     "Brightness", "Peak LED output level");
    rows_[4] = context.ui.createCard(root_, 258, LV_SYMBOL_REFRESH,
                                     "Speed", "Animation tempo");

    for (uint8_t index = 0; index < SETTING_COUNT; ++index) {
        valueLabels_[index] = context.ui.createLabel(
            rows_[index].root, "--", 184, 10, 12, context.ui.text());
        lv_obj_set_width(valueLabels_[index], 88);
        lv_label_set_long_mode(valueLabels_[index], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(valueLabels_[index], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(valueLabels_[index], LV_ALIGN_RIGHT_MID, -9,
                     index == 3 ? -5 : 0);
    }

    brightnessBar_ = lv_bar_create(rows_[3].root);
    lv_obj_set_size(brightnessBar_, 84, 5);
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

void RgbSettingsApp::onUpdateView(AppContext& context) {
    if (root_ == nullptr) {
        return;
    }

    if (renderedSelected_ != static_cast<int8_t>(selected_)) {
        for (uint8_t index = 0; index < SETTING_COUNT; ++index) {
            context.ui.setCardFocused(rows_[index], index == selected_);
        }
        context.ui.centerFocused(root_, rows_[selected_].root);
        renderedSelected_ = static_cast<int8_t>(selected_);
    }

    const RgbSnapshot rgb = context.rgb.snapshot();
    if (!renderedValid_ || renderedEnabled_ != rgb.enabled) {
        const String value = choiceText(rgb.enabled ? "On" : "Off");
        lv_label_set_text(valueLabels_[0], value.c_str());
        renderedEnabled_ = rgb.enabled;
    }
    const uint8_t effect = static_cast<uint8_t>(rgb.effect);
    if (!renderedValid_ || renderedEffect_ != effect) {
        const String value = choiceText(RgbService::effectName(rgb.effect));
        lv_label_set_text(valueLabels_[1], value.c_str());
        renderedEffect_ = effect;
    }
    if (!renderedValid_ || renderedColor_ != rgb.colorIndex) {
        const String value = choiceText(RgbService::paletteName(rgb.colorIndex));
        lv_label_set_text(valueLabels_[2], value.c_str());
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
        RgbService::paletteRgb(rgb.colorIndex, red, green, blue);
        lv_obj_set_style_text_color(valueLabels_[2],
                                    lv_color_make(red, green, blue), 0);
        lv_obj_set_style_bg_color(brightnessBar_,
                                  lv_color_make(red, green, blue),
                                  LV_PART_INDICATOR);
        renderedColor_ = rgb.colorIndex;
    }
    if (!renderedValid_ || renderedBrightness_ != rgb.brightnessPercent) {
        const String value = String("< ") + rgb.brightnessPercent + "% >";
        lv_label_set_text(valueLabels_[3], value.c_str());
        lv_bar_set_value(brightnessBar_, rgb.brightnessPercent, LV_ANIM_OFF);
        renderedBrightness_ = rgb.brightnessPercent;
    }
    if (!renderedValid_ || renderedSpeed_ != rgb.speedIndex) {
        const String value = choiceText(RgbService::speedName(rgb.speedIndex));
        lv_label_set_text(valueLabels_[4], value.c_str());
        renderedSpeed_ = rgb.speedIndex;
    }
    renderedValid_ = true;
}

void RgbSettingsApp::adjustSelected(int8_t delta, AppContext& context) {
    RgbService& rgb = context.rgb;
    switch (selected_) {
        case 0:
            rgb.setEnabled(!rgb.enabled());
            break;

        case 1: {
            int16_t index = static_cast<uint8_t>(rgb.effect());
            index = delta < 0
                        ? (index == 0 ? RgbService::EFFECT_COUNT - 1U : index - 1)
                        : (index + 1) % RgbService::EFFECT_COUNT;
            rgb.setEffect(static_cast<RgbEffect>(index));
            break;
        }

        case 2: {
            int16_t index = rgb.colorIndex();
            index = delta < 0
                        ? (index == 0 ? RgbService::PALETTE_COUNT - 1U : index - 1)
                        : (index + 1) % RgbService::PALETTE_COUNT;
            rgb.setColorIndex(static_cast<uint8_t>(index));
            break;
        }

        case 3: {
            const uint8_t count = static_cast<uint8_t>(
                sizeof(BRIGHTNESS_VALUES) / sizeof(BRIGHTNESS_VALUES[0]));
            int16_t index = brightnessIndex(rgb.brightnessPercent());
            index = delta < 0 ? (index == 0 ? count - 1U : index - 1)
                              : (index + 1) % count;
            rgb.setBrightnessPercent(BRIGHTNESS_VALUES[index]);
            break;
        }

        case 4: {
            int16_t index = rgb.speedIndex();
            index = delta < 0
                        ? (index == 0 ? RgbService::SPEED_COUNT - 1U : index - 1)
                        : (index + 1) % RgbService::SPEED_COUNT;
            rgb.setSpeedIndex(static_cast<uint8_t>(index));
            break;
        }

        default:
            break;
    }
}

uint8_t RgbSettingsApp::brightnessIndex(uint8_t value) const {
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
