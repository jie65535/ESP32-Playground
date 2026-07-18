#include <Arduino.h>
#include <TFT_eSPI.h>

#include "ui/BitmapFont.h"

namespace {

constexpr uint8_t LCD_BACKLIGHT_PIN = 45;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t RENDER_INTERVAL_MS = 1000;
constexpr uint32_t SERIAL_STATUS_INTERVAL_MS = 10000;
constexpr uint16_t SCREEN_WIDTH = 320;
constexpr uint16_t SCREEN_HEIGHT = 240;
constexpr size_t SCREENSHOT_BYTES =
    static_cast<size_t>(SCREEN_WIDTH) * SCREEN_HEIGHT * 2U;
constexpr size_t SCREENSHOT_CHUNK = 1024;
constexpr size_t COMMAND_BUFFER_SIZE = 64;

enum class Page : uint8_t {
    System,
    Display,
    Network,
    Count,
};

TFT_eSPI display;
TFT_eSprite canvas(&display);
BitmapFont bitmapFont;
Page page = Page::System;
bool canvasReady = false;
bool colorTest = false;
bool screenshotInProgress = false;
bool redrawRequested = true;
uint32_t lastRenderMs = 0;
uint32_t lastSerialStatusMs = 0;
String commandBuffer;

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

void drawText(const char* text, int16_t x, int16_t y, uint16_t color,
              BitmapFontSize size = BitmapFontSize::Small12,
              BitmapTextAlign align = BitmapTextAlign::Left) {
    bitmapFont.draw(canvas, text, x, y, color, size, align);
}

void drawText(const String& text, int16_t x, int16_t y, uint16_t color,
              BitmapFontSize size = BitmapFontSize::Small12,
              BitmapTextAlign align = BitmapTextAlign::Left) {
    drawText(text.c_str(), x, y, color, size, align);
}

void drawValue(const char* label, const String& value, int16_t y) {
    drawText(label, 16, y, TFT_LIGHTGREY, BitmapFontSize::Small12);
    drawText(value, 150, y, TFT_WHITE, BitmapFontSize::Bold12);
}

void drawFooter(const char* text) {
    canvas.fillRect(0, 222, SCREEN_WIDTH, 18, TFT_DARKGREEN);
    drawText(text, 12, 223, TFT_WHITE, BitmapFontSize::Small12);
}

void drawHeader(const char* pageName) {
    canvas.fillRect(0, 0, SCREEN_WIDTH, 30, TFT_DARKCYAN);
    drawText("ESP32 Playground", 12, 7, TFT_WHITE, BitmapFontSize::Bold12);
    drawText(pageName, 308, 8, TFT_WHITE, BitmapFontSize::Small12,
             BitmapTextAlign::Right);
}

void drawSystemPage(uint32_t nowMs) {
    drawHeader("SYSTEM");
    drawText("ES3N28P / R8N16", 16, 40, TFT_WHITE, BitmapFontSize::Bold12);
    drawText("ILI9341  320x240", 16, 62, TFT_WHITE,
             BitmapFontSize::Small12);
    drawText("当前 数据", 16, 80, TFT_CYAN, BitmapFontSize::Small12);
    drawValue("Uptime", String(nowMs / 1000U) + " s", 108);
    drawValue("Free heap", formatBytes(ESP.getFreeHeap()), 132);
    drawValue("Free PSRAM", formatBytes(ESP.getFreePsram()), 156);
    drawValue("Flash", formatBytes(ESP.getFlashChipSize()), 180);
    drawFooter("system telemetry");
}

void drawDisplayPage() {
    drawHeader("DISPLAY");
    drawText("BGR + inversion on", 16, 42, TFT_WHITE, BitmapFontSize::Small12);
    drawText("RGB565 PSRAM sprite", 16, 64, TFT_WHITE,
             BitmapFontSize::Small12);
    drawText("正常 均衡", 16, 88, TFT_YELLOW, BitmapFontSize::Small12);
    const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE,
                               TFT_CYAN, TFT_MAGENTA, TFT_YELLOW};
    const char* labels[] = {"R", "G", "B", "C", "M", "Y"};
    for (uint8_t index = 0; index < 6; ++index) {
        const int16_t x = 16 + static_cast<int16_t>(index % 3U) * 96;
        const int16_t y = 118 + static_cast<int16_t>(index / 3U) * 36;
        canvas.fillRect(x, y, 80, 26, colors[index]);
        drawText(labels[index], x + 35, y + 6, TFT_BLACK,
                 BitmapFontSize::Small12, BitmapTextAlign::Center);
    }
    drawFooter("display color calibration");
}

