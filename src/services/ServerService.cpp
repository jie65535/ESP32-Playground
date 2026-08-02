#include "services/ServerService.h"

namespace {

bool asciiAlphaNumeric(char value) {
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9');
}

}  // namespace

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

void ServerService::tick(uint32_t nowMs, const WifiSnapshot& wifi,
                         const RuntimeSnapshot& runtime) {
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
        sendHeartbeat(wifi, runtime);
        nextHeartbeatMs_ = nowMs + HEARTBEAT_INTERVAL_MS;
    }
}

bool ServerService::setTarget(const String& host, uint32_t port) {
    String normalizedHost = host;
    normalizedHost.trim();
    if (!validHost(normalizedHost) || port == 0 || port > 65535) {
        log_->println(F("[server] invalid host or port"));
        return false;
    }
    host_ = normalizedHost;
    port_ = static_cast<uint16_t>(port);
    enabled_ = true;
    if (preferencesReady_) {
        preferences_.putString("host", host_);
        preferences_.putUShort("port", port_);
        preferences_.putBool("enabled", true);
    }
    lastError_ = "";
    state_ = ServerState::WaitingWifi;
    backoffStep_ = 0;
    nextAttemptMs_ = millis();
    log_->print(F("[server] target saved: "));
    log_->print(host_);
    log_->print(':');
    log_->println(port_);
    return true;
}

bool ServerService::validHost(const String& host) {
    if (host.isEmpty() || host.length() > 63U) {
        return false;
    }

    bool labelStart = true;
    char previous = '\0';
    for (size_t index = 0; index < host.length(); ++index) {
        const char value = host[index];
        if (asciiAlphaNumeric(value)) {
            labelStart = false;
        } else if (value == '-') {
            if (labelStart) {
                return false;
            }
        } else if (value == '.') {
            if (labelStart || previous == '-') {
                return false;
            }
            labelStart = true;
        } else {
            return false;
        }
        previous = value;
    }
    return !labelStart && previous != '-';
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

bool ServerService::pollCommand(uint32_t& requestId, String& commandLine) {
    if (commandCount_ == 0) {
        return false;
    }
    PendingCommand& pending = commandQueue_[commandHead_];
    requestId = pending.requestId;
    commandLine = pending.line;
    pending = PendingCommand{};
    commandHead_ = static_cast<uint8_t>(
        (commandHead_ + 1U) % COMMAND_QUEUE_SIZE);
    commandCount_--;
    return true;
}

void ServerService::sendAck(uint32_t requestId, bool ok, const char* message) {
    if (!client_.connected()) {
        return;
    }
    client_.print(F("ACK "));
    client_.print(requestId);
    client_.print(ok ? F(" OK ") : F(" ERROR "));
    client_.println(message == nullptr ? "" : message);
    messageCount_++;
}

void ServerService::sendState(uint32_t requestId, const char* appName,
                              const WifiSnapshot& wifi) {
    if (!client_.connected()) {
        return;
    }
    client_.print(F("STATE "));
    client_.print(requestId);
    client_.print(F(" {\"app\":\""));
    client_.print(appName == nullptr ? "unknown" : appName);
    client_.print(F("\",\"wifi_state\":\""));
    client_.print(wifi.state == WifiState::Connected ? "connected" : "other");
    client_.print(F("\",\"ip\":\""));
    client_.print(wifi.ip);
    client_.print(F("\",\"rssi\":"));
    client_.print(wifi.rssi);
    client_.print(F(",\"heap\":"));
    client_.print(ESP.getFreeHeap());
    client_.println(F("}"));
    messageCount_++;
}

void ServerService::closeClient() {
    if (client_.connected()) {
        client_.stop();
    }
    clearCommandQueue();
}

bool ServerService::enqueueCommand(uint32_t requestId,
                                   const String& commandLine) {
    if (commandCount_ >= COMMAND_QUEUE_SIZE) {
        return false;
    }
    PendingCommand& pending = commandQueue_[commandTail_];
    pending.requestId = requestId;
    pending.line = commandLine;
    commandTail_ = static_cast<uint8_t>(
        (commandTail_ + 1U) % COMMAND_QUEUE_SIZE);
    commandCount_++;
    return true;
}

void ServerService::clearCommandQueue() {
    for (PendingCommand& pending : commandQueue_) {
        pending = PendingCommand{};
    }
    commandHead_ = 0;
    commandTail_ = 0;
    commandCount_ = 0;
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

void ServerService::sendHeartbeat(const WifiSnapshot& wifi,
                                  const RuntimeSnapshot& runtime) {
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
    client_.print(F(",\"main_loop_busy\":"));
    client_.print(runtime.mainLoopBusyPercent);
    client_.print(F(",\"min_free_heap\":"));
    client_.print(runtime.minimumFreeHeap);
    client_.print(F(",\"free_psram\":"));
    client_.print(runtime.freePsram);
    client_.print(F(",\"tasks\":"));
    client_.print(runtime.taskCount);
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
            } else if (receiveBuffer_.startsWith("CMD ")) {
                const String payload = receiveBuffer_.substring(4);
                const int separator = payload.indexOf(' ');
                if (separator <= 0) {
                    String requestText = payload;
                    requestText.trim();
                    bool valid = !requestText.isEmpty();
                    for (size_t index = 0; index < requestText.length(); ++index) {
                        if (requestText[index] < '0' ||
                            requestText[index] > '9') {
                            valid = false;
                            break;
                        }
                    }
                    sendAck(valid ? requestText.toInt() : 0, false,
                            valid ? "empty_command" : "bad_command");
                } else {
                    const String requestText = payload.substring(0, separator);
                    bool valid = !requestText.isEmpty();
                    for (size_t index = 0; index < requestText.length(); ++index) {
                        if (requestText[index] < '0' || requestText[index] > '9') {
                            valid = false;
                            break;
                        }
                    }
                    if (!valid) {
                        sendAck(0, false, "bad_request_id");
                    } else {
                        const uint32_t requestId = requestText.toInt();
                        const String commandLine =
                            payload.substring(separator + 1);
                        if (commandLine.isEmpty()) {
                            sendAck(requestId, false, "empty_command");
                        } else if (!enqueueCommand(requestId, commandLine)) {
                            sendAck(requestId, false, "busy");
                        }
                    }
                }
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
