#include "services/DisplayService.h"

DisplayService::DisplayService() : canvas_(&display_) {}

void DisplayService::holdBacklightOff() {
    pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(LCD_BACKLIGHT_PIN, LOW);
}

void DisplayService::begin() {
    holdBacklightOff();
    display_.begin();
    display_.setRotation(1);
    display_.setTextWrap(false);
    canvas_.setColorDepth(16);
    canvas_.setAttribute(PSRAM_ENABLE, true);
    canvasReady_ = canvas_.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT) != nullptr;
    if (!canvasReady_) {
        Serial.println(F("[display] PSRAM sprite allocation failed"));
        return;
    }
    startFrame();
    pushFrame();
}

bool DisplayService::ready() const {
    return canvasReady_;
}

void DisplayService::startFrame(uint16_t background) {
    if (canvasReady_ && !screenshotInProgress_) {
        canvas_.fillSprite(background);
    }
}

void DisplayService::pushFrame() {
    if (canvasReady_ && !screenshotInProgress_) {
        canvas_.pushSprite(0, 0);
    }
}

void DisplayService::drawText(const char* text, int16_t x, int16_t y,
                              uint16_t color, BitmapFontSize size,
                              BitmapTextAlign align) {
    if (canvasReady_) {
        bitmapFont_.draw(canvas_, text, x, y, color, size, align);
    }
}

void DisplayService::drawText(const String& text, int16_t x, int16_t y,
                              uint16_t color, BitmapFontSize size,
                              BitmapTextAlign align) {
    drawText(text.c_str(), x, y, color, size, align);
}

void DisplayService::drawValue(const char* label, const String& value,
                               int16_t y) {
    drawText(label, 16, y, TFT_LIGHTGREY, BitmapFontSize::Small12);
    drawText(value, 150, y, TFT_WHITE, BitmapFontSize::Bold12);
}

void DisplayService::drawHeader(const char* title) {
    fillRect(0, 0, SCREEN_WIDTH, 30, TFT_DARKCYAN);
    drawText("PlaygroundOS", 12, 7, TFT_WHITE, BitmapFontSize::Bold12);
    drawText(title, 308, 8, TFT_WHITE, BitmapFontSize::Small12,
             BitmapTextAlign::Right);
}

void DisplayService::drawFooter(const char* text) {
    fillRect(0, 222, SCREEN_WIDTH, 18, TFT_DARKGREEN);
    drawText(text, 12, 223, TFT_WHITE, BitmapFontSize::Small12);
}

void DisplayService::drawFooter(const String& text) {
    drawFooter(text.c_str());
}

void DisplayService::fillRect(int16_t x, int16_t y, int16_t width,
                              int16_t height, uint16_t color) {
    if (canvasReady_) {
        canvas_.fillRect(x, y, width, height, color);
    }
}

void DisplayService::pushBacklightOn() {
    digitalWrite(LCD_BACKLIGHT_PIN, HIGH);
}

void DisplayService::writeScreenshot(Stream& output) {
    constexpr size_t SCREENSHOT_BYTES =
        static_cast<size_t>(SCREEN_WIDTH) * SCREEN_HEIGHT * 2U;
    if (!canvasReady_ || screenshotInProgress_) {
        output.println(F("[screenshot] unavailable"));
        return;
    }

    screenshotInProgress_ = true;
    const uint8_t* pixels = static_cast<const uint8_t*>(canvas_.getPointer());
    if (pixels == nullptr) {
        screenshotInProgress_ = false;
        output.println(F("[screenshot] framebuffer unavailable"));
        return;
    }

    output.print(F("PLAYGROUND_SCREENSHOT 1 320 240 RGB565BE "));
    output.println(SCREENSHOT_BYTES);
    output.flush();
    size_t offset = 0;
    while (offset < SCREENSHOT_BYTES) {
        const size_t remaining = SCREENSHOT_BYTES - offset;
        const size_t chunk = remaining < SCREENSHOT_CHUNK ? remaining
                                                           : SCREENSHOT_CHUNK;
        const size_t written = output.write(pixels + offset, chunk);
        if (written == 0) {
            delay(1);
            continue;
        }
        offset += written;
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
