#pragma once

#include "core/App.h"
#include "ui/UiRuntime.h"

class ControllerSettingsApp final : public IApp {
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
    static constexpr uint8_t SETTING_COUNT = 4;
    static constexpr uint32_t IDLE_TIMEOUT_VALUES[] = {
        0, 5UL * 60UL * 1000UL, 15UL * 60UL * 1000UL,
        30UL * 60UL * 1000UL,
    };

    lv_obj_t* root_ = nullptr;
    UiCard statusCard_;
    UiCard rows_[SETTING_COUNT];
    lv_obj_t* statusValue_ = nullptr;
    lv_obj_t* valueLabels_[SETTING_COUNT] = {};
    uint8_t selected_ = 0;
    int8_t renderedSelected_ = -1;
    bool renderedConnected_ = false;
    bool renderedScanning_ = false;
    GamepadScanMode renderedScanMode_ = GamepadScanMode::None;
    uint32_t renderedScanSeconds_ = UINT32_MAX;
    uint32_t renderedReconnectSeconds_ = UINT32_MAX;
    uint32_t renderedTimeoutMs_ = UINT32_MAX;
    uint32_t renderedPacketCount_ = UINT32_MAX;

    void adjustSelected(int8_t delta, AppContext& context);
    void activateSelected(AppContext& context);
    void cycleIdleTimeout(int8_t delta, AppContext& context);
    uint8_t idleTimeoutIndex(uint32_t timeoutMs) const;
    String idleTimeoutText(uint32_t timeoutMs) const;
};
