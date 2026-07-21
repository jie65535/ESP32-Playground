#include "services/DisplayService.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_crc.h>

#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_lcd_ili9341.h>

#include <algorithm>

namespace {

constexpr spi_host_device_t LCD_SPI_HOST = SPI2_HOST;
constexpr int LCD_CS_PIN = 10;
constexpr int LCD_MOSI_PIN = 11;
constexpr int LCD_CLOCK_PIN = 12;
constexpr int LCD_MISO_PIN = 13;
constexpr int LCD_DC_PIN = 46;
constexpr uint32_t LCD_PIXEL_CLOCK_HZ = 40U * 1000U * 1000U;
constexpr size_t LCD_MAX_TRANSFER_BYTES =
    static_cast<size_t>(DisplayService::SCREEN_WIDTH) * 40U * sizeof(uint16_t);
constexpr uint16_t DIRECT_PRESENT_ROWS = 40U;
// A disconnected or stalled USB CDC host must not leave loopTask trapped in
// the screenshot writer forever.  A normal 1 KiB chunk is much shorter than
// this even at the nominal 115200 baud console setting.
constexpr uint32_t SCREENSHOT_WRITE_STALL_TIMEOUT_MS = 3000UL;

struct __attribute__((packed)) ScreenshotFrameHeader {
    uint8_t magic[4];       // "PGS2"
    uint8_t version;
    uint8_t pixelFormat;    // RGB565BE
    uint16_t headerBytes;
    uint16_t width;
    uint16_t height;
    uint32_t payloadBytes;
    uint32_t requestId;
    uint32_t sequence;
};

struct __attribute__((packed)) ScreenshotFrameTrailer {
    uint8_t magic[4];       // "PGE2"
    uint32_t requestId;
    uint32_t sequence;
    uint32_t payloadCrc32;
};

static_assert(sizeof(ScreenshotFrameHeader) == 24,
              "Screenshot header layout changed");
static_assert(sizeof(ScreenshotFrameTrailer) == 16,
              "Screenshot trailer layout changed");

bool logEspError(const __FlashStringHelper* operation, esp_err_t error) {
    if (error == ESP_OK) {
        return true;
    }
    Serial.print(F("[display] "));
    Serial.print(operation);
    Serial.print(F(" failed: "));
    Serial.println(esp_err_to_name(error));
    return false;
}

}  // namespace

DisplayService::DisplayService() = default;

bool DisplayService::beginNativePanel() {
    dmaDoneSemaphore_ = xSemaphoreCreateBinary();
    if (dmaDoneSemaphore_ == nullptr) {
        Serial.println(F("[display] native DMA semaphore allocation failed"));
        return false;
    }

    spi_bus_config_t busConfig = {};
    busConfig.sclk_io_num = LCD_CLOCK_PIN;
    busConfig.mosi_io_num = LCD_MOSI_PIN;
    busConfig.miso_io_num = LCD_MISO_PIN;
    busConfig.quadwp_io_num = -1;
    busConfig.quadhd_io_num = -1;
    busConfig.max_transfer_sz = LCD_MAX_TRANSFER_BYTES;
    if (!logEspError(F("SPI2 DMA bus initialization"),
                     spi_bus_initialize(LCD_SPI_HOST, &busConfig,
                                        SPI_DMA_CH_AUTO))) {
        return false;
    }

    esp_lcd_panel_io_spi_config_t ioConfig = {};
    ioConfig.dc_gpio_num = LCD_DC_PIN;
    ioConfig.cs_gpio_num = LCD_CS_PIN;
    ioConfig.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    ioConfig.lcd_cmd_bits = 8;
    ioConfig.lcd_param_bits = 8;
    ioConfig.spi_mode = 0;
    ioConfig.trans_queue_depth = 2;
    ioConfig.on_color_trans_done = onNativeColorTransferDone;
    ioConfig.user_ctx = this;
    if (!logEspError(F("ILI9341 panel IO installation"),
                     esp_lcd_new_panel_io_spi(
                         static_cast<esp_lcd_spi_bus_handle_t>(LCD_SPI_HOST),
                         &ioConfig, &panelIo_))) {
        return false;
    }

    esp_lcd_panel_dev_config_t panelConfig = {};
    panelConfig.reset_gpio_num = -1;
    panelConfig.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panelConfig.bits_per_pixel = 16;
    if (!logEspError(F("ILI9341 panel installation"),
                     esp_lcd_new_panel_ili9341(panelIo_, &panelConfig,
                                               &panel_))) {
        return false;
    }

    if (!logEspError(F("ILI9341 panel reset"),
                     esp_lcd_panel_reset(panel_)) ||
        !logEspError(F("ILI9341 panel initialization"),
                     esp_lcd_panel_init(panel_)) ||
        !logEspError(F("ILI9341 inversion enable"),
                     esp_lcd_panel_invert_color(panel_, true)) ||
        !logEspError(F("ILI9341 landscape rotation"),
                     esp_lcd_panel_swap_xy(panel_, true)) ||
        !logEspError(F("ILI9341 mirror configuration"),
                     esp_lcd_panel_mirror(panel_, false, false)) ||
        !logEspError(F("ILI9341 display enable"),
                     esp_lcd_panel_disp_on_off(panel_, true))) {
        return false;
    }

    dmaEnabled_ = true;
    Serial.println(F("[display] backend=esp_lcd host=SPI2 pins=10/11/12/13/46 "
                     "clock=40MHz queue=2"));
    return true;
}

