#include "apps/SystemInfoApp.h"

#include "services/DisplayService.h"
#include "ui/UiRuntime.h"

AppId SystemInfoApp::id() const {
    return AppId::SystemInfo;
}

const char* SystemInfoApp::name() const {
    return "System";
}

void SystemInfoApp::onEnter(AppContext&) {}

void SystemInfoApp::onExit(AppContext&) {
    root_ = nullptr;
    uptimeValue_ = nullptr;
    heapValue_ = nullptr;
    psramValue_ = nullptr;
    flashValue_ = nullptr;
}

void SystemInfoApp::onCommand(const AppCommand&, AppContext&) {}

void SystemInfoApp::onTick(uint32_t nowMs, AppContext&) {
    nowMs_ = nowMs;
}

lv_obj_t* SystemInfoApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot("DEVICE / SYSTEM", "System");

    UiCard identity = context.ui.createCard(
        root_, 58, LV_SYMBOL_SETTINGS, "ESP32-S3",
        "ES3N28P  /  R8N16  /  240 MHz");
    identity.normalX = 12;
    identity.normalWidth = 296;
    context.ui.setCardFocused(identity, false, false);

    context.ui.createValueRow(root_, "UPTIME", "--", 108, &uptimeValue_);
    context.ui.createValueRow(root_, "FREE HEAP", "--", 130, &heapValue_);
    context.ui.createValueRow(root_, "FREE PSRAM", "--", 152, &psramValue_);
    context.ui.createValueRow(root_, "FLASH", "--", 174, &flashValue_);
    return root_;
}

void SystemInfoApp::onUpdateView(AppContext&) {
    if (uptimeValue_ == nullptr) {
        return;
    }

    const String uptime = String(nowMs_ / 1000U) + " s";
    const String heap = DisplayService::formatBytes(ESP.getFreeHeap());
    const String psram = DisplayService::formatBytes(ESP.getFreePsram());
    const String flash = DisplayService::formatBytes(ESP.getFlashChipSize());
    lv_label_set_text(uptimeValue_, uptime.c_str());
    lv_label_set_text(heapValue_, heap.c_str());
    lv_label_set_text(psramValue_, psram.c_str());
    lv_label_set_text(flashValue_, flash.c_str());
}
