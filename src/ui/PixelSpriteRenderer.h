#pragma once

#include "ui/PixelSprite.h"

#include <lvgl.h>

namespace pgos {

class PixelSpriteRenderer final {
public:
    PixelSpriteRenderer() = delete;

    // Use LVGL's single-task RGB565A8 path when possible. The horizontal-run
    // path remains as a fallback for mirrored or scaled sprites.
    static void draw(lv_layer_t* layer, const PixelSprite& sprite, int32_t x,
                     int32_t y, uint8_t scale = 1, bool flipX = false);

    // Draw only a vertical source slice. This is used for the native big
    // Mario crouch pose, whose collision height is shorter than its standing
    // 16x32 frame.
    static void drawRegion(lv_layer_t* layer, const PixelSprite& sprite,
                           int32_t x, int32_t y, uint16_t sourceY,
                           uint16_t sourceHeight, uint8_t scale = 1,
                           bool flipX = false);

private:
    static lv_color_t colorFromRgb565(uint16_t value);
    static const lv_image_dsc_t* imageDescriptor(const PixelSprite& sprite);
};

}  // namespace pgos
