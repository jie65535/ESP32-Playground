#include "apps/LauncherApp.h"

#include "ui/UiRuntime.h"

namespace {

const char* itemName(uint8_t index) {
    switch (index) {
        case 0: return "System";
        case 1: return "Time";
        case 2: return "Color Lab";
        case 3: return "Display";
        case 4: return "Sound";
        case 5: return "RGB Light";
        case 6: return "Controller";
        case 7: return "Console";
        case 8: return "Connectivity";
        default: return "Unknown";
    }
}

const char* itemDescription(uint8_t index) {
    switch (index) {
        case 0: return "memory, silicon and runtime";
        case 1: return "battery-backed PCF8563 clock";
        case 2: return "display and visual experiments";
        case 3: return "brightness and screen timeout";
        case 4: return "volume and interaction feedback";
        case 5: return "color, brightness and animated effects";
        case 6: return "Bluetooth gamepad and power policy";
        case 7: return "server address and wireless control";
        case 8: return "Wi-Fi, IP and network services";
        default: return "";
    }
}

const char* itemSymbol(uint8_t index) {
    switch (index) {
        case 0: return LV_SYMBOL_SETTINGS;
        case 1: return LV_SYMBOL_LOOP;
        case 2: return LV_SYMBOL_IMAGE;
        case 3: return LV_SYMBOL_EYE_OPEN;
        case 4: return LV_SYMBOL_VOLUME_MAX;
        case 5: return LV_SYMBOL_TINT;
        case 6: return LV_SYMBOL_BLUETOOTH;
        case 7: return LV_SYMBOL_UPLOAD;
        case 8: return LV_SYMBOL_WIFI;
        default: return LV_SYMBOL_LIST;
    }
}

AppId itemApp(uint8_t index) {
    switch (index) {
        case 0: return AppId::SystemInfo;
        case 1: return AppId::Time;
        case 2: return AppId::DisplayTest;
        case 3: return AppId::DisplaySettings;
        case 4: return AppId::SoundSettings;
        case 5: return AppId::RgbSettings;
        case 6: return AppId::ControllerSettings;
        case 7: return AppId::ConsoleSettings;
        case 8: return AppId::NetworkSettings;
        default: return AppId::Count;
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
    renderedSelection_ = -1;
}

void LauncherApp::onExit(AppContext&) {
    root_ = nullptr;
    for (UiCard& card : cards_) {
        card = UiCard{};
    }
}

void LauncherApp::onCommand(const AppCommand& command, AppContext&) {
    if (command.type == AppCommandType::Previous ||
        command.type == AppCommandType::Left) {
        selected_ = selected_ == 0 ? ITEM_COUNT - 1U : selected_ - 1U;
    } else if (command.type == AppCommandType::Next ||
               command.type == AppCommandType::Right) {
        selected_ = static_cast<uint8_t>((selected_ + 1U) % ITEM_COUNT);
    } else if (command.type == AppCommandType::Activate) {
        requested_ = itemApp(selected_);
    }
}

void LauncherApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* LauncherApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot("PLAYGROUND / HOME", "Applications");
    for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
        cards_[index] = context.ui.createCard(
            root_, 58 + static_cast<int16_t>(index) * 50,
            itemSymbol(index), itemName(index), itemDescription(index));
    }
    return root_;
}

void LauncherApp::onUpdateView(AppContext& context) {
    if (renderedSelection_ == static_cast<int8_t>(selected_)) {
        return;
    }
    for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
        context.ui.setCardFocused(cards_[index], index == selected_);
    }
    context.ui.centerFocused(root_, cards_[selected_].root);
    renderedSelection_ = static_cast<int8_t>(selected_);
}

AppId LauncherApp::requestedApp() const {
    return requested_;
}
