#include "services/BleGamepadService.h"

#include <algorithm>
#include <cstring>

#if defined(PGOS_BLE_GAMEPAD_BACKEND)
BleGamepadService* BleGamepadService::instance_ = nullptr;
#endif

namespace {

bool validAutoDisconnect(uint32_t timeoutMs) {
    return timeoutMs == 0 ||
           (timeoutMs >= 5UL * 60UL * 1000UL &&
            timeoutMs <= 60UL * 60UL * 1000UL);
}

int16_t clampAxis(int32_t value) {
    return static_cast<int16_t>(std::max<int32_t>(-32768, std::min<int32_t>(32767, value)));
}

uint16_t clampTrigger(int32_t value) {
    return static_cast<uint16_t>(std::max<int32_t>(0, std::min<int32_t>(65535, value)));
}

}  // namespace

bool BleGamepadService::begin(Stream& log) {
    log_ = &log;
    snapshot_ = GamepadSnapshot{};
    preferencesReady_ = preferences_.begin("pgos_gamepad", false);
    if (preferencesReady_) {
        const uint32_t stored = preferences_.getUInt(
            "idle_ms", DEFAULT_AUTO_DISCONNECT_MS);
        autoDisconnectMs_ = validAutoDisconnect(stored)
                                ? stored
                                : DEFAULT_AUTO_DISCONNECT_MS;
    } else {
        log.println(F("[gamepad] NVS unavailable; using default idle timeout"));
    }
    started_ = true;

#if defined(PGOS_BLE_GAMEPAD_BACKEND)
    instance_ = this;
    BP32.setup(&BleGamepadService::onConnectedController,
               &BleGamepadService::onDisconnectedController, false);
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);
    snapshot_.enabled = true;
    log.println(F("[gamepad] Bluepad32 BLE host ready"));
    startPairingScan();
#else
    log.println(F("[gamepad] backend disabled; shell API ready"));
#endif
    updateSnapshotMeta(millis());
    return true;
}

void BleGamepadService::tick(uint32_t nowMs) {
    if (!started_) {
        return;
    }

#if defined(PGOS_BLE_GAMEPAD_BACKEND)
    if (BP32.update()) {
        updateController(nowMs);
    }
    if (snapshot_.connected) {
        reconnectScheduler_.cancel();
        if (scanning_) {
            stopScan(nowMs, false);
        }
    } else if (scanning_ &&
               static_cast<int32_t>(nowMs - scanDeadlineMs_) >= 0) {
        stopScan(nowMs, true);
    } else if (!scanning_ && reconnectScheduler_.due(nowMs)) {
        startScan(GamepadScanMode::Reconnect, RECONNECT_SCAN_WINDOW_MS, nowMs);
    }
    if (snapshot_.connected && !disconnectPending_ && autoDisconnectMs_ > 0 &&
        lastActivityMs_ != 0 &&
        static_cast<int32_t>(nowMs - lastActivityMs_) >=
            static_cast<int32_t>(autoDisconnectMs_)) {
        if (log_ != nullptr) {
            log_->println(F("[gamepad] idle timeout; disconnecting controller"));
        }
        disconnectController(DisconnectReason::Idle);
    }
#else
    (void)nowMs;
#endif
    updateSnapshotMeta(nowMs);
}

const GamepadSnapshot& BleGamepadService::snapshot() const {
    return snapshot_;
}

bool BleGamepadService::pollEvent(GamepadEvent& event) {
    if (eventCount_ == 0) {
        return false;
    }
    event = eventQueue_[eventHead_];
    eventHead_ = static_cast<uint8_t>((eventHead_ + 1U) % EVENT_QUEUE_SIZE);
    eventCount_--;
    return true;
}

bool BleGamepadService::requestRumble(uint16_t durationMs,
                                      uint8_t weakMagnitude,
                                      uint8_t strongMagnitude) {
#if defined(PGOS_BLE_GAMEPAD_BACKEND)
    for (ControllerPtr controller : controllers_) {
        if (controller == nullptr || !controller->isConnected() ||
            !controller->isGamepad()) {
            continue;
        }
        controller->playDualRumble(0, durationMs, weakMagnitude,
                                   strongMagnitude);
        return true;
    }
#else
    (void)durationMs;
    (void)weakMagnitude;
    (void)strongMagnitude;
#endif
    return false;
}

bool BleGamepadService::startPairingScan() {
#if defined(PGOS_BLE_GAMEPAD_BACKEND)
    const uint32_t nowMs = millis();
    return startScan(GamepadScanMode::Pairing, PAIRING_SCAN_WINDOW_MS, nowMs);
#else
    return false;
#endif
}

void BleGamepadService::stopPairingScan() {
#if defined(PGOS_BLE_GAMEPAD_BACKEND)
    const uint32_t nowMs = millis();
    stopScan(nowMs, !snapshot_.connected);
#endif
}

