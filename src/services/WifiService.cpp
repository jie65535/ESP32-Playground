#include "services/WifiService.h"

#include <WiFi.h>

void WifiService::begin(Print& log) {
    log_ = &log;
    preferencesReady_ = preferences_.begin("wifi", false);
    if (preferencesReady_) {
        enabled_ = preferences_.getBool("enabled", true);
        ssid_ = preferences_.getString("ssid", "");
        password_ = preferences_.getString("pass", "");
    } else {
        lastError_ = "NVS unavailable";
        log.println(F("[wifi] Preferences NVS unavailable"));
    }

    if (!enabled_) {
        WiFi.mode(WIFI_OFF);
        state_ = WifiState::Disabled;
        log.println(F("[wifi] off (restored from settings)"));
        return;
    }

    configureStationMode();
    if (ssid_.isEmpty()) {
        state_ = WifiState::NoCredentials;
        log.println(F("[wifi] no saved credentials; use wifi scan to configure"));
    } else {
        state_ = WifiState::Ready;
        nextAttemptMs_ = millis() + INITIAL_RETRY_DELAY_MS;
        log.print(F("[wifi] saved credentials found for ssid="));
        log.print(ssid_);
        log.println(F("; auto-connect scheduled"));
    }
}

void WifiService::tick(uint32_t nowMs) {
    if (!enabled_) {
        return;
    }

    if (state_ == WifiState::Scanning) {
        if (scanRetryAtMs_ != 0) {
            if (nowMs < scanRetryAtMs_) {
                return;
            }
            scanRetryAtMs_ = 0;
            if (scanRadioResetPending_) {
                WiFi.mode(WIFI_OFF);
                configureStationMode();
                scanRadioResetPending_ = false;
            }
            startScanAttempt(nowMs);
            return;
        }

        const int16_t result = WiFi.scanComplete();
        if (result >= 0) {
            finishScan(result);
        } else if (result < WIFI_SCAN_RUNNING) {
            handleScanFailure(nowMs, "scan failed");
        }
        return;
    }

    if (state_ == WifiState::Ready ||
        (state_ == WifiState::Backoff && nowMs >= nextAttemptMs_)) {
        startConnect(nowMs);
        return;
    }

    if (state_ == WifiState::Connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            state_ = WifiState::Connected;
            backoffStep_ = 0;
            lastError_ = "";
            log_->print(F("[wifi] connected ssid="));
            log_->print(ssid_);
            log_->print(F(" ip="));
            log_->print(WiFi.localIP());
            log_->print(F(" rssi="));
            log_->print(WiFi.RSSI());
            log_->println(F(" dBm"));
        } else if (nowMs - connectStartedMs_ >= CONNECT_TIMEOUT_MS) {
            scheduleReconnect(nowMs, "connect timeout");
        }
        return;
    }

    if (state_ == WifiState::Connected && WiFi.status() != WL_CONNECTED) {
        scheduleReconnect(nowMs, "connection lost");
    }
}

bool WifiService::enabled() const {
    return enabled_;
}

void WifiService::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (preferencesReady_) {
        preferences_.putBool("enabled", enabled_);
    }

    if (!enabled_) {
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
        state_ = WifiState::Disabled;
        scanCount_ = -1;
        selectedSsid_ = "";
        lastError_ = "";
        log_->println(F("[wifi] off; scanning and auto-connect disabled"));
    } else {
        configureStationMode();
        backoffStep_ = 0;
        lastError_ = "";
        state_ = ssid_.isEmpty() ? WifiState::NoCredentials : WifiState::Ready;
        nextAttemptMs_ = millis();
        log_->println(F("[wifi] on"));
    }
}

void WifiService::toggleEnabled() {
    setEnabled(!enabled_);
}

bool WifiService::startScan() {
    if (!enabled_) {
        log_->println(F("[wifi] off; use wifi on before scanning"));
        return false;
    }
    if (state_ == WifiState::Scanning) {
        log_->println(F("[wifi] scan already running"));
        return true;
    }

    if (state_ == WifiState::Connecting) {
        WiFi.disconnect(false, false);
    }
    state_ = WifiState::Scanning;
    scanCount_ = -1;
    selectedSsid_ = "";
    lastError_ = "";
    scanRetryCount_ = 0;
    scanRetryAtMs_ = 0;
    scanRadioResetPending_ = false;
    startScanAttempt(millis());
    return true;
}

void WifiService::cancelScan() {
    if (state_ != WifiState::Scanning) {
        return;
    }
    WiFi.scanDelete();
    scanRetryAtMs_ = 0;
    scanRadioResetPending_ = false;
    state_ = idleState();
    lastError_ = "";
    log_->println(F("[wifi] scan cancelled"));
}

