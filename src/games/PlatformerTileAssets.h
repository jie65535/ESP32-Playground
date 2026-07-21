#pragma once

#include <cstdint>

namespace pgos {

constexpr uint8_t PLATFORMER_SOURCE_TILE_SIZE = 16;
constexpr uint16_t PLATFORMER_BLOCK_TILE_COUNT = 48U * 22U;
constexpr uint16_t PLATFORMER_ENEMY_TILE_COUNT = 35U * 15U;
constexpr uint16_t PLATFORMER_PLAYER_TILE_COUNT = 25U * 16U;

struct PlatformerPackedTileSheet {
    const uint16_t* palette = nullptr;
    const uint8_t* pixels = nullptr;
    uint32_t pixelDataSize = 0;
    uint16_t tileCount = 0;
    uint8_t paletteSize = 0;
    uint8_t bitsPerPixel = 0;
};

extern const PlatformerPackedTileSheet PLATFORMER_BLOCK_TILES;
extern const PlatformerPackedTileSheet PLATFORMER_ENEMY_TILES;
extern const PlatformerPackedTileSheet PLATFORMER_PLAYER_TILES;
extern const uint16_t
    PLATFORMER_BLOCK_REFERENCE_IDS[PLATFORMER_BLOCK_TILE_COUNT];
extern const uint16_t
    PLATFORMER_ENEMY_REFERENCE_IDS[PLATFORMER_ENEMY_TILE_COUNT];

uint8_t platformerPackedTilePixel(const PlatformerPackedTileSheet& sheet,
                                  uint16_t tileId, uint8_t x, uint8_t y);

}  // namespace pgos
