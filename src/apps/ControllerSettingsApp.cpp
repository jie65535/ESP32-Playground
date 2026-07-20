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
    root_ = context.ui.createPageRoot("CONTROLLER / BLUETOOTH", "Controller",
                                      "BLE gamepad and power policy");

    statusCard_ = context.ui.createCard(root_, 58, LV_SYMBOL_BLUETOOTH,
                                        "Reconnect standby",
                                        "Automatic bounded scan");
    statusValue_ = context.ui.createLabel(statusCard_.root, "Scan off", 196,
                                          10, 12, context.ui.muted());
    lv_obj_set_width(statusValue_, 76);
    lv_label_set_long_mode(statusValue_, LV_LABEL_LONG_CLIP);
    lv_obj_align(statusValue_, LV_ALIGN_RIGHT_MID, -9, 0);

    rows_[0] = context.ui.createCard(root_, 108, LV_SYMBOL_REFRESH,
                                     "Pair controller", "Bounded 60 second scan");
    rows_[1] = context.ui.createCard(root_, 158, LV_SYMBOL_POWER,
                                     "Idle disconnect", "No meaningful input timeout");
    rows_[2] = context.ui.createCard(root_, 208, LV_SYMBOL_VOLUME_MAX,
                                     "Rumble test", "Verify controller feedback");
    rows_[3] = context.ui.createCard(root_, 258, LV_SYMBOL_CLOSE,
                                     "Disconnect now", "Saved bond is kept");

    for (uint8_t index = 0; index < SETTING_COUNT; ++index) {
        valueLabels_[index] = context.ui.createLabel(
            rows_[index].root, "", 196, 10, 12, context.ui.text());
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
                              gamepad.model[0] == '\0' ? "Gamepad connected"
                                                       : gamepad.model);
            char detail[48];
            snprintf(detail, sizeof(detail), "VID %04X / PID %04X",
                     gamepad.vendorId, gamepad.productId);
            lv_label_set_text(statusCard_.subtitle, detail);
            if (gamepad.battery == 0) {
                lv_label_set_text(statusValue_, "Battery ?");
            } else {
                const uint32_t batteryPercent =
                    (static_cast<uint32_t>(gamepad.battery) * 100U + 127U) / 255U;
                char batteryText[16];
                snprintf(batteryText, sizeof(batteryText), "%lu%%",
                         static_cast<unsigned long>(batteryPercent));
                lv_label_set_text(statusValue_, batteryText);
            }
        } else if (gamepad.scanMode == GamepadScanMode::Pairing) {
            lv_label_set_text(statusCard_.title, "Pairing scan");
            lv_label_set_text(statusCard_.subtitle,
                              "New and bonded controllers accepted");
            char scanText[16];
            snprintf(scanText, sizeof(scanText), "%lus left",
                     static_cast<unsigned long>(scanSeconds));
            lv_label_set_text(statusValue_, scanText);
        } else if (gamepad.scanMode == GamepadScanMode::Reconnect) {
            lv_label_set_text(statusCard_.title, "Reconnect scan");
            lv_label_set_text(statusCard_.subtitle,
                              "Looking for a controller");
            char scanText[16];
            snprintf(scanText, sizeof(scanText), "%lus left",
                     static_cast<unsigned long>(scanSeconds));
            lv_label_set_text(statusValue_, scanText);
        } else if (gamepad.reconnectScheduled) {
            lv_label_set_text(statusCard_.title, "Reconnect standby");
            lv_label_set_text(statusCard_.subtitle,
                              "Automatic bounded scan");
            char waitText[16];
            snprintf(waitText, sizeof(waitText), "%lus to scan",
                     static_cast<unsigned long>(reconnectSeconds));
            lv_label_set_text(statusValue_, waitText);
        } else {
            lv_label_set_text(statusCard_.title, "Waiting");
            lv_label_set_text(statusCard_.subtitle,
                              "Start a pairing scan");
            lv_label_set_text(statusValue_, "Scan off");
        }
        lv_label_set_text(
            valueLabels_[0],
            gamepad.scanMode == GamepadScanMode::Pairing ? "< Stop >"
                                                         : "< Pair >");
        lv_label_set_text(valueLabels_[2], gamepad.connected ? "< Test >"
                                                             : "Unavailable");
        lv_label_set_text(valueLabels_[3], gamepad.connected ? "< Disconnect >"
                                                             : "Unavailable");
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
        return "Never";
    }
    return String(timeoutMs / 60000UL) + " min";
}
