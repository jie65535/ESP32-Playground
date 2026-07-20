#include "apps/ControllerSettingsApp.h"

#include "services/BleGamepadService.h"

#include <cstring>

constexpr uint32_t ControllerSettingsApp::IDLE_TIMEOUT_VALUES[];

AppId ControllerSettingsApp::id() const {
    return AppId::ControllerSettings;
}

const char* ControllerSettingsApp::name() const {
    return "Controller";
}

void ControllerSettingsApp::onEnter(AppContext&) {
    selected_ = 0;
    renderedSelected_ = -1;
    renderedConnected_ = false;
    renderedScanning_ = false;
    renderedScanMode_ = GamepadScanMode::None;
    renderedScanSeconds_ = UINT32_MAX;
    renderedReconnectSeconds_ = UINT32_MAX;
    renderedTimeoutMs_ = UINT32_MAX;
    renderedPacketCount_ = UINT32_MAX;
}

void ControllerSettingsApp::onExit(AppContext&) {
    root_ = nullptr;
    statusCard_ = UiCard{};
    std::memset(rows_, 0, sizeof(rows_));
    std::memset(valueLabels_, 0, sizeof(valueLabels_));
    statusValue_ = nullptr;
}

void ControllerSettingsApp::onCommand(const AppCommand& command,
                                      AppContext& context) {
    switch (command.type) {
        case AppCommandType::Previous:
            selected_ = selected_ == 0 ? SETTING_COUNT - 1U : selected_ - 1U;
            break;
        case AppCommandType::Next:
            selected_ = static_cast<uint8_t>((selected_ + 1U) % SETTING_COUNT);
            break;
        case AppCommandType::Left:
            adjustSelected(-1, context);
            break;
        case AppCommandType::Right:
            adjustSelected(1, context);
            break;
        case AppCommandType::Activate:
            activateSelected(context);
            break;
        default:
            break;
    }
}

void ControllerSettingsApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* ControllerSettingsApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot(nullptr, "Controller",
                                      "蓝牙手柄与断开策略");

    statusCard_ = context.ui.createCard(
        root_, UiRuntime::CARD_START_WITH_SUBTITLE_Y,
                                        UiIcon::Bluetooth,
                                        "等待连接", "自动限时重连扫描");
    statusValue_ = context.ui.createLabel(statusCard_.root, "未扫描", 196,
                                          10, 12, context.ui.muted());
    context.ui.applyBodyFont(statusValue_);
    lv_obj_set_width(statusValue_, 76);
    lv_label_set_long_mode(statusValue_, LV_LABEL_LONG_CLIP);
    lv_obj_align(statusValue_, LV_ALIGN_RIGHT_MID, -9, 0);

    rows_[0] = context.ui.createCard(
        root_, UiRuntime::CARD_START_WITH_SUBTITLE_Y + UiRuntime::CARD_STEP_Y,
        UiIcon::Gamepad,
                                     "配对手柄", "开启 60 秒限时扫描");
    rows_[1] = context.ui.createCard(
        root_, UiRuntime::CARD_START_WITH_SUBTITLE_Y +
                   2 * UiRuntime::CARD_STEP_Y,
        UiIcon::Settings,
                                     "空闲断开", nullptr);
    rows_[2] = context.ui.createCard(
        root_, UiRuntime::CARD_START_WITH_SUBTITLE_Y +
                   3 * UiRuntime::CARD_STEP_Y,
        UiIcon::Gamepad,
                                     "震动测试", nullptr);
    rows_[3] = context.ui.createCard(
        root_, UiRuntime::CARD_START_WITH_SUBTITLE_Y +
                   4 * UiRuntime::CARD_STEP_Y,
        UiIcon::Bluetooth,
                                     "立即断开", "保留已保存的配对关系");

    for (uint8_t index = 0; index < SETTING_COUNT; ++index) {
        valueLabels_[index] = context.ui.createLabel(
            rows_[index].root, "", 196, 10, 12, context.ui.text());
        context.ui.applyBodyFont(valueLabels_[index]);
        lv_obj_set_width(valueLabels_[index], 76);
        lv_label_set_long_mode(valueLabels_[index], LV_LABEL_LONG_CLIP);
        lv_obj_align(valueLabels_[index], LV_ALIGN_RIGHT_MID, -9, 0);
        lv_obj_set_style_text_align(valueLabels_[index], LV_TEXT_ALIGN_RIGHT, 0);
    }
    return root_;
}

