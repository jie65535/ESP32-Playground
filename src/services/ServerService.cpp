#include "services/ServerService.h"

void ServerService::begin(Print& log) {
    log_ = &log;
    receiveBuffer_.reserve(256);
    preferencesReady_ = preferences_.begin("server", false);
    if (preferencesReady_) {
        enabled_ = preferences_.getBool("enabled", false);
        host_ = preferences_.getString("host", "");
        port_ = preferences_.getUShort("port", DEFAULT_PORT);
    } else {
        lastError_ = "NVS unavailable";
    }

    if (!enabled_) {
        state_ = ServerState::Disabled;
    } else if (host_.isEmpty()) {
        state_ = ServerState::NoTarget;
    } else {
        state_ = ServerState::WaitingWifi;
        nextAttemptMs_ = millis();
    }

    log_->print(F("[server] state="));
    log_->print(stateName());
    if (!host_.isEmpty()) {
        log_->print(F(" target="));
        log_->print(host_);
        log_->print(':');
        log_->print(port_);
    }
    log_->println();
}

void ServerService::tick(uint32_t nowMs, const WifiSnapshot& wifi) {
    if (!enabled_) {
        return;
    }
    if (host_.isEmpty()) {
        state_ = ServerState::NoTarget;
        closeClient();
        return;
    }
    if (wifi.state != WifiState::Connected) {
        state_ = ServerState::WaitingWifi;
        closeClient();
        return;
    }

    if (!client_.connected()) {
        if (nowMs >= nextAttemptMs_) {
            attemptConnect(nowMs, wifi);
        }
        return;
    }

    state_ = ServerState::Connected;
    readIncoming();
    if (nowMs >= nextHeartbeatMs_) {
        sendHeartbeat(wifi);
        nextHeartbeatMs_ = nowMs + HEARTBEAT_INTERVAL_MS;
    }
}

bool ServerService::setTarget(const String& host, uint32_t port) {
    if (host.isEmpty() || host.length() > 63 || port == 0 || port > 65535) {
        log_->println(F("[server] invalid host or port"));
        return false;
    }
    host_ = host;
    port_ = static_cast<uint16_t>(port);
    if (preferencesReady_) {
        preferences_.putString("host", host_);
        preferences_.putUShort("port", port_);
    }
    lastError_ = "";
    state_ = enabled_ ? ServerState::WaitingWifi : ServerState::Disabled;
    nextAttemptMs_ = millis();
    log_->print(F("[server] target saved: "));
    log_->print(host_);
    log_->print(':');
    log_->println(port_);
    return true;
}

void ServerService::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (preferencesReady_) {
        preferences_.putBool("enabled", enabled_);
    }
    if (!enabled_) {
        closeClient();
        state_ = ServerState::Disabled;
        lastError_ = "";
        log_->println(F("[server] off"));
    } else if (host_.isEmpty()) {
        state_ = ServerState::NoTarget;
        log_->println(F("[server] on, but no target configured"));
    } else {
        state_ = ServerState::WaitingWifi;
        nextAttemptMs_ = millis();
        backoffStep_ = 0;
        log_->println(F("[server] on"));
    }
}

void ServerService::toggleEnabled() {
    setEnabled(!enabled_);
}

void ServerService::connectNow() {
    if (!enabled_) {
        setEnabled(true);
    }
    nextAttemptMs_ = millis();
    backoffStep_ = 0;
    lastError_ = "";
    if (host_.isEmpty()) {
        state_ = ServerState::NoTarget;
    } else {
        state_ = ServerState::WaitingWifi;
    }
}

void ServerService::clearTarget() {
    closeClient();
    host_ = "";
    port_ = DEFAULT_PORT;
    enabled_ = false;
    state_ = ServerState::Disabled;
    lastError_ = "";
    if (preferencesReady_) {
        preferences_.remove("host");
        preferences_.remove("port");
        preferences_.putBool("enabled", false);
    }
    log_->println(F("[server] target cleared"));
}

ServerSnapshot ServerService::snapshot() const {
    ServerSnapshot value;
    value.enabled = enabled_;
    value.state = state_;
    value.host = host_;
    value.port = port_;
    value.reconnectCount = reconnectCount_;
    value.messageCount = messageCount_;
    value.lastError = lastError_;
    return value;
}

