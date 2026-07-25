#include "ui/TinyLmFont.h"

#include "ui/TinyLmFontData.h"

namespace {

constexpr uint8_t FONT_LINE_HEIGHT = 16;
constexpr uint8_t FONT_BOX_WIDTH = 12;

const TinyLmBitmapGlyph* findGlyph(uint32_t codepoint,
                                   uint32_t* indexOut = nullptr) {
    size_t low = 0;
    size_t high = TINYLM_FONT_GLYPH_COUNT;
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        const uint32_t value =
            pgm_read_dword(&TINYLM_FONT_GLYPHS[middle].codepoint);
        if (value < codepoint) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    if (low >= TINYLM_FONT_GLYPH_COUNT ||
        pgm_read_dword(&TINYLM_FONT_GLYPHS[low].codepoint) != codepoint) {
        return nullptr;
    }
    if (indexOut != nullptr) {
        *indexOut = static_cast<uint32_t>(low + 1U);
    }
    return &TINYLM_FONT_GLYPHS[low];
}

bool getGlyphDescriptor(const lv_font_t*, lv_font_glyph_dsc_t* out,
                        uint32_t unicode, uint32_t) {
    if (out == nullptr) {
        return false;
    }
    uint32_t glyphId = 0;
    const TinyLmBitmapGlyph* glyph = findGlyph(unicode, &glyphId);
    if (glyph == nullptr) {
        return false;
    }
    out->adv_w = pgm_read_byte(&glyph->advance);
    out->box_w = FONT_BOX_WIDTH;
    out->box_h = FONT_LINE_HEIGHT;
    out->ofs_x = 0;
    out->ofs_y = 0;
    out->stride = 0;
    out->format = LV_FONT_GLYPH_FORMAT_A1;
    out->is_placeholder = false;
    out->gid.index = glyphId;
    return true;
}

const void* getGlyphBitmap(lv_font_glyph_dsc_t* descriptor,
                           lv_draw_buf_t* drawBuffer) {
    if (descriptor == nullptr || descriptor->gid.index == 0 ||
        drawBuffer == nullptr || drawBuffer->data == nullptr) {
        return nullptr;
    }
    const uint32_t index = descriptor->gid.index - 1U;
    if (index >= TINYLM_FONT_GLYPH_COUNT) {
        return nullptr;
    }
    const TinyLmBitmapGlyph& glyph = TINYLM_FONT_GLYPHS[index];
    const uint32_t stride = drawBuffer->header.stride;
    for (uint8_t row = 0; row < FONT_LINE_HEIGHT; ++row) {
        const uint16_t bits = static_cast<uint16_t>(
            (pgm_read_byte(&glyph.bitmap[row * 2U]) << 8U) |
            pgm_read_byte(&glyph.bitmap[row * 2U + 1U]));
        uint8_t* destination = drawBuffer->data + row * stride;
        for (uint8_t column = 0; column < FONT_BOX_WIDTH; ++column) {
            destination[column] =
                (bits & (1U << (15U - column))) ? 0xFFU : 0x00U;
        }
    }
    return drawBuffer;
}

lv_font_t makeFont() {
    lv_font_t result{};
    result.get_glyph_dsc = getGlyphDescriptor;
    result.get_glyph_bitmap = getGlyphBitmap;
    result.release_glyph = nullptr;
    result.line_height = FONT_LINE_HEIGHT;
    result.base_line = 0;
    result.subpx = LV_FONT_SUBPX_NONE;
    result.kerning = LV_FONT_KERNING_NONE;
    result.static_bitmap = 0;
    result.underline_position = -2;
    result.underline_thickness = 1;
    return result;
}

lv_font_t STORY_FONT = makeFont();

}  // namespace

namespace TinyLmFont {

const lv_font_t* font() {
    return &STORY_FONT;
}

bool supports(uint32_t codepoint) {
    return findGlyph(codepoint) != nullptr;
}

}  // namespace TinyLmFont