bool IRAM_ATTR DisplayService::onNativeColorTransferDone(
    esp_lcd_panel_io_handle_t panelIo,
    esp_lcd_panel_io_event_data_t* eventData, void* userContext) {
    (void)panelIo;
    (void)eventData;
    auto* display = static_cast<DisplayService*>(userContext);
    if (display == nullptr) {
        return false;
    }

    display->dmaDone_ = true;
    display->dmaCompletedUs_ = static_cast<uint64_t>(esp_timer_get_time());
    if (display->dmaDoneSemaphore_ == nullptr) {
        return false;
    }

    BaseType_t highPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(display->dmaDoneSemaphore_,
                           &highPriorityTaskWoken);
    return highPriorityTaskWoken == pdTRUE;
}

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

    if (!beginNativePanel()) {
        return;
    }

    if (!ledcAttachChannel(LCD_BACKLIGHT_PIN, BACKLIGHT_PWM_FREQUENCY,
                           BACKLIGHT_PWM_RESOLUTION,
                           BACKLIGHT_PWM_CHANNEL)) {
        Serial.println(F("[display] backlight PWM attach failed"));
    }
    ledcWriteChannel(BACKLIGHT_PWM_CHANNEL, 0);

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

void DisplayService::setCaptureEnabled(bool enabled) {
    if (captureEnabled_ == enabled) {
        return;
    }

    captureEnabled_ = enabled;
    captureDirtyRowsRemaining_ = enabled ? SCREEN_HEIGHT : 0;
    if (enabled) {
        memset(captureDirtyRows_, 0, sizeof(captureDirtyRows_));
        /* Force a complete LVGL redraw so a newly connected mirror gets a
         * coherent keyframe instead of a partly stale shadow buffer. */
        lv_obj_invalidate(lv_screen_active());
        lv_obj_invalidate(lv_layer_top());
    }
}

bool DisplayService::captureEnabled() const {
    return captureEnabled_;
}

bool DisplayService::captureReady() const {
    return captureEnabled_ && captureDirtyRowsRemaining_ == 0;
}

DisplayService::FlushMetrics DisplayService::flushMetrics() const {
    return lastFlushMetrics_;
}

bool DisplayService::initDma() {
    if (dmaEnabled_) {
        return true;
    }

    dmaEnabled_ = panel_ != nullptr && panelIo_ != nullptr &&
                  dmaDoneSemaphore_ != nullptr;
    Serial.println(dmaEnabled_ ? F("[display] native SPI DMA enabled")
                               : F("[display] native SPI DMA unavailable"));
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
    if (!dmaPending_ || !dmaDone_) {
        return false;
    }
    if (dmaDoneSemaphore_ != nullptr) {
        (void)xSemaphoreTake(dmaDoneSemaphore_, 0);
    }

    completeDmaTransfer();
    return true;
}

void DisplayService::waitForDma() {
    if (!dmaPending_) {
        return;
    }

    const uint64_t waitStartedUs = esp_timer_get_time();
    if (!dmaDone_ && dmaDoneSemaphore_ != nullptr) {
        (void)xSemaphoreTake(dmaDoneSemaphore_, portMAX_DELAY);
    } else if (dmaDoneSemaphore_ != nullptr) {
        (void)xSemaphoreTake(dmaDoneSemaphore_, 0);
    }
    activeFlushMetrics_.waitUs += static_cast<uint32_t>(min<uint64_t>(
        esp_timer_get_time() - waitStartedUs, UINT32_MAX));
    completeDmaTransfer();
}

bool DisplayService::dmaPending() const {
    return dmaPending_;
}

