#include "apps/NetworkSettingsApp.h"

#include "services/DisplayService.h"
#include "services/ServerService.h"
#include "services/WifiService.h"

namespace {

String rssiText(const WifiSnapshot& snapshot) {
    return snapshot.state == WifiState::Connected
               ? String(snapshot.rssi) + " dBm"
               : String("--");
}

String shorten(const String& value, size_t maxLength) {
    if (value.length() <= maxLength) {
        return value;
    }
    return value.substring(0, maxLength - 3U) + "...";
}

}  // namespace

AppId NetworkSettingsApp::id() const {
    return AppId::NetworkSettings;
}

const char* NetworkSettingsApp::name() const {
    return "Network";
}

void NetworkSettingsApp::onEnter(AppContext&) {}

void NetworkSettingsApp::onExit(AppContext&) {
    setupStage_ = SetupStage::Inactive;
}

void NetworkSettingsApp::onCommand(const AppCommand& command,
                                   AppContext& context) {
    WifiService& wifi = context.wifi;
    switch (command.type) {
        case AppCommandType::Previous:
            if (setupStage_ == SetupStage::Selecting) {
                moveCursor(-1, context);
            }
            break;
        case AppCommandType::Next:
            if (setupStage_ == SetupStage::Selecting) {
                moveCursor(1, context);
            }
            break;
        case AppCommandType::Activate:
            if (setupStage_ == SetupStage::Selecting) {
                selectCurrent(context);
            } else if (setupStage_ == SetupStage::Inactive) {
                wifi.toggleEnabled();
            }
            break;
        case AppCommandType::WifiWizard:
            startWizard(context);
            break;
        case AppCommandType::WifiWizardCancel:
            setupStage_ = SetupStage::Inactive;
            context.console.println(F("[wifi-ui] setup cancelled"));
            break;
        case AppCommandType::WifiScan:
            wifi.startScan();
            break;
        case AppCommandType::WifiSelect:
            wifi.selectScanIndex(command.number);
            break;
        case AppCommandType::WifiSsid:
            wifi.selectSsid(command.value);
            break;
        case AppCommandType::WifiPassword:
            if (setupStage_ == SetupStage::AwaitingPassword) {
                if (wifi.saveSelectedPassword(command.value)) {
                    setupStage_ = SetupStage::Inactive;
                    context.console.println(F("[wifi-ui] credentials saved"));
                }
            } else {
                wifi.saveSelectedPassword(command.value);
            }
            break;
        case AppCommandType::WifiOpen:
            if (setupStage_ == SetupStage::AwaitingPassword) {
                if (wifi.saveSelectedOpen()) {
                    setupStage_ = SetupStage::Inactive;
                    context.console.println(F("[wifi-ui] open network saved"));
                }
            } else {
                wifi.saveSelectedOpen();
            }
            break;
        case AppCommandType::WifiStatus:
            wifi.printStatus(context.console);
            break;
        case AppCommandType::WifiReconnect:
            wifi.reconnect();
            break;
        case AppCommandType::WifiClear:
            wifi.clearCredentials();
            setupStage_ = SetupStage::Inactive;
            break;
        case AppCommandType::WifiOn:
            wifi.setEnabled(true);
            break;
        case AppCommandType::WifiOff:
            wifi.setEnabled(false);
            setupStage_ = SetupStage::Inactive;
            break;
        case AppCommandType::WifiToggle:
            wifi.toggleEnabled();
            break;
        case AppCommandType::WifiHelp:
            context.console.println(F("Wi-Fi: wifi on|off|toggle|scan|wizard"));
            context.console.println(F("       wifi select <index> | wifi ssid <name>"));
            context.console.println(F("       wifi password <value> | wifi open"));
            context.console.println(F("       wifi status | wifi reconnect | wifi clear"));
            break;
        case AppCommandType::ServerSet:
            context.server.setTarget(command.value, command.number);
            break;
        case AppCommandType::ServerStatus:
            context.server.printStatus(context.console);
            break;
        case AppCommandType::ServerOn:
            context.server.setEnabled(true);
            break;
        case AppCommandType::ServerOff:
            context.server.setEnabled(false);
            break;
        case AppCommandType::ServerToggle:
            context.server.toggleEnabled();
            break;
        case AppCommandType::ServerConnect:
            context.server.connectNow();
            break;
        case AppCommandType::ServerClear:
            context.server.clearTarget();
            break;
        case AppCommandType::ServerHelp:
            context.console.println(F("Server: server set <host> <port>"));
            context.console.println(F("        server status | server on | server off"));
            context.console.println(F("        server connect | server clear"));
            break;
        default:
            break;
    }
}

void NetworkSettingsApp::onTick(uint32_t, AppContext& context) {
    if (setupStage_ != SetupStage::Scanning) {
        return;
    }
    const WifiSnapshot snapshot = context.wifi.snapshot();
    if (snapshot.state == WifiState::Scanning) {
        return;
    }
    if (snapshot.scanCount > 0) {
        setupStage_ = SetupStage::Selecting;
        cursor_ = 0;
        windowStart_ = 0;
        context.console.print(F("[wifi-ui] networks ready: "));
        context.console.print(snapshot.scanCount);
        context.console.println(F("; use up/down/ok"));
    } else {
        setupStage_ = SetupStage::Inactive;
        context.console.println(F("[wifi-ui] scan produced no usable networks"));
    }
}

