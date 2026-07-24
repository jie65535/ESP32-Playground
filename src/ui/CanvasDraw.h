#pragma once

#include <lvgl.h>

namespace pgos {

void drawRect(lv_layer_t* layer, const lv_area_t& area, lv_color_t color,
              int32_t radius = 0, lv_opa_t opacity = LV_OPA_COVER);

// LVGL aligns text horizontally but does not center a single line vertically
// inside an arbitrary draw area. Normalize the area to the font's line height
// so every self-drawn game HUD and button uses the same centering rule.
void drawText(lv_layer_t* layer, const char* text, lv_area_t area,
              lv_color_t color, const lv_font_t* font,
              lv_text_align_t align = LV_TEXT_ALIGN_CENTER);

void drawTextSingleLine(lv_layer_t* layer, const char* text, lv_area_t area,
                        lv_color_t color, const lv_font_t* font,
                        lv_text_align_t align = LV_TEXT_ALIGN_CENTER);

}  // namespace pgos
