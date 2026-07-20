#pragma once

#include "core/AppTypes.h"

#include <Arduino.h>
#include <Preferences.h>

class WifiService {
public:
    void begin(Print& log);
    void tick(uint32_t nowMs);

    bool enabled() const;
    void setEnabled(bool enabled);
    void toggleEnabled();

    bool startScan();
    void cancelScan();
    bool selectScanIndex(int32_t index);
    bool selectSsid(const String& ssid);
    bool saveSelectedPassword(const String& password);
    bool saveSelectedOpen();
    void reconnect();
    void clearCredentials();

    WifiSnapshot snapshot() const;
    const char* stateName() const;
    int16_t scanCount() const;
    String scanSsid(int16_t index) const;
    int32_t scanRssi(int16_t index) const;
    bool scanIsOpen(int16_t index) const;
    uint32_t scanGeneration() const;
    bool lastScanSucceeded() const;
    const String& selectedSsid() const;
    void printStatus(Print& output) const;

private:
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
    static constexpr uint32_t INITIAL_RETRY_DELAY_MS = 1000;
    static constexpr uint32_t MAX_RETRY_DELAY_MS = 30000;
    static constexpr uint32_t SCAN_RETRY_DELAY_MS = 750;
    static constexpr uint8_t SCAN_MAX_RETRIES = 2;

    Preferences preferences_;
    Print* log_ = nullptr;
    bool preferencesReady_ = false;
    bool enabled_ = true;
    String ssid_;
    String password_;
    String selectedSsid_;
    WifiState state_ = WifiState::NoCredentials;
    String lastError_;
    uint32_t connectStartedMs_ = 0;
    uint32_t nextAttemptMs_ = 0;
    uint32_t reconnectCount_ = 0;
    uint8_t backoffStep_ = 0;
    int16_t scanCount_ = -1;
    uint32_t scanGeneration_ = 0;
    bool lastScanSucceeded_ = false;
    uint8_t scanRetryCount_ = 0;
    uint32_t scanRetryAtMs_ = 0;
    bool scanRadioResetPending_ = false;

    void startConnect(uint32_t nowMs);
    void configureStationMode();
    void scheduleReconnect(uint32_t nowMs, const char* reason);
    void startScanAttempt(uint32_t nowMs);
    void handleScanFailure(uint32_t nowMs, const char* reason);
    void finishScan(int16_t count);
    bool saveCredentials(const String& ssid, const String& password);
    WifiState idleState() const;
};
