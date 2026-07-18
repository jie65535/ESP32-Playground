#pragma once

#include "core/App.h"

class SystemInfoApp final : public IApp {
public:
    AppId id() const override;
    const char* name() const override;
    void onEnter(AppContext& context) override;
    void onExit(AppContext& context) override;
    void onCommand(const AppCommand& command, AppContext& context) override;
    void onTick(uint32_t nowMs, AppContext& context) override;
    void onRender(AppContext& context) override;

private:
    uint32_t nowMs_ = 0;
};
