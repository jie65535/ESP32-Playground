#pragma once

#include "core/App.h"

class AppManager {
public:
    explicit AppManager(AppContext& context);

    void registerApp(IApp& app);
    void begin(AppId initial);
    void handleCommand(const AppCommand& command);
    void tick(uint32_t nowMs);
    void render();
    void nextApp();
    void previousApp();
    bool activate(AppId id);
    AppId currentId() const;
    const char* currentName() const;

private:
    static constexpr uint8_t MAX_APPS = 8;
    AppContext& context_;
    IApp* apps_[MAX_APPS] = {};
    uint8_t appCount_ = 0;
    uint8_t currentIndex_ = 0;
    bool active_ = false;
};
