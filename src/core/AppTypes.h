#pragma once

#include <Arduino.h>

enum class AppId : uint8_t {
    SystemInfo,
    Time,
    DisplayTest,
    DisplaySettings,
    SoundSettings,
    RgbSettings,
    ControllerSettings,
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
    PageTime,
    PageDisplay,
    PageDisplaySettings,
    PageSound,
    PageRgb,
    PageGamepad,
    PageConsole,
    PageNetwork,
    ColorTest,
    Screenshot,
    Status,
    Help,
    TimeStatus,
    TimeSet,
    I2cScan,
    GamepadStatus,
    GamepadRumble,
    GamepadScan,
    GamepadStopScan,
    GamepadDisconnect,
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

// A gamepad is a continuous input source.  Keep its latest state separate
// from InputRouter's low-rate AppCommand queue so games can sample it at their
// own frame rate without allocating or flooding the shell queue.
enum GamepadButtonMask : uint16_t {
    GamepadButtonA = 1U << 0U,
    GamepadButtonB = 1U << 1U,
    GamepadButtonX = 1U << 2U,
    GamepadButtonY = 1U << 3U,
    GamepadButtonShoulderL = 1U << 4U,
    GamepadButtonShoulderR = 1U << 5U,
    GamepadButtonTriggerL = 1U << 6U,
    GamepadButtonTriggerR = 1U << 7U,
    GamepadButtonThumbL = 1U << 8U,
    GamepadButtonThumbR = 1U << 9U,
};

enum GamepadMiscButtonMask : uint8_t {
    GamepadMiscSystem = 1U << 0U,
    GamepadMiscSelect = 1U << 1U,
    GamepadMiscStart = 1U << 2U,
    GamepadMiscCapture = 1U << 3U,
};

enum GamepadDpadMask : uint8_t {
    GamepadDpadUp = 1U << 0U,
    GamepadDpadDown = 1U << 1U,
    GamepadDpadRight = 1U << 2U,
    GamepadDpadLeft = 1U << 3U,
};

enum class GamepadEventType : uint8_t {
    Connected,
    Disconnected,
    ButtonDown,
    ButtonUp,
    DpadDown,
    DpadUp,
    MiscDown,
    MiscUp,
};

struct GamepadSnapshot {
    bool enabled = false;
    bool connected = false;
    bool scanning = false;
    uint8_t slot = 0xFF;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    uint16_t buttons = 0;
    uint8_t miscButtons = 0;
    uint8_t dpad = 0;
    int16_t axisX = 0;
    int16_t axisY = 0;
    int16_t axisRX = 0;
    int16_t axisRY = 0;
    uint16_t brake = 0;
    uint16_t throttle = 0;
    uint8_t battery = 0;
    uint32_t packetCount = 0;
    uint32_t lastUpdateMs = 0;
    uint32_t lastInputMs = 0;
    uint32_t connectedSinceMs = 0;
    uint32_t scanRemainingMs = 0;
    uint32_t autoDisconnectMs = 0;
    uint32_t disconnectCount = 0;
    uint32_t scanStartCount = 0;
    char model[32] = {};
};

struct GamepadEvent {
    GamepadEventType type = GamepadEventType::Disconnected;
    uint8_t slot = 0xFF;
    uint16_t code = 0;
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
    uint32_t lastGamepadUs = 0;
    uint32_t lastWifiUs = 0;
    uint32_t lastServerUs = 0;
    uint32_t lastMirrorUs = 0;
    uint32_t lastBenchmarkUs = 0;
    uint32_t lastTimeUs = 0;
    uint32_t lastAudioUs = 0;
    uint32_t lastDisplayUs = 0;
    uint32_t lastUiUs = 0;
    uint32_t lastFlushCopyUs = 0;
    uint32_t lastFlushTransferUs = 0;
    uint32_t lastFlushWallUs = 0;
    uint32_t lastFlushWaitUs = 0;
    uint32_t lastFlushPixels = 0;
    uint16_t lastFlushAreas = 0;
};