void NetworkSettingsApp::onRender(AppContext& context) {
    if (setupStage_ != SetupStage::Inactive) {
        drawWizard(context);
        return;
    }

    DisplayService& display = context.display;
    const WifiSnapshot snapshot = context.wifi.snapshot();
    display.startFrame();
    display.drawHeader("NETWORK");
    display.drawText("Wi-Fi Station", 16, 44, TFT_WHITE, BitmapFontSize::Bold12);
    display.drawText("2.4 GHz Station", 16, 64, TFT_WHITE,
                     BitmapFontSize::Small12);
    display.drawValue("State", context.wifi.stateName(), 88);
    display.drawValue("SSID", snapshot.ssid.isEmpty() ? String("--")
                                                        : shorten(snapshot.ssid, 19),
                      112);
    display.drawValue("IP", snapshot.ip, 136);
    display.drawValue("RSSI", rssiText(snapshot), 160);
    display.drawValue("Retry", String(snapshot.reconnectCount), 184);
    if (snapshot.state == WifiState::Scanning) {
        display.drawFooter("scanning networks...");
    } else if (snapshot.scanCount >= 0) {
        display.drawFooter(String("scan ready: ") + snapshot.scanCount +
                           " network(s)  W: setup");
    } else if (!snapshot.lastError.isEmpty()) {
        display.drawFooter(snapshot.lastError);
    } else {
        display.drawFooter("Enter: Wi-Fi on/off   W: setup");
    }
    display.pushFrame();
}

bool NetworkSettingsApp::handlesNavigation() const {
    return setupStage_ == SetupStage::Selecting;
}

void NetworkSettingsApp::startWizard(AppContext& context) {
    context.wifi.setEnabled(true);
    setupStage_ = SetupStage::Scanning;
    cursor_ = 0;
    windowStart_ = 0;
    context.console.println(F("[wifi-ui] scanning; please wait"));
    context.wifi.startScan();
}

void NetworkSettingsApp::moveCursor(int16_t delta, AppContext& context) {
    const int16_t count = context.wifi.scanCount();
    if (count <= 0) {
        return;
    }
    if (delta < 0) {
        cursor_ = cursor_ == 0 ? count - 1 : cursor_ - 1;
    } else {
        cursor_ = static_cast<int16_t>((cursor_ + 1) % count);
    }
    if (cursor_ < windowStart_) {
        windowStart_ = cursor_;
    } else if (cursor_ >= windowStart_ + VISIBLE_ROWS) {
        windowStart_ = cursor_ - VISIBLE_ROWS + 1;
    }
}

void NetworkSettingsApp::selectCurrent(AppContext& context) {
    if (!context.wifi.selectScanIndex(cursor_)) {
        return;
    }
    setupStage_ = SetupStage::AwaitingPassword;
    context.console.print(F("[wifi-ui] password required for "));
    context.console.print(context.wifi.selectedSsid());
    context.console.println(F("; use hidden host input"));
}

void NetworkSettingsApp::drawWizard(AppContext& context) {
    DisplayService& display = context.display;
    display.startFrame();
    display.drawHeader("WIFI SETUP");
    if (setupStage_ == SetupStage::Scanning) {
        display.drawText("Scanning 2.4 GHz networks...", 16, 78, TFT_WHITE,
                         BitmapFontSize::Bold12);
        display.drawFooter("please wait...");
        display.pushFrame();
        return;
    }

    if (setupStage_ == SetupStage::AwaitingPassword) {
        display.drawText("Network selected", 16, 54, TFT_LIGHTGREY,
                         BitmapFontSize::Small12);
        display.drawText(shorten(context.wifi.selectedSsid(), 25), 16, 82,
                         TFT_WHITE, BitmapFontSize::Bold12);
        display.drawText("Enter password in host console", 16, 122, TFT_YELLOW,
                         BitmapFontSize::Small12);
        display.drawFooter("password is hidden   Q: cancel");
        display.pushFrame();
        return;
    }

    display.drawText("Up/Down select, Enter connect", 12, 36, TFT_LIGHTGREY,
                     BitmapFontSize::Small12);
    const int16_t count = context.wifi.scanCount();
    for (uint8_t row = 0; row < VISIBLE_ROWS; ++row) {
        const int16_t index = windowStart_ + row;
        if (index >= count) {
            break;
        }
        const int16_t y = 56 + static_cast<int16_t>(row) * 20;
        if (index == cursor_) {
            display.fillRect(8, y - 3, 304, 18, TFT_DARKCYAN);
            display.drawText(">", 12, y, TFT_YELLOW, BitmapFontSize::Bold12);
        }
        display.drawText(String(index), 28, y, TFT_LIGHTGREY,
                         BitmapFontSize::Small12);
        display.drawText(shorten(context.wifi.scanSsid(index), 22), 52, y,
                         TFT_WHITE, BitmapFontSize::Small12);
        display.drawText(String(context.wifi.scanRssi(index)) + "dBm", 304, y,
                         TFT_LIGHTGREY, BitmapFontSize::Small12,
                         BitmapTextAlign::Right);
    }
    display.drawFooter("up/down move   enter select   Q cancel");
    display.pushFrame();
}
