#include "apps/NetworkSettingsApp.h"

#include "services/ServerService.h"
#include "services/WifiService.h"
#include "ui/UiRuntime.h"

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

void NetworkSettingsApp::onEnter(AppContext&) {
    lastViewSignature_ = "";
}

void NetworkSettingsApp::onExit(AppContext&) {
    setupStage_ = SetupStage::Inactive;
    root_ = nullptr;
    lastViewSignature_ = "";
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
    lastViewSignature_ = "";
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
    lastViewSignature_ = "";
}

lv_obj_t* NetworkSettingsApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot("NETWORK / RADIO", "Connectivity",
                                      "Wi-Fi station and wireless console");
    lastViewSignature_ = "";
    return root_;
}

void NetworkSettingsApp::onUpdateView(AppContext& context) {
    if (root_ == nullptr) {
        return;
    }
    const String signature = viewSignature(context);
    if (signature == lastViewSignature_) {
        return;
    }
    lastViewSignature_ = signature;
    rebuildView(context);
}

bool NetworkSettingsApp::handlesNavigation() const {
    return setupStage_ == SetupStage::Selecting;
}

void NetworkSettingsApp::startWizard(AppContext& context) {
    context.wifi.setEnabled(true);
    setupStage_ = SetupStage::Scanning;
    cursor_ = 0;
    windowStart_ = 0;
    lastViewSignature_ = "";
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
    lastViewSignature_ = "";
}

void NetworkSettingsApp::selectCurrent(AppContext& context) {
    if (!context.wifi.selectScanIndex(cursor_)) {
        return;
    }
    setupStage_ = SetupStage::AwaitingPassword;
    lastViewSignature_ = "";
    context.console.print(F("[wifi-ui] password required for "));
    context.console.print(context.wifi.selectedSsid());
    context.console.println(F("; use hidden host input"));
}

void NetworkSettingsApp::rebuildView(AppContext& context) {
    lv_obj_clean(root_);
    switch (setupStage_) {
        case SetupStage::Scanning:
            buildScanning(context);
            break;
        case SetupStage::Selecting:
            buildSelection(context);
            break;
        case SetupStage::AwaitingPassword:
            buildPassword(context);
            break;
        case SetupStage::Inactive:
        default:
            buildInactive(context, context.wifi.snapshot());
            break;
    }
}

void NetworkSettingsApp::buildHeader(AppContext& context,
                                     const char* eyebrow, const char* title,
                                     const char* subtitle) {
    context.ui.createLabel(root_, eyebrow, 16, 10, 12, context.ui.accent());
    context.ui.createLabel(root_, title, 16, 27, 24, context.ui.text());
    if (subtitle != nullptr) {
        context.ui.createLabel(root_, subtitle, 17, 57, 12, context.ui.muted());
    }
}

void NetworkSettingsApp::buildInactive(AppContext& context,
                                       const WifiSnapshot& snapshot) {
    buildHeader(context, "NETWORK / RADIO", "Connectivity",
                "Enter toggles Wi-Fi  /  W opens setup");

    const String title = snapshot.ssid.isEmpty()
                             ? String("Wi-Fi Station")
                             : shorten(snapshot.ssid, 24);
    String subtitle = context.wifi.stateName();
    if (snapshot.state == WifiState::Connected) {
        subtitle += "  /  2.4 GHz";
    }
    UiCard connection = context.ui.createCard(
        root_, 76, LV_SYMBOL_WIFI, title.c_str(), subtitle.c_str());
    connection.normalX = 12;
    connection.normalWidth = 296;
    context.ui.setCardFocused(connection,
                              snapshot.state == WifiState::Connected, false);

    lv_obj_t* value = nullptr;
    context.ui.createValueRow(root_, "IP ADDRESS", snapshot.ip.c_str(), 126,
                              &value);
    const String rssi = rssiText(snapshot);
    context.ui.createValueRow(root_, "SIGNAL", rssi.c_str(), 148, &value);
    const String retry = String(snapshot.reconnectCount);
    context.ui.createValueRow(root_, "RETRIES", retry.c_str(), 170, &value);

    const ServerSnapshot server = context.server.snapshot();
    String serverValue = server.host.isEmpty() ? String("not configured")
                                                : server.host + ":" + server.port;
    context.ui.createValueRow(root_, "TCP TARGET", serverValue.c_str(), 192,
                              &value);
}

