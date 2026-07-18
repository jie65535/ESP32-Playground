#pragma once

#include "core/App.h"

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

private:
    static constexpr uint8_t VISIBLE_ROWS = 6;
    enum class SetupStage : uint8_t {
        Inactive,
        Scanning,
        Selecting,
        AwaitingPassword,
    };

    SetupStage setupStage_ = SetupStage::Inactive;
    int16_t cursor_ = 0;
    int16_t windowStart_ = 0;
    lv_obj_t* root_ = nullptr;
    String lastViewSignature_;

    void startWizard(AppContext& context);
    void moveCursor(int16_t delta, AppContext& context);
    void selectCurrent(AppContext& context);
    void rebuildView(AppContext& context);
    void buildHeader(AppContext& context, const char* eyebrow,
                     const char* title, const char* subtitle);
    void buildInactive(AppContext& context, const WifiSnapshot& snapshot);
    void buildScanning(AppContext& context);
    void buildSelection(AppContext& context);
    void buildPassword(AppContext& context);
    String viewSignature(AppContext& context) const;
};