bool BleGamepadService::disconnectController() {
    return disconnectController(DisconnectReason::Manual);
}

bool BleGamepadService::disconnectController(DisconnectReason reason) {
#if defined(PGOS_BLE_GAMEPAD_BACKEND)
    if (disconnectPending_) {
        return true;
    }
    for (ControllerPtr controller : controllers_) {
        if (controller == nullptr || !controller->isConnected()) {
            continue;
        }
        const uint32_t nowMs = millis();
        stopScan(nowMs, false);
        reconnectScheduler_.cancel();
        disconnectPending_ = true;
        disconnectReason_ = reason;
        if (log_ != nullptr) {
            log_->println(reason == DisconnectReason::Idle
                              ? F("[gamepad] disconnect requested reason=idle")
                              : F("[gamepad] disconnect requested reason=manual"));
        }
        controller->disconnect();
        return true;
    }
#endif
    return false;
}

void BleGamepadService::setAutoDisconnectMs(uint32_t timeoutMs) {
    if (!validAutoDisconnect(timeoutMs)) {
        return;
    }
    autoDisconnectMs_ = timeoutMs;
    snapshot_.autoDisconnectMs = autoDisconnectMs_;
    saveSettings();
}

uint32_t BleGamepadService::autoDisconnectMs() const {
    return autoDisconnectMs_;
}

void BleGamepadService::printStatus(Print& output) const {
    output.print(F("[gamepad] state="));
    output.print(snapshot_.connected ? F("connected")
                                     : snapshot_.scanning
                                           ? snapshot_.scanMode ==
                                                     GamepadScanMode::Pairing
                                                 ? F("pairing")
                                                 : F("reconnecting")
                                           : F("standby"));
    output.print(F(" scan="));
    output.print(snapshot_.scanMode == GamepadScanMode::Pairing
                     ? F("pairing")
                     : snapshot_.scanMode == GamepadScanMode::Reconnect
                           ? F("reconnect")
                           : F("off"));
    if (snapshot_.scanning) {
        output.print(F(" remaining_ms="));
        output.print(snapshot_.scanRemainingMs);
    } else if (snapshot_.reconnectScheduled) {
        output.print(F(" next_scan_ms="));
        output.print(snapshot_.reconnectRemainingMs);
    }
    if (snapshot_.connected) {
        output.print(F(" slot="));
        output.print(snapshot_.slot);
        output.print(F(" vid="));
        if (snapshot_.vendorId < 0x1000) output.print('0');
        if (snapshot_.vendorId < 0x0100) output.print('0');
        if (snapshot_.vendorId < 0x0010) output.print('0');
        output.print(snapshot_.vendorId, HEX);
        output.print(F(" pid="));
        if (snapshot_.productId < 0x1000) output.print('0');
        if (snapshot_.productId < 0x0100) output.print('0');
        if (snapshot_.productId < 0x0010) output.print('0');
        output.print(snapshot_.productId, HEX);
        output.print(F(" model="));
        output.print(snapshot_.model);
        output.print(F(" packets="));
        output.print(snapshot_.packetCount);
        output.print(F(" timeout_ms="));
        output.print(snapshot_.autoDisconnectMs);
        output.print(F(" input_age_ms="));
        output.print(millis() - lastActivityMs_);
        output.print(F(" axes="));
        output.print(snapshot_.axisX);
        output.print(',');
        output.print(snapshot_.axisY);
        output.print(',');
        output.print(snapshot_.axisRX);
        output.print(',');
        output.print(snapshot_.axisRY);
        output.print(F(" triggers="));
        output.print(snapshot_.brake);
        output.print(',');
        output.print(snapshot_.throttle);
    }
    output.print(F(" scans="));
    output.print(snapshot_.scanStartCount);
    output.print(F(" disconnects="));
    output.print(snapshot_.disconnectCount);
    output.print(F(" events_dropped="));
    output.println(droppedEvents_);
}

void BleGamepadService::updateSnapshotMeta(uint32_t nowMs) {
    snapshot_.enabled = started_;
    snapshot_.scanning = scanning_;
    snapshot_.scanMode = scanMode_;
    snapshot_.reconnectScheduled =
        !snapshot_.connected && !scanning_ && reconnectScheduler_.scheduled();
    snapshot_.scanRemainingMs =
        scanning_ && static_cast<int32_t>(scanDeadlineMs_ - nowMs) > 0
            ? scanDeadlineMs_ - nowMs
            : 0;
    snapshot_.reconnectRemainingMs =
        snapshot_.reconnectScheduled ? reconnectScheduler_.remainingMs(nowMs) : 0;
    snapshot_.lastInputMs = lastActivityMs_;
    snapshot_.connectedSinceMs = connectedSinceMs_;
    snapshot_.autoDisconnectMs = autoDisconnectMs_;
    snapshot_.disconnectCount = disconnectCount_;
    snapshot_.scanStartCount = scanStartCount_;
}

