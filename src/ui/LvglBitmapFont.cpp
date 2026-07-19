#include "ui/LvglBitmapFont.h"

#include "ui/BitmapFontData.h"

namespace {

constexpr uint8_t FONT_LINE_HEIGHT = 16;

struct FontFace {
    const BitmapGlyph* glyphs;
    size_t count;
    uint8_t boxWidth;
};

const FontFace SMALL_FACE = {
    FONT12_GLYPHS,
    FONT12_GLYPH_COUNT,
    12,
};

const FontFace BOLD_FACE = {
    FONT_BOLD12_GLYPHS,
    FONT_BOLD12_GLYPH_COUNT,
    16,
};

const BitmapGlyph* findGlyph(const FontFace& face, uint32_t codepoint,
                             uint32_t* indexOut = nullptr) {
    size_t low = 0;
    size_t high = face.count;
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        const uint32_t value = pgm_read_dword(&face.glyphs[middle].codepoint);
        if (value < codepoint) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }

    if (low >= face.count ||
        pgm_read_dword(&face.glyphs[low].codepoint) != codepoint) {
        return nullptr;
    }

    if (indexOut != nullptr) {
        // Zero is reserved for an invalid LVGL glyph id.
        *indexOut = static_cast<uint32_t>(low + 1U);
    }
    return &face.glyphs[low];
}

const FontFace* faceFromFont(const lv_font_t* font) {
    return font == nullptr
               ? nullptr
               : static_cast<const FontFace*>(font->dsc);
}

const BitmapGlyph* glyphFromDescriptor(const lv_font_glyph_dsc_t* descriptor,
                                       const FontFace** faceOut = nullptr) {
    if (descriptor == nullptr || descriptor->resolved_font == nullptr ||
        descriptor->gid.index == 0) {
        return nullptr;
    }

    const FontFace* face = faceFromFont(descriptor->resolved_font);
    if (face == nullptr) {
        return nullptr;
    }

    const uint32_t index = descriptor->gid.index - 1U;
    if (index >= face->count) {
        return nullptr;
    }

    if (faceOut != nullptr) {
        *faceOut = face;
    }
    return &face->glyphs[index];
}

bool getGlyphDescriptor(const lv_font_t* font, lv_font_glyph_dsc_t* out,
                        uint32_t unicode, uint32_t) {
    const FontFace* face = faceFromFont(font);
    if (face == nullptr || out == nullptr) {
        return false;
    }

    uint32_t glyphId = 0;
    const BitmapGlyph* glyph = findGlyph(*face, unicode, &glyphId);
    if (glyph == nullptr) {
        // Returning false lets an optional lv_font_t fallback handle the
        // character.  The current PGOS subset deliberately stays small.
        return false;
    }

    out->adv_w = pgm_read_byte(&glyph->advance);
    out->box_w = face->boxWidth;
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
    const FontFace* face = nullptr;
    const BitmapGlyph* glyph = glyphFromDescriptor(descriptor, &face);
    if (glyph == nullptr || face == nullptr || drawBuffer == nullptr ||
        drawBuffer->data == nullptr) {
        return nullptr;
    }

    const uint32_t stride = drawBuffer->header.stride;
    for (uint8_t row = 0; row < FONT_LINE_HEIGHT; ++row) {
        const uint16_t bits = static_cast<uint16_t>(
            (pgm_read_byte(&glyph->bitmap[row * 2U]) << 8U) |
            pgm_read_byte(&glyph->bitmap[row * 2U + 1U]));
        uint8_t* destination = drawBuffer->data + row * stride;
        for (uint8_t column = 0; column < face->boxWidth; ++column) {
            destination[column] =
                (bits & (1U << (15U - column))) ? 0xFFU : 0x00U;
        }
    }

    return drawBuffer;
}

lv_font_t makeFont(const FontFace* face) {
    lv_font_t result{};
    result.get_glyph_dsc = getGlyphDescriptor;
    result.get_glyph_bitmap = getGlyphBitmap;
    result.release_glyph = nullptr;
    result.line_height = FONT_LINE_HEIGHT;
    // BitmapFont's existing 16-pixel line box is top-origin aligned.  A zero
    // LVGL baseline keeps the glyph bitmap at the top of that same line box.
    result.base_line = 0;
    result.subpx = LV_FONT_SUBPX_NONE;
    result.kerning = LV_FONT_KERNING_NONE;
    result.static_bitmap = 0;
    result.underline_position = -2;
    result.underline_thickness = 1;
    result.dsc = face;
    result.fallback = nullptr;
    result.user_data = nullptr;
    return result;
}

lv_font_t SMALL_FONT = makeFont(&SMALL_FACE);
lv_font_t BOLD_FONT = makeFont(&BOLD_FACE);

}  // namespace

namespace PgosBitmapFont {

const lv_font_t* font(BitmapFontSize size) {
    return size == BitmapFontSize::Small12 ? &SMALL_FONT : &BOLD_FONT;
}

}  // namespace PgosBitmapFont