bool WifiService::selectScanIndex(int32_t index) {
    if (scanCount_ < 0 || index < 0 || index >= scanCount_) {
        log_->println(F("[wifi] invalid index; run wifi scan first"));
        return false;
    }
    const String value = WiFi.SSID(static_cast<int16_t>(index));
    if (value.isEmpty()) {
        log_->println(F("[wifi] hidden SSID cannot be selected by index"));
        return false;
    }
    selectedSsid_ = value;
    log_->print(F("[wifi] selected ssid="));
    log_->print(selectedSsid_);
    log_->println(F("; enter wifi password <value>"));
    return true;
}

bool WifiService::selectSsid(const String& ssid) {
    if (ssid.isEmpty() || ssid.length() > 32) {
        log_->println(F("[wifi] SSID must contain 1 to 32 bytes"));
        return false;
    }
    selectedSsid_ = ssid;
    log_->print(F("[wifi] selected ssid="));
    log_->print(selectedSsid_);
    log_->println(F("; enter wifi password <value>"));
    return true;
}

bool WifiService::saveSelectedPassword(const String& password) {
    if (selectedSsid_.isEmpty()) {
        log_->println(F("[wifi] select an SSID first"));
        return false;
    }
    if (password.length() < 8 || password.length() > 63) {
        log_->println(F("[wifi] WPA/WPA2 password must contain 8 to 63 bytes"));
        return false;
    }
    return saveCredentials(selectedSsid_, password);
}

bool WifiService::saveSelectedOpen() {
    if (selectedSsid_.isEmpty()) {
        log_->println(F("[wifi] select an SSID first"));
        return false;
    }
    return saveCredentials(selectedSsid_, "");
}

void WifiService::reconnect() {
    if (!enabled_) {
        log_->println(F("[wifi] off; use wifi on first"));
        return;
    }
    if (ssid_.isEmpty()) {
        log_->println(F("[wifi] no saved SSID"));
        return;
    }
    WiFi.disconnect(false, false);
    backoffStep_ = 0;
    nextAttemptMs_ = millis();
    state_ = WifiState::Ready;
    lastError_ = "";
    log_->println(F("[wifi] reconnect requested"));
}

void WifiService::clearCredentials() {
    if (preferencesReady_) {
        preferences_.remove("ssid");
        preferences_.remove("pass");
    }
    WiFi.disconnect(false, false);
    ssid_ = "";
    password_ = "";
    selectedSsid_ = "";
    scanCount_ = -1;
    lastError_ = "";
    state_ = enabled_ ? WifiState::NoCredentials : WifiState::Disabled;
    log_->println(F("[wifi] credentials cleared"));
}

WifiSnapshot WifiService::snapshot() const {
    WifiSnapshot value;
    value.state = state_;
    value.ssid = ssid_;
    value.ip = state_ == WifiState::Connected ? WiFi.localIP().toString() : "--";
    value.rssi = state_ == WifiState::Connected ? WiFi.RSSI() : 0;
    value.reconnectCount = reconnectCount_;
    value.scanCount = scanCount_;
    value.lastError = lastError_;
    return value;
}

const char* WifiService::stateName() const {
    switch (state_) {
        case WifiState::Disabled:
            return "off";
        case WifiState::NoCredentials:
            return "no credentials";
        case WifiState::Ready:
            return "ready";
        case WifiState::Scanning:
            return "scanning";
        case WifiState::Connecting:
            return "connecting";
        case WifiState::Connected:
            return "connected";
        case WifiState::Backoff:
            return "backoff";
    }
    return "unknown";
}

int16_t WifiService::scanCount() const {
    return scanCount_;
}

String WifiService::scanSsid(int16_t index) const {
    String value = WiFi.SSID(index);
    return value.isEmpty() ? String("<hidden>") : value;
}

int32_t WifiService::scanRssi(int16_t index) const {
    return WiFi.RSSI(index);
}

bool WifiService::scanIsOpen(int16_t index) const {
    if (index < 0 || index >= scanCount_) {
        return false;
    }
    return WiFi.encryptionType(static_cast<uint8_t>(index)) == WIFI_AUTH_OPEN;
}

uint32_t WifiService::scanGeneration() const {
    return scanGeneration_;
}

bool WifiService::lastScanSucceeded() const {
    return lastScanSucceeded_;
}

const String& WifiService::selectedSsid() const {
    return selectedSsid_;
}

void WifiService::printStatus(Print& output) const {
    const WifiSnapshot value = snapshot();
    output.print(F("[wifi] state="));
    output.print(stateName());
    output.print(F(" ssid="));
    output.print(value.ssid.isEmpty() ? "--" : value.ssid);
    output.print(F(" ip="));
    output.print(value.ip);
    output.print(F(" rssi="));
    output.print(value.rssi);
    output.print(F(" dBm reconnects="));
    output.print(value.reconnectCount);
    output.print(F(" scan="));
    output.print(value.scanCount);
    if (!value.lastError.isEmpty()) {
        output.print(F(" error="));
        output.print(value.lastError);
    }
    output.println();
}

