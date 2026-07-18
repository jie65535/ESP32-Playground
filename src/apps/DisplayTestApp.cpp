#include "apps/DisplayTestApp.h"

#include "services/DisplayService.h"

AppId DisplayTestApp::id() const {
    return AppId::DisplayTest;
}

const char* DisplayTestApp::name() const {
    return "Display";
}

void DisplayTestApp::onEnter(AppContext&) {}

void DisplayTestApp::onExit(AppContext&) {}

void DisplayTestApp::onCommand(const AppCommand& command, AppContext&) {
    if (command.type == AppCommandType::Activate ||
        command.type == AppCommandType::ColorTest) {
        colorTest_ = !colorTest_;
    }
}

void DisplayTestApp::onTick(uint32_t, AppContext&) {}

void DisplayTestApp::onRender(AppContext& context) {
    DisplayService& display = context.display;
    display.startFrame();
    if (colorTest_) {
        display.drawText("ILI9341 color calibration", 12, 8, TFT_WHITE,
                         BitmapFontSize::Bold12);
        const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE,
                                   TFT_CYAN, TFT_MAGENTA, TFT_YELLOW};
        const char* labels[] = {"RED", "GREEN", "BLUE", "CYAN", "MAGENTA",
                                "YELLOW"};
        for (uint8_t index = 0; index < 6; ++index) {
            const int16_t x = static_cast<int16_t>(index % 3U) * 106;
            const int16_t y = 48 + static_cast<int16_t>(index / 3U) * 82;
            display.fillRect(x, y, 104, 76, colors[index]);
            display.drawText(labels[index], x + 52, y + 30, TFT_BLACK,
                             BitmapFontSize::Small12, BitmapTextAlign::Center);
        }
        display.drawText("color calibration", 12, 223, TFT_WHITE,
                         BitmapFontSize::Small12);
    } else {
        display.drawHeader("DISPLAY");
        display.drawText("BGR + inversion on", 16, 42, TFT_WHITE,
                         BitmapFontSize::Small12);
        display.drawText("RGB565 PSRAM sprite", 16, 64, TFT_WHITE,
                         BitmapFontSize::Small12);
        display.drawText("normal / balanced", 16, 88, TFT_YELLOW,
                         BitmapFontSize::Small12);
        const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE,
                                   TFT_CYAN, TFT_MAGENTA, TFT_YELLOW};
        const char* labels[] = {"R", "G", "B", "C", "M", "Y"};
        for (uint8_t index = 0; index < 6; ++index) {
            const int16_t x = 16 + static_cast<int16_t>(index % 3U) * 96;
            const int16_t y = 118 + static_cast<int16_t>(index / 3U) * 36;
            display.fillRect(x, y, 80, 26, colors[index]);
            display.drawText(labels[index], x + 35, y + 6, TFT_BLACK,
                             BitmapFontSize::Small12, BitmapTextAlign::Center);
        }
        display.drawFooter("display color calibration");
    }
    display.pushFrame();
}
