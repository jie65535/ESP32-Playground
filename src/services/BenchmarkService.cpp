#include "services/BenchmarkService.h"

#include <esp_timer.h>

void BenchmarkService::begin(Print& log) {
    log_ = &log;
    resultBuffer_.reserve(96);
    for (size_t index = 0; index < BUFFER_SIZE; ++index) {
        buffer_[index] = static_cast<uint8_t>((index * 31U + 17U) & 0xFFU);
    }
}

bool BenchmarkService::startUpload(const String& host, uint16_t port,
                                   uint32_t bytes) {
    return start(host, port, bytes, BenchmarkState::Uploading);
}

bool BenchmarkService::startDownload(const String& host, uint16_t port,
                                     uint32_t bytes) {
    return start(host, port, bytes, BenchmarkState::Downloading);
}

void BenchmarkService::tick(uint32_t nowMs) {
    if (state_ == BenchmarkState::Idle || state_ == BenchmarkState::Complete ||
        state_ == BenchmarkState::Error) {
        return;
    }

    if (state_ == BenchmarkState::Connecting) {
        attemptConnect(nowMs);
        return;
    }

    if (nowMs - lastActivityMs_ >= TRANSFER_TIMEOUT_MS) {
        fail("transfer timeout");
        return;
    }

    if (state_ == BenchmarkState::Uploading) {
        tickUpload(nowMs);
    } else if (state_ == BenchmarkState::Downloading) {
        tickDownload(nowMs);
    } else if (state_ == BenchmarkState::WaitingResult) {
        tickResult(nowMs);
    }
}

void BenchmarkService::cancel() {
    client_.stop();
    state_ = BenchmarkState::Idle;
    lastError_ = "";
    log_->println(F("[bench] cancelled"));
}

BenchmarkSnapshot BenchmarkService::snapshot() const {
    BenchmarkSnapshot value;
    value.state = state_;
    value.totalBytes = totalBytes_;
    value.transferredBytes = transferredBytes_;
    value.elapsedUs = elapsedUs_;
    value.mbps = mbps_;
    value.lastError = lastError_;
    return value;
}

const char* BenchmarkService::stateName() const {
    switch (state_) {
        case BenchmarkState::Idle:
            return "idle";
        case BenchmarkState::Connecting:
            return "connecting";
        case BenchmarkState::Uploading:
            return "uploading";
        case BenchmarkState::Downloading:
            return "downloading";
        case BenchmarkState::WaitingResult:
            return "waiting result";
        case BenchmarkState::Complete:
            return "complete";
        case BenchmarkState::Error:
            return "error";
    }
    return "unknown";
}

void BenchmarkService::printStatus(Print& output) const {
    output.print(F("[bench] state="));
    output.print(stateName());
    output.print(F(" bytes="));
    output.print(transferredBytes_);
    output.print('/');
    output.print(totalBytes_);
    output.print(F(" elapsed_us="));
    output.print(static_cast<unsigned long>(elapsedUs_));
    output.print(F(" mbps="));
    output.print(mbps_, 2);
    if (!lastError_.isEmpty()) {
        output.print(F(" error="));
        output.print(lastError_);
    }
    output.println();
}

bool BenchmarkService::start(const String& host, uint16_t port, uint32_t bytes,
                             BenchmarkState transferState) {
    if (host.isEmpty() || port == 0 || bytes == 0) {
        log_->println(F("[bench] configure a server target and positive byte count"));
        return false;
    }
    client_.stop();
    host_ = host;
    port_ = port;
    totalBytes_ = bytes;
    transferredBytes_ = 0;
    startedUs_ = 0;
    elapsedUs_ = 0;
    mbps_ = 0.0F;
    lastError_ = "";
    resultBuffer_ = "";
    state_ = BenchmarkState::Connecting;
    // Remember the requested direction until the connection is ready.
    resultBuffer_ = transferState == BenchmarkState::Uploading ? "UPLOAD"
                                                                : "DOWNLOAD";
    lastActivityMs_ = millis();
    log_->print(F("[bench] starting "));
    log_->print(resultBuffer_);
    log_->print(F(" bytes="));
    log_->println(totalBytes_);
    return true;
}

