#pragma once

#include "core/App.h"
#include "ui/UiRuntime.h"

class ConsoleSettingsApp final : public IApp {
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
    static constexpr uint8_t ITEM_COUNT = 2;

    lv_obj_t* root_ = nullptr;
    UiCard rows_[ITEM_COUNT];
    lv_obj_t* valueLabels_[ITEM_COUNT] = {};
    uint8_t selected_ = 0;
    int8_t renderedSelected_ = -1;
    String lastStatusSignature_;

    String statusSignature(AppContext& context) const;
};
