#include "apps/SystemInfoApp.h"

#include "services/DisplayService.h"
#include "services/RuntimeMonitorService.h"
#include "ui/UiRuntime.h"

namespace {

uint8_t percent(uint32_t value, uint32_t total) {
    return total == 0 ? 0 : static_cast<uint8_t>(
        min<uint64_t>(100U, static_cast<uint64_t>(value) * 100U / total));
}

}  // namespace

AppId SystemInfoApp::id() const {
    return AppId::SystemInfo;
}

const char* SystemInfoApp::name() const {
    return "System";
}

void SystemInfoApp::onEnter(AppContext&) {
    selected_ = 0;
    renderedSelected_ = -1;
}

void SystemInfoApp::onExit(AppContext&) {
    root_ = nullptr;
    for (UiCard& card : cards_) {
        card = UiCard{};
    }
}

void SystemInfoApp::onCommand(const AppCommand& command, AppContext&) {
    if (command.type == AppCommandType::Previous ||
        command.type == AppCommandType::Left) {
        selected_ = selected_ == 0 ? ITEM_COUNT - 1U : selected_ - 1U;
    } else if (command.type == AppCommandType::Next ||
               command.type == AppCommandType::Right) {
        selected_ = static_cast<uint8_t>((selected_ + 1U) % ITEM_COUNT);
    }
}

void SystemInfoApp::onTick(uint32_t nowMs, AppContext&) {
    nowMs_ = nowMs;
}

lv_obj_t* SystemInfoApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot(nullptr, "System Monitor");
    cards_[0] = context.ui.createCard(root_, UiRuntime::CARD_START_Y, UiIcon::System,
                                      "ESP32-S3", "R8N16 / 240 MHz");
    cards_[1] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + UiRuntime::CARD_STEP_Y, UiIcon::System,
                                      "主循环", "--");
    cards_[2] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + 2 * UiRuntime::CARD_STEP_Y, UiIcon::Display,
                                      "LCD 刷新", "--");
    cards_[3] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + 3 * UiRuntime::CARD_STEP_Y, UiIcon::System,
                                      "内部内存", "--");
    cards_[4] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + 4 * UiRuntime::CARD_STEP_Y, UiIcon::System,
                                      "PSRAM", "--");
    cards_[5] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + 5 * UiRuntime::CARD_STEP_Y, UiIcon::Tools,
                                      "Flash 与 OTA", "--");
    return root_;
}

void SystemInfoApp::onUpdateView(AppContext& context) {
    if (root_ == nullptr) {
        return;
    }

    if (renderedSelected_ != static_cast<int8_t>(selected_)) {
        for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
            context.ui.setCardFocused(cards_[index], index == selected_);
        }
        context.ui.centerFocused(root_, cards_[selected_].root);
        renderedSelected_ = static_cast<int8_t>(selected_);
    }

    const RuntimeSnapshot runtime = context.runtime.snapshot();
    const String identity = String("运行 ") + nowMs_ / 1000U +
                            "s / 任务 " + runtime.taskCount;
    const String loop = String("duty ") + runtime.mainLoopBusyPercent +
                        "% / ui " + String(runtime.lastUiUs / 1000.0F, 1) +
                        " ms / net " +
                        String(runtime.lastMirrorUs / 1000.0F, 1) + " ms";
    const String flush = String("SPI ") +
                         String(runtime.lastFlushTransferUs / 1000.0F, 1) +
                         " ms / copy " +
                         String(runtime.lastFlushCopyUs / 1000.0F, 1) +
                         " ms / wait " +
                         String(runtime.lastFlushWaitUs / 1000.0F, 1) +
                         " ms / " + runtime.lastFlushAreas + " areas";
    const String heap = String(percent(runtime.freeHeap, runtime.heapSize)) +
                        "% 可用 / 低水位 " +
                        DisplayService::formatBytes(runtime.minimumFreeHeap);
    const String psram = DisplayService::formatBytes(runtime.freePsram) +
                         " 可用 / " +
                         DisplayService::formatBytes(runtime.psramSize);
    const String flash = String("app ") +
                         DisplayService::formatBytes(runtime.sketchSize) +
                         " / OTA " +
                         DisplayService::formatBytes(runtime.freeSketchSpace);
    lv_label_set_text(cards_[0].subtitle, identity.c_str());
    lv_label_set_text(cards_[1].subtitle, loop.c_str());
    lv_label_set_text(cards_[2].subtitle, flush.c_str());
    lv_label_set_text(cards_[3].subtitle, heap.c_str());
    lv_label_set_text(cards_[4].subtitle, psram.c_str());
    lv_label_set_text(cards_[5].subtitle, flash.c_str());
}
