#include "ui/UiIcons.h"

#include "ui/UiIconData.inc"

namespace PgosUiIcons {

const lv_image_dsc_t* image(UiIcon icon) {
    switch (icon) {
        case UiIcon::Gamepad: return &ICON_GAMEPAD_2;
        case UiIcon::Settings: return &ICON_SETTINGS_2;
        case UiIcon::Tools: return &ICON_WRENCH;
        case UiIcon::Display: return &ICON_MONITOR;
        case UiIcon::Sound: return &ICON_VOLUME_2;
        case UiIcon::Light: return &ICON_LIGHTBULB;
        case UiIcon::Bluetooth: return &ICON_BLUETOOTH;
        case UiIcon::Wifi: return &ICON_WIFI;
        case UiIcon::Remote: return &ICON_RADIO_TOWER;
        case UiIcon::System: return &ICON_GAUGE;
        case UiIcon::Clock: return &ICON_CLOCK_3;
        case UiIcon::Palette: return &ICON_PALETTE;
        case UiIcon::Snake: return &ICON_CHERRY;
        case UiIcon::Tetris: return &ICON_BLOCKS;
        case UiIcon::Breakout: return &ICON_BRICK_WALL;
        case UiIcon::Blackjack: return &ICON_SPADE;
        default: return &ICON_SETTINGS_2;
    }
}

}  // namespace PgosUiIcons