void drawNetworkPage() {
    drawHeader("NETWORK");
    drawText("Wi-Fi Station", 16, 44, TFT_WHITE, BitmapFontSize::Bold12);
    drawText("2.4 GHz router", 16, 70, TFT_WHITE, BitmapFontSize::Small12);
    drawText("当前 正常", 16, 94, TFT_GREEN, BitmapFontSize::Small12);
    drawValue("State", "next: Wi-Fi", 122);
    drawValue("IP", "not connected", 146);
    drawValue("RSSI", "-- dBm", 170);
    drawFooter("network placeholder");
}

void drawColorTest() {
    canvas.fillSprite(TFT_BLACK);
    drawText("ILI9341 color calibration", 12, 8, TFT_WHITE,
             BitmapFontSize::Bold12);
    const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE,
                               TFT_CYAN, TFT_MAGENTA, TFT_YELLOW};
    const char* labels[] = {"RED", "GREEN", "BLUE", "CYAN", "MAGENTA", "YELLOW"};
    for (uint8_t index = 0; index < 6; ++index) {
        const int16_t x = static_cast<int16_t>(index % 3U) * 106;
        const int16_t y = 48 + static_cast<int16_t>(index / 3U) * 82;
        canvas.fillRect(x, y, 104, 76, colors[index]);
        drawText(labels[index], x + 52, y + 30, TFT_BLACK,
                 BitmapFontSize::Small12, BitmapTextAlign::Center);
    }
    drawText("color calibration", 12, 223, TFT_WHITE, BitmapFontSize::Small12);
}

void render(uint32_t nowMs) {
    if (!canvasReady || screenshotInProgress) {
        return;
    }

    // Draw the complete frame off-screen, then transfer it once. This keeps
    // the LCD from showing the clear-and-redraw process.
    if (colorTest) {
        drawColorTest();
    } else {
        canvas.fillSprite(TFT_NAVY);
        switch (page) {
            case Page::System:
                drawSystemPage(nowMs);
                break;
            case Page::Display:
                drawDisplayPage();
                break;
            case Page::Network:
                drawNetworkPage();
                break;
            case Page::Count:
                break;
        }
    }
    canvas.pushSprite(0, 0);
    redrawRequested = false;
}

void requestRedraw() {
    redrawRequested = true;
}

void selectPage(Page next) {
    page = next;
    colorTest = false;
    requestRedraw();
}

void previousPage() {
    const uint8_t current = static_cast<uint8_t>(page);
    const uint8_t count = static_cast<uint8_t>(Page::Count);
    selectPage(static_cast<Page>(current == 0 ? count - 1U : current - 1U));
}

void nextPage() {
    const uint8_t current = static_cast<uint8_t>(page);
    const uint8_t count = static_cast<uint8_t>(Page::Count);
    selectPage(static_cast<Page>((current + 1U) % count));
}