void BleGamepadService::saveSettings() {
    if (preferencesReady_) {
        preferences_.putUInt("idle_ms", autoDisconnectMs_);
    }
}

void BleGamepadService::pushEvent(GamepadEventType type, uint8_t slot,
                                   uint16_t code) {
    if (eventCount_ >= EVENT_QUEUE_SIZE) {
        droppedEvents_++;
        return;
    }
    eventQueue_[eventTail_].type = type;
    eventQueue_[eventTail_].slot = slot;
    eventQueue_[eventTail_].code = code;
    eventTail_ = static_cast<uint8_t>((eventTail_ + 1U) % EVENT_QUEUE_SIZE);
    eventCount_++;
}

bool BleGamepadService::startScan(GamepadScanMode mode, uint32_t durationMs,
                                  uint32_t nowMs) {
#if defined(PGOS_BLE_GAMEPAD_BACKEND)
    if (!started_ || snapshot_.connected || mode == GamepadScanMode::None) {
        return false;
    }
    reconnectScheduler_.cancel();
    scanDeadlineMs_ = nowMs + durationMs;
    const bool wasScanning = scanning_;
    scanMode_ = mode;
    if (!wasScanning) {
        BP32.enableNewBluetoothConnections(true);
        scanning_ = true;
        scanStartCount_++;
    }
    if (log_ != nullptr) {
        log_->printf("[gamepad] %s scan %s duration=%lums\n",
                     mode == GamepadScanMode::Pairing ? "pairing" : "reconnect",
                     wasScanning ? "extended" : "started",
                     static_cast<unsigned long>(durationMs));
    }
    updateSnapshotMeta(nowMs);
    return true;
#else
    (void)mode;
    (void)durationMs;
    (void)nowMs;
    return false;
#endif
}

void BleGamepadService::stopScan(uint32_t nowMs, bool scheduleReconnect) {
#if defined(PGOS_BLE_GAMEPAD_BACKEND)
    if (scanning_) {
        BP32.enableNewBluetoothConnections(false);
        scanning_ = false;
        scanMode_ = GamepadScanMode::None;
        scanDeadlineMs_ = 0;
        if (log_ != nullptr) {
            log_->println(F("[gamepad] scan stopped"));
        }
    }
    if (scheduleReconnect && !snapshot_.connected) {
        reconnectScheduler_.scheduleAfterScan(nowMs);
    }
    updateSnapshotMeta(nowMs);
#else
    (void)nowMs;
    (void)scheduleReconnect;
#endif
}

#if defined(PGOS_BLE_GAMEPAD_BACKEND)

void BleGamepadService::onConnectedController(ControllerPtr controller) {
    if (instance_ != nullptr) {
        instance_->handleConnected(controller);
    }
}

void BleGamepadService::onDisconnectedController(ControllerPtr controller) {
    if (instance_ != nullptr) {
        instance_->handleDisconnected(controller);
    }
}

void BleGamepadService::handleConnected(ControllerPtr controller) {
    for (uint8_t slot = 0; slot < BP32_MAX_GAMEPADS; ++slot) {
        if (controllers_[slot] != nullptr) {
            continue;
        }
        controllers_[slot] = controller;
        const ControllerProperties properties = controller->getProperties();
        snapshot_ = GamepadSnapshot{};
        snapshot_.enabled = true;
        snapshot_.connected = true;
        snapshot_.slot = slot;
        snapshot_.vendorId = properties.vendor_id;
        snapshot_.productId = properties.product_id;
        const String model = controller->getModelName();
        model.toCharArray(snapshot_.model, sizeof(snapshot_.model));
        previousButtons_ = 0;
        previousDpad_ = 0;
        previousMiscButtons_ = 0;
        activityTracker_.reset();
        reconnectScheduler_.cancel();
        disconnectPending_ = false;
        disconnectReason_ = DisconnectReason::None;
        connectedSinceMs_ = millis();
        lastActivityMs_ = connectedSinceMs_;
        pushEvent(GamepadEventType::Connected, slot, 0);
        if (log_ != nullptr) {
            log_->printf("[gamepad] connected slot=%u model=%s vid=%04x pid=%04x\n",
                         slot, snapshot_.model, snapshot_.vendorId,
                         snapshot_.productId);
        }
        return;
    }
    if (log_ != nullptr) {
        log_->println(F("[gamepad] connected but no free slot"));
    }
}

