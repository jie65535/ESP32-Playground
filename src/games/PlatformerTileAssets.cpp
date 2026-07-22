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

bool platformerEnemySourceCreatesEntity(uint16_t sourceTileId) {
    if (sourceTileId >= PLATFORMER_ENEMY_TILE_COUNT) {
        return false;
    }
    switch (sourceTileId) {
        case 73U:
        case 79U:
        case 83U:
        case 85U:
        case 91U:
        case 490U:
        case 492U:
        case 496U:
            return false;
        default:
            break;
    }
    switch (PLATFORMER_ENEMY_REFERENCE_IDS[sourceTileId]) {
        case 38U:
        case 39U:
        case 40U:
        case 44U:
        case 48U:
        case 50U:
        case 56U:
        case 61U:
        case 70U:
        case 71U:
        case 81U:
        case 87U:
        case 90U:
        case 455U:
        case 498U:
        case 504U:
            return true;
        default:
            return false;
    }
}

}  // namespace pgos
