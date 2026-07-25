#pragma once

#include "core/App.h"
#include "ui/UiRuntime.h"

class TinyLmApp final : public IApp {
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
    static constexpr uint8_t PRESET_COUNT = 4;
    static constexpr uint16_t MAX_GENERATED_TOKENS = 200;
    static constexpr uint32_t VIEW_UPDATE_INTERVAL_MS = 120;

    enum class Page : uint8_t {
        Presets,
        Generate,
    };

    Page page_ = Page::Presets;
    uint8_t selected_ = 0;
    int8_t renderedSelection_ = -1;
    uint32_t activeGeneration_ = 0;
    uint32_t lastViewUpdateMs_ = 0;
    bool storyDirty_ = false;

    lv_obj_t* root_ = nullptr;
    UiCard cards_[PRESET_COUNT];
    lv_obj_t* storyPanel_ = nullptr;
    lv_obj_t* storyLabel_ = nullptr;
    lv_obj_t* infoLabel_ = nullptr;
    lv_obj_t* hintLabel_ = nullptr;

    String storyText_;
    uint32_t utf8Codepoint_ = 0;
    uint32_t utf8Minimum_ = 0;
    uint8_t utf8Expected_ = 0;

    void startSelected(AppContext& context);
    void showPresets(AppContext& context);
    void showGeneration();
    void consumeTokens(AppContext& context);
    void appendByte(uint8_t value);
    void appendCodepoint(uint32_t codepoint);
    void resetDecoder();
    void refreshGenerationView(AppContext& context);
};
