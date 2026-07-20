#include "ui/OnScreenKeyboard.h"

#include "ui/UiRuntime.h"

#include <cstring>

namespace {

constexpr uint32_t LOWER_BUTTON_COUNT = 34;
constexpr uint32_t SPECIAL_BUTTON_COUNT = 43;
constexpr lv_style_selector_t FOCUSED_KEY_SELECTOR =
    static_cast<lv_style_selector_t>(LV_PART_ITEMS) |
    static_cast<lv_style_selector_t>(LV_STATE_FOCUSED);

const char* const LOWER_MAP[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    "z", "x", "c", "v", "b", "n", "m", ".", "-", "_", "\n",
    "123", "ABC", " ", LV_SYMBOL_BACKSPACE, LV_SYMBOL_OK, "",
};

const char* const UPPER_MAP[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "Z", "X", "C", "V", "B", "N", "M", ".", "-", "_", "\n",
    "123", "abc", " ", LV_SYMBOL_BACKSPACE, LV_SYMBOL_OK, "",
};

const char* const SPECIAL_MAP[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
    "+", "=", "[", "]", "{", "}", "\\", "|", "/", "?", "\n",
    "~", "`", ";", ":", "'", "\"", ",", "<", ">", "\n",
    "abc", " ", LV_SYMBOL_BACKSPACE, LV_SYMBOL_OK, "",
};

const lv_buttonmatrix_ctrl_t TEXT_CTRL[LOWER_BUTTON_COUNT] = {
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_2, LV_BUTTONMATRIX_CTRL_WIDTH_2,
    LV_BUTTONMATRIX_CTRL_WIDTH_6, LV_BUTTONMATRIX_CTRL_WIDTH_2,
    LV_BUTTONMATRIX_CTRL_WIDTH_2,
};

const lv_buttonmatrix_ctrl_t SPECIAL_CTRL[SPECIAL_BUTTON_COUNT] = {
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1, LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_1,
    LV_BUTTONMATRIX_CTRL_WIDTH_2, LV_BUTTONMATRIX_CTRL_WIDTH_6,
    LV_BUTTONMATRIX_CTRL_WIDTH_2, LV_BUTTONMATRIX_CTRL_WIDTH_2,
};

static_assert(sizeof(TEXT_CTRL) / sizeof(TEXT_CTRL[0]) == LOWER_BUTTON_COUNT,
              "text keyboard control map mismatch");
static_assert(sizeof(SPECIAL_CTRL) / sizeof(SPECIAL_CTRL[0]) ==
                  SPECIAL_BUTTON_COUNT,
              "special keyboard control map mismatch");

bool sameKey(const char* left, const char* right) {
    return left != nullptr && right != nullptr && std::strcmp(left, right) == 0;
}

}  // namespace

