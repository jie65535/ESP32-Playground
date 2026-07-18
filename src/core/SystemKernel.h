#pragma once

#include "apps/DisplayTestApp.h"
#include "apps/LauncherApp.h"
#include "apps/NetworkSettingsApp.h"
#include "apps/SystemInfoApp.h"
#include "core/AppManager.h"
#include "services/ConsoleService.h"
#include "services/DisplayService.h"
#include "services/WifiService.h"

class SystemKernel {
public:
    SystemKernel();
    void setup();
    void loop();

private:
    DisplayService display_;
    WifiService wifi_;
    ConsoleService console_;
    AppContext context_;
    AppManager appManager_;
    SystemInfoApp systemInfoApp_;
    DisplayTestApp displayTestApp_;
    NetworkSettingsApp networkSettingsApp_;
    LauncherApp launcherApp_;
    uint32_t lastRenderMs_ = 0;
    uint32_t lastStatusMs_ = 0;
    bool redrawRequested_ = true;

    void handleCommand(const AppCommand& command);
    void printStatus();
    void requestRedraw();
};
