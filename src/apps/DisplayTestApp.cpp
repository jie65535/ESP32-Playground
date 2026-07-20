#include "apps/DisplayTestApp.h"

#include "ui/UiRuntime.h"

namespace {

constexpr uint32_t SWATCH_COLORS[] = {
    0xF04444, 0x48C774, 0x4B7BEC,
    0x35C6D8, 0xBE63D9, 0xF3C84B,
};

const char* SWATCH_NAMES[] = {"红", "绿", "蓝", "青", "品红", "黄"};

}  // namespace

AppId DisplayTestApp::id() const {
    return AppId::DisplayTest;
}

const char* DisplayTestApp::name() const {
    return "Display";
}

void DisplayTestApp::onEnter(AppContext&) {
    renderedColorTest_ = !colorTest_;
}

void DisplayTestApp::onExit(AppContext&) {
    root_ = nullptr;
    modeLabel_ = nullptr;
    memset(swatches_, 0, sizeof(swatches_));
}

void DisplayTestApp::onCommand(const AppCommand& command, AppContext&) {
    if (command.type == AppCommandType::Activate ||
        command.type == AppCommandType::ColorTest) {
        colorTest_ = !colorTest_;
    }
}

void DisplayTestApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* DisplayTestApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot(nullptr, "Color Lab");
    modeLabel_ = context.ui.createLabel(root_, "标准 RGB565", 206, 14, 11,
                                        context.ui.muted());
    context.ui.applyBodyFont(modeLabel_);

    for (uint8_t index = 0; index < 6; ++index) {
        const int16_t column = index % 3U;
        const int16_t row = index / 3U;
        lv_obj_t* swatch = lv_obj_create(root_);
        lv_obj_remove_style_all(swatch);
        lv_obj_set_size(swatch, 88, 42);
        lv_obj_set_pos(swatch, 16 + column * 100, 60 + row * 50);
        lv_obj_set_style_radius(swatch, 8, 0);
        lv_obj_set_style_bg_color(swatch, lv_color_hex(SWATCH_COLORS[index]), 0);
        lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(swatch, 1, 0);
        lv_obj_set_style_border_color(swatch, context.ui.panelRaised(), 0);
        lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);
        context.ui.createBodyLabel(swatch, SWATCH_NAMES[index], 9, 13,
                                   lv_color_hex(0x080B10), true);
        swatches_[index] = swatch;
    }

    context.ui.createBodyLabel(root_, "BGR 色序 / 反色开启", 18, 165,
                               context.ui.muted());
    context.ui.createLabel(root_, "320 x 240  /  40 MHz SPI", 168, 165, 12,
                           context.ui.text());
    context.ui.createBodyLabel(root_, "屏幕测试 / 中文像素字体", 18, 190,
                               context.ui.text());
    return root_;
}

void DisplayTestApp::onUpdateView(AppContext& context) {
    if (modeLabel_ == nullptr || renderedColorTest_ == colorTest_) {
        return;
    }
    renderedColorTest_ = colorTest_;
    lv_label_set_text(modeLabel_, colorTest_ ? "校准模式" : "标准 RGB565");
    lv_obj_set_style_text_color(modeLabel_,
                                colorTest_ ? context.ui.accent() : context.ui.muted(),
                                0);
    for (lv_obj_t* swatch : swatches_) {
        lv_obj_set_style_border_width(swatch, colorTest_ ? 3 : 1, 0);
        lv_obj_set_style_border_color(swatch,
                                      colorTest_ ? context.ui.text()
                                                 : context.ui.panelRaised(),
                                      0);
    }
}
