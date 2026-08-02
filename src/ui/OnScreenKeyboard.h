#pragma once

#include "core/AppTypes.h"

#include <lvgl.h>

class UiRuntime;

enum class OnScreenKeyboardAction : uint8_t {
    None,
    Edited,
    ModeChanged,
    Ready,
};

class OnScreenKeyboard {
public:
    void create(lv_obj_t* parent, UiRuntime& ui,
                int16_t textAreaY, int16_t textAreaHeight,
                int16_t keyboardY, int16_t keyboardHeight,
                bool password, uint32_t maxLength,
                const char* placeholder);
    void reset();

    bool handleNavigation(AppCommandType command);
    OnScreenKeyboardAction activate();
    bool cycleMode();

    const char* text() const;
    size_t length() const;
    void setText(const char* text);
    void clear();

private:
    lv_obj_t* textArea_ = nullptr;
    lv_obj_t* keyboard_ = nullptr;
    lv_keyboard_mode_t mode_ = LV_KEYBOARD_MODE_USER_1;

    void setMode(lv_keyboard_mode_t mode);
};