void BenchmarkService::attemptConnect(uint32_t nowMs) {
    const String direction = resultBuffer_;
    if (!client_.connect(host_.c_str(), port_, CONNECT_TIMEOUT_MS)) {
        fail("connect failed");
        return;
    }
    client_.print(F("PGOS_BENCH/1 "));
    client_.print(direction);
    client_.print(' ');
    client_.println(totalBytes_);
    transferredBytes_ = 0;
    resultBuffer_ = "";
    lastActivityMs_ = nowMs;
    state_ = direction == "UPLOAD" ? BenchmarkState::Uploading
                                    : BenchmarkState::Downloading;
}

void BenchmarkService::tickUpload(uint32_t nowMs) {
    if (!client_.connected()) {
        fail("connection lost during upload");
        return;
    }
    const uint32_t remaining = totalBytes_ - transferredBytes_;
    const size_t chunk = remaining < BUFFER_SIZE ? remaining : BUFFER_SIZE;
    if (startedUs_ == 0) {
        startedUs_ = esp_timer_get_time();
    }
    const size_t written = client_.write(buffer_, chunk);
    if (written > 0) {
        transferredBytes_ += written;
        lastActivityMs_ = nowMs;
    }
    if (transferredBytes_ >= totalBytes_) {
        state_ = BenchmarkState::WaitingResult;
        resultBuffer_ = "";
    }
}

void BenchmarkService::tickDownload(uint32_t nowMs) {
    const int available = client_.available();
    if (available <= 0) {
        if (!client_.connected()) {
            fail("connection lost during download");
        }
        return;
    }
    const uint32_t remaining = totalBytes_ - transferredBytes_;
    const size_t chunk = min<size_t>(
        min<size_t>(static_cast<size_t>(available), BUFFER_SIZE), remaining);
    const int read = client_.read(buffer_, chunk);
    if (read <= 0) {
        return;
    }
    if (startedUs_ == 0) {
        startedUs_ = esp_timer_get_time();
    }
    transferredBytes_ += static_cast<uint32_t>(read);
    lastActivityMs_ = nowMs;
    if (transferredBytes_ >= totalBytes_) {
        const uint64_t elapsed = esp_timer_get_time() - startedUs_;
        client_.print(F("RESULT "));
        client_.print(totalBytes_);
        client_.print(' ');
        client_.println(static_cast<unsigned long>(elapsed));
        complete(elapsed);
    }
}

void BenchmarkService::tickResult(uint32_t nowMs) {
    while (client_.available() > 0) {
        const char character = static_cast<char>(client_.read());
        lastActivityMs_ = nowMs;
        if (character == '\n') {
            resultBuffer_.trim();
            unsigned long bytes = 0;
            unsigned long elapsed = 0;
            if (sscanf(resultBuffer_.c_str(), "RESULT %lu %lu", &bytes,
                       &elapsed) == 2 && bytes == totalBytes_ && elapsed > 0) {
                complete(elapsed);
            } else {
                fail("invalid result frame");
            }
            resultBuffer_ = "";
            return;
        }
        if (resultBuffer_.length() < 95) {
            resultBuffer_ += character;
        } else {
            fail("result frame too long");
            return;
        }
    }
    if (!client_.connected() && client_.available() == 0) {
        fail("connection lost before result");
    }
}

void BenchmarkService::complete(uint64_t elapsedUs) {
    elapsedUs_ = elapsedUs;
    mbps_ = elapsedUs_ == 0
                ? 0.0F
                : static_cast<float>(totalBytes_) * 8.0F /
                      static_cast<float>(elapsedUs_);
    state_ = BenchmarkState::Complete;
    client_.stop();
    log_->print(F("[bench] complete bytes="));
    log_->print(totalBytes_);
    log_->print(F(" elapsed_us="));
    log_->print(static_cast<unsigned long>(elapsedUs_));
    log_->print(F(" mbps="));
    log_->println(mbps_, 2);
}

void BenchmarkService::fail(const char* error) {
    client_.stop();
    state_ = BenchmarkState::Error;
    lastError_ = error;
    log_->print(F("[bench] error: "));
    log_->println(error);
}
