#pragma once

#include <Arduino.h>
#include <Preferences.h>
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
    void tick(uint32_t nowMs);
    bool noteActivity(uint32_t nowMs);

    uint8_t brightnessPercent() const;
    void setBrightnessPercent(uint8_t percent);
    uint32_t screenTimeoutSeconds() const;
    void setScreenTimeoutSeconds(uint32_t seconds);
    bool screenIsOff() const;
    void printStatus(Print& output) const;

    /* Called by the LVGL display driver. */
    void flush(const lv_area_t& area, const uint8_t* pixels);

    /* The framebuffer export format remains RGB565BE for the host tools. */
    void writeScreenshot(Stream& output);
    bool copyShadowRgb565BE(size_t offset, uint8_t* destination,
                            size_t length) const;
    static String formatBytes(size_t bytes);

private:
    static constexpr uint8_t LCD_BACKLIGHT_PIN = 45;
    static constexpr uint8_t BACKLIGHT_PWM_CHANNEL = 0;
    static constexpr uint32_t BACKLIGHT_PWM_FREQUENCY = 5000;
    static constexpr uint8_t BACKLIGHT_PWM_RESOLUTION = 8;
    static constexpr size_t SCREENSHOT_CHUNK = 1024;
    static constexpr uint8_t DEFAULT_BRIGHTNESS_PERCENT = 100;
    static constexpr uint32_t DEFAULT_TIMEOUT_SECONDS = 0;

    TFT_eSPI display_;
    Preferences preferences_;
    uint16_t* shadow_ = nullptr;
    bool ready_ = false;
    bool screenshotInProgress_ = false;
    bool backlightArmed_ = false;
    bool screenOff_ = false;
    bool preferencesReady_ = false;
    bool settingsDirty_ = false;
    uint8_t brightnessPercent_ = DEFAULT_BRIGHTNESS_PERCENT;
    uint32_t screenTimeoutSeconds_ = DEFAULT_TIMEOUT_SECONDS;
    uint32_t lastActivityMs_ = 0;
    uint32_t settingsSaveDueMs_ = 0;

    void holdBacklightOff();
    void applyBacklight();
    void saveSettings();
};