void OnScreenKeyboard::create(lv_obj_t* parent, UiRuntime& ui,
                              int16_t textAreaY, int16_t textAreaHeight,
                              int16_t keyboardY, int16_t keyboardHeight,
                              bool password, uint32_t maxLength,
                              const char* placeholder) {
    reset();

    textArea_ = lv_textarea_create(parent);
    lv_textarea_set_one_line(textArea_, true);
    lv_textarea_set_max_length(textArea_, maxLength);
    lv_textarea_set_placeholder_text(textArea_, placeholder);
    lv_textarea_set_password_bullet(textArea_, "*");
    lv_textarea_set_password_show_time(textArea_, 0);
    lv_textarea_set_password_mode(textArea_, password);
    lv_obj_set_size(textArea_, 296, textAreaHeight);
    lv_obj_set_pos(textArea_, 12, textAreaY);
    lv_obj_set_scrollbar_mode(textArea_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(textArea_, 6, 0);
    lv_obj_set_style_bg_color(textArea_, ui.panel(), 0);
    lv_obj_set_style_bg_opa(textArea_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(textArea_, 2, 0);
    lv_obj_set_style_border_color(textArea_, ui.accent(), 0);
    lv_obj_set_style_text_color(textArea_, ui.text(), 0);
    lv_obj_set_style_text_font(textArea_, ui.font(14), 0);
    lv_obj_set_style_pad_left(textArea_, 8, 0);
    lv_obj_set_style_pad_right(textArea_, 8, 0);
    lv_obj_set_style_pad_top(textArea_, 6, 0);
    lv_obj_set_style_text_color(textArea_, ui.accent(), LV_PART_CURSOR);
    lv_obj_set_style_border_color(textArea_, ui.accent(), LV_PART_CURSOR);
    lv_obj_add_state(textArea_, LV_STATE_FOCUSED);

    keyboard_ = lv_keyboard_create(parent);
    lv_keyboard_set_map(keyboard_, LV_KEYBOARD_MODE_USER_1,
                        LOWER_MAP, TEXT_CTRL);
    lv_keyboard_set_map(keyboard_, LV_KEYBOARD_MODE_USER_2,
                        UPPER_MAP, TEXT_CTRL);
    lv_keyboard_set_map(keyboard_, LV_KEYBOARD_MODE_USER_3,
                        SPECIAL_MAP, SPECIAL_CTRL);
    lv_keyboard_set_textarea(keyboard_, textArea_);
    lv_keyboard_set_popovers(keyboard_, false);
    // The keyboard constructor aligns itself to the parent's bottom edge.
    // Reset the alignment before treating x/y as page-local coordinates.
    lv_obj_set_align(keyboard_, LV_ALIGN_TOP_LEFT);
    lv_obj_set_size(keyboard_, 308, keyboardHeight);
    lv_obj_set_pos(keyboard_, 6, keyboardY);
    lv_obj_set_style_radius(keyboard_, 6, 0);
    lv_obj_set_style_bg_color(keyboard_, ui.panel(), 0);
    lv_obj_set_style_bg_opa(keyboard_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(keyboard_, 0, 0);
    lv_obj_set_style_pad_all(keyboard_, 2, 0);
    lv_obj_set_style_pad_row(keyboard_, 2, 0);
    lv_obj_set_style_pad_column(keyboard_, 2, 0);
    lv_obj_set_style_text_font(keyboard_, ui.font(12), LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard_, ui.text(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard_, ui.panelRaised(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(keyboard_, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(keyboard_, 4, LV_PART_ITEMS);
    lv_obj_set_style_border_width(keyboard_, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(
        keyboard_, ui.accent(), FOCUSED_KEY_SELECTOR);
    lv_obj_set_style_text_color(
        keyboard_, ui.background(), FOCUSED_KEY_SELECTOR);
    lv_obj_add_state(
        keyboard_, static_cast<lv_state_t>(LV_STATE_FOCUSED |
                                           LV_STATE_FOCUS_KEY));

    setMode(LV_KEYBOARD_MODE_USER_1);
}

void OnScreenKeyboard::reset() {
    textArea_ = nullptr;
    keyboard_ = nullptr;
    mode_ = LV_KEYBOARD_MODE_USER_1;
}

bool OnScreenKeyboard::handleNavigation(AppCommandType command) {
    if (keyboard_ == nullptr) {
        return false;
    }

    uint32_t key = 0;
    switch (command) {
        case AppCommandType::Previous:
            key = LV_KEY_UP;
            break;
        case AppCommandType::Next:
            key = LV_KEY_DOWN;
            break;
        case AppCommandType::Left:
            key = LV_KEY_LEFT;
            break;
        case AppCommandType::Right:
            key = LV_KEY_RIGHT;
            break;
        default:
            return false;
    }
    lv_obj_send_event(keyboard_, LV_EVENT_KEY, &key);
    return true;
}

OnScreenKeyboardAction OnScreenKeyboard::activate() {
    if (keyboard_ == nullptr) {
        return OnScreenKeyboardAction::None;
    }
    const uint32_t selected = lv_buttonmatrix_get_selected_button(keyboard_);
    if (selected == LV_BUTTONMATRIX_BUTTON_NONE) {
        lv_buttonmatrix_set_selected_button(keyboard_, 0);
        return OnScreenKeyboardAction::ModeChanged;
    }
    const char* key = lv_buttonmatrix_get_button_text(keyboard_, selected);
    if (sameKey(key, "ABC")) {
        setMode(LV_KEYBOARD_MODE_USER_2);
        return OnScreenKeyboardAction::ModeChanged;
    }
    if (sameKey(key, "abc")) {
        setMode(LV_KEYBOARD_MODE_USER_1);
        return OnScreenKeyboardAction::ModeChanged;
    }
    if (sameKey(key, "123")) {
        setMode(LV_KEYBOARD_MODE_USER_3);
        return OnScreenKeyboardAction::ModeChanged;
    }
    if (sameKey(key, LV_SYMBOL_OK)) {
        return OnScreenKeyboardAction::Ready;
    }

    uint32_t eventButton = selected;
    lv_obj_send_event(keyboard_, LV_EVENT_VALUE_CHANGED, &eventButton);
    return OnScreenKeyboardAction::Edited;
}

bool OnScreenKeyboard::cycleMode() {
    if (keyboard_ == nullptr) {
        return false;
    }
    if (mode_ == LV_KEYBOARD_MODE_USER_1) {
        setMode(LV_KEYBOARD_MODE_USER_2);
    } else if (mode_ == LV_KEYBOARD_MODE_USER_2) {
        setMode(LV_KEYBOARD_MODE_USER_3);
    } else {
        setMode(LV_KEYBOARD_MODE_USER_1);
    }
    return true;
}

const char* OnScreenKeyboard::text() const {
    return textArea_ == nullptr ? "" : lv_textarea_get_text(textArea_);
}

size_t OnScreenKeyboard::length() const {
    return std::strlen(text());
}

void OnScreenKeyboard::clear() {
    if (textArea_ != nullptr) {
        lv_textarea_set_text(textArea_, "");
    }
}

void OnScreenKeyboard::setMode(lv_keyboard_mode_t mode) {
    if (keyboard_ == nullptr) {
        return;
    }
    mode_ = mode;
    lv_keyboard_set_mode(keyboard_, mode_);
    lv_obj_update_layout(keyboard_);
    lv_buttonmatrix_set_selected_button(keyboard_, 0);
}
