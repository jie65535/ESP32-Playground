#include "apps/NetworkSettingsApp.h"

#include "services/WifiService.h"
#include "ui/UiRuntime.h"

namespace {

String rssiText(const WifiSnapshot& snapshot) {
    return snapshot.state == WifiState::Connected
               ? String(snapshot.rssi) + " dBm"
               : String("--");
}

const char* wifiStateText(WifiState state) {
    switch (state) {
        case WifiState::Disabled: return "已关闭";
        case WifiState::NoCredentials: return "等待配置";
        case WifiState::Ready: return "准备连接";
        case WifiState::Scanning: return "正在扫描";
        case WifiState::Connecting: return "正在连接";
        case WifiState::Connected: return "已连接";
        case WifiState::Backoff: return "稍后重试";
        default: return "未知";
    }
}

String shorten(const String& value, size_t maxLength) {
    if (value.length() <= maxLength) {
        return value;
    }
    return value.substring(0, maxLength - 3U) + "...";
}

String choiceText(const char* value) {
    return String("< ") + value + " >";
}

}  // namespace

AppId NetworkSettingsApp::id() const {
    return AppId::NetworkSettings;
}

const char* NetworkSettingsApp::name() const {
    return "Network";
}

void NetworkSettingsApp::onEnter(AppContext& context) {
    setupStage_ = SetupStage::Inactive;
    inactiveCursor_ = context.wifi.snapshot().ssid.isEmpty() ? 1U : 0U;
    setupNotice_ = "";
    passwordError_ = "";
    lastViewSignature_ = "";
}

void NetworkSettingsApp::onExit(AppContext&) {
    setupStage_ = SetupStage::Inactive;
    root_ = nullptr;
    inputMetaLabel_ = nullptr;
    keyboard_.reset();
    setupNotice_ = "";
    passwordError_ = "";
    lastViewSignature_ = "";
}

void NetworkSettingsApp::onCommand(const AppCommand& command,
                                   AppContext& context) {
    WifiService& wifi = context.wifi;

    if (setupStage_ == SetupStage::EnteringPassword) {
        if (keyboard_.handleNavigation(command.type)) {
            return;
        }
        if (command.type == AppCommandType::QuickDrop) {
            keyboard_.cycleMode();
            return;
        }
        if (command.type == AppCommandType::Activate) {
            const OnScreenKeyboardAction action = keyboard_.activate();
            if (action == OnScreenKeyboardAction::Ready) {
                submitPassword(context);
            } else if (action == OnScreenKeyboardAction::Edited) {
                passwordError_ = "";
                updatePasswordMeta(context);
            }
            return;
        }
    }

    switch (command.type) {
        case AppCommandType::Previous:
            if (setupStage_ == SetupStage::Selecting) {
                moveCursor(-1, context);
            } else if (setupStage_ == SetupStage::Inactive) {
                moveInactiveCursor(-1);
            }
            break;
        case AppCommandType::Next:
            if (setupStage_ == SetupStage::Selecting) {
                moveCursor(1, context);
            } else if (setupStage_ == SetupStage::Inactive) {
                moveInactiveCursor(1);
            }
            break;
        case AppCommandType::Left:
            if (setupStage_ == SetupStage::Selecting) {
                moveCursor(-1, context);
            } else if (setupStage_ == SetupStage::Inactive &&
                       inactiveCursor_ == 0 && wifi.enabled()) {
                wifi.setEnabled(false);
                lastViewSignature_ = "";
            }
            break;
        case AppCommandType::Right:
            if (setupStage_ == SetupStage::Selecting) {
                moveCursor(1, context);
            } else if (setupStage_ == SetupStage::Inactive &&
                       inactiveCursor_ == 0 && !wifi.enabled()) {
                wifi.setEnabled(true);
                lastViewSignature_ = "";
            }
            break;
        case AppCommandType::Activate:
            if (setupStage_ == SetupStage::Selecting) {
                selectCurrent(context);
            } else if (setupStage_ == SetupStage::Inactive) {
                activateInactive(context);
            }
            break;
        case AppCommandType::WifiWizard:
            startWizard(context);
            break;
        case AppCommandType::WifiWizardCancel:
            cancelWizard(context);
            context.console.println(F("[wifi-ui] setup cancelled"));
            break;
        case AppCommandType::WifiScan:
            wifi.startScan();
            lastViewSignature_ = "";
            break;
        case AppCommandType::WifiSelect:
            wifi.selectScanIndex(command.number);
            lastViewSignature_ = "";
            break;
        case AppCommandType::WifiSsid:
            wifi.selectSsid(command.value);
            lastViewSignature_ = "";
            break;
        case AppCommandType::WifiPassword:
            if (wifi.saveSelectedPassword(command.value)) {
                setupStage_ = SetupStage::Inactive;
                passwordError_ = "";
                context.console.println(F("[wifi-ui] credentials saved"));
            }
            lastViewSignature_ = "";
            break;
        case AppCommandType::WifiOpen:
            if (wifi.saveSelectedOpen()) {
                setupStage_ = SetupStage::Inactive;
                passwordError_ = "";
                context.console.println(F("[wifi-ui] open network saved"));
            }
            lastViewSignature_ = "";
            break;
        case AppCommandType::WifiStatus:
            wifi.printStatus(context.console);
            break;
        case AppCommandType::WifiReconnect:
            wifi.reconnect();
            lastViewSignature_ = "";
            break;
        case AppCommandType::WifiClear:
            wifi.clearCredentials();
            setupStage_ = SetupStage::Inactive;
            inactiveCursor_ = 1;
            lastViewSignature_ = "";
            break;
        case AppCommandType::WifiOn:
            wifi.setEnabled(true);
            lastViewSignature_ = "";
            break;
        case AppCommandType::WifiOff:
            wifi.setEnabled(false);
            setupStage_ = SetupStage::Inactive;
            lastViewSignature_ = "";
            break;
        case AppCommandType::WifiToggle:
            wifi.toggleEnabled();
            lastViewSignature_ = "";
            break;
        case AppCommandType::WifiHelp:
            context.console.println(F("Wi-Fi: wifi on|off|toggle|scan|wizard"));
            context.console.println(F("       wifi select <index> | wifi ssid <name>"));
            context.console.println(F("       wifi password <value> | wifi open"));
            context.console.println(F("       wifi status | wifi reconnect | wifi clear"));
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
        setupNotice_ = "";
        context.console.print(F("[wifi-ui] networks ready: "));
        context.console.print(snapshot.scanCount);
        context.console.println(F("; use arrows/ok"));
    } else {
        setupStage_ = SetupStage::Inactive;
        setupNotice_ = snapshot.lastError.isEmpty()
                           ? String("未发现可用网络")
                           : String("扫描失败，请重试");
        context.console.println(F("[wifi-ui] scan produced no usable networks"));
    }
    lastViewSignature_ = "";
}

