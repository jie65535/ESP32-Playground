#pragma once

#include <cstdint>

namespace pgos {

// Offline-converted image data in LVGL's RGB565A8 layout: all little-endian
// RGB565 bytes followed by the A8 opacity plane. Keeping the packed format
// independent from LVGL lets host tools validate assets without pulling the
// device renderer into the test binary or duplicating each pixel plane.
struct PixelSprite {
    const char* name = nullptr;
    uint16_t width = 0;
    uint16_t height = 0;
    const uint8_t* imageData = nullptr;
    uint32_t imageDataSize = 0;
    // Opaque RGB565 assets (such as the precomposed level strip) omit the A8
    // plane and use LVGL's plain RGB565 decoder path.
    bool opaque = false;

    constexpr uint32_t pixelCount() const {
        return static_cast<uint32_t>(width) * height;
    }

    constexpr uint32_t expectedDataSize() const {
        return pixelCount() * (opaque ? 2U : 3U);
    }

    constexpr bool valid() const {
        return width != 0 && height != 0 && imageData != nullptr &&
               imageDataSize >= expectedDataSize();
    }

    uint16_t pixelAt(uint32_t index) const {
        const uint32_t offset = index * 2U;
        return static_cast<uint16_t>(imageData[offset]) |
               static_cast<uint16_t>(
                   static_cast<uint16_t>(imageData[offset + 1U]) << 8U);
    }

    uint8_t opacityAt(uint32_t index) const {
        return opaque ? 0xFFU : imageData[pixelCount() * 2U + index];
    }
};

}  // namespace pgos
