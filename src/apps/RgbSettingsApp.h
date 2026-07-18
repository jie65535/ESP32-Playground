#pragma once

#include "core/App.h"
#include "ui/UiRuntime.h"

class RgbSettingsApp final : public IApp {
public:
    AppId id() const override;
    const char* name() const override;
    void onEnter(AppContext& context) override;
    void onExit(AppContext& context) override;
    void onCommand(const AppCommand& command, AppContext& context) override;
    void onTick(uint32_t nowMs, AppContext& context) override;
    lv_obj_t* onCreateView(AppContext& context) override;
    void onUpdateView(AppContext& context) override;

private:
    static constexpr uint8_t SETTING_COUNT = 5;
    static constexpr uint8_t BRIGHTNESS_VALUES[] = {10, 25, 40, 60, 80, 100};

    lv_obj_t* root_ = nullptr;
    UiCard rows_[SETTING_COUNT];
    lv_obj_t* valueLabels_[SETTING_COUNT] = {};
    lv_obj_t* brightnessBar_ = nullptr;
    uint8_t selected_ = 0;
    int8_t renderedSelected_ = -1;
    bool renderedValid_ = false;
    bool renderedEnabled_ = false;
    uint8_t renderedEffect_ = 0;
    uint8_t renderedColor_ = 0;
    uint8_t renderedBrightness_ = 0;
    uint8_t renderedSpeed_ = 0;

    void adjustSelected(int8_t delta, AppContext& context);
    uint8_t brightnessIndex(uint8_t value) const;
};