lv_obj_t* NetworkSettingsApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot(nullptr, "Network");
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

bool NetworkSettingsApp::onBack(AppContext& context) {
    switch (setupStage_) {
        case SetupStage::EnteringPassword:
            setupStage_ = SetupStage::Selecting;
            passwordError_ = "";
            lastViewSignature_ = "";
            return true;
        case SetupStage::Selecting:
            setupStage_ = SetupStage::Inactive;
            setupNotice_ = "";
            lastViewSignature_ = "";
            return true;
        case SetupStage::Scanning:
            cancelWizard(context);
            return true;
        case SetupStage::Inactive:
        default:
            return false;
    }
}

void NetworkSettingsApp::startWizard(AppContext& context) {
    if (!context.wifi.enabled()) {
        context.wifi.setEnabled(true);
    }
    setupStage_ = SetupStage::Scanning;
    cursor_ = 0;
    windowStart_ = 0;
    setupNotice_ = "";
    passwordError_ = "";
    lastViewSignature_ = "";
    context.console.println(F("[wifi-ui] scanning; please wait"));
    if (!context.wifi.startScan()) {
        setupStage_ = SetupStage::Inactive;
        setupNotice_ = "扫描无法启动";
    }
}

void NetworkSettingsApp::cancelWizard(AppContext& context) {
    if (setupStage_ == SetupStage::Scanning) {
        context.wifi.cancelScan();
    }
    setupStage_ = SetupStage::Inactive;
    setupNotice_ = "";
    passwordError_ = "";
    lastViewSignature_ = "";
}

void NetworkSettingsApp::moveInactiveCursor(int16_t delta) {
    if (delta < 0) {
        inactiveCursor_ = inactiveCursor_ == 0
                              ? INACTIVE_ITEM_COUNT - 1U
                              : inactiveCursor_ - 1U;
    } else {
        inactiveCursor_ = static_cast<uint8_t>(
            (inactiveCursor_ + 1U) % INACTIVE_ITEM_COUNT);
    }
    setupNotice_ = "";
    lastViewSignature_ = "";
}