void writeScreenshot() {
    if (!canvasReady || screenshotInProgress) {
        Serial.println(F("[screenshot] unavailable"));
        return;
    }

    render(millis());
    screenshotInProgress = true;
    const uint8_t* pixels = static_cast<const uint8_t*>(canvas.getPointer());
    if (pixels == nullptr) {
        screenshotInProgress = false;
        Serial.println(F("[screenshot] framebuffer unavailable"));
        return;
    }

    Serial.print(F("PLAYGROUND_SCREENSHOT 1 320 240 RGB565BE "));
    Serial.println(SCREENSHOT_BYTES);
    Serial.flush();
    size_t offset = 0;
    while (offset < SCREENSHOT_BYTES) {
        const size_t remaining = SCREENSHOT_BYTES - offset;
        const size_t chunk = remaining < SCREENSHOT_CHUNK ? remaining
                                                           : SCREENSHOT_CHUNK;
        const size_t written = Serial.write(pixels + offset, chunk);
        if (written == 0) {
            delay(1);
            continue;
        }
        offset += written;
        yield();
    }
    Serial.print(F("\nPLAYGROUND_SCREENSHOT_END\n"));
    Serial.flush();
    screenshotInProgress = false;
}

void printStatus() {
    Serial.printf("[status] page=%u color_test=%s canvas=%s heap=%lu psram=%lu\n",
                  static_cast<unsigned>(page), colorTest ? "yes" : "no",
                  canvasReady ? "ready" : "failed",
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getFreePsram()));
}

void printHelp() {
    Serial.println(F("Commands: up | down | ok | page system | page display | page network"));
    Serial.println(F("          color_test | screenshot | status | help"));
}

void handleCommand(String command) {
    command.trim();
    command.toLowerCase();
    if (command == "up") {
        previousPage();
    } else if (command == "down") {
        nextPage();
    } else if (command == "ok") {
        colorTest = !colorTest;
        requestRedraw();
    } else if (command == "page system") {
        selectPage(Page::System);
    } else if (command == "page display") {
        selectPage(Page::Display);
    } else if (command == "page network") {
        selectPage(Page::Network);
    } else if (command == "color_test") {
        colorTest = true;
        requestRedraw();
    } else if (command == "screenshot") {
        writeScreenshot();
    } else if (command == "status") {
        printStatus();
    } else if (command == "help" || command == "?") {
        printHelp();
    } else if (!command.isEmpty()) {
        Serial.print(F("unknown command: "));
        Serial.println(command);
        printHelp();
    }
}

void scanSerial() {
    while (Serial.available() > 0) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\r' || character == '\n') {
            if (!commandBuffer.isEmpty()) {
                handleCommand(commandBuffer);
                commandBuffer = "";
            }
        } else if (commandBuffer.length() < COMMAND_BUFFER_SIZE) {
            commandBuffer += character;
        } else {
            commandBuffer = "";
            Serial.println(F("command discarded: too long"));
        }
    }
}

}  // namespace

void setup() {
    holdBacklightOff();
    Serial.begin(SERIAL_BAUD);
    commandBuffer.reserve(COMMAND_BUFFER_SIZE);
    delay(50);

    display.begin();
    display.setRotation(1);
    display.setTextWrap(false);
    canvas.setColorDepth(16);
    canvas.setAttribute(PSRAM_ENABLE, true);
    canvasReady = canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT) != nullptr;
    if (!canvasReady) {
        Serial.println(F("[display] PSRAM sprite allocation failed"));
    }
    render(0);

    // Only expose the first complete frame after LCD initialization.
    digitalWrite(LCD_BACKLIGHT_PIN, HIGH);

    Serial.println();
    Serial.printf("ESP32 Playground started build=%s %s\n", __DATE__, __TIME__);
    Serial.printf("chip=%s flash=%uMB psram=%s psram=%uMB\n",
                  ESP.getChipModel(),
                  static_cast<unsigned>(ESP.getFlashChipSize() / (1024U * 1024U)),
                  psramFound() ? "yes" : "no",
                  static_cast<unsigned>(ESP.getPsramSize() / (1024U * 1024U)));
    printHelp();
    lastRenderMs = millis();
    lastSerialStatusMs = lastRenderMs;
}

void loop() {
    const uint32_t nowMs = millis();
    scanSerial();
    if (redrawRequested || nowMs - lastRenderMs >= RENDER_INTERVAL_MS) {
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
