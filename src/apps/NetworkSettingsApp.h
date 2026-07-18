#pragma once

#include "core/App.h"

class NetworkSettingsApp final : public IApp {
public:
    AppId id() const override;
    const char* name() const override;
    void onEnter(AppContext& context) override;
    void onExit(AppContext& context) override;
    void onCommand(const AppCommand& command, AppContext& context) override;
    void onTick(uint32_t nowMs, AppContext& context) override;
    void onRender(AppContext& context) override;
    bool handlesNavigation() const override;

private:
    static constexpr uint8_t VISIBLE_ROWS = 8;
    enum class SetupStage : uint8_t {
        Inactive,
        Scanning,
        Selecting,
        AwaitingPassword,
    };

    SetupStage setupStage_ = SetupStage::Inactive;
    int16_t cursor_ = 0;
    int16_t windowStart_ = 0;

    void startWizard(AppContext& context);
    void moveCursor(int16_t delta, AppContext& context);
    void selectCurrent(AppContext& context);
    void drawWizard(AppContext& context);
};
