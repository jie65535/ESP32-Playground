#include "core/SystemKernel.h"

namespace {

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t RENDER_INTERVAL_MS = 1000;
constexpr uint32_t SERIAL_STATUS_INTERVAL_MS = 10000;

bool isWifiCommand(AppCommandType type) {
    switch (type) {
        case AppCommandType::WifiScan:
        case AppCommandType::WifiSelect:
        case AppCommandType::WifiSsid:
        case AppCommandType::WifiPassword:
        case AppCommandType::WifiOpen:
        case AppCommandType::WifiStatus:
        case AppCommandType::WifiReconnect:
        case AppCommandType::WifiClear:
        case AppCommandType::WifiOn:
        case AppCommandType::WifiOff:
        case AppCommandType::WifiToggle:
        case AppCommandType::WifiWizard:
        case AppCommandType::WifiWizardCancel:
        case AppCommandType::WifiHelp:
            return true;
        default:
            return false;
    }
}

bool isServerCommand(AppCommandType type) {
    switch (type) {
        case AppCommandType::ServerSet:
        case AppCommandType::ServerStatus:
        case AppCommandType::ServerOn:
        case AppCommandType::ServerOff:
        case AppCommandType::ServerToggle:
        case AppCommandType::ServerConnect:
        case AppCommandType::ServerClear:
        case AppCommandType::ServerHelp:
            return true;
        default:
            return false;
    }
}

}  // namespace

SystemKernel::SystemKernel()
    : context_{display_, wifi_, server_, Serial}, appManager_(context_) {}

void SystemKernel::setup() {
    Serial.begin(SERIAL_BAUD);
    delay(50);
    display_.begin();
    wifi_.begin(Serial);
    server_.begin(Serial);
    console_.begin(Serial);

    appManager_.registerApp(systemInfoApp_);
    appManager_.registerApp(displayTestApp_);
    appManager_.registerApp(networkSettingsApp_);
    appManager_.registerApp(launcherApp_);
    appManager_.begin(AppId::SystemInfo);

    display_.startFrame();
    appManager_.render();
    display_.pushBacklightOn();

    Serial.println();
    Serial.printf("PlaygroundOS started build=%s %s\n", __DATE__, __TIME__);
    Serial.printf("chip=%s flash=%uMB psram=%s psram=%uMB\n",
                  ESP.getChipModel(),
                  static_cast<unsigned>(ESP.getFlashChipSize() / (1024U * 1024U)),
                  psramFound() ? "yes" : "no",
                  static_cast<unsigned>(ESP.getPsramSize() / (1024U * 1024U)));
    ConsoleService::printHelp(Serial);
    lastRenderMs_ = millis();
    lastStatusMs_ = lastRenderMs_;
    redrawRequested_ = false;
}

void SystemKernel::loop() {
    const uint32_t nowMs = millis();
    AppCommand command;
    while (console_.poll(command)) {
        handleCommand(command);
    }

    wifi_.tick(nowMs);
    server_.tick(nowMs, wifi_.snapshot());
    appManager_.tick(nowMs);
    if (redrawRequested_ || nowMs - lastRenderMs_ >= RENDER_INTERVAL_MS) {
        lastRenderMs_ = nowMs;
        appManager_.render();
        redrawRequested_ = false;
        if (nowMs - lastStatusMs_ >= SERIAL_STATUS_INTERVAL_MS) {
            lastStatusMs_ = nowMs;
            Serial.print(F("[status] uptime="));
            Serial.print(nowMs / 1000U);
            Serial.print(F("s app="));
            Serial.print(appManager_.currentName());
            Serial.print(F(" heap="));
            Serial.print(ESP.getFreeHeap());
            Serial.print(F(" psram="));
            Serial.println(ESP.getFreePsram());
            wifi_.printStatus(Serial);
            server_.printStatus(Serial);
        }
    }
}

void SystemKernel::handleCommand(const AppCommand& command) {
    switch (command.type) {
        case AppCommandType::PageSystem:
            appManager_.activate(AppId::SystemInfo);
            break;
        case AppCommandType::PageDisplay:
            appManager_.activate(AppId::DisplayTest);
            break;
        case AppCommandType::PageNetwork:
            appManager_.activate(AppId::NetworkSettings);
            break;
        case AppCommandType::ColorTest:
            appManager_.activate(AppId::DisplayTest);
            appManager_.handleCommand(command);
            break;
        case AppCommandType::Screenshot:
            display_.writeScreenshot(Serial);
            break;
        case AppCommandType::Status:
            printStatus();
            break;
        case AppCommandType::Help:
            ConsoleService::printHelp(Serial);
            break;
        case AppCommandType::Unknown:
            Serial.print(F("unknown command: "));
            Serial.println(command.value);
            ConsoleService::printHelp(Serial);
            break;
        default:
            if (isWifiCommand(command.type) || isServerCommand(command.type)) {
                appManager_.activate(AppId::NetworkSettings);
                appManager_.handleCommand(command);
            } else {
                appManager_.handleCommand(command);
            }
            break;
    }
    requestRedraw();
}

void SystemKernel::printStatus() {
    Serial.print(F("[status] app="));
    Serial.print(appManager_.currentName());
    Serial.print(F(" heap="));
    Serial.print(ESP.getFreeHeap());
    Serial.print(F(" psram="));
    Serial.println(ESP.getFreePsram());
    wifi_.printStatus(Serial);
}

void SystemKernel::requestRedraw() {
    redrawRequested_ = true;
}
