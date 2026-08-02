#include "apps/ConsoleSettingsApp.h"

#include "services/ServerService.h"
#include "ui/UiRuntime.h"

namespace {

String choiceText(const char* value) {
    return String("< ") + value + " >";
}

const char* serverStateText(ServerState state) {
    switch (state) {
        case ServerState::Disabled: return "已关闭";
        case ServerState::NoTarget: return "等待配置";
        case ServerState::WaitingWifi: return "等待网络";
        case ServerState::Connecting: return "正在连接";
        case ServerState::Connected: return "已连接";
        case ServerState::Backoff: return "稍后重试";
        default: return "未知";
    }
}

String shorten(const String& value, size_t maxLength) {
    if (value.length() <= maxLength) {
        return value;
    }
    return value.substring(0, maxLength - 3U) + "...";
}

String targetText(const ServerSnapshot& snapshot) {
    if (snapshot.host.isEmpty()) {
        return "尚未配置";
    }
    return shorten(snapshot.host + ":" + snapshot.port, 27U);
}

}  // namespace

AppId ConsoleSettingsApp::id() const {
    return AppId::ConsoleSettings;
}

const char* ConsoleSettingsApp::name() const {
    return "Console";
}

void ConsoleSettingsApp::onEnter(AppContext& context) {
    viewMode_ = ViewMode::Main;
    selected_ = context.server.snapshot().host.isEmpty() ? 0U : 1U;
    notice_ = "";
    inputError_ = "";
    lastViewSignature_ = "";
}

void ConsoleSettingsApp::onExit(AppContext&) {
    root_ = nullptr;
    inputMetaLabel_ = nullptr;
    keyboard_.reset();
    viewMode_ = ViewMode::Main;
    notice_ = "";
    inputError_ = "";
    lastViewSignature_ = "";
}

void ConsoleSettingsApp::onCommand(const AppCommand& command,
                                   AppContext& context) {
    if (viewMode_ == ViewMode::EditingHost) {
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
                submitHost(context);
            } else if (action == OnScreenKeyboardAction::Edited) {
                inputError_ = "";
                updateInputMeta(context);
            }
            return;
        }
    }

    switch (command.type) {
        case AppCommandType::Previous:
            moveSelection(-1);
            break;
        case AppCommandType::Next:
            moveSelection(1);
            break;
        case AppCommandType::Left:
            if (selected_ == 1U) {
                context.server.setEnabled(false);
                notice_ = "无线控制已关闭";
            }
            break;
        case AppCommandType::Right:
            if (selected_ == 1U) {
                context.server.setEnabled(true);
                notice_ = context.server.snapshot().host.isEmpty()
                              ? String("请先配置服务器地址")
                              : String("无线控制已开启");
            }
            break;
        case AppCommandType::Activate:
            activateSelected(context);
            break;
        case AppCommandType::ServerSet:
            if (context.server.setTarget(command.value, command.number)) {
                notice_ = "服务器地址已保存";
            } else {
                notice_ = "服务器地址无效";
            }
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
            notice_ = "正在重新连接";
            break;
        case AppCommandType::ServerClear:
            context.server.clearTarget();
            selected_ = 0;
            notice_ = "服务器地址已清除";
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

void ConsoleSettingsApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* ConsoleSettingsApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot(nullptr, "Remote Control");
    lastViewSignature_ = "";
    return root_;
}

