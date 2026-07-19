#pragma once

#include "apps/DisplayTestApp.h"
#include "apps/DisplaySettingsApp.h"
#include "apps/SoundSettingsApp.h"
#include "apps/RgbSettingsApp.h"
#include "apps/ControllerSettingsApp.h"
#include "apps/ConsoleSettingsApp.h"
#include "apps/LauncherApp.h"
#include "apps/NetworkSettingsApp.h"
#include "apps/SystemInfoApp.h"
#include "apps/TimeApp.h"
#include "core/AppManager.h"
#include "core/InputRouter.h"
#include "services/ConsoleService.h"
#include "services/BenchmarkService.h"
#include "services/AudioService.h"
#include "services/BleGamepadService.h"
#include "services/DisplayService.h"
#include "services/I2cBusService.h"
#include "services/RgbService.h"
#include "services/ServerService.h"
#include "services/MirrorService.h"
#include "services/RuntimeMonitorService.h"
#include "services/TimeService.h"
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
    I2cBusService i2c_;
    AudioService audio_;
    TimeService time_;
    RgbService rgb_;
    WifiService wifi_;
    ServerService server_;
    BleGamepadService gamepad_;
    MirrorService mirror_;
    BenchmarkService benchmark_;
    ConsoleService console_;
    RuntimeMonitorService runtime_;
    AppContext context_;
    AppManager appManager_;
    InputRouter inputRouter_;
    SystemInfoApp systemInfoApp_;
    TimeApp timeApp_;
    DisplayTestApp displayTestApp_;
    DisplaySettingsApp displaySettingsApp_;
    SoundSettingsApp soundSettingsApp_;
    RgbSettingsApp rgbSettingsApp_;
    ControllerSettingsApp controllerSettingsApp_;
    ConsoleSettingsApp consoleSettingsApp_;
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
