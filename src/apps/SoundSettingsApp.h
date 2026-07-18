#pragma once

#include "core/App.h"
#include "ui/UiRuntime.h"

class SoundSettingsApp final : public IApp {
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
    static constexpr uint8_t SETTING_COUNT = 3;

    lv_obj_t* root_ = nullptr;
    UiCard rows_[SETTING_COUNT];
    lv_obj_t* valueLabels_[SETTING_COUNT] = {};
    uint8_t selected_ = 0;
    int8_t renderedSelected_ = -1;
    uint8_t renderedVolume_ = 0;
    bool renderedFeedback_ = false;
    bool renderedReady_ = false;

    void adjustSelected(int8_t delta, AppContext& context);
    void cycleVolume(int8_t delta, AppContext& context);
    String volumeText(const AudioService& audio) const;
};
