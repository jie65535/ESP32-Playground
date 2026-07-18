#pragma once

#include "core/App.h"
#include "ui/UiRuntime.h"

class DisplaySettingsApp final : public IApp {
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
    static constexpr uint8_t SETTING_COUNT = 2;
    static constexpr uint8_t BRIGHTNESS_VALUES[] = {20, 40, 60, 80, 100};
    static constexpr uint32_t TIMEOUT_VALUES[] = {0, 15, 30, 60, 300, 900};

    lv_obj_t* root_ = nullptr;
    UiCard rows_[SETTING_COUNT];
    lv_obj_t* valueLabels_[SETTING_COUNT] = {};
    lv_obj_t* brightnessBar_ = nullptr;
    uint8_t selected_ = 0;
    int8_t renderedSelected_ = -1;
    uint8_t renderedBrightness_ = 0;
    uint32_t renderedTimeout_ = UINT32_MAX;

    void adjustSelected(int8_t delta, AppContext& context);
    void cycleBrightness(int8_t delta, AppContext& context);
    void cycleTimeout(int8_t delta, AppContext& context);
    uint8_t brightnessIndex(uint8_t value) const;
    uint8_t timeoutIndex(uint32_t seconds) const;
    String timeoutText(uint32_t seconds) const;
};
