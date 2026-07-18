#include "ui/BitmapFont.h"

#include "ui/BitmapFontData.h"

namespace {

const BitmapGlyph* findGlyph(uint32_t codepoint, BitmapFontSize size) {
    const BitmapGlyph* glyphs =
        size == BitmapFontSize::Small12 ? FONT12_GLYPHS
                                        : FONT_BOLD12_GLYPHS;
    const size_t count = size == BitmapFontSize::Small12
                             ? FONT12_GLYPH_COUNT
                             : FONT_BOLD12_GLYPH_COUNT;
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        const uint32_t value = pgm_read_dword(&glyphs[middle].codepoint);
        if (value < codepoint) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    if (low >= count || pgm_read_dword(&glyphs[low].codepoint) != codepoint) {
        return nullptr;
    }
    return &glyphs[low];
}

uint8_t fontColumns(BitmapFontSize size) {
    return size == BitmapFontSize::Small12 ? 12 : 16;
}

uint8_t fontRows(BitmapFontSize) {
    return 16;
}

}  // namespace

int16_t BitmapFont::textWidth(const char* utf8, BitmapFontSize size) const {
    int16_t width = 0;
    while (*utf8 != '\0') {
        const uint32_t codepoint = nextCodepoint(utf8);
        const BitmapGlyph* glyph = findGlyph(codepoint, size);
        width += glyph == nullptr ? fontColumns(size)
                                  : pgm_read_byte(&glyph->advance);
    }
    return width;
}

int16_t BitmapFont::lineHeight(BitmapFontSize size) const {
    return fontRows(size);
}

void BitmapFont::draw(TFT_eSPI& display, const char* utf8, int16_t x,
                      int16_t y, uint16_t color, BitmapFontSize size,
                      BitmapTextAlign align) const {
    const uint8_t columns = fontColumns(size);
    const uint8_t rows = fontRows(size);
    const int16_t width = textWidth(utf8, size);
    if (align == BitmapTextAlign::Center) {
        x -= width / 2;
    } else if (align == BitmapTextAlign::Right) {
        x -= width;
    }

    display.startWrite();
    while (*utf8 != '\0') {
        const uint32_t codepoint = nextCodepoint(utf8);
        const BitmapGlyph* glyph = findGlyph(codepoint, size);
        if (glyph == nullptr) {
            display.drawRect(x + 1, y + 1, columns - 2, rows - 2, color);
            x += columns;
            continue;
        }

        for (uint8_t row = 0; row < rows; ++row) {
            const uint16_t bits = static_cast<uint16_t>(
                (pgm_read_byte(&glyph->bitmap[row * 2U]) << 8U) |
                pgm_read_byte(&glyph->bitmap[row * 2U + 1U]));
            uint8_t column = 0;
            while (column < columns) {
                while (column < columns &&
                       (bits & (1U << (15U - column))) == 0) {
                    ++column;
                }
                const uint8_t runStart = column;
                while (column < columns &&
                       (bits & (1U << (15U - column))) != 0) {
                    ++column;
                }
                if (column > runStart) {
                    display.drawFastHLine(x + runStart, y + row,
                                          column - runStart, color);
                }
            }
        }
        x += pgm_read_byte(&glyph->advance);
    }
    display.endWrite();
}

uint32_t BitmapFont::nextCodepoint(const char*& cursor) {
    const uint8_t first = static_cast<uint8_t>(*cursor++);
    if (first < 0x80) {
        return first;
    }
    if ((first & 0xE0) == 0xC0) {
        const uint8_t second = static_cast<uint8_t>(*cursor++);
        return ((first & 0x1F) << 6U) | (second & 0x3F);
    }
    if ((first & 0xF0) == 0xE0) {
        const uint8_t second = static_cast<uint8_t>(*cursor++);
        const uint8_t third = static_cast<uint8_t>(*cursor++);
        return ((first & 0x0F) << 12U) | ((second & 0x3F) << 6U) |
               (third & 0x3F);
    }
    return '?';
}