void NetworkSettingsApp::activateInactive(AppContext& context) {
    switch (inactiveCursor_) {
        case 0:
            context.wifi.toggleEnabled();
            break;
        case 1:
            startWizard(context);
            return;
        case 2:
            if (!context.wifi.snapshot().ssid.isEmpty()) {
                context.wifi.reconnect();
            } else {
                setupNotice_ = "尚未保存网络";
            }
            break;
        default:
            break;
    }
    lastViewSignature_ = "";
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
    setupNotice_ = "";
    lastViewSignature_ = "";
}

void NetworkSettingsApp::selectCurrent(AppContext& context) {
    const bool open = context.wifi.scanIsOpen(cursor_);
    if (!context.wifi.selectScanIndex(cursor_)) {
        setupNotice_ = "隐藏网络暂不支持设备端配置";
        lastViewSignature_ = "";
        return;
    }

    if (open) {
        if (context.wifi.saveSelectedOpen()) {
            setupStage_ = SetupStage::Inactive;
            setupNotice_ = "开放网络已保存";
            context.console.println(F("[wifi-ui] open network saved"));
        } else {
            setupNotice_ = "网络保存失败";
        }
    } else {
        setupStage_ = SetupStage::EnteringPassword;
        passwordError_ = "";
        context.console.print(F("[wifi-ui] on-device password entry for "));
        context.console.println(context.wifi.selectedSsid());
    }
    lastViewSignature_ = "";
}

void NetworkSettingsApp::submitPassword(AppContext& context) {
    const size_t passwordLength = keyboard_.length();
    if (passwordLength < 8U) {
        passwordError_ = "密码至少需要 8 个字符";
        updatePasswordMeta(context);
        return;
    }
    if (passwordLength > 63U) {
        passwordError_ = "密码不能超过 63 个字符";
        updatePasswordMeta(context);
        return;
    }

    const String password(keyboard_.text());
    if (!context.wifi.saveSelectedPassword(password)) {
        passwordError_ = "凭据保存失败";
        updatePasswordMeta(context);
        return;
    }

    keyboard_.clear();
    setupStage_ = SetupStage::Inactive;
    setupNotice_ = "凭据已保存，正在连接";
    passwordError_ = "";
    lastViewSignature_ = "";
    context.console.println(F("[wifi-ui] credentials saved (password hidden)"));
}

void NetworkSettingsApp::updatePasswordMeta(AppContext& context) {
    if (inputMetaLabel_ == nullptr) {
        return;
    }
    if (!passwordError_.isEmpty()) {
        lv_label_set_text(inputMetaLabel_, passwordError_.c_str());
        lv_obj_set_style_text_color(inputMetaLabel_, context.ui.accent(), 0);
        return;
    }
    const String value = String(keyboard_.length()) + " / 63";
    lv_label_set_text(inputMetaLabel_, value.c_str());
    lv_obj_set_style_text_color(inputMetaLabel_, context.ui.muted(), 0);
}

void NetworkSettingsApp::rebuildView(AppContext& context) {
    keyboard_.reset();
    inputMetaLabel_ = nullptr;
    lv_obj_clean(root_);
    lv_obj_set_scroll_dir(root_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(root_, 72, 0);
    switch (setupStage_) {
        case SetupStage::Scanning:
            buildScanning(context);
            break;
        case SetupStage::Selecting:
            buildSelection(context);
            break;
        case SetupStage::EnteringPassword:
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
        context.ui.createBodyLabel(root_, subtitle, 17, 57,
                                   context.ui.muted());
    }
}

void NetworkSettingsApp::buildInactive(AppContext& context,
                                       const WifiSnapshot& snapshot) {
    buildHeader(context, "NETWORK / RADIO", "Network", nullptr);

    const String title = snapshot.ssid.isEmpty()
                             ? String("尚未配置网络")
                             : shorten(snapshot.ssid, 18);
    String subtitle = wifiStateText(snapshot.state);
    if (snapshot.state == WifiState::Connected) {
        subtitle = snapshot.ip;
    }
    if (!setupNotice_.isEmpty()) {
        subtitle += "  /  ";
        subtitle += setupNotice_;
    }
    UiCard status = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y, UiIcon::Wifi,
        title.c_str(), subtitle.c_str());

    const int16_t actionY = UiRuntime::CARD_START_Y + UiRuntime::CARD_STEP_Y;
    UiCard rows[INACTIVE_ITEM_COUNT];
    rows[0] = context.ui.createCard(root_, actionY, UiIcon::Wifi,
                                    "Wi-Fi 电源", nullptr);
    rows[1] = context.ui.createCard(root_, actionY + UiRuntime::CARD_STEP_Y,
                                    UiIcon::Settings, "配置网络", nullptr);
    rows[2] = context.ui.createCard(root_, actionY + 2 * UiRuntime::CARD_STEP_Y,
                                    UiIcon::Wifi, "重新连接", nullptr);

    const String values[INACTIVE_ITEM_COUNT] = {
        choiceText(context.wifi.enabled() ? "开" : "关"),
        choiceText("扫描"),
        snapshot.ssid.isEmpty() ? String("不可用") : choiceText("连接"),
    };
    for (uint8_t index = 0; index < INACTIVE_ITEM_COUNT; ++index) {
        lv_obj_t* value = context.ui.createLabel(
            rows[index].root, values[index].c_str(), 198, 10, 12,
            index == 2 && snapshot.ssid.isEmpty()
                ? context.ui.dim()
                : context.ui.text());
        context.ui.applyBodyFont(value);
        lv_obj_set_width(value, 82);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(value, LV_ALIGN_RIGHT_MID, -9, 0);
        context.ui.setCardFocused(rows[index], index == inactiveCursor_, false);
    }
    context.ui.centerFocused(root_, rows[inactiveCursor_].root, false);

    if (snapshot.state == WifiState::Connected) {
        const String rssi = rssiText(snapshot);
        lv_obj_t* signal = context.ui.createLabel(
            status.root, rssi.c_str(), 192, 13, 10,
            context.ui.muted());
        lv_obj_set_width(signal, 76);
        lv_obj_set_style_text_align(signal, LV_TEXT_ALIGN_RIGHT, 0);
    }
}

