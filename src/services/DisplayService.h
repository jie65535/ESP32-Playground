#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/semphr.h>

#include <lvgl.h>

class DisplayService {
public:
    struct FlushMetrics {
        uint32_t copyUs = 0;
        uint32_t transferUs = 0;
        uint32_t wallUs = 0;
        uint32_t waitUs = 0;
        uint32_t pixels = 0;
        uint16_t areas = 0;
    };

    DisplayService();

    static constexpr uint16_t SCREEN_WIDTH = 320;
    static constexpr uint16_t SCREEN_HEIGHT = 240;

    void begin();
    bool ready() const;
    void pushBacklightOn();
    void tick(uint32_t nowMs);
    bool noteActivity(uint32_t nowMs);
    void setCaptureEnabled(bool enabled);
    bool captureEnabled() const;
    bool captureReady() const;
    FlushMetrics flushMetrics() const;
    bool initDma();
    bool dmaEnabled() const;
    void setAsyncFlushEnabled(bool enabled);
    bool asyncFlushEnabled() const;
    bool finishDmaIfReady();
    void waitForDma();

    uint8_t brightnessPercent() const;
    void setBrightnessPercent(uint8_t percent);
    uint32_t screenTimeoutSeconds() const;
    void setScreenTimeoutSeconds(uint32_t seconds);
    bool screenIsOff() const;
    void printStatus(Print& output) const;

    /* Called by the LVGL display driver. */
    bool flush(const lv_area_t& area, const uint8_t* pixels, bool lastArea);

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

    esp_lcd_panel_io_handle_t panelIo_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    SemaphoreHandle_t dmaDoneSemaphore_ = nullptr;
    volatile bool dmaDone_ = false;
    volatile uint64_t dmaCompletedUs_ = 0;
    Preferences preferences_;
    uint16_t* shadow_ = nullptr;
    bool ready_ = false;
    bool screenshotInProgress_ = false;
    bool backlightArmed_ = false;
    bool screenOff_ = false;
    bool captureEnabled_ = false;
    uint16_t captureDirtyRowsRemaining_ = 0;
    bool captureDirtyRows_[SCREEN_HEIGHT] = {};
    bool preferencesReady_ = false;
    bool settingsDirty_ = false;
    uint8_t brightnessPercent_ = DEFAULT_BRIGHTNESS_PERCENT;
    uint32_t screenTimeoutSeconds_ = DEFAULT_TIMEOUT_SECONDS;
    uint32_t lastActivityMs_ = 0;
    uint32_t settingsSaveDueMs_ = 0;
    FlushMetrics activeFlushMetrics_;
    FlushMetrics lastFlushMetrics_;
    bool dmaEnabled_ = false;
    bool asyncFlushEnabled_ = false;
    bool dmaPending_ = false;
    bool dmaPendingLast_ = false;
    uint64_t dmaStartedUs_ = 0;
    uint64_t flushFrameStartedUs_ = 0;

    void holdBacklightOff();
    bool beginNativePanel();
    static bool IRAM_ATTR onNativeColorTransferDone(
        esp_lcd_panel_io_handle_t panelIo,
        esp_lcd_panel_io_event_data_t* eventData, void* userContext);
    void applyBacklight();
    void saveSettings();
    void completeDmaTransfer();
    void finishFlushFrame();
    void markCaptureRows(int32_t y1, int32_t y2);
};
