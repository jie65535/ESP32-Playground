#pragma once

#include "ui/BitmapFont.h"

#include <lvgl.h>

namespace PgosBitmapFont {

// Returns the LVGL view of the existing Fusion Pixel bitmap font data.  The
// returned objects are static and must not be freed by the caller.
const lv_font_t* font(BitmapFontSize size);

}  // namespace PgosBitmapFont
