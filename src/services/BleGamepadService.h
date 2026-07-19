#pragma once

#include "core/AppTypes.h"

#include <Arduino.h>
#include <Preferences.h>

#if defined(PGOS_BLE_GAMEPAD_BACKEND)
#include <Bluepad32.h>
#endif

class BleGamepadService {
public:
    static constexpr uint8_t EVENT_QUEUE_SIZE = 32;
    static constexpr uint32_t PAIRING_SCAN_WINDOW_MS = 60000UL;
    static constexpr uint32_t DEFAULT_AUTO_DISCONNECT_MS = 15UL * 60UL * 1000UL;

    bool begin(Stream& log);
    void tick(uint32_t nowMs);

    const GamepadSnapshot& snapshot() const;
    bool pollEvent(GamepadEvent& event);
    bool requestRumble(uint16_t durationMs, uint8_t weakMagnitude,
                       uint8_t strongMagnitude);
    bool startPairingScan();
    void stopPairingScan();
    bool disconnectController();
    void setAutoDisconnectMs(uint32_t timeoutMs);
    uint32_t autoDisconnectMs() const;
    void printStatus(Print& output) const;

private:
    Stream* log_ = nullptr;
    Preferences preferences_;
    GamepadSnapshot snapshot_;
    GamepadEvent eventQueue_[EVENT_QUEUE_SIZE] = {};
    uint8_t eventHead_ = 0;
    uint8_t eventTail_ = 0;
    uint8_t eventCount_ = 0;
    uint32_t droppedEvents_ = 0;
    bool started_ = false;
    bool preferencesReady_ = false;
    bool scanning_ = false;
    uint32_t scanDeadlineMs_ = 0;
    uint32_t lastActivityMs_ = 0;
    uint32_t connectedSinceMs_ = 0;
    uint32_t autoDisconnectMs_ = DEFAULT_AUTO_DISCONNECT_MS;
    uint32_t disconnectCount_ = 0;
    uint32_t scanStartCount_ = 0;
    bool disconnectPending_ = false;
    bool rumblePending_ = false;
    uint16_t rumbleDurationMs_ = 0;
    uint8_t rumbleWeakMagnitude_ = 0;
    uint8_t rumbleStrongMagnitude_ = 0;

#if defined(PGOS_BLE_GAMEPAD_BACKEND)
    ControllerPtr controllers_[BP32_MAX_GAMEPADS] = {};
    uint16_t previousButtons_ = 0;
    uint8_t previousDpad_ = 0;
    uint8_t previousMiscButtons_ = 0;
    static BleGamepadService* instance_;

    static void onConnectedController(ControllerPtr controller);
    static void onDisconnectedController(ControllerPtr controller);
    void handleConnected(ControllerPtr controller);
    void handleDisconnected(ControllerPtr controller);
    void updateController(uint32_t nowMs);
    void clearSnapshot();
#endif

    void updateSnapshotMeta(uint32_t nowMs);
    void saveSettings();
    void pushEvent(GamepadEventType type, uint8_t slot, uint16_t code);
    void processPendingRumble();
};
