#include "apps/LauncherApp.h"

#include "ui/UiRuntime.h"

namespace {

const char* itemName(uint8_t index) {
    switch (index) {
        case 0: return "System";
        case 1: return "Color Lab";
        case 2: return "Network";
        default: return "Unknown";
    }
}

const char* itemDescription(uint8_t index) {
    switch (index) {
        case 0: return "device info";
        case 1: return "RGB565 lab";
        case 2: return "Wi-Fi / TCP";
        default: return "";
    }
}

const char* itemSymbol(uint8_t index) {
    switch (index) {
        case 0: return LV_SYMBOL_SETTINGS;
        case 1: return LV_SYMBOL_IMAGE;
        case 2: return LV_SYMBOL_WIFI;
        default: return LV_SYMBOL_LIST;
    }
}

AppId itemApp(uint8_t index) {
    switch (index) {
        case 0: return AppId::SystemInfo;
        case 1: return AppId::DisplayTest;
        case 2: return AppId::NetworkSettings;
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
    switch (command.type) {
        case AppCommandType::Previous:
            moveVertical(-1);
            break;
        case AppCommandType::Next:
            moveVertical(1);
            break;
        case AppCommandType::Left:
            moveHorizontal(-1);
            break;
        case AppCommandType::Right:
            moveHorizontal(1);
            break;
        case AppCommandType::Activate:
            requested_ = itemApp(selected_);
            break;
        default:
            break;
    }
}

void LauncherApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* LauncherApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot("PLAYGROUND / HOME", "Applications",
                                      "Arrow keys move  /  Enter opens");
    for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
        const int16_t column = index % GRID_COLUMNS;
        const int16_t row = index / GRID_COLUMNS;
        const int16_t x = 12 + column * 152;
        const int16_t y = 82 + row * 62;
        cards_[index] = context.ui.createCard(
            root_, y,
            itemSymbol(index), itemName(index), itemDescription(index));
        cards_[index].normalX = x + 5;
        cards_[index].focusedX = x;
        cards_[index].normalWidth = 138;
        cards_[index].focusedWidth = 146;
        lv_obj_set_pos(cards_[index].root, cards_[index].normalX, y);
        lv_obj_set_size(cards_[index].root, cards_[index].normalWidth, 52);
        lv_obj_set_pos(cards_[index].icon, 7, 11);
        lv_obj_set_pos(cards_[index].title, 47, 8);
        lv_obj_set_pos(cards_[index].subtitle, 48, 29);
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
    renderedSelection_ = static_cast<int8_t>(selected_);
}

AppId LauncherApp::requestedApp() const {
    return requested_;
}

void LauncherApp::moveHorizontal(int8_t delta) {
    const uint8_t rowStart = (selected_ / GRID_COLUMNS) * GRID_COLUMNS;
    const uint8_t remaining = ITEM_COUNT - rowStart;
    const uint8_t rowCount = remaining < GRID_COLUMNS ? remaining : GRID_COLUMNS;
    const uint8_t column = selected_ - rowStart;
    if (delta < 0) {
        selected_ = rowStart + (column == 0 ? rowCount - 1U : column - 1U);
    } else {
        selected_ = rowStart + static_cast<uint8_t>((column + 1U) % rowCount);
    }
}

void LauncherApp::moveVertical(int8_t delta) {
    const uint8_t rowCount =
        static_cast<uint8_t>((ITEM_COUNT + GRID_COLUMNS - 1U) / GRID_COLUMNS);
    const uint8_t currentRow = selected_ / GRID_COLUMNS;
    const uint8_t currentColumn = selected_ % GRID_COLUMNS;
    const uint8_t targetRow = delta < 0
                                  ? (currentRow == 0 ? rowCount - 1U
                                                     : currentRow - 1U)
                                  : static_cast<uint8_t>((currentRow + 1U) % rowCount);
    const uint8_t targetStart = targetRow * GRID_COLUMNS;
    const uint8_t remaining = ITEM_COUNT - targetStart;
    const uint8_t targetCount = remaining < GRID_COLUMNS ? remaining : GRID_COLUMNS;
    const uint8_t targetColumn =
        currentColumn < targetCount ? currentColumn : targetCount - 1U;
    selected_ = targetStart + targetColumn;
}
