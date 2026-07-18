#pragma once

#include "core/AppTypes.h"

#include <Arduino.h>
#include <WiFi.h>

class DisplayService;

class MirrorService {
public:
    static constexpr uint16_t DEFAULT_PORT = 19002;

    void begin(Print& log);
    void tick(uint32_t nowMs, const WifiSnapshot& wifi,
              const ServerSnapshot& server, DisplayService& display);

    void setEnabled(bool enabled);
    void toggleEnabled();
    MirrorSnapshot snapshot() const;
    void printStatus(Print& output) const;

private:
    static constexpr uint32_t INITIAL_RETRY_DELAY_MS = 1000;
    static constexpr uint32_t MAX_RETRY_DELAY_MS = 30000;
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 100;
    static constexpr uint32_t FRAME_INTERVAL_MS = 200;
    static constexpr size_t HEADER_BYTES = 22;
    static constexpr size_t CHUNK_BYTES = 8192;
    static constexpr uint32_t FRAME_BYTES = 320U * 240U * 2U;

    enum class SendStage : uint8_t {
        Idle,
        Header,
        Payload,
    };

    Print* log_ = nullptr;
    WiFiClient client_;
    bool enabled_ = false;
    bool connected_ = false;
    String lastError_;
    String host_;
    uint16_t port_ = DEFAULT_PORT;
    uint32_t nextAttemptMs_ = 0;
    uint32_t nextFrameMs_ = 0;
    uint32_t frameId_ = 0;
    uint32_t frameCount_ = 0;
    uint8_t backoffStep_ = 0;
    SendStage stage_ = SendStage::Idle;
    uint8_t header_[HEADER_BYTES] = {};
    size_t headerOffset_ = 0;
    size_t payloadOffset_ = 0;
    uint8_t* frameBuffer_ = nullptr;

    void closeClient();
    void scheduleRetry(uint32_t nowMs, const char* reason);
    bool prepareFrame(uint32_t nowMs, DisplayService& display);
    bool sendSome(uint32_t nowMs);
    static void putU16(uint8_t* destination, uint16_t value);
    static void putU32(uint8_t* destination, uint32_t value);
};
