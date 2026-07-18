#pragma once

#include "apps/DisplayTestApp.h"
#include "apps/DisplaySettingsApp.h"
#include "apps/SoundSettingsApp.h"
#include "apps/LauncherApp.h"
#include "apps/NetworkSettingsApp.h"
#include "apps/SystemInfoApp.h"
#include "core/AppManager.h"
#include "core/InputRouter.h"
#include "services/ConsoleService.h"
#include "services/BenchmarkService.h"
#include "services/AudioService.h"
#include "services/DisplayService.h"
#include "services/ServerService.h"
#include "services/WifiService.h"
#include "ui/UiRuntime.h"

class SystemKernel {
public:
    SystemKernel();
    void setup();
    void loop();

private:
    DisplayService display_;
    UiRuntime ui_;
    AudioService audio_;
    WifiService wifi_;
    ServerService server_;
    BenchmarkService benchmark_;
    ConsoleService console_;
    AppContext context_;
    AppManager appManager_;
    InputRouter inputRouter_;
    SystemInfoApp systemInfoApp_;
    DisplayTestApp displayTestApp_;
    DisplaySettingsApp displaySettingsApp_;
    SoundSettingsApp soundSettingsApp_;
    NetworkSettingsApp networkSettingsApp_;
    LauncherApp launcherApp_;
    uint32_t lastRenderMs_ = 0;
    uint32_t lastStatusMs_ = 0;
    bool redrawRequested_ = true;
    bool uiReady_ = false;

    bool handleCommand(const RoutedCommand& routed);
    void printStatus();
    void requestRedraw();
};
