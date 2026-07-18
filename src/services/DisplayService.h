#pragma once

#include "core/AppTypes.h"
#include "ui/BitmapFont.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayService {
public:
    DisplayService();
    static constexpr uint16_t SCREEN_WIDTH = 320;
    static constexpr uint16_t SCREEN_HEIGHT = 240;

    void begin();
    bool ready() const;
    void startFrame(uint16_t background = TFT_NAVY);
    void pushFrame();

    void drawText(const char* text, int16_t x, int16_t y, uint16_t color,
                  BitmapFontSize size = BitmapFontSize::Small12,
                  BitmapTextAlign align = BitmapTextAlign::Left);
    void drawText(const String& text, int16_t x, int16_t y, uint16_t color,
                  BitmapFontSize size = BitmapFontSize::Small12,
                  BitmapTextAlign align = BitmapTextAlign::Left);
    void drawValue(const char* label, const String& value, int16_t y);
    void drawHeader(const char* title);
    void drawFooter(const char* text);
    void drawFooter(const String& text);
    void fillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                  uint16_t color);
    void pushBacklightOn();

    void writeScreenshot(Stream& output);
    static String formatBytes(size_t bytes);

private:
    static constexpr uint8_t LCD_BACKLIGHT_PIN = 45;
    static constexpr size_t SCREENSHOT_CHUNK = 1024;

    TFT_eSPI display_;
    TFT_eSprite canvas_;
    BitmapFont bitmapFont_;
    bool canvasReady_ = false;
    bool screenshotInProgress_ = false;

    void holdBacklightOff();
};
