#include "services/DisplayService.h"

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
    output.println(screenOff_ ? F("off") : F("on"));
}

void DisplayService::flush(const lv_area_t& area, const uint8_t* pixels) {
    if (!ready_ || pixels == nullptr || screenshotInProgress_) {
        return;
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
        return;
    }

    const int32_t width = x2 - x1 + 1;
    const int32_t height = y2 - y1 + 1;
    const size_t screenWidth = SCREEN_WIDTH;

    const uint16_t* colors = reinterpret_cast<const uint16_t*>(pixels);
    for (int32_t row = 0; row < height; ++row) {
        const uint16_t* source = colors + row * width;
        uint16_t* target = shadow_ +
                           static_cast<size_t>(y1 + row) * screenWidth + x1;
        memcpy(target, source, static_cast<size_t>(width) * sizeof(uint16_t));
    }

    display_.startWrite();
    display_.setAddrWindow(x1, y1, width, height);
    display_.pushColors(const_cast<uint16_t*>(colors),
                         width * height, true);
    display_.endWrite();
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
