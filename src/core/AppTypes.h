#pragma once

#include <Arduino.h>

enum class AppId : uint8_t {
    SystemInfo,
    DisplayTest,
    DisplaySettings,
    SoundSettings,
    RgbSettings,
    ConsoleSettings,
    NetworkSettings,
    Launcher,
    Count,
};

enum class AppCommandType : uint8_t {
    None,
    Unknown,
    Previous,
    Next,
    Left,
    Right,
    Activate,
    Back,
    Home,
    PageSystem,
    PageDisplay,
    PageDisplaySettings,
    PageSound,
    PageRgb,
    PageConsole,
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
    MirrorOn,
    MirrorOff,
    MirrorToggle,
    MirrorStatus,
    BenchUpload,
    BenchDownload,
    BenchStatus,
    BenchCancel,
};

enum class BenchmarkState : uint8_t {
    Idle,
    Connecting,
    Uploading,
    Downloading,
    WaitingResult,
    Complete,
    Error,
};

struct BenchmarkSnapshot {
    BenchmarkState state = BenchmarkState::Idle;
    uint32_t totalBytes = 0;
    uint32_t transferredBytes = 0;
    uint64_t elapsedUs = 0;
    float mbps = 0.0F;
    String lastError;
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

struct MirrorSnapshot {
    bool enabled = false;
    bool connected = false;
    uint16_t port = 19002;
    uint32_t frameCount = 0;
    String host;
    String lastError;
};

struct RuntimeSnapshot {
    uint8_t mainLoopBusyPercent = 0;
    uint32_t mainLoopWorkUs = 0;
    uint32_t freeHeap = 0;
    uint32_t heapSize = 0;
    uint32_t minimumFreeHeap = 0;
    uint32_t freePsram = 0;
    uint32_t psramSize = 0;
    uint32_t flashSize = 0;
    uint32_t sketchSize = 0;
    uint32_t freeSketchSpace = 0;
    uint32_t taskCount = 0;
    uint32_t lastWifiUs = 0;
    uint32_t lastServerUs = 0;
    uint32_t lastMirrorUs = 0;
    uint32_t lastBenchmarkUs = 0;
    uint32_t lastAudioUs = 0;
    uint32_t lastDisplayUs = 0;
    uint32_t lastUiUs = 0;
    uint32_t lastFlushCopyUs = 0;
    uint32_t lastFlushTransferUs = 0;
    uint32_t lastFlushPixels = 0;
    uint16_t lastFlushAreas = 0;
};
