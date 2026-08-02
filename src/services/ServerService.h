#pragma once

#include "core/AppTypes.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

class ServerService {
public:
    static constexpr uint16_t DEFAULT_PORT = 19000;

    void begin(Print& log);
    void tick(uint32_t nowMs, const WifiSnapshot& wifi,
              const RuntimeSnapshot& runtime);

    bool setTarget(const String& host, uint32_t port);
    static bool validHost(const String& host);
    void setEnabled(bool enabled);
    void toggleEnabled();
    void connectNow();
    void clearTarget();

    ServerSnapshot snapshot() const;
    const char* stateName() const;
    void printStatus(Print& output) const;
    bool pollCommand(uint32_t& requestId, String& commandLine);
    void sendAck(uint32_t requestId, bool ok, const char* message);
    void sendState(uint32_t requestId, const char* appName,
                   const WifiSnapshot& wifi);

private:
    static constexpr uint8_t COMMAND_QUEUE_SIZE = 8;
    static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 5000;
    static constexpr uint32_t INITIAL_RETRY_DELAY_MS = 1000;
    static constexpr uint32_t MAX_RETRY_DELAY_MS = 30000;
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 500;

    Preferences preferences_;
    Print* log_ = nullptr;
    WiFiClient client_;
    bool preferencesReady_ = false;
    bool enabled_ = false;
    String host_;
    uint16_t port_ = DEFAULT_PORT;
    ServerState state_ = ServerState::Disabled;
    String lastError_;
    uint32_t nextAttemptMs_ = 0;
    uint32_t nextHeartbeatMs_ = 0;
    uint32_t reconnectCount_ = 0;
    uint32_t messageCount_ = 0;
    uint8_t backoffStep_ = 0;
    String receiveBuffer_;
    struct PendingCommand {
        uint32_t requestId = 0;
        String line;
    };
    PendingCommand commandQueue_[COMMAND_QUEUE_SIZE];
    uint8_t commandHead_ = 0;
    uint8_t commandTail_ = 0;
    uint8_t commandCount_ = 0;

    void closeClient();
    bool enqueueCommand(uint32_t requestId, const String& commandLine);
    void clearCommandQueue();
    void attemptConnect(uint32_t nowMs, const WifiSnapshot& wifi);
    void scheduleRetry(uint32_t nowMs, const char* reason);
    void sendHello(const WifiSnapshot& wifi);
    void sendHeartbeat(const WifiSnapshot& wifi,
                       const RuntimeSnapshot& runtime);
    void readIncoming();
    String deviceId() const;
};
