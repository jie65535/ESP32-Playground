#include <Arduino.h>
#include <TFT_eSPI.h>

#include <esp_heap_caps.h>

namespace {

constexpr uint8_t LCD_BACKLIGHT_PIN = 45;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t RENDER_INTERVAL_MS = 1000;
constexpr uint32_t SERIAL_STATUS_INTERVAL_MS = 10000;

TFT_eSPI display;
TFT_eSprite canvas(&display);
bool canvasReady = false;
uint32_t lastRenderMs = 0;
uint32_t lastSerialStatusMs = 0;

void holdBacklightOff() {
    pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(LCD_BACKLIGHT_PIN, LOW);
}

String formatBytes(size_t bytes) {
    if (bytes >= 1024U * 1024U) {
        return String(static_cast<float>(bytes) / (1024.0F * 1024.0F), 1) +
               " MB";
    }
    return String(static_cast<float>(bytes) / 1024.0F, 1) + " KB";
}

void drawValue(TFT_eSprite& target, const char* label, const String& value,
               int16_t y) {
    target.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
    target.drawString(label, 16, y, 2);
    target.setTextColor(TFT_WHITE, TFT_NAVY);
    target.drawString(value, 150, y, 2);
}

void render(uint32_t nowMs) {
    if (!canvasReady) {
        return;
    }

    // Draw the complete frame off-screen, then transfer it once. This keeps
    // the LCD from showing the clear-and-redraw process.
    canvas.fillSprite(TFT_NAVY);
    canvas.fillRect(0, 0, 320, 30, TFT_DARKCYAN);
    canvas.fillRect(0, 222, 320, 18, TFT_DARKGREEN);

    canvas.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    canvas.drawString("ESP32 Playground", 12, 7, 2);

    canvas.setTextColor(TFT_WHITE, TFT_NAVY);
    canvas.drawString("ES3N28P / R8N16", 16, 40, 2);
    canvas.drawString("ILI9341  320x240", 16, 62, 2);

    drawValue(canvas, "Uptime", String(nowMs / 1000U) + " s", 92);
    drawValue(canvas, "Free heap", formatBytes(ESP.getFreeHeap()), 116);
    drawValue(canvas, "Free PSRAM", formatBytes(ESP.getFreePsram()), 140);
    drawValue(canvas, "Flash", formatBytes(ESP.getFlashChipSize()), 164);
    drawValue(canvas, "Wi-Fi", "planned v0.2", 188);

    canvas.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    canvas.drawString("USB CDC 115200   v0.1", 12, 224, 1);
    canvas.pushSprite(0, 0);
}

}  // namespace

void setup() {
    holdBacklightOff();
    Serial.begin(SERIAL_BAUD);
    delay(50);

    display.begin();
    display.setRotation(1);
    display.setTextWrap(false);
    canvas.setColorDepth(16);
    canvas.setAttribute(PSRAM_ENABLE, true);
    canvasReady = canvas.createSprite(320, 240) != nullptr;
    if (!canvasReady) {
        Serial.println(F("[display] PSRAM sprite allocation failed"));
    }
    render(0);

    // Only expose the first complete frame after LCD initialization.
    digitalWrite(LCD_BACKLIGHT_PIN, HIGH);

    Serial.println();
    Serial.println(F("ESP32 Playground v0.1 started"));
    Serial.printf("chip=%s flash=%uMB psram=%s psram=%uMB\n",
                  ESP.getChipModel(),
                  static_cast<unsigned>(ESP.getFlashChipSize() / (1024U * 1024U)),
                  psramFound() ? "yes" : "no",
                  static_cast<unsigned>(ESP.getPsramSize() / (1024U * 1024U)));
    lastRenderMs = millis();
    lastSerialStatusMs = lastRenderMs;
}

void loop() {
    const uint32_t nowMs = millis();
    if (nowMs - lastRenderMs >= RENDER_INTERVAL_MS) {
        lastRenderMs = nowMs;
        render(nowMs);
        if (nowMs - lastSerialStatusMs >= SERIAL_STATUS_INTERVAL_MS) {
            lastSerialStatusMs = nowMs;
            Serial.printf("[status] uptime=%lus heap=%lu psram=%lu\n",
                          static_cast<unsigned long>(nowMs / 1000U),
                          static_cast<unsigned long>(ESP.getFreeHeap()),
                          static_cast<unsigned long>(ESP.getFreePsram()));
        }
    }
}
