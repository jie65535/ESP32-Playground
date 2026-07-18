#include "apps/LauncherApp.h"

#include "ui/UiRuntime.h"

namespace {

const char* itemName(uint8_t index) {
    switch (index) {
        case 0: return "System";
        case 1: return "Color Lab";
        case 2: return "Display";
        case 3: return "Sound";
        case 4: return "Console";
        case 5: return "Connectivity";
        default: return "Unknown";
    }
}

const char* itemDescription(uint8_t index) {
    switch (index) {
        case 0: return "memory, silicon and runtime";
        case 1: return "display and visual experiments";
        case 2: return "brightness and screen timeout";
        case 3: return "volume and interaction feedback";
        case 4: return "server address and wireless control";
        case 5: return "Wi-Fi, IP and network services";
        default: return "";
    }
}

const char* itemSymbol(uint8_t index) {
    switch (index) {
        case 0: return LV_SYMBOL_SETTINGS;
        case 1: return LV_SYMBOL_IMAGE;
        case 2: return LV_SYMBOL_EYE_OPEN;
        case 3: return LV_SYMBOL_VOLUME_MAX;
        case 4: return LV_SYMBOL_UPLOAD;
        case 5: return LV_SYMBOL_WIFI;
        default: return LV_SYMBOL_LIST;
    }
}

AppId itemApp(uint8_t index) {
    switch (index) {
        case 0: return AppId::SystemInfo;
        case 1: return AppId::DisplayTest;
        case 2: return AppId::DisplaySettings;
        case 3: return AppId::SoundSettings;
        case 4: return AppId::ConsoleSettings;
        case 5: return AppId::NetworkSettings;
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