void BleGamepadService::handleDisconnected(ControllerPtr controller) {
    for (uint8_t slot = 0; slot < BP32_MAX_GAMEPADS; ++slot) {
        if (controllers_[slot] != controller) {
            continue;
        }
        controllers_[slot] = nullptr;
        pushEvent(GamepadEventType::Disconnected, slot, 0);
        const DisconnectReason reason = disconnectReason_;
        disconnectPending_ = false;
        disconnectReason_ = DisconnectReason::None;
        disconnectCount_++;
        clearSnapshot();
        const uint32_t nowMs = millis();
        if (reason == DisconnectReason::None) {
            reconnectScheduler_.scheduleAfterUnexpectedDisconnect(nowMs);
        } else {
            reconnectScheduler_.scheduleAfterIntentionalDisconnect(nowMs);
        }
        if (log_ != nullptr) {
            log_->printf("[gamepad] disconnected slot=%u reconnect_in=%lums\n",
                         slot,
                         static_cast<unsigned long>(
                             reconnectScheduler_.remainingMs(nowMs)));
        }
        return;
    }
}

void BleGamepadService::clearSnapshot() {
    snapshot_ = GamepadSnapshot{};
    snapshot_.enabled = started_;
    previousButtons_ = 0;
    previousDpad_ = 0;
    previousMiscButtons_ = 0;
    activityTracker_.reset();
    lastActivityMs_ = 0;
    connectedSinceMs_ = 0;
}

void BleGamepadService::updateController(uint32_t nowMs) {
    for (uint8_t slot = 0; slot < BP32_MAX_GAMEPADS; ++slot) {
        ControllerPtr controller = controllers_[slot];
        if (controller == nullptr || !controller->isConnected() ||
            !controller->isGamepad() || !controller->hasData()) {
            continue;
        }

        const uint16_t buttons = controller->buttons();
        const uint8_t dpad = controller->dpad();
        const uint8_t miscButtons =
            static_cast<uint8_t>(controller->miscButtons());
        const int16_t axisX = clampAxis(controller->axisX());
        const int16_t axisY = clampAxis(controller->axisY());
        const int16_t axisRX = clampAxis(controller->axisRX());
        const int16_t axisRY = clampAxis(controller->axisRY());
        const uint16_t brake = clampTrigger(controller->brake());
        const uint16_t throttle = clampTrigger(controller->throttle());
        pgos::GamepadActivitySample activitySample;
        activitySample.buttons = buttons;
        activitySample.dpad = dpad;
        activitySample.miscButtons = miscButtons;
        activitySample.axes[0] = axisX;
        activitySample.axes[1] = axisY;
        activitySample.axes[2] = axisRX;
        activitySample.axes[3] = axisRY;
        activitySample.triggers[0] = brake;
        activitySample.triggers[1] = throttle;
        if (activityTracker_.update(activitySample)) {
            lastActivityMs_ = nowMs;
        }
        const uint16_t buttonChanges = buttons ^ previousButtons_;
        for (uint16_t bit = 1; bit != 0; bit <<= 1U) {
            if ((buttonChanges & bit) == 0) {
                continue;
            }
            pushEvent((buttons & bit) != 0 ? GamepadEventType::ButtonDown
                                           : GamepadEventType::ButtonUp,
                      slot, bit);
        }
        const uint8_t miscChanges = miscButtons ^ previousMiscButtons_;
        for (uint8_t bit = 1; bit <= GamepadMiscCapture; bit <<= 1U) {
            if ((miscChanges & bit) == 0) {
                continue;
            }
            pushEvent((miscButtons & bit) != 0 ? GamepadEventType::MiscDown
                                               : GamepadEventType::MiscUp,
                      slot, bit);
        }
        const uint8_t dpadChanges = dpad ^ previousDpad_;
        for (uint8_t bit = 1; bit <= GamepadDpadLeft; bit <<= 1U) {
            if ((dpadChanges & bit) == 0) {
                continue;
            }
            pushEvent((dpad & bit) != 0 ? GamepadEventType::DpadDown
                                        : GamepadEventType::DpadUp,
                      slot, bit);
        }
        previousButtons_ = buttons;
        previousDpad_ = dpad;
        previousMiscButtons_ = miscButtons;

        snapshot_.connected = true;
        snapshot_.slot = slot;
        snapshot_.buttons = buttons;
        snapshot_.miscButtons = miscButtons;
        snapshot_.dpad = dpad;
        snapshot_.axisX = axisX;
        snapshot_.axisY = axisY;
        snapshot_.axisRX = axisRX;
        snapshot_.axisRY = axisRY;
        snapshot_.brake = brake;
        snapshot_.throttle = throttle;
        snapshot_.battery = controller->battery();
        snapshot_.packetCount++;
        snapshot_.lastUpdateMs = nowMs;
        snapshot_.lastInputMs = lastActivityMs_;
        return;
    }
}

#endif
