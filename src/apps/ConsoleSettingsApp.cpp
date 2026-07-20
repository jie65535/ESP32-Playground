#include "apps/ConsoleSettingsApp.h"

#include "services/ServerService.h"
#include "ui/UiRuntime.h"

namespace {

String settingValue(const char* value) {
    return String("< ") + value + " >";
}

const char* serverStateText(ServerState state) {
    switch (state) {
        case ServerState::Disabled: return "关闭";
        case ServerState::NoTarget: return "待配置";
        case ServerState::WaitingWifi: return "等待网络";
        case ServerState::Connecting: return "连接中";
        case ServerState::Connected: return "已连接";
        case ServerState::Backoff: return "稍后重试";
        default: return "未知";
    }
}

String targetText(const ServerSnapshot& snapshot) {
    if (snapshot.host.isEmpty()) {
        return "尚未配置";
    }
    String value = snapshot.host + ":" + snapshot.port;
    if (value.length() > 22U) {
        value = value.substring(0, 19U) + "...";
    }
    return value;
}

}  // namespace

AppId ConsoleSettingsApp::id() const {
    return AppId::ConsoleSettings;
}

const char* ConsoleSettingsApp::name() const {
    return "Console";
}

void ConsoleSettingsApp::onEnter(AppContext&) {
    selected_ = 0;
    renderedSelected_ = -1;
    lastStatusSignature_ = "";
}

void ConsoleSettingsApp::onExit(AppContext&) {
    root_ = nullptr;
    memset(rows_, 0, sizeof(rows_));
    memset(valueLabels_, 0, sizeof(valueLabels_));
    lastStatusSignature_ = "";
}

void ConsoleSettingsApp::onCommand(const AppCommand& command,
                                   AppContext& context) {
    switch (command.type) {
        case AppCommandType::Previous:
            selected_ = selected_ == 0 ? ITEM_COUNT - 1U : selected_ - 1U;
            break;
        case AppCommandType::Next:
            selected_ = static_cast<uint8_t>((selected_ + 1U) % ITEM_COUNT);
            break;
        case AppCommandType::Left:
        case AppCommandType::Right:
        case AppCommandType::Activate:
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
    lastStatusSignature_ = "";
}

void ConsoleSettingsApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* ConsoleSettingsApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot(
        nullptr, "Remote Control", "Studio 与无线控制连接");

    rows_[0] = context.ui.createCard(
        root_, UiRuntime::CARD_START_WITH_SUBTITLE_Y, UiIcon::Remote,
        "服务器地址", "远程主机与控制端口");
    rows_[1] = context.ui.createCard(
        root_, UiRuntime::CARD_START_WITH_SUBTITLE_Y + UiRuntime::CARD_STEP_Y,
        UiIcon::Wifi, "无线控制台", nullptr);

    for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
        valueLabels_[index] = context.ui.createLabel(
            rows_[index].root, "< -- >", 194, 10, 12, context.ui.text());
        context.ui.applyBodyFont(valueLabels_[index]);
        lv_obj_set_width(valueLabels_[index], 96);
        lv_label_set_long_mode(valueLabels_[index], LV_LABEL_LONG_CLIP);
        lv_obj_align(valueLabels_[index], LV_ALIGN_RIGHT_MID, -9, -1);
    }

    return root_;
}

void ConsoleSettingsApp::onUpdateView(AppContext& context) {
    if (root_ == nullptr) {
        return;
    }

    if (renderedSelected_ != static_cast<int8_t>(selected_)) {
        for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
            context.ui.setCardFocused(rows_[index], index == selected_);
        }
        context.ui.centerFocused(root_, rows_[selected_].root);
        renderedSelected_ = static_cast<int8_t>(selected_);
    }

    const String signature = statusSignature(context);
    if (signature == lastStatusSignature_) {
        return;
    }
    lastStatusSignature_ = signature;

    const ServerSnapshot server = context.server.snapshot();
    const String target = targetText(server);
    lv_label_set_text(rows_[0].subtitle, target.c_str());
    const String targetState = settingValue(
        server.host.isEmpty() ? "未设置" : "已保存");
    lv_label_set_text(valueLabels_[0], targetState.c_str());
    const String serverState = settingValue(serverStateText(server.state));
    lv_label_set_text(valueLabels_[1], serverState.c_str());
}

String ConsoleSettingsApp::statusSignature(AppContext& context) const {
    const ServerSnapshot server = context.server.snapshot();
    String signature;
    signature += static_cast<uint8_t>(server.state);
    signature += '|';
    signature += static_cast<uint8_t>(server.enabled);
    signature += '|';
    signature += server.host;
    signature += '|';
    signature += server.port;
    return signature;
}
