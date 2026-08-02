#pragma once

#include "core/App.h"
#include "ui/OnScreenKeyboard.h"
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
    bool onBack(AppContext& context) override;

private:
    enum class ViewMode : uint8_t {
        Main,
        EditingHost,
    };

    static constexpr uint8_t ITEM_COUNT = 3;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* inputMetaLabel_ = nullptr;
    OnScreenKeyboard keyboard_;
    ViewMode viewMode_ = ViewMode::Main;
    uint8_t selected_ = 0;
    String notice_;
    String inputError_;
    String lastViewSignature_;

    void moveSelection(int16_t delta);
    void activateSelected(AppContext& context);
    void startHostEditor();
    void submitHost(AppContext& context);
    void updateInputMeta(AppContext& context);
    void rebuildView(AppContext& context);
    void buildMain(AppContext& context);
    void buildHostEditor(AppContext& context);
    String viewSignature(AppContext& context) const;
};
