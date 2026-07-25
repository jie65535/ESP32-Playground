#pragma once

#include <lvgl.h>

namespace TinyLmFont {

const lv_font_t* font();
bool supports(uint32_t codepoint);

}  // namespace TinyLmFont
