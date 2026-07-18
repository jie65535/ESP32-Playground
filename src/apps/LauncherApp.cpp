#include "apps/LauncherApp.h"

#include "services/DisplayService.h"

namespace {

const char* itemName(uint8_t index) {
    switch (index) {
        case 0:
            return "System Info";
        case 1:
            return "Display Test";
        case 2:
            return "Wi-Fi Settings";
        default:
            return "Unknown";
    }
}

AppId itemApp(uint8_t index) {
    switch (index) {
        case 0:
            return AppId::SystemInfo;
        case 1:
            return AppId::DisplayTest;
        case 2:
            return AppId::NetworkSettings;
        default:
            return AppId::Count;
    }
}

}  // namespace

AppId LauncherApp::id() const {
    return AppId::Launcher;
}

const char* LauncherApp::name() const {
    return "Desktop";
}

void LauncherApp::onEnter(AppContext&) {
    requested_ = AppId::Count;
}

void LauncherApp::onExit(AppContext&) {}

void LauncherApp::onCommand(const AppCommand& command, AppContext&) {
    if (command.type == AppCommandType::Previous) {
        selected_ = selected_ == 0 ? ITEM_COUNT - 1U : selected_ - 1U;
    } else if (command.type == AppCommandType::Next) {
        selected_ = static_cast<uint8_t>((selected_ + 1U) % ITEM_COUNT);
    } else if (command.type == AppCommandType::Activate) {
        requested_ = itemApp(selected_);
    }
}

void LauncherApp::onTick(uint32_t, AppContext&) {}

void LauncherApp::onRender(AppContext& context) {
    DisplayService& display = context.display;
    display.startFrame();
    display.drawHeader("DESKTOP");
    display.drawText("PlaygroundOS", 16, 42, TFT_WHITE, BitmapFontSize::Bold12);
    display.drawText("Select an application", 16, 64, TFT_LIGHTGREY,
                     BitmapFontSize::Small12);
    for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
        const int16_t y = 92 + static_cast<int16_t>(index) * 32;
        if (index == selected_) {
            display.fillRect(10, y - 4, 300, 26, TFT_DARKCYAN);
            display.drawText(">", 18, y, TFT_YELLOW, BitmapFontSize::Bold12);
        }
        display.drawText(itemName(index), 38, y, TFT_WHITE,
                         BitmapFontSize::Bold12);
    }
    display.drawFooter("up/down select   enter open");
    display.pushFrame();
}

bool LauncherApp::handlesNavigation() const {
    return true;
}

AppId LauncherApp::requestedApp() const {
    return requested_;
}