void NetworkSettingsApp::buildScanning(AppContext& context) {
    buildHeader(context, "NETWORK / SETUP", "Scanning",
                "Looking for nearby 2.4 GHz networks");
    context.ui.createLabel(root_, LV_SYMBOL_REFRESH, 145, 89, 28,
                           context.ui.accent());
    context.ui.createLabel(root_, "Non-blocking radio scan", 74, 130, 16,
                           context.ui.text());
    context.ui.createLabel(root_, "The console and services remain responsive",
                           42, 157, 12, context.ui.muted());
}

void NetworkSettingsApp::buildSelection(AppContext& context) {
    buildHeader(context, "NETWORK / SETUP", "Choose a network",
                "Up / Down moves  /  Enter selects");
    const int16_t count = context.wifi.scanCount();
    for (uint8_t row = 0; row < VISIBLE_ROWS; ++row) {
        const int16_t index = windowStart_ + row;
        if (index >= count) {
            break;
        }
        const bool selected = index == cursor_;
        const int16_t y = 79 + static_cast<int16_t>(row) * 22;
        lv_obj_t* item = lv_obj_create(root_);
        lv_obj_remove_style_all(item);
        lv_obj_set_size(item, selected ? 296 : 284, 20);
        lv_obj_set_pos(item, selected ? 12 : 18, y);
        lv_obj_set_style_radius(item, 6, 0);
        lv_obj_set_style_bg_color(item,
                                  selected ? context.ui.accentSoft()
                                           : context.ui.panel(),
                                  0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(item, selected ? 1 : 0, 0);
        lv_obj_set_style_border_color(item, context.ui.accent(), 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        context.ui.createLabel(item, selected ? ">" : "", 7, 2, 12,
                               context.ui.accent());
        const String ssid = shorten(context.wifi.scanSsid(index), 25);
        context.ui.createLabel(item, ssid.c_str(), 22, 2, 12,
                               context.ui.text());
        const String signal = String(context.wifi.scanRssi(index)) + " dBm";
        lv_obj_t* signalLabel = context.ui.createLabel(
            item, signal.c_str(), 274, 2, 12, context.ui.muted());
        lv_obj_align(signalLabel, LV_ALIGN_RIGHT_MID, -7, 0);
    }
}

void NetworkSettingsApp::buildPassword(AppContext& context) {
    buildHeader(context, "NETWORK / SETUP", "Network selected",
                "Password entry stays on the trusted USB console");
    const String ssid = shorten(context.wifi.selectedSsid(), 28);
    UiCard selected = context.ui.createCard(root_, 84, LV_SYMBOL_WIFI,
                                            ssid.c_str(), "credential required");
    selected.normalX = 12;
    selected.normalWidth = 296;
    context.ui.setCardFocused(selected, true);
    context.ui.createLabel(root_, LV_SYMBOL_EYE_CLOSE, 24, 145, 20,
                           context.ui.accent());
    context.ui.createLabel(root_, "Type the password in playground_console",
                           58, 145, 12, context.ui.text());
    context.ui.createLabel(root_, "It will be stored in device NVS, never Git",
                           58, 170, 12, context.ui.muted());
}

String NetworkSettingsApp::viewSignature(AppContext& context) const {
    const WifiSnapshot wifi = context.wifi.snapshot();
    const ServerSnapshot server = context.server.snapshot();
    String signature = String(static_cast<uint8_t>(setupStage_));
    signature += '|';
    signature += static_cast<uint8_t>(wifi.state);
    signature += '|';
    signature += wifi.ssid;
    signature += '|';
    signature += wifi.ip;
    signature += '|';
    signature += wifi.rssi;
    signature += '|';
    signature += wifi.scanCount;
    signature += '|';
    signature += cursor_;
    signature += '|';
    signature += windowStart_;
    signature += '|';
    signature += server.host;
    signature += '|';
    signature += server.port;
    signature += '|';
    signature += static_cast<uint8_t>(server.state);
    return signature;
}
