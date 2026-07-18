#pragma once

#include "core/App.h"
#include "ui/UiRuntime.h"

class LauncherApp final : public IApp {
public:
    AppId id() const override;
    const char* name() const override;
    void onEnter(AppContext& context) override;
    void onExit(AppContext& context) override;
    void onCommand(const AppCommand& command, AppContext& context) override;
    void onTick(uint32_t nowMs, AppContext& context) override;
    lv_obj_t* onCreateView(AppContext& context) override;
    void onUpdateView(AppContext& context) override;
    AppId requestedApp() const override;

private:
    static constexpr uint8_t ITEM_COUNT = 3;
    static constexpr uint8_t GRID_COLUMNS = 2;
    uint8_t selected_ = 0;
    int8_t renderedSelection_ = -1;
    AppId requested_ = AppId::Count;
    lv_obj_t* root_ = nullptr;
    UiCard cards_[ITEM_COUNT];

    void moveHorizontal(int8_t delta);
    void moveVertical(int8_t delta);
};
