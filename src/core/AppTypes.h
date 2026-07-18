#pragma once

#include <Arduino.h>

enum class AppId : uint8_t {
    SystemInfo,
    DisplayTest,
    NetworkSettings,
    Launcher,
    Count,
};

enum class AppCommandType : uint8_t {
    None,
    Unknown,
    Previous,
    Next,
    Activate,
    PageSystem,
    PageDisplay,
    PageNetwork,
    ColorTest,
    Screenshot,
    Status,
    Help,
    WifiScan,
    WifiSelect,
    WifiSsid,
    WifiPassword,
    WifiOpen,
    WifiStatus,
    WifiReconnect,
    WifiClear,
    WifiOn,
    WifiOff,
    WifiToggle,
    WifiWizard,
    WifiWizardCancel,
    WifiHelp,
    ServerSet,
    ServerStatus,
    ServerOn,
    ServerOff,
    ServerToggle,
    ServerConnect,
    ServerClear,
    ServerHelp,
};

struct AppCommand {
    AppCommandType type = AppCommandType::None;
    String value;
    int32_t number = -1;
};

enum class InputSource : uint8_t {
    Usb,
    Tcp,
    Ble,
    Local,
};

struct RoutedCommand {
    AppCommand command;
    InputSource source = InputSource::Usb;
    uint32_t requestId = 0;
};

enum class WifiState : uint8_t {
    Disabled,
    NoCredentials,
    Ready,
    Scanning,
    Connecting,
    Connected,
    Backoff,
};

struct WifiSnapshot {
    WifiState state = WifiState::NoCredentials;
    String ssid;
    String ip;
    int32_t rssi = 0;
    uint32_t reconnectCount = 0;
    int16_t scanCount = -1;
    String lastError;
};

enum class ServerState : uint8_t {
    Disabled,
    NoTarget,
    WaitingWifi,
    Connecting,
    Connected,
    Backoff,
};

struct ServerSnapshot {
    bool enabled = false;
    ServerState state = ServerState::Disabled;
    String host;
    uint16_t port = 19000;
    uint32_t reconnectCount = 0;
    uint32_t messageCount = 0;
    String lastError;
};