void WifiService::startConnect(uint32_t nowMs) {
    if (ssid_.isEmpty()) {
        state_ = WifiState::NoCredentials;
        return;
    }
    configureStationMode();
    WiFi.begin(ssid_.c_str(), password_.c_str());
    connectStartedMs_ = nowMs;
    state_ = WifiState::Connecting;
    lastError_ = "";
    log_->print(F("[wifi] connecting ssid="));
    log_->println(ssid_);
}

void WifiService::scheduleReconnect(uint32_t nowMs, const char* reason) {
    WiFi.disconnect(false, false);
    state_ = WifiState::Backoff;
    lastError_ = reason;
    reconnectCount_++;

    uint32_t delayMs = INITIAL_RETRY_DELAY_MS;
    for (uint8_t step = 0; step < backoffStep_ && delayMs < MAX_RETRY_DELAY_MS;
         ++step) {
        delayMs = min(delayMs * 2U, MAX_RETRY_DELAY_MS);
    }
    backoffStep_ = min<uint8_t>(backoffStep_ + 1U, 8U);
    nextAttemptMs_ = nowMs + delayMs;
    log_->print(F("[wifi] "));
    log_->print(reason);
    log_->print(F("; retry in "));
    log_->print(delayMs / 1000U);
    log_->println(F("s"));
}

void WifiService::startScanAttempt(uint32_t nowMs) {
    const int16_t previous = WiFi.scanComplete();
    if (previous >= 0) {
        WiFi.scanDelete();
    }
    configureStationMode();
    const int16_t result = WiFi.scanNetworks(true, true, false, 600);
    if (result == WIFI_SCAN_RUNNING) {
        log_->print(F("[wifi] scan started (non-blocking), attempt="));
        log_->println(scanRetryCount_ + 1U);
    } else if (result >= 0) {
        finishScan(result);
    } else {
        handleScanFailure(nowMs, "scan could not start");
    }
}

void WifiService::configureStationMode() {
    WiFi.mode(WIFI_STA);
    // This device is primarily an interactive, externally powered
    // development board. Disable modem sleep to avoid 100 ms-class latency
    // spikes on the TCP control channel; a battery-oriented profile can
    // restore WIFI_PS_MIN_MODEM later.
    WiFi.setSleep(false);
}

void WifiService::handleScanFailure(uint32_t nowMs, const char* reason) {
    WiFi.scanDelete();
    if (scanRetryCount_ < SCAN_MAX_RETRIES) {
        scanRetryCount_++;
        scanRetryAtMs_ = nowMs + SCAN_RETRY_DELAY_MS;
        scanRadioResetPending_ = WiFi.status() != WL_CONNECTED;
        log_->print(F("[wifi] "));
        log_->print(reason);
        log_->print(F("; retrying scan "));
        log_->print(scanRetryCount_ + 1U);
        log_->print('/');
        log_->println(SCAN_MAX_RETRIES + 1U);
        return;
    }

    scanCount_ = -1;
    scanGeneration_++;
    lastScanSucceeded_ = false;
    lastError_ = reason;
    state_ = idleState();
    log_->println(F("[wifi] scan failed after retries"));
}

void WifiService::finishScan(int16_t count) {
    scanCount_ = count;
    scanGeneration_++;
    lastScanSucceeded_ = true;
    lastError_ = "";
    state_ = idleState();

    log_->print(F("[wifi] scan results: "));
    log_->print(count);
    log_->println(F(" network(s)"));
    for (int16_t index = 0; index < count; ++index) {
        log_->print(F("  ["));
        log_->print(index);
        log_->print(F("] "));
        log_->print(scanSsid(index));
        log_->print(F(" RSSI="));
        log_->print(scanRssi(index));
        log_->print(F(" dBm channel="));
        log_->println(WiFi.channel(index));
    }
    log_->println(F("[wifi] choose with: wifi select <index>"));
    log_->print(F("[wifi] scan complete: "));
    log_->print(count);
    log_->println(F(" network(s)"));
}

bool WifiService::saveCredentials(const String& ssid, const String& password) {
    if (!preferencesReady_) {
        log_->println(F("[wifi] NVS unavailable; credentials were not saved"));
        return false;
    }
    preferences_.putString("ssid", ssid);
    preferences_.putString("pass", password);
    ssid_ = ssid;
    password_ = password;
    selectedSsid_ = "";
    scanCount_ = -1;
    WiFi.scanDelete();
    lastError_ = "";
    backoffStep_ = 0;
    nextAttemptMs_ = millis();
    state_ = enabled_ ? WifiState::Ready : WifiState::Disabled;
    log_->print(F("[wifi] credentials saved for ssid="));
    log_->print(ssid_);
    log_->println(F(" (password hidden)"));
    return true;
}

WifiState WifiService::idleState() const {
    if (!enabled_) {
        return WifiState::Disabled;
    }
    if (WiFi.status() == WL_CONNECTED) {
        return WifiState::Connected;
    }
    return ssid_.isEmpty() ? WifiState::NoCredentials : WifiState::Ready;
}
