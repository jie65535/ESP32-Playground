#include "services/MirrorService.h"

#include "services/DisplayService.h"

#include <esp_heap_caps.h>

namespace {

constexpr uint8_t FRAME_VERSION = 1;
constexpr uint8_t FRAME_TYPE_RGB565 = 1;

}  // namespace

void MirrorService::begin(Print& log) {
    log_ = &log;
    frameBuffer_ = static_cast<uint8_t*>(heap_caps_malloc(
        FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (frameBuffer_ == nullptr) {
        lastError_ = "PSRAM frame buffer unavailable";
    }
    if (log_ != nullptr) {
        log_->print(F("[mirror] state="));
        log_->print(enabled_ ? F("on") : F("off"));
        log_->print(F(" buffer="));
        log_->println(frameBuffer_ == nullptr ? F("unavailable") : F("ready"));
    }
}

void MirrorService::tick(uint32_t nowMs, const WifiSnapshot& wifi,
                         const ServerSnapshot& server,
                         DisplayService& display) {
    if (!enabled_ || frameBuffer_ == nullptr) {
        display.setCaptureEnabled(false);
        closeClient();
        return;
    }

    host_ = server.host;
    port_ = server.port >= 65534U
                ? DEFAULT_PORT
                : static_cast<uint16_t>(server.port + 2U);
    if (host_.isEmpty() || wifi.state != WifiState::Connected ||
        server.state != ServerState::Connected) {
        display.setCaptureEnabled(false);
        closeClient();
        return;
    }

    if (!client_.connected()) {
        display.setCaptureEnabled(false);
        if (static_cast<int32_t>(nowMs - nextAttemptMs_) < 0) {
            return;
        }
        closeClient();
        if (!client_.connect(host_.c_str(), port_, CONNECT_TIMEOUT_MS)) {
            display.setCaptureEnabled(false);
            scheduleRetry(nowMs, "connect failed");
            return;
        }
        client_.setTimeout(1);
        connected_ = true;
        backoffStep_ = 0;
        lastError_ = "";
        stage_ = SendStage::Idle;
        nextFrameMs_ = nowMs;
        if (log_ != nullptr) {
            log_->print(F("[mirror] connected "));
            log_->print(host_);
            log_->print(':');
            log_->println(port_);
        }
    }

    display.setCaptureEnabled(true);
    if (!display.captureReady()) {
        nextFrameMs_ = nowMs + 50U;
        return;
    }

    if (stage_ == SendStage::Idle &&
        static_cast<int32_t>(nowMs - nextFrameMs_) >= 0) {
        if (!prepareFrame(nowMs, display)) {
            scheduleRetry(nowMs, "frame snapshot failed");
            return;
        }
    }
    sendSome(nowMs);
}

void MirrorService::setEnabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    if (!enabled_) {
        closeClient();
    } else {
        nextAttemptMs_ = millis();
    }
    if (log_ != nullptr) {
        log_->print(F("[mirror] "));
        log_->println(enabled_ ? F("on") : F("off"));
    }
}

void MirrorService::toggleEnabled() {
    setEnabled(!enabled_);
}

MirrorSnapshot MirrorService::snapshot() const {
    MirrorSnapshot value;
    value.enabled = enabled_;
    value.connected = connected_;
    value.port = port_;
    value.frameCount = frameCount_;
    value.host = host_;
    value.lastError = lastError_;
    return value;
}

void MirrorService::printStatus(Print& output) const {
    const MirrorSnapshot value = snapshot();
    output.print(F("[mirror] state="));
    output.print(!value.enabled ? F("off")
                                : value.connected ? F("connected")
                                                   : F("waiting"));
    output.print(F(" target="));
    output.print(value.host.isEmpty() ? "--" : value.host);
    output.print(':');
    output.print(value.port);
    output.print(F(" frames="));
    output.print(value.frameCount);
    if (!value.lastError.isEmpty()) {
        output.print(F(" error="));
        output.print(value.lastError);
    }
    output.println();
}

void MirrorService::closeClient() {
    if (client_.connected()) {
        client_.stop();
    }
    connected_ = false;
    stage_ = SendStage::Idle;
    headerOffset_ = 0;
    payloadOffset_ = 0;
}

void MirrorService::scheduleRetry(uint32_t nowMs, const char* reason) {
    closeClient();
    lastError_ = reason;
    uint32_t delayMs = INITIAL_RETRY_DELAY_MS;
    for (uint8_t step = 0; step < backoffStep_ && delayMs < MAX_RETRY_DELAY_MS;
         ++step) {
        delayMs = min(delayMs * 2U, MAX_RETRY_DELAY_MS);
    }
    backoffStep_ = min<uint8_t>(backoffStep_ + 1U, 8U);
    nextAttemptMs_ = nowMs + delayMs;
}

bool MirrorService::prepareFrame(uint32_t nowMs, DisplayService& display) {
    if (frameBuffer_ == nullptr ||
        !display.copyShadowRgb565BE(0, frameBuffer_, FRAME_BYTES)) {
        return false;
    }
    header_[0] = 'P';
    header_[1] = 'G';
    header_[2] = 'M';
    header_[3] = 'F';
    header_[4] = FRAME_VERSION;
    header_[5] = FRAME_TYPE_RGB565;
    putU16(header_ + 6, 320);
    putU16(header_ + 8, 240);
    putU32(header_ + 10, FRAME_BYTES);
    putU32(header_ + 14, frameId_++);
    putU32(header_ + 18, nowMs);
    stage_ = SendStage::Header;
    headerOffset_ = 0;
    payloadOffset_ = 0;
    return true;
}

bool MirrorService::sendSome(uint32_t nowMs) {
    size_t budget = CHUNK_BYTES;
    while (budget > 0 && client_.connected()) {
        if (stage_ == SendStage::Header) {
            const size_t remaining = HEADER_BYTES - headerOffset_;
            const size_t count = min(remaining, budget);
            const size_t written = client_.write(header_ + headerOffset_, count);
            if (written == 0) {
                return false;
            }
            headerOffset_ += written;
            budget -= written;
            if (headerOffset_ == HEADER_BYTES) {
                stage_ = SendStage::Payload;
            }
            continue;
        }

        if (stage_ != SendStage::Payload) {
            return true;
        }

        if (payloadOffset_ == FRAME_BYTES) {
            stage_ = SendStage::Idle;
            frameCount_++;
            nextFrameMs_ = nowMs + FRAME_INTERVAL_MS;
            return true;
        }

        const size_t remaining = FRAME_BYTES - payloadOffset_;
        const size_t count = min(remaining, budget);
        const size_t written = client_.write(frameBuffer_ + payloadOffset_, count);
        if (written == 0) {
            return false;
        }
        payloadOffset_ += written;
        budget -= written;
    }
    return true;
}

void MirrorService::putU16(uint8_t* destination, uint16_t value) {
    destination[0] = static_cast<uint8_t>(value >> 8U);
    destination[1] = static_cast<uint8_t>(value & 0xffU);
}

void MirrorService::putU32(uint8_t* destination, uint32_t value) {
    destination[0] = static_cast<uint8_t>(value >> 24U);
    destination[1] = static_cast<uint8_t>(value >> 16U);
    destination[2] = static_cast<uint8_t>(value >> 8U);
    destination[3] = static_cast<uint8_t>(value & 0xffU);
}
