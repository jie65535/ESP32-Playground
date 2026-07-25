#pragma once

#include <lvgl.h>

enum class UiIcon : uint8_t {
    Gamepad,
    Settings,
    Tools,
    Display,
    Sound,
    Mic,
    Activity,
    Sliders,
    AudioWaveform,
    Sparkles,
    Headphones,
    Play,
    Light,
    Bluetooth,
    Wifi,
    Remote,
    System,
    Clock,
    Palette,
    Snake,
    Tetris,
    Breakout,
    Blackjack,
    Minesweeper,
};

namespace PgosUiIcons {

const lv_image_dsc_t* image(UiIcon icon);

}  // namespace PgosUiIcons
