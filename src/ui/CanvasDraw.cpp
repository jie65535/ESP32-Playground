#include "ui/CanvasDraw.h"

namespace pgos {

void drawRect(lv_layer_t* layer, const lv_area_t& area, lv_color_t color,
              int32_t radius, lv_opa_t opacity) {
    if (layer == nullptr) {
        return;
    }
    lv_draw_rect_dsc_t descriptor;
    lv_draw_rect_dsc_init(&descriptor);
    descriptor.bg_color = color;
    descriptor.bg_opa = opacity;
    descriptor.radius = radius;
    lv_draw_rect(layer, &descriptor, &area);
}

void drawText(lv_layer_t* layer, const char* text, lv_area_t area,
              lv_color_t color, const lv_font_t* font,
              lv_text_align_t align) {
    if (layer == nullptr || text == nullptr || font == nullptr) {
        return;
    }
    const int32_t areaHeight = area.y2 - area.y1 + 1;
    const int32_t lineHeight = lv_font_get_line_height(font);
    if (areaHeight > lineHeight) {
        area.y1 += static_cast<lv_coord_t>((areaHeight - lineHeight) / 2);
        area.y2 = area.y1 + static_cast<lv_coord_t>(lineHeight - 1);
    }

    lv_draw_label_dsc_t descriptor;
    lv_draw_label_dsc_init(&descriptor);
    descriptor.color = color;
    descriptor.font = font;
    descriptor.text = text;
    descriptor.text_local = true;
    descriptor.align = align;
    descriptor.opa = LV_OPA_COVER;
    lv_draw_label(layer, &descriptor, &area);
}

}  // namespace pgos