bool DisplayService::presentRgb565(int16_t x, int16_t y, uint16_t width,
                                   uint16_t height, uint16_t* pixels,
                                   uint16_t stride) {
    if (!ready_ || pixels == nullptr || width == 0U || height == 0U ||
        stride < width || dmaPending_ || x < 0 || y < 0 ||
        x + width > SCREEN_WIDTH || y + height > SCREEN_HEIGHT) {
        return false;
    }
    if (stride != width) {
        return false;
    }
    for (uint16_t row = 0; row < height; row += DIRECT_PRESENT_ROWS) {
        const uint16_t rows = std::min<uint16_t>(
            DIRECT_PRESENT_ROWS, static_cast<uint16_t>(height - row));
        const bool lastArea = row + rows == height;
        uint16_t* chunk = pixels + static_cast<size_t>(row) * stride;
        const lv_area_t area = {
            static_cast<lv_coord_t>(x),
            static_cast<lv_coord_t>(y + row),
            static_cast<lv_coord_t>(x + width - 1),
            static_cast<lv_coord_t>(y + row + rows - 1),
        };
        if (!flush(area, reinterpret_cast<const uint8_t*>(chunk), lastArea)) {
            lv_draw_sw_rgb565_swap(reinterpret_cast<uint8_t*>(chunk),
                                   static_cast<uint32_t>(width) * rows);
            return false;
        }
        waitForDma();
        lv_draw_sw_rgb565_swap(reinterpret_cast<uint8_t*>(chunk),
                               static_cast<uint32_t>(width) * rows);
    }
    return true;
}

void DisplayService::completeDmaTransfer() {
    const bool completedLastArea = dmaPendingLast_;
    dmaPending_ = false;
    dmaDone_ = false;
    const uint64_t completedUs =
        dmaCompletedUs_ != 0 ? dmaCompletedUs_ : esp_timer_get_time();
    dmaCompletedUs_ = 0;
    activeFlushMetrics_.transferUs += static_cast<uint32_t>(min<uint64_t>(
        completedUs - dmaStartedUs_, UINT32_MAX));
    dmaPendingLast_ = false;
    if (completedLastArea) {
        finishFlushFrame();
    }
}

void DisplayService::finishFlushFrame() {
    if (flushFrameStartedUs_ != 0) {
        activeFlushMetrics_.wallUs = static_cast<uint32_t>(min<uint64_t>(
            esp_timer_get_time() - flushFrameStartedUs_, UINT32_MAX));
        flushFrameStartedUs_ = 0;
    }
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
    output.print(F(" backend=esp_lcd"));
    output.print(F(" dma="));
    output.print(dmaEnabled_ ? F("on") : F("off"));
    output.print(F(" async="));
    output.print(asyncFlushEnabled_ ? F("on") : F("off"));
    output.print(F(" flush_us spi="));
    output.print(lastFlushMetrics_.transferUs);
    output.print(F(" wall="));
    output.print(lastFlushMetrics_.wallUs);
    output.print(F(" wait="));
    output.print(lastFlushMetrics_.waitUs);
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

    if (activeFlushMetrics_.areas == 0 && flushFrameStartedUs_ == 0) {
        flushFrameStartedUs_ = esp_timer_get_time();
    }

    const uint16_t* colors = reinterpret_cast<const uint16_t*>(pixels);
    if (captureEnabled_) {
        const uint64_t copyStartedUs = esp_timer_get_time();
        for (int32_t row = 0; row < height; ++row) {
            const uint16_t* source = colors + row * width;
            uint16_t* target = shadow_ +
                               static_cast<size_t>(y1 + row) * screenWidth + x1;
            memcpy(target, source, static_cast<size_t>(width) * sizeof(uint16_t));
        }
        activeFlushMetrics_.copyUs += static_cast<uint32_t>(
            min<uint64_t>(esp_timer_get_time() - copyStartedUs, UINT32_MAX));
        if (x1 == 0 && x2 == SCREEN_WIDTH - 1) {
            markCaptureRows(y1, y2);
        }
    }

    if (asyncFlushEnabled_) {
        if (dmaPending_) {
            waitForDma();
        }

        dmaStartedUs_ = esp_timer_get_time();
        dmaDone_ = false;
        dmaCompletedUs_ = 0;
        if (dmaDoneSemaphore_ != nullptr) {
            (void)xSemaphoreTake(dmaDoneSemaphore_, 0);
        }
        lv_draw_sw_rgb565_swap(const_cast<uint8_t*>(pixels),
                               static_cast<uint32_t>(width * height));
        dmaPending_ = true;
        dmaPendingLast_ = lastArea;
        const esp_err_t error = esp_lcd_panel_draw_bitmap(
            panel_, x1, y1, x2 + 1, y2 + 1, pixels);
        if (error != ESP_OK) {
            dmaPending_ = false;
            dmaPendingLast_ = false;
            Serial.print(F("[display] native DMA flush failed: "));
            Serial.println(esp_err_to_name(error));
            if (lastArea) {
                finishFlushFrame();
            }
            return false;
        }
        activeFlushMetrics_.pixels += static_cast<uint32_t>(width * height);
        activeFlushMetrics_.areas++;
        return true;
    }

    if (dmaPending_) {
        waitForDma();
    }
    dmaStartedUs_ = esp_timer_get_time();
    dmaDone_ = false;
    dmaCompletedUs_ = 0;
    if (dmaDoneSemaphore_ != nullptr) {
        (void)xSemaphoreTake(dmaDoneSemaphore_, 0);
    }
    lv_draw_sw_rgb565_swap(const_cast<uint8_t*>(pixels),
                           static_cast<uint32_t>(width * height));
    dmaPending_ = true;
    dmaPendingLast_ = lastArea;
    const esp_err_t error = esp_lcd_panel_draw_bitmap(
        panel_, x1, y1, x2 + 1, y2 + 1, pixels);
    if (error != ESP_OK) {
        dmaPending_ = false;
        dmaPendingLast_ = false;
        Serial.print(F("[display] native sync flush failed: "));
        Serial.println(esp_err_to_name(error));
        if (lastArea) {
            finishFlushFrame();
        }
        return false;
    }
    activeFlushMetrics_.pixels += static_cast<uint32_t>(width * height);
    activeFlushMetrics_.areas++;
    waitForDma();
    return false;
}