void NetworkSettingsApp::buildScanning(AppContext& context) {
    buildHeader(context, "NETWORK / SETUP", "Scanning", nullptr);
    context.ui.createLabel(root_, LV_SYMBOL_REFRESH, 145, 89, 28,
                           context.ui.accent());
    context.ui.createBodyLabel(root_, "正在扫描无线网络", 88, 130,
                               context.ui.text(), true);
    context.ui.createBodyLabel(root_, "控制台与后台服务保持运行", 78, 157,
                               context.ui.muted());
}

void NetworkSettingsApp::buildSelection(AppContext& context) {
    buildHeader(context, "NETWORK / SETUP", "Wi-Fi Networks", nullptr);
    if (!setupNotice_.isEmpty()) {
        context.ui.createBodyLabel(root_, setupNotice_.c_str(), 17, 59,
                                   context.ui.accent());
    }
    const int16_t count = context.wifi.scanCount();
    lv_obj_t* selectedItem = nullptr;
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
        if (selected) {
            selectedItem = item;
        }

        context.ui.createLabel(item, selected ? ">" : "", 7, 2, 12,
                               context.ui.accent());
        const String ssid = shorten(context.wifi.scanSsid(index), 23);
        context.ui.createLabel(item, ssid.c_str(), 22, 2, 12,
                               context.ui.text());
        String signal = String(context.wifi.scanRssi(index)) + " dBm";
        if (context.wifi.scanIsOpen(index)) {
            signal += " OPEN";
        }
        lv_obj_t* signalLabel = context.ui.createLabel(
            item, signal.c_str(), 274, 2, 10, context.ui.muted());
        lv_obj_align(signalLabel, LV_ALIGN_RIGHT_MID, -7, 0);
    }
    context.ui.centerFocused(root_, selectedItem);
}

void NetworkSettingsApp::buildPassword(AppContext& context) {
    lv_obj_set_scroll_dir(root_, LV_DIR_NONE);
    lv_obj_set_style_pad_bottom(root_, 0, 0);
    context.ui.createLabel(root_, "NETWORK / SETUP", 12, 5, 10,
                           context.ui.accent());
    context.ui.createLabel(root_, "Wi-Fi Password", 12, 19, 20,
                           context.ui.text());
    const String ssid = String("SSID  ") +
                        shorten(context.wifi.selectedSsid(), 30);
    context.ui.createBodyLabel(root_, ssid.c_str(), 14, 45,
                               context.ui.muted());

    keyboard_.create(root_, context.ui, 61, 32, 111, 101,
                     true, 63, "Password");
    inputMetaLabel_ = context.ui.createBodyLabel(
        root_, "0 / 63", 14, 94, context.ui.muted());
    lv_obj_set_width(inputMetaLabel_, 292);
    lv_obj_set_style_text_align(inputMetaLabel_, LV_TEXT_ALIGN_RIGHT, 0);
    updatePasswordMeta(context);
}

String NetworkSettingsApp::viewSignature(AppContext& context) const {
    const WifiSnapshot wifi = context.wifi.snapshot();
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
    signature += inactiveCursor_;
    signature += '|';
    signature += cursor_;
    signature += '|';
    signature += windowStart_;
    signature += '|';
    signature += setupNotice_;
    return signature;
}
