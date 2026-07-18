#include "services/DisplayService.h"

#include <esp_timer.h>
#include <esp_heap_caps.h>

DisplayService::DisplayService() = default;

void DisplayService::holdBacklightOff() {
    pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(LCD_BACKLIGHT_PIN, LOW);
}

void DisplayService::begin() {
    holdBacklightOff();

    preferencesReady_ = preferences_.begin("pgos_display", false);
    if (preferencesReady_) {
        brightnessPercent_ = preferences_.getUChar(
            "brightness", DEFAULT_BRIGHTNESS_PERCENT);
        if (brightnessPercent_ < 10 || brightnessPercent_ > 100) {
            brightnessPercent_ = DEFAULT_BRIGHTNESS_PERCENT;
        }
        screenTimeoutSeconds_ = preferences_.getUInt(
            "timeout_s", DEFAULT_TIMEOUT_SECONDS);
        if (screenTimeoutSeconds_ > 24U * 60U * 60U) {
            screenTimeoutSeconds_ = DEFAULT_TIMEOUT_SECONDS;
        }
    } else {
        Serial.println(F("[display] NVS settings unavailable; using defaults"));
    }

    display_.begin();
    display_.setRotation(1);
    display_.setTextWrap(false);

    ledcSetup(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_PWM_FREQUENCY,
              BACKLIGHT_PWM_RESOLUTION);
    ledcAttachPin(LCD_BACKLIGHT_PIN, BACKLIGHT_PWM_CHANNEL);
    ledcWrite(BACKLIGHT_PWM_CHANNEL, 0);

    const size_t pixels = static_cast<size_t>(SCREEN_WIDTH) * SCREEN_HEIGHT;
    shadow_ = static_cast<uint16_t*>(heap_caps_malloc(
        pixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (shadow_ == nullptr) {
        shadow_ = static_cast<uint16_t*>(malloc(pixels * sizeof(uint16_t)));
    }

    if (shadow_ == nullptr) {
        Serial.println(F("[display] LVGL shadow framebuffer allocation failed"));
        return;
    }

    memset(shadow_, 0, pixels * sizeof(uint16_t));
    ready_ = true;
    lastActivityMs_ = millis();
}

bool DisplayService::ready() const {
    return ready_;
}

void DisplayService::pushBacklightOn() {
    backlightArmed_ = true;
    screenOff_ = false;
    lastActivityMs_ = millis();
    applyBacklight();
}

void DisplayService::tick(uint32_t nowMs) {
    if (!ready_) {
        return;
    }

    if (settingsDirty_ &&
        static_cast<int32_t>(nowMs - settingsSaveDueMs_) >= 0) {
        saveSettings();
    }

    if (backlightArmed_ && !screenOff_ && screenTimeoutSeconds_ > 0 &&
        nowMs - lastActivityMs_ >= screenTimeoutSeconds_ * 1000UL) {
        screenOff_ = true;
        applyBacklight();
        Serial.println(F("[display] screen backlight off (idle timeout)"));
    }
}

bool DisplayService::noteActivity(uint32_t nowMs) {
    const bool woke = screenOff_;
    lastActivityMs_ = nowMs;
    if (woke) {
        screenOff_ = false;
        applyBacklight();
        Serial.println(F("[display] backlight on (activity)"));
    }
    return woke;
}

DisplayService::FlushMetrics DisplayService::flushMetrics() const {
    return lastFlushMetrics_;
}

bool DisplayService::initDma() {
    if (dmaEnabled_) {
        return true;
    }

    dmaBuffer_ = static_cast<uint16_t*>(heap_caps_malloc(
        DMA_BUFFER_PIXELS * sizeof(uint16_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    if (dmaBuffer_ == nullptr) {
        Serial.println(F("[display] DMA staging buffer unavailable"));
        return false;
    }

    dmaEnabled_ = display_.initDMA();
    if (!dmaEnabled_) {
        heap_caps_free(dmaBuffer_);
        dmaBuffer_ = nullptr;
        Serial.println(F("[display] SPI DMA unavailable"));
        return false;
    }
    display_.setSwapBytes(true);
    Serial.println(dmaEnabled_ ? F("[display] SPI DMA enabled")
                               : F("[display] SPI DMA unavailable"));
    return dmaEnabled_;
}

bool DisplayService::dmaEnabled() const {
    return dmaEnabled_;
}

void DisplayService::setAsyncFlushEnabled(bool enabled) {
    asyncFlushEnabled_ = enabled && dmaEnabled_;
    Serial.println(asyncFlushEnabled_ ? F("[display] async flush enabled")
                                      : F("[display] sync flush enabled"));
}

bool DisplayService::asyncFlushEnabled() const {
    return asyncFlushEnabled_;
}

bool DisplayService::finishDmaIfReady() {
    if (!dmaPending_ || display_.dmaBusy()) {
        return false;
    }

    completeDmaTransfer();
    return true;
}

void DisplayService::waitForDma() {
    if (!dmaPending_) {
        return;
    }

    display_.dmaWait();
    completeDmaTransfer();
}

void DisplayService::completeDmaTransfer() {
    dmaPending_ = false;
    activeFlushMetrics_.transferUs += static_cast<uint32_t>(min<uint64_t>(
        esp_timer_get_time() - dmaStartedUs_, UINT32_MAX));
    if (dmaPendingLast_) {
        finishFlushFrame();
    }
    dmaPendingLast_ = false;
}

void DisplayService::finishFlushFrame() {
    lastFlushMetrics_ = activeFlushMetrics_;
    activeFlushMetrics_ = FlushMetrics{};
}

uint8_t DisplayService::brightnessPercent() const {
    return brightnessPercent_;
}

void DisplayService::setBrightnessPercent(uint8_t percent) {
    percent = constrain(percent, static_cast<uint8_t>(10),
                         static_cast<uint8_t>(100));
    if (percent == brightnessPercent_) {
        return;
    }
    brightnessPercent_ = percent;
    settingsDirty_ = true;
    settingsSaveDueMs_ = millis() + 750U;
    applyBacklight();
}

uint32_t DisplayService::screenTimeoutSeconds() const {
    return screenTimeoutSeconds_;
}

void DisplayService::setScreenTimeoutSeconds(uint32_t seconds) {
    if (seconds > 24U * 60U * 60U) {
        seconds = DEFAULT_TIMEOUT_SECONDS;
    }
    if (seconds == screenTimeoutSeconds_) {
        return;
    }
    screenTimeoutSeconds_ = seconds;
    settingsDirty_ = true;
    settingsSaveDueMs_ = millis() + 750U;
    lastActivityMs_ = millis();
    if (screenOff_) {
        screenOff_ = false;
        applyBacklight();
    }
}

bool DisplayService::screenIsOff() const {
    return screenOff_;
}

void DisplayService::printStatus(Print& output) const {
    output.print(F("[display] brightness="));
    output.print(brightnessPercent_);
    output.print(F("% timeout="));
    if (screenTimeoutSeconds_ == 0) {
        output.print(F("never"));
    } else {
        output.print(screenTimeoutSeconds_);
        output.print(F("s"));
    }
    output.print(F(" backlight="));
    output.print(screenOff_ ? F("off") : F("on"));
    output.print(F(" flush_us spi="));
    output.print(lastFlushMetrics_.transferUs);
    output.print(F(" copy="));
    output.print(lastFlushMetrics_.copyUs);
    output.print(F(" areas="));
    output.print(lastFlushMetrics_.areas);
    output.print(F(" pixels="));
    output.println(lastFlushMetrics_.pixels);
}

bool DisplayService::flush(const lv_area_t& area, const uint8_t* pixels,
                           bool lastArea) {
    if (!ready_ || pixels == nullptr || screenshotInProgress_) {
        return false;
    }

    int32_t x1 = area.x1;
    int32_t y1 = area.y1;
    int32_t x2 = area.x2;
    int32_t y2 = area.y2;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= SCREEN_WIDTH) x2 = SCREEN_WIDTH - 1;
    if (y2 >= SCREEN_HEIGHT) y2 = SCREEN_HEIGHT - 1;
    if (x1 > x2 || y1 > y2) {
        return false;
    }

    const int32_t width = x2 - x1 + 1;
    const int32_t height = y2 - y1 + 1;
    const size_t screenWidth = SCREEN_WIDTH;

    const uint16_t* colors = reinterpret_cast<const uint16_t*>(pixels);
    const uint64_t copyStartedUs = esp_timer_get_time();
    for (int32_t row = 0; row < height; ++row) {
        const uint16_t* source = colors + row * width;
        uint16_t* target = shadow_ +
                           static_cast<size_t>(y1 + row) * screenWidth + x1;
        memcpy(target, source, static_cast<size_t>(width) * sizeof(uint16_t));
    }
    activeFlushMetrics_.copyUs += static_cast<uint32_t>(
        min<uint64_t>(esp_timer_get_time() - copyStartedUs, UINT32_MAX));

    if (asyncFlushEnabled_) {
        if (dmaPending_) {
            waitForDma();
        }

        dmaStartedUs_ = esp_timer_get_time();
        display_.startWrite();
        display_.pushImageDMA(
            x1, y1, width, height,
            const_cast<uint16_t*>(reinterpret_cast<const uint16_t*>(pixels)),
            dmaBuffer_);
        display_.dmaWait();
        display_.endWrite();
        activeFlushMetrics_.transferUs += static_cast<uint32_t>(min<uint64_t>(
            esp_timer_get_time() - dmaStartedUs_, UINT32_MAX));
        activeFlushMetrics_.pixels += static_cast<uint32_t>(width * height);
        activeFlushMetrics_.areas++;
        if (lastArea) {
            finishFlushFrame();
        }
        return false;
    }

    const uint64_t transferStartedUs = esp_timer_get_time();
    display_.startWrite();
    display_.setAddrWindow(x1, y1, width, height);
    display_.pushColors(const_cast<uint16_t*>(colors),
                         width * height, true);
    display_.endWrite();
    activeFlushMetrics_.transferUs += static_cast<uint32_t>(
        min<uint64_t>(esp_timer_get_time() - transferStartedUs, UINT32_MAX));
    activeFlushMetrics_.pixels += static_cast<uint32_t>(width * height);
    activeFlushMetrics_.areas++;
    if (lastArea) {
        finishFlushFrame();
    }
    return false;
}

void DisplayService::writeScreenshot(Stream& output) {
    constexpr size_t screenshotBytes =
        static_cast<size_t>(SCREEN_WIDTH) * SCREEN_HEIGHT * 2U;

    if (!ready_ || shadow_ == nullptr || screenshotInProgress_) {
        output.println(F("[screenshot] unavailable"));
        return;
    }

    screenshotInProgress_ = true;
    output.print(F("PLAYGROUND_SCREENSHOT 1 320 240 RGB565BE "));
    output.println(screenshotBytes);
    output.flush();

    uint8_t chunk[SCREENSHOT_CHUNK];
    size_t offset = 0;
    while (offset < screenshotBytes) {
        const size_t count = min(sizeof(chunk), screenshotBytes - offset);
        const size_t firstPixel = offset / 2U;
        for (size_t index = 0; index < count / 2U; ++index) {
            const uint16_t value = shadow_[firstPixel + index];
            chunk[index * 2U] = static_cast<uint8_t>(value >> 8U);
            chunk[index * 2U + 1U] = static_cast<uint8_t>(value & 0xffU);
        }

        size_t written = 0;
        while (written < count) {
            const size_t step = output.write(chunk + written, count - written);
            if (step == 0) {
                delay(1);
                continue;
            }
            written += step;
        }
        offset += count;
        yield();
    }

    output.print(F("\nPLAYGROUND_SCREENSHOT_END\n"));
    output.flush();
    screenshotInProgress_ = false;
}

bool DisplayService::copyShadowRgb565BE(size_t offset, uint8_t* destination,
                                        size_t length) const {
    const size_t totalBytes =
        static_cast<size_t>(SCREEN_WIDTH) * SCREEN_HEIGHT * 2U;
    if (!ready_ || shadow_ == nullptr || destination == nullptr ||
        offset > totalBytes || length > totalBytes - offset ||
        (offset & 1U) != 0 || (length & 1U) != 0) {
        return false;
    }

    const size_t firstPixel = offset / 2U;
    for (size_t index = 0; index < length / 2U; ++index) {
        const uint16_t value = shadow_[firstPixel + index];
        destination[index * 2U] = static_cast<uint8_t>(value >> 8U);
        destination[index * 2U + 1U] = static_cast<uint8_t>(value & 0xffU);
    }
    return true;
}

String DisplayService::formatBytes(size_t bytes) {
    if (bytes >= 1024U * 1024U) {
        return String(static_cast<float>(bytes) / (1024.0F * 1024.0F), 1) +
               " MB";
    }
    return String(static_cast<float>(bytes) / 1024.0F, 1) + " KB";
}

void DisplayService::applyBacklight() {
    if (!backlightArmed_ || screenOff_) {
        ledcWrite(BACKLIGHT_PWM_CHANNEL, 0);
        return;
    }
    const uint32_t duty = static_cast<uint32_t>(brightnessPercent_) * 255U / 100U;
    ledcWrite(BACKLIGHT_PWM_CHANNEL, duty);
}

void DisplayService::saveSettings() {
    settingsDirty_ = false;
    if (!preferencesReady_) {
        return;
    }
    preferences_.putUChar("brightness", brightnessPercent_);
    preferences_.putUInt("timeout_s", screenTimeoutSeconds_);
    Serial.println(F("[display] settings saved"));
}
