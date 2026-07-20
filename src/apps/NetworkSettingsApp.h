#pragma once

#include "core/App.h"
#include "ui/OnScreenKeyboard.h"

class NetworkSettingsApp final : public IApp {
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
    static constexpr uint8_t VISIBLE_ROWS = 6;
    static constexpr uint8_t INACTIVE_ITEM_COUNT = 3;
    enum class SetupStage : uint8_t {
        Inactive,
        Scanning,
        Selecting,
        EnteringPassword,
    };

    SetupStage setupStage_ = SetupStage::Inactive;
    uint8_t inactiveCursor_ = 0;
    int16_t cursor_ = 0;
    int16_t windowStart_ = 0;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* inputMetaLabel_ = nullptr;
    OnScreenKeyboard keyboard_;
    String setupNotice_;
    String passwordError_;
    String lastViewSignature_;

    void startWizard(AppContext& context);
    void cancelWizard(AppContext& context);
    void moveInactiveCursor(int16_t delta);
    void activateInactive(AppContext& context);
    void moveCursor(int16_t delta, AppContext& context);
    void selectCurrent(AppContext& context);
    void submitPassword(AppContext& context);
    void updatePasswordMeta(AppContext& context);
    void rebuildView(AppContext& context);
    void buildHeader(AppContext& context, const char* eyebrow,
                     const char* title, const char* subtitle);
    void buildInactive(AppContext& context, const WifiSnapshot& snapshot);
    void buildScanning(AppContext& context);
    void buildSelection(AppContext& context);
    void buildPassword(AppContext& context);
    String viewSignature(AppContext& context) const;
};
