#include "games/PlatformerTileAssets.h"

namespace pgos {

uint8_t platformerPackedTilePixel(const PlatformerPackedTileSheet& sheet,
                                  uint16_t tileId, uint8_t x, uint8_t y) {
    if (sheet.pixels == nullptr || tileId >= sheet.tileCount ||
        x >= PLATFORMER_SOURCE_TILE_SIZE || y >= PLATFORMER_SOURCE_TILE_SIZE ||
        sheet.bitsPerPixel == 0U || sheet.bitsPerPixel > 8U) {
        return 0;
    }

    constexpr uint16_t PIXELS_PER_TILE =
        PLATFORMER_SOURCE_TILE_SIZE * PLATFORMER_SOURCE_TILE_SIZE;
    const uint32_t pixel = static_cast<uint32_t>(tileId) * PIXELS_PER_TILE +
                           static_cast<uint16_t>(y) * PLATFORMER_SOURCE_TILE_SIZE + x;
    const uint32_t bit = pixel * sheet.bitsPerPixel;
    const uint32_t byte = bit >> 3U;
    if (byte >= sheet.pixelDataSize) {
        return 0;
    }
    uint16_t packed = sheet.pixels[byte];
    if ((bit & 7U) + sheet.bitsPerPixel > 8U) {
        if (byte + 1U >= sheet.pixelDataSize) {
            return 0;
        }
        packed |= static_cast<uint16_t>(sheet.pixels[byte + 1U]) << 8U;
    }
    const uint8_t mask = static_cast<uint8_t>((1U << sheet.bitsPerPixel) - 1U);
    return static_cast<uint8_t>((packed >> (bit & 7U)) & mask);
}

}  // namespace pgos