void ControllerSettingsApp::onUpdateView(AppContext& context) {
    if (root_ == nullptr) {
        return;
    }
    const GamepadSnapshot& gamepad = context.gamepad.snapshot();
    const uint32_t scanSeconds =
        gamepad.scanning ? (gamepad.scanRemainingMs + 999U) / 1000U : 0;
    const uint32_t reconnectSeconds = gamepad.reconnectScheduled
        ? (gamepad.reconnectRemainingMs + 999U) / 1000U
        : 0;

    if (renderedSelected_ != static_cast<int8_t>(selected_)) {
        for (uint8_t index = 0; index < SETTING_COUNT; ++index) {
            context.ui.setCardFocused(rows_[index], index == selected_);
        }
        context.ui.centerFocused(root_, rows_[selected_].root);
        renderedSelected_ = static_cast<int8_t>(selected_);
    }

    if (renderedConnected_ != gamepad.connected ||
        renderedScanning_ != gamepad.scanning ||
        renderedScanMode_ != gamepad.scanMode ||
        renderedScanSeconds_ != scanSeconds ||
        renderedReconnectSeconds_ != reconnectSeconds ||
        renderedPacketCount_ != gamepad.packetCount) {
        if (gamepad.connected) {
            lv_label_set_text(statusCard_.title,
                              gamepad.model[0] == '\0' ? "手柄已连接"
                                                       : gamepad.model);
            char detail[48];
            snprintf(detail, sizeof(detail), "VID %04X / PID %04X",
                     gamepad.vendorId, gamepad.productId);
            lv_label_set_text(statusCard_.subtitle, detail);
            if (gamepad.battery == 0) {
                lv_label_set_text(statusValue_, "电量 ?");
            } else {
                const uint32_t batteryPercent =
                    (static_cast<uint32_t>(gamepad.battery) * 100U + 127U) / 255U;
                char batteryText[16];
                snprintf(batteryText, sizeof(batteryText), "%lu%%",
                         static_cast<unsigned long>(batteryPercent));
                lv_label_set_text(statusValue_, batteryText);
            }
        } else if (gamepad.scanMode == GamepadScanMode::Pairing) {
            lv_label_set_text(statusCard_.title, "正在配对");
            lv_label_set_text(statusCard_.subtitle,
                              "接受新手柄和已配对手柄");
            char scanText[16];
            snprintf(scanText, sizeof(scanText), "%lu 秒",
                     static_cast<unsigned long>(scanSeconds));
            lv_label_set_text(statusValue_, scanText);
        } else if (gamepad.scanMode == GamepadScanMode::Reconnect) {
            lv_label_set_text(statusCard_.title, "正在重连");
            lv_label_set_text(statusCard_.subtitle,
                              "正在查找已配对手柄");
            char scanText[16];
            snprintf(scanText, sizeof(scanText), "%lu 秒",
                     static_cast<unsigned long>(scanSeconds));
            lv_label_set_text(statusValue_, scanText);
        } else if (gamepad.reconnectScheduled) {
            lv_label_set_text(statusCard_.title, "等待重连");
            lv_label_set_text(statusCard_.subtitle,
                              "稍后自动开启限时扫描");
            char waitText[16];
            snprintf(waitText, sizeof(waitText), "%lu 秒后",
                     static_cast<unsigned long>(reconnectSeconds));
            lv_label_set_text(statusValue_, waitText);
        } else {
            lv_label_set_text(statusCard_.title, "等待连接");
            lv_label_set_text(statusCard_.subtitle,
                              "可手动启动配对扫描");
            lv_label_set_text(statusValue_, "未扫描");
        }
        lv_label_set_text(
            valueLabels_[0],
            gamepad.scanMode == GamepadScanMode::Pairing ? "< 停止 >"
                                                         : "< 配对 >");
        lv_label_set_text(valueLabels_[2], gamepad.connected ? "< 测试 >"
                                                             : "不可用");
        lv_label_set_text(valueLabels_[3], gamepad.connected ? "< 断开 >"
                                                             : "不可用");
        renderedConnected_ = gamepad.connected;
        renderedScanning_ = gamepad.scanning;
        renderedScanMode_ = gamepad.scanMode;
        renderedScanSeconds_ = scanSeconds;
        renderedReconnectSeconds_ = reconnectSeconds;
        renderedPacketCount_ = gamepad.packetCount;
    }

    if (renderedTimeoutMs_ != gamepad.autoDisconnectMs) {
        const String value = String("< ") +
                             idleTimeoutText(gamepad.autoDisconnectMs) + " >";
        lv_label_set_text(valueLabels_[1], value.c_str());
        renderedTimeoutMs_ = gamepad.autoDisconnectMs;
    }
}

void ControllerSettingsApp::adjustSelected(int8_t delta, AppContext& context) {
    if (selected_ == 0) {
        if (delta < 0 &&
            context.gamepad.snapshot().scanMode == GamepadScanMode::Pairing) {
            context.gamepad.stopPairingScan();
        } else if (delta > 0) {
            context.gamepad.startPairingScan();
        }
    } else if (selected_ == 1) {
        cycleIdleTimeout(delta, context);
    }
}

void ControllerSettingsApp::activateSelected(AppContext& context) {
    switch (selected_) {
        case 0:
            if (context.gamepad.snapshot().scanMode ==
                GamepadScanMode::Pairing) {
                context.gamepad.stopPairingScan();
            } else {
                context.gamepad.startPairingScan();
            }
            break;
        case 1:
            cycleIdleTimeout(1, context);
            break;
        case 2:
            context.gamepad.requestRumble(400, 160, 200);
            break;
        case 3:
            context.gamepad.disconnectController();
            break;
        default:
            break;
    }
}

void ControllerSettingsApp::cycleIdleTimeout(int8_t delta,
                                             AppContext& context) {
    const int16_t count = static_cast<int16_t>(
        sizeof(IDLE_TIMEOUT_VALUES) / sizeof(IDLE_TIMEOUT_VALUES[0]));
    int16_t index = idleTimeoutIndex(context.gamepad.autoDisconnectMs());
    index = delta < 0 ? (index == 0 ? count - 1 : index - 1)
                      : (index + 1) % count;
    context.gamepad.setAutoDisconnectMs(IDLE_TIMEOUT_VALUES[index]);
}

uint8_t ControllerSettingsApp::idleTimeoutIndex(uint32_t timeoutMs) const {
    for (uint8_t index = 0;
         index < sizeof(IDLE_TIMEOUT_VALUES) / sizeof(IDLE_TIMEOUT_VALUES[0]);
         ++index) {
        if (IDLE_TIMEOUT_VALUES[index] == timeoutMs) {
            return index;
        }
    }
    return 0;
}

String ControllerSettingsApp::idleTimeoutText(uint32_t timeoutMs) const {
    if (timeoutMs == 0) {
        return "永不";
    }
    return String(timeoutMs / 60000UL) + " 分钟";
}