void ConsoleSettingsApp::onUpdateView(AppContext& context) {
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

bool ConsoleSettingsApp::onBack(AppContext&) {
    if (viewMode_ != ViewMode::EditingHost) {
        return false;
    }
    viewMode_ = ViewMode::Main;
    inputError_ = "";
    lastViewSignature_ = "";
    return true;
}

void ConsoleSettingsApp::moveSelection(int16_t delta) {
    if (delta < 0) {
        selected_ = selected_ == 0U ? ITEM_COUNT - 1U : selected_ - 1U;
    } else {
        selected_ = static_cast<uint8_t>((selected_ + 1U) % ITEM_COUNT);
    }
    notice_ = "";
    lastViewSignature_ = "";
}

void ConsoleSettingsApp::activateSelected(AppContext& context) {
    switch (selected_) {
        case 0:
            startHostEditor();
            return;
        case 1:
            context.server.toggleEnabled();
            notice_ = context.server.snapshot().enabled
                          ? String("无线控制已开启")
                          : String("无线控制已关闭");
            break;
        case 2:
            if (context.server.snapshot().host.isEmpty()) {
                notice_ = "请先配置服务器地址";
                selected_ = 0;
            } else {
                context.server.connectNow();
                notice_ = "正在重新连接";
            }
            break;
        default:
            break;
    }
    lastViewSignature_ = "";
}

void ConsoleSettingsApp::startHostEditor() {
    viewMode_ = ViewMode::EditingHost;
    inputError_ = "";
    notice_ = "";
    lastViewSignature_ = "";
}

void ConsoleSettingsApp::submitHost(AppContext& context) {
    String host = keyboard_.text();
    host.trim();
    if (!ServerService::validHost(host)) {
        inputError_ = "请输入有效的主机名或 IPv4 地址";
        updateInputMeta(context);
        return;
    }

    uint16_t port = context.server.snapshot().port;
    if (port == 0U) {
        port = ServerService::DEFAULT_PORT;
    }
    if (!context.server.setTarget(host, port)) {
        inputError_ = "服务器地址保存失败";
        updateInputMeta(context);
        return;
    }

    viewMode_ = ViewMode::Main;
    notice_ = "地址已保存，正在连接";
    inputError_ = "";
    lastViewSignature_ = "";
}

void ConsoleSettingsApp::updateInputMeta(AppContext& context) {
    if (inputMetaLabel_ == nullptr) {
        return;
    }
    if (!inputError_.isEmpty()) {
        lv_label_set_text(inputMetaLabel_, inputError_.c_str());
        lv_obj_set_style_text_color(inputMetaLabel_, context.ui.accent(), 0);
        return;
    }
    const String value = String(keyboard_.length()) + " / 63";
    lv_label_set_text(inputMetaLabel_, value.c_str());
    lv_obj_set_style_text_color(inputMetaLabel_, context.ui.muted(), 0);
}

void ConsoleSettingsApp::rebuildView(AppContext& context) {
    keyboard_.reset();
    inputMetaLabel_ = nullptr;
    lv_obj_clean(root_);
    lv_obj_set_scroll_dir(root_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(root_, 72, 0);
    if (viewMode_ == ViewMode::EditingHost) {
        buildHostEditor(context);
    } else {
        buildMain(context);
    }
}

void ConsoleSettingsApp::buildMain(AppContext& context) {
    const ServerSnapshot snapshot = context.server.snapshot();
    context.ui.createLabel(root_, "Remote Control", 16, 12, 24,
                           context.ui.text());
    const String subtitle = notice_.isEmpty()
                                ? String("Studio 与无线控制连接")
                                : notice_;
    context.ui.createBodyLabel(root_, subtitle.c_str(), 17, 43,
                               notice_.isEmpty() ? context.ui.muted()
                                                 : context.ui.accent());

    UiCard rows[ITEM_COUNT];
    rows[0] = context.ui.createCard(
        root_, UiRuntime::CARD_START_WITH_SUBTITLE_Y, UiIcon::Remote,
        "服务器地址", targetText(snapshot).c_str());
    rows[1] = context.ui.createCard(
        root_, UiRuntime::CARD_START_WITH_SUBTITLE_Y + UiRuntime::CARD_STEP_Y,
        UiIcon::Wifi, "无线控制", serverStateText(snapshot.state));
    rows[2] = context.ui.createCard(
        root_, UiRuntime::CARD_START_WITH_SUBTITLE_Y +
                   2 * UiRuntime::CARD_STEP_Y,
        UiIcon::Remote, "立即连接", "重新建立 Studio 会话");

    const String values[ITEM_COUNT] = {
        String("编辑 >"),
        choiceText(snapshot.enabled ? "开" : "关"),
        snapshot.host.isEmpty() ? String("不可用") : String("连接 >"),
    };
    for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
        const bool unavailable = index == 2U && snapshot.host.isEmpty();
        lv_obj_t* value = context.ui.createLabel(
            rows[index].root, values[index].c_str(), 198, 10, 12,
            unavailable ? context.ui.dim() : context.ui.text());
        context.ui.applyBodyFont(value);
        lv_obj_set_width(value, 82);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(value, LV_ALIGN_RIGHT_MID, -9, 0);
        context.ui.setCardFocused(rows[index], index == selected_, false);
    }
    context.ui.centerFocused(root_, rows[selected_].root, false);
}

void ConsoleSettingsApp::buildHostEditor(AppContext& context) {
    lv_obj_set_scroll_dir(root_, LV_DIR_NONE);
    lv_obj_set_style_pad_bottom(root_, 0, 0);
    context.ui.createLabel(root_, "REMOTE / SERVER", 12, 5, 10,
                           context.ui.accent());
    context.ui.createLabel(root_, "服务器地址", 12, 19, 20,
                           context.ui.text());
    const uint16_t port = context.server.snapshot().port;
    const String portText = String("控制端口  ") +
                            (port == 0U ? ServerService::DEFAULT_PORT : port);
    context.ui.createBodyLabel(root_, portText.c_str(), 14, 45,
                               context.ui.muted());

    keyboard_.create(root_, context.ui, 61, 32, 111, 101,
                     false, 63, "192.168.1.4");
    keyboard_.setText(context.server.snapshot().host.c_str());
    inputMetaLabel_ = context.ui.createBodyLabel(
        root_, "0 / 63", 14, 94, context.ui.muted());
    lv_obj_set_width(inputMetaLabel_, 292);
    lv_obj_set_style_text_align(inputMetaLabel_, LV_TEXT_ALIGN_RIGHT, 0);
    updateInputMeta(context);
}

String ConsoleSettingsApp::viewSignature(AppContext& context) const {
    if (viewMode_ == ViewMode::EditingHost) {
        return "editing";
    }
    const ServerSnapshot snapshot = context.server.snapshot();
    String signature = String(static_cast<uint8_t>(viewMode_));
    signature += '|';
    signature += selected_;
    signature += '|';
    signature += static_cast<uint8_t>(snapshot.state);
    signature += '|';
    signature += static_cast<uint8_t>(snapshot.enabled);
    signature += '|';
    signature += snapshot.host;
    signature += '|';
    signature += snapshot.port;
    signature += '|';
    signature += snapshot.lastError;
    signature += '|';
    signature += notice_;
    return signature;
}
