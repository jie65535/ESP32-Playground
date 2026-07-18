#pragma once

#include "core/App.h"

class DisplayTestApp final : public IApp {
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
    bool colorTest_ = false;
    bool renderedColorTest_ = false;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* modeLabel_ = nullptr;
    lv_obj_t* swatches_[6] = {};
};
