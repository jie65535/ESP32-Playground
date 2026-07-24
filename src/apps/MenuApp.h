#pragma once

#include "core/App.h"
#include "ui/UiRuntime.h"

struct MenuItemDefinition {
    UiIcon icon;
    const char* title;
    const char* subtitle;
    AppId target;
};

struct MenuDefinition {
    AppId id;
    const char* name;
    const char* title;
    const char* subtitle;
    const MenuItemDefinition* items;
    uint8_t itemCount;
};

class MenuApp final : public IApp {
public:
    static constexpr uint8_t MAX_ITEMS = 8;

    explicit MenuApp(const MenuDefinition& definition);

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
    const MenuDefinition& definition_;
    uint8_t selected_ = 0;
    int8_t renderedSelection_ = -1;
    AppId requested_ = AppId::Count;
    lv_obj_t* root_ = nullptr;
    UiCard cards_[MAX_ITEMS];

    uint8_t itemCount() const;
};
