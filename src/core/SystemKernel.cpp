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

bool isRemoteAllowed(AppCommandType type) {
    switch (type) {
        case AppCommandType::Previous:
        case AppCommandType::Next:
        case AppCommandType::Left:
        case AppCommandType::Right:
        case AppCommandType::Activate:
        case AppCommandType::Back:
        case AppCommandType::Home:
        case AppCommandType::PageSystem:
        case AppCommandType::PageDisplay:
        case AppCommandType::PageNetwork:
        case AppCommandType::ColorTest:
        case AppCommandType::Status:
        case AppCommandType::WifiStatus:
        case AppCommandType::ServerStatus:
        case AppCommandType::BenchStatus:
            return true;
        default:
            return false;
    }
}

}  // namespace

SystemKernel::SystemKernel()
    : ui_(display_),
      context_{display_, wifi_, server_, Serial, ui_},
      appManager_(context_) {}

void SystemKernel::setup() {
    Serial.begin(SERIAL_BAUD);
    delay(50);
    display_.begin();
    uiReady_ = ui_.begin();
    wifi_.begin(Serial);
    server_.begin(Serial);
    benchmark_.begin(Serial);
    console_.begin(Serial);

    if (uiReady_) {
        appManager_.registerApp(systemInfoApp_);
        appManager_.registerApp(displayTestApp_);
        appManager_.registerApp(networkSettingsApp_);
        appManager_.registerApp(launcherApp_);
        appManager_.begin(AppId::Launcher);
        ui_.updateStatus(wifi_.snapshot(), server_.snapshot(), millis());
        ui_.tick();
        display_.pushBacklightOn();
    } else {
        Serial.println(F("[ui] startup failed; display backlight remains off"));
    }

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
        if (!inputRouter_.push(command, InputSource::Usb)) {
            Serial.println(F("[input] USB command dropped: queue full"));
        }
    }

    wifi_.tick(nowMs);
    server_.tick(nowMs, wifi_.snapshot());
    benchmark_.tick(nowMs);
    ui_.updateStatus(wifi_.snapshot(), server_.snapshot(), nowMs);

    uint32_t requestId = 0;
    String remoteLine;
    while (server_.pollCommand(requestId, remoteLine)) {
        AppCommand remoteCommand;
        if (!ConsoleService::parseLine(remoteLine, remoteCommand)) {
            server_.sendAck(requestId, false, "parse_failed");
        } else if (!inputRouter_.push(remoteCommand, InputSource::Tcp,
                                      requestId)) {
            server_.sendAck(requestId, false, "input_queue_full");
        }
    }

    RoutedCommand routed;
    while (inputRouter_.poll(routed)) {
        handleCommand(routed);
    }

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
            benchmark_.printStatus(Serial);
        }
    }
    ui_.tick();
}

bool SystemKernel::handleCommand(const RoutedCommand& routed) {
    const AppCommand& command = routed.command;
    if (routed.source == InputSource::Tcp && !isRemoteAllowed(command.type)) {
        server_.sendAck(routed.requestId, false, "command_not_allowed");
        return false;
    }

    bool handled = true;
    switch (command.type) {
        case AppCommandType::Back:
        case AppCommandType::Home:
            appManager_.activate(AppId::Launcher, UiPageTransition::Backward);
            break;
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
        case AppCommandType::BenchUpload: {
            const ServerSnapshot server = server_.snapshot();
            if (server.host.isEmpty() || command.number <= 0) {
                Serial.println(F("[bench] configure server target and byte count"));
                handled = false;
            } else {
                handled = benchmark_.startUpload(
                    server.host, BenchmarkService::DEFAULT_PORT,
                    static_cast<uint32_t>(command.number));
            }
            break;
        }
        case AppCommandType::BenchDownload: {
            const ServerSnapshot server = server_.snapshot();
            if (server.host.isEmpty() || command.number <= 0) {
                Serial.println(F("[bench] configure server target and byte count"));
                handled = false;
            } else {
                handled = benchmark_.startDownload(
                    server.host, BenchmarkService::DEFAULT_PORT,
                    static_cast<uint32_t>(command.number));
            }
            break;
        }
        case AppCommandType::BenchStatus:
            benchmark_.printStatus(Serial);
            break;
        case AppCommandType::BenchCancel:
            benchmark_.cancel();
            break;
        case AppCommandType::Help:
            ConsoleService::printHelp(Serial);
            break;
        case AppCommandType::Unknown:
            Serial.print(F("unknown command: "));
            Serial.println(command.value);
            ConsoleService::printHelp(Serial);
            handled = false;
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
    if (routed.source == InputSource::Tcp) {
        server_.sendAck(routed.requestId, handled, handled ? "accepted" : "unknown");
        if (handled && command.type == AppCommandType::Status) {
            server_.sendState(routed.requestId, appManager_.currentName(),
                              wifi_.snapshot());
        }
    }
    return handled;
}

void SystemKernel::printStatus() {
    Serial.print(F("[status] app="));
    Serial.print(appManager_.currentName());
    Serial.print(F(" heap="));
    Serial.print(ESP.getFreeHeap());
    Serial.print(F(" psram="));
    Serial.println(ESP.getFreePsram());
    wifi_.printStatus(Serial);
    server_.printStatus(Serial);
    benchmark_.printStatus(Serial);
}

void SystemKernel::requestRedraw() {
    redrawRequested_ = true;
}
