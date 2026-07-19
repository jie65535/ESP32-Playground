#pragma once

#include "core/App.h"
#include "ui/UiRuntime.h"

class TimeApp final : public IApp {
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
    lv_obj_t* root_ = nullptr;
    lv_obj_t* timeLabel_ = nullptr;
    lv_obj_t* dateLabel_ = nullptr;
    lv_obj_t* weekdayLabel_ = nullptr;
    UiCard statusCard_;
    UiCard setCard_;
    uint8_t selected_ = 0;
    int8_t renderedSelected_ = -1;
};
