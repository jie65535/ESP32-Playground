#include "apps/SystemInfoApp.h"

#include "services/DisplayService.h"

AppId SystemInfoApp::id() const {
    return AppId::SystemInfo;
}

const char* SystemInfoApp::name() const {
    return "System";
}

void SystemInfoApp::onEnter(AppContext&) {}

void SystemInfoApp::onExit(AppContext&) {}

void SystemInfoApp::onCommand(const AppCommand&, AppContext&) {}

void SystemInfoApp::onTick(uint32_t nowMs, AppContext&) {
    nowMs_ = nowMs;
}

void SystemInfoApp::onRender(AppContext& context) {
    DisplayService& display = context.display;
    display.startFrame();
    display.drawHeader("SYSTEM");
    display.drawText("ES3N28P / R8N16", 16, 40, TFT_WHITE,
                     BitmapFontSize::Bold12);
    display.drawText("ILI9341  320x240", 16, 62, TFT_WHITE,
                     BitmapFontSize::Small12);
    display.drawText("PlaygroundOS", 16, 80, TFT_CYAN,
                     BitmapFontSize::Small12);
    display.drawValue("Uptime", String(nowMs_ / 1000U) + " s", 108);
    display.drawValue("Free heap", DisplayService::formatBytes(ESP.getFreeHeap()),
                      132);
    display.drawValue("Free PSRAM",
                      DisplayService::formatBytes(ESP.getFreePsram()), 156);
    display.drawValue("Flash",
                      DisplayService::formatBytes(ESP.getFlashChipSize()), 180);
    display.drawFooter("system telemetry");
    display.pushFrame();
}
