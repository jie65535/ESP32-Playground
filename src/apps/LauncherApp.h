#pragma once

#include "core/App.h"

class LauncherApp final : public IApp {
public:
    AppId id() const override;
    const char* name() const override;
    void onEnter(AppContext& context) override;
    void onExit(AppContext& context) override;
    void onCommand(const AppCommand& command, AppContext& context) override;
    void onTick(uint32_t nowMs, AppContext& context) override;
    void onRender(AppContext& context) override;
    bool handlesNavigation() const override;
    AppId requestedApp() const override;

private:
    static constexpr uint8_t ITEM_COUNT = 3;
    uint8_t selected_ = 0;
    AppId requested_ = AppId::Count;
};
