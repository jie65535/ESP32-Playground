#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <lvgl.h>

class DisplayService {
public:
    DisplayService();

    static constexpr uint16_t SCREEN_WIDTH = 320;
    static constexpr uint16_t SCREEN_HEIGHT = 240;

    void begin();
    bool ready() const;
    void pushBacklightOn();

    /* Called by the LVGL display driver. */
    void flush(const lv_area_t& area, const uint8_t* pixels);

    /* The framebuffer export format remains RGB565BE for the host tools. */
    void writeScreenshot(Stream& output);
    static String formatBytes(size_t bytes);

private:
    static constexpr uint8_t LCD_BACKLIGHT_PIN = 45;
    static constexpr size_t SCREENSHOT_CHUNK = 1024;

    TFT_eSPI display_;
    uint16_t* shadow_ = nullptr;
    bool ready_ = false;
    bool screenshotInProgress_ = false;

    void holdBacklightOff();
};