void DisplayService::markCaptureRows(int32_t y1, int32_t y2) {
    if (!captureEnabled_ || captureDirtyRowsRemaining_ == 0 ||
        y1 > y2) {
        return;
    }
    for (int32_t row = max<int32_t>(0, y1);
         row <= min<int32_t>(SCREEN_HEIGHT - 1, y2); ++row) {
        if (!captureDirtyRows_[row]) {
            captureDirtyRows_[row] = true;
            if (captureDirtyRowsRemaining_ > 0) {
                captureDirtyRowsRemaining_--;
            }
        }
    }
}

void DisplayService::writeScreenshot(Stream& output, uint32_t requestId) {
    constexpr size_t screenshotBytes =
        static_cast<size_t>(SCREEN_WIDTH) * SCREEN_HEIGHT * 2U;

    if (!ready_ || shadow_ == nullptr || screenshotInProgress_) {
        output.println(F("[screenshot] unavailable"));
        return;
    }

    screenshotInProgress_ = true;
    const uint32_t sequence = ++screenshotSequence_;

    auto writeBytes = [&](const uint8_t* data, size_t length) -> bool {
        size_t written = 0;
        uint32_t stalledSinceMs = 0;
        while (written < length) {
            const size_t step = output.write(data + written, length - written);
            if (step == 0) {
                if (stalledSinceMs == 0) {
                    stalledSinceMs = millis();
                } else if (millis() - stalledSinceMs >=
                           SCREENSHOT_WRITE_STALL_TIMEOUT_MS) {
                    return false;
                }
                delay(1);
                continue;
            }
            stalledSinceMs = 0;
            written += step;
        }
        return true;
    };

    ScreenshotFrameHeader header = {
        {'P', 'G', 'S', '2'},
        2,
        1,
        static_cast<uint16_t>(sizeof(ScreenshotFrameHeader)),
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        static_cast<uint32_t>(screenshotBytes),
        requestId,
        sequence,
    };
    if (!writeBytes(reinterpret_cast<const uint8_t*>(&header),
                    sizeof(header))) {
        screenshotInProgress_ = false;
        return;
    }
    output.flush();

    uint8_t chunk[SCREENSHOT_CHUNK];
    size_t offset = 0;
    uint32_t payloadCrc = 0;
    while (offset < screenshotBytes) {
        const size_t count = min(sizeof(chunk), screenshotBytes - offset);
        const size_t firstPixel = offset / 2U;
        for (size_t index = 0; index < count / 2U; ++index) {
            const uint16_t value = shadow_[firstPixel + index];
            chunk[index * 2U] = static_cast<uint8_t>(value >> 8U);
            chunk[index * 2U + 1U] = static_cast<uint8_t>(value & 0xffU);
        }
        payloadCrc = esp_crc32_le(payloadCrc, chunk,
                                  static_cast<uint32_t>(count));

        if (!writeBytes(chunk, count)) {
            // The host most likely timed out or closed the CDC handle.  Do
            // not print here: the stream is still a binary frame and a text
            // log would corrupt recovery.
            screenshotInProgress_ = false;
            return;
        }
        offset += count;
        yield();
    }

    ScreenshotFrameTrailer trailer = {
        {'P', 'G', 'E', '2'}, requestId, sequence, payloadCrc};
    if (!writeBytes(reinterpret_cast<const uint8_t*>(&trailer),
                    sizeof(trailer))) {
        screenshotInProgress_ = false;
        return;
    }
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
        ledcWriteChannel(BACKLIGHT_PWM_CHANNEL, 0);
        return;
    }
    const uint32_t duty = static_cast<uint32_t>(brightnessPercent_) * 255U / 100U;
    ledcWriteChannel(BACKLIGHT_PWM_CHANNEL, duty);
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
