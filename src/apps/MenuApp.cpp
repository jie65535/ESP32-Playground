#include "apps/MenuApp.h"

MenuApp::MenuApp(const MenuDefinition& definition) : definition_(definition) {}

AppId MenuApp::id() const {
    return definition_.id;
}

const char* MenuApp::name() const {
    return definition_.name;
}

void MenuApp::onEnter(AppContext&) {
    requested_ = AppId::Count;
    renderedSelection_ = -1;
    if (selected_ >= itemCount()) {
        selected_ = 0;
    }
}

void MenuApp::onExit(AppContext&) {
    root_ = nullptr;
    for (UiCard& card : cards_) {
        card = UiCard{};
    }
}

void MenuApp::onCommand(const AppCommand& command, AppContext&) {
    const uint8_t count = itemCount();
    if (count == 0) {
        return;
    }
    if (command.type == AppCommandType::Previous ||
        command.type == AppCommandType::Left) {
        selected_ = selected_ == 0 ? count - 1U : selected_ - 1U;
    } else if (command.type == AppCommandType::Next ||
               command.type == AppCommandType::Right) {
        selected_ = static_cast<uint8_t>((selected_ + 1U) % count);
    } else if (command.type == AppCommandType::Activate) {
        requested_ = definition_.items[selected_].target;
    }
}

void MenuApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* MenuApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot(nullptr, definition_.title,
                                      definition_.subtitle);
    const int16_t startY =
        definition_.subtitle == nullptr ? UiRuntime::CARD_START_Y
                                        : UiRuntime::CARD_START_WITH_SUBTITLE_Y;
    for (uint8_t index = 0; index < itemCount(); ++index) {
        const MenuItemDefinition& item = definition_.items[index];
        cards_[index] = context.ui.createCard(
            root_, startY + static_cast<int16_t>(index) * UiRuntime::CARD_STEP_Y,
            item.icon, item.title, item.subtitle);
    }
    return root_;
}

void MenuApp::onUpdateView(AppContext& context) {
    const uint8_t count = itemCount();
    if (root_ == nullptr || count == 0 ||
        renderedSelection_ == static_cast<int8_t>(selected_)) {
        return;
    }
    for (uint8_t index = 0; index < count; ++index) {
        context.ui.setCardFocused(cards_[index], index == selected_);
    }
    context.ui.centerFocused(root_, cards_[selected_].root);
    renderedSelection_ = static_cast<int8_t>(selected_);
}

AppId MenuApp::requestedApp() const {
    return requested_;
}

uint8_t MenuApp::itemCount() const {
    return definition_.itemCount > MAX_ITEMS ? MAX_ITEMS
                                              : definition_.itemCount;
}
