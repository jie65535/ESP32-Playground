#include "services/DisplayService.h"

#include <esp_heap_caps.h>

DisplayService::DisplayService() = default;

void DisplayService::holdBacklightOff() {
    pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(LCD_BACKLIGHT_PIN, LOW);
}

void DisplayService::begin() {
    holdBacklightOff();

    display_.begin();
    display_.setRotation(1);
    display_.setTextWrap(false);

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
}

bool DisplayService::ready() const {
    return ready_;
}

void DisplayService::pushBacklightOn() {
    digitalWrite(LCD_BACKLIGHT_PIN, HIGH);
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
