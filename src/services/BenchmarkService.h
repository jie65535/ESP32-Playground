#pragma once

#include "core/AppTypes.h"

#include <Arduino.h>
#include <WiFi.h>

class BenchmarkService {
public:
    static constexpr uint16_t DEFAULT_PORT = 19001;

    void begin(Print& log);
    bool startUpload(const String& host, uint16_t port, uint32_t bytes);
    bool startDownload(const String& host, uint16_t port, uint32_t bytes);
    void tick(uint32_t nowMs);
    void cancel();

    BenchmarkSnapshot snapshot() const;
    const char* stateName() const;
    void printStatus(Print& output) const;

private:
    static constexpr size_t BUFFER_SIZE = 4096;
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 500;
    static constexpr uint32_t TRANSFER_TIMEOUT_MS = 30000;

    Print* log_ = nullptr;
    WiFiClient client_;
    BenchmarkState state_ = BenchmarkState::Idle;
    String host_;
    uint16_t port_ = DEFAULT_PORT;
    uint32_t totalBytes_ = 0;
    uint32_t transferredBytes_ = 0;
    uint64_t startedUs_ = 0;
    uint64_t elapsedUs_ = 0;
    uint32_t lastActivityMs_ = 0;
    float mbps_ = 0.0F;
    String lastError_;
    String resultBuffer_;
    uint8_t buffer_[BUFFER_SIZE] = {};

    bool start(const String& host, uint16_t port, uint32_t bytes,
               BenchmarkState transferState);
    void attemptConnect(uint32_t nowMs);
    void tickUpload(uint32_t nowMs);
    void tickDownload(uint32_t nowMs);
    void tickResult(uint32_t nowMs);
    void complete(uint64_t elapsedUs);
    void fail(const char* error);
};
