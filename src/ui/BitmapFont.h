#pragma once

#include <stddef.h>
#include <stdint.h>

struct BitmapCanvas {
    uint16_t* pixels = nullptr;
    int16_t width = 0;
    int16_t height = 0;
    uint16_t stride = 0;
};

enum class BitmapFontSize : uint8_t {
    Small12,
    Bold12,
};

enum class BitmapTextAlign : uint8_t {
    Left,
    Center,
    Right,
};

class BitmapFont {
public:
    int16_t textWidth(const char* utf8, BitmapFontSize size) const;
    int16_t lineHeight(BitmapFontSize size) const;
    void draw(BitmapCanvas& canvas, const char* utf8, int16_t x, int16_t y,
              uint16_t color, BitmapFontSize size = BitmapFontSize::Bold12,
              BitmapTextAlign align = BitmapTextAlign::Left) const;

private:
    static uint32_t nextCodepoint(const char*& cursor);
};