const char* ServerService::stateName() const {
    switch (state_) {
        case ServerState::Disabled:
            return "off";
        case ServerState::NoTarget:
            return "no target";
        case ServerState::WaitingWifi:
            return "waiting wifi";
        case ServerState::Connecting:
            return "connecting";
        case ServerState::Connected:
            return "connected";
        case ServerState::Backoff:
            return "backoff";
    }
    return "unknown";
}

void ServerService::printStatus(Print& output) const {
    output.print(F("[server] state="));
    output.print(stateName());
    output.print(F(" target="));
    output.print(host_.isEmpty() ? "--" : host_);
    output.print(':');
    output.print(port_);
    output.print(F(" reconnects="));
    output.print(reconnectCount_);
    output.print(F(" messages="));
    output.print(messageCount_);
    if (!lastError_.isEmpty()) {
        output.print(F(" error="));
        output.print(lastError_);
    }
    output.println();
}

void ServerService::closeClient() {
    if (client_.connected()) {
        client_.stop();
    }
}

void ServerService::attemptConnect(uint32_t nowMs, const WifiSnapshot& wifi) {
    state_ = ServerState::Connecting;
    log_->print(F("[server] connecting "));
    log_->print(host_);
    log_->print(':');
    log_->println(port_);

    closeClient();
    if (client_.connect(host_.c_str(), port_, CONNECT_TIMEOUT_MS)) {
        state_ = ServerState::Connected;
        backoffStep_ = 0;
        lastError_ = "";
        nextHeartbeatMs_ = nowMs;
        sendHello(wifi);
        log_->println(F("[server] connected"));
    } else {
        scheduleRetry(nowMs, "connect failed");
    }
}

void ServerService::scheduleRetry(uint32_t nowMs, const char* reason) {
    closeClient();
    state_ = ServerState::Backoff;
    lastError_ = reason;
    reconnectCount_++;
    uint32_t delayMs = INITIAL_RETRY_DELAY_MS;
    for (uint8_t step = 0; step < backoffStep_ && delayMs < MAX_RETRY_DELAY_MS;
         ++step) {
        delayMs = min(delayMs * 2U, MAX_RETRY_DELAY_MS);
    }
    backoffStep_ = min<uint8_t>(backoffStep_ + 1U, 8U);
    nextAttemptMs_ = nowMs + delayMs;
    log_->print(F("[server] "));
    log_->print(reason);
    log_->print(F("; retry in "));
    log_->print(delayMs / 1000U);
    log_->println(F("s"));
}

void ServerService::sendHello(const WifiSnapshot& wifi) {
    client_.print(F("PGOS/1 HELLO {\"device_id\":\""));
    client_.print(deviceId());
    client_.print(F("\",\"firmware\":\"0.1.0\",\"ip\":\""));
    client_.print(wifi.ip);
    client_.print(F("\"}\n"));
    messageCount_++;
}

void ServerService::sendHeartbeat(const WifiSnapshot& wifi) {
    client_.print(F("{\"type\":\"heartbeat\",\"device_id\":\""));
    client_.print(deviceId());
    client_.print(F("\",\"uptime_ms\":"));
    client_.print(millis());
    client_.print(F(",\"ip\":\""));
    client_.print(wifi.ip);
    client_.print(F("\",\"rssi\":"));
    client_.print(wifi.rssi);
    client_.print(F(",\"heap\":"));
    client_.print(ESP.getFreeHeap());
    client_.print(F("}\n"));
    messageCount_++;
}

void ServerService::readIncoming() {
    while (client_.available() > 0) {
        const char character = static_cast<char>(client_.read());
        if (character == '\n') {
            receiveBuffer_.trim();
            if (receiveBuffer_ == "PING") {
                client_.println(F("PONG"));
                messageCount_++;
            } else if (!receiveBuffer_.isEmpty()) {
                log_->print(F("[server] rx "));
                log_->println(receiveBuffer_);
            }
            receiveBuffer_ = "";
        } else if (receiveBuffer_.length() < 240) {
            receiveBuffer_ += character;
        } else {
            receiveBuffer_ = "";
            log_->println(F("[server] incoming frame discarded: too long"));
        }
    }
}

String ServerService::deviceId() const {
    const uint64_t mac = ESP.getEfuseMac();
    char value[24] = {};
    snprintf(value, sizeof(value), "pgos-%08lX%08lX",
             static_cast<unsigned long>(mac >> 32),
             static_cast<unsigned long>(mac & 0xFFFFFFFFULL));
    return String(value);
}
