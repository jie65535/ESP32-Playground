#pragma once

#include "core/App.h"
#include "ui/UiRuntime.h"

class SystemInfoApp final : public IApp {
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
    static constexpr uint8_t ITEM_COUNT = 5;

    uint32_t nowMs_ = 0;
    lv_obj_t* root_ = nullptr;
    UiCard cards_[ITEM_COUNT];
    uint8_t selected_ = 0;
    int8_t renderedSelected_ = -1;
};
