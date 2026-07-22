#pragma once

#include "games/PlatformerCampaignData.h"
#include "games/PlatformerLevelRuntime.h"
#include "games/PlatformerTileAssets.h"

#include <cstdint>
#include <cstddef>

namespace pgos {

struct PlatformerDecodedTileSheet {
    const PlatformerPackedTileSheet* source = nullptr;
    const uint8_t* pixels = nullptr;
    size_t pixelCount = 0;
};

class PlatformerTileRenderer final {
public:
    PlatformerTileRenderer() = delete;

    static void render(const PlatformerCampaignLevel& level,
                       int32_t cameraX, int32_t cameraY, uint16_t* target,
                       uint16_t width, uint16_t height, uint16_t stride,
                       bool showEnemySpawns = false,
                       const PlatformerDecodedTileSheet* blockCache = nullptr,
                       const PlatformerDecodedTileSheet* enemyCache = nullptr);
    static void render(const PlatformerLevelRuntime& runtime,
                       int32_t cameraX, int32_t cameraY, uint16_t* target,
                       uint16_t width, uint16_t height, uint16_t stride,
                       bool showEnemySpawns = false,
                       const PlatformerDecodedTileSheet* blockCache = nullptr,
                       const PlatformerDecodedTileSheet* enemyCache = nullptr);

    static void renderBase(
        const PlatformerLevelRuntime& runtime, int32_t cameraX,
        int32_t cameraY, uint16_t* target, uint16_t width, uint16_t height,
        uint16_t stride,
        const PlatformerDecodedTileSheet* blockCache = nullptr);
    static void drawAboveForeground(
        const PlatformerLevelRuntime& runtime, int32_t cameraX,
        int32_t cameraY, uint16_t* target, uint16_t width, uint16_t height,
        uint16_t stride,
        const PlatformerDecodedTileSheet* blockCache = nullptr);

    static PlatformerBackgroundColor backgroundAt(
        const PlatformerCampaignLevel& level, int32_t cameraX,
        int32_t cameraY);

    static void drawTile(const PlatformerPackedTileSheet& sheet,
                         uint16_t tileId, int16_t x, int16_t y,
                         uint16_t* target, uint16_t width, uint16_t height,
                         uint16_t stride, bool flipX = false,
                         bool flipY = false);
    static void drawTile(const PlatformerDecodedTileSheet& sheet,
                         uint16_t tileId, int16_t x, int16_t y,
                         uint16_t* target, uint16_t width, uint16_t height,
                         uint16_t stride, bool flipX = false,
                         bool flipY = false);

    static size_t decodedPixelCount(const PlatformerPackedTileSheet& sheet);
    static bool decode(const PlatformerPackedTileSheet& sheet,
                       uint8_t* target, size_t targetSize,
                       PlatformerDecodedTileSheet& decoded);
};

}  // namespace pgos
