#include "games/PlatformerTileRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace pgos {
namespace {

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red * 31U / 255U) << 11U) |
                                 ((green * 63U / 255U) << 5U) |
                                 (blue * 31U / 255U));
}

constexpr uint16_t SKY_BLUE = rgb565(92, 148, 252);
constexpr uint16_t PIXELS_PER_TILE =
    PLATFORMER_SOURCE_TILE_SIZE * PLATFORMER_SOURCE_TILE_SIZE;

struct BackgroundAnchor {
    int32_t verticalDistance = 0;
    int32_t horizontalDistance = 0;
    PlatformerBackgroundColor color = PlatformerBackgroundColor::Black;
};

void considerBackgroundAnchor(BackgroundAnchor& best, int32_t cameraColumn,
                              int32_t cameraRow,
                              const PlatformerPoint& anchor,
                              PlatformerBackgroundColor color) {
    const int32_t verticalDistance = std::abs(cameraRow - anchor.y);
    const int32_t horizontalDistance = std::abs(cameraColumn - anchor.x);
    if (verticalDistance < best.verticalDistance ||
        (verticalDistance == best.verticalDistance &&
         horizontalDistance < best.horizontalDistance)) {
        best = {verticalDistance, horizontalDistance, color};
    }
}

bool skipBlockTile(PlatformerMapLayer layer, uint16_t tileId,
                   bool runtimeRender) {
    if (tileId >= PLATFORMER_BLOCK_TILE_COUNT) {
        return true;
    }
    const uint16_t reference = PLATFORMER_BLOCK_REFERENCE_IDS[tileId];
    if (layer == PlatformerMapLayer::Background &&
        (reference == 391U || reference == 393U)) {
        return true;
    }
    if (runtimeRender && reference == 152U) {
        return true;
    }
    return reference == 394U || reference == 762U || reference == 810U ||
           reference == 811U || reference == 858U || reference == 859U;
}

void drawCachedOrPacked(const PlatformerPackedTileSheet& source,
                        const PlatformerDecodedTileSheet* cache,
                        uint16_t tileId, int16_t x, int16_t y,
                        uint16_t* target, uint16_t width, uint16_t height,
                        uint16_t stride, bool flipX = false) {
    if (cache != nullptr && cache->source == &source) {
        PlatformerTileRenderer::drawTile(*cache, tileId, x, y, target, width,
                                         height, stride, flipX);
    } else {
        PlatformerTileRenderer::drawTile(source, tileId, x, y, target, width,
                                         height, stride, flipX);
    }
}

void fillBackground(uint16_t color, uint16_t* target, uint16_t width,
                    uint16_t height, uint16_t stride) {
    for (uint16_t row = 0; row < height; ++row) {
        std::fill_n(target + static_cast<size_t>(row) * stride, width, color);
    }
}

void drawLayer(const PlatformerCampaignLevel& level, PlatformerMapLayer layer,
               int32_t cameraX, int32_t cameraY, uint16_t* target,
               uint16_t width, uint16_t height, uint16_t stride,
               const PlatformerLevelRuntime* runtime,
               const PlatformerDecodedTileSheet* blockCache) {
    constexpr int32_t TILE = PLATFORMER_SOURCE_TILE_SIZE;
    const int32_t firstColumn = std::max<int32_t>(0, cameraX / TILE);
    const int32_t firstRow = std::max<int32_t>(0, cameraY / TILE);
    const int32_t lastColumn = std::min<int32_t>(
        level.width, (cameraX + static_cast<int32_t>(width) + TILE - 1) / TILE);
    const int32_t lastRow = std::min<int32_t>(
        level.height, (cameraY + static_cast<int32_t>(height) + TILE - 1) / TILE);
    const PlatformerTileLayer& data =
        level.layers[static_cast<size_t>(layer)];
    if (data.rowOffsets == nullptr || data.runs == nullptr) {
        return;
    }

    for (int32_t row = firstRow; row < lastRow; ++row) {
        uint16_t cursor = data.rowOffsets[row];
        const uint16_t end = data.rowOffsets[row + 1];
        if (cursor > end || end > data.runDataSize) {
            continue;
        }
        int32_t column = 0;
        while (cursor + 3U <= end && column < lastColumn) {
            const uint8_t runLength = data.runs[cursor];
            uint16_t tileId = static_cast<uint16_t>(data.runs[cursor + 1U]) |
                              static_cast<uint16_t>(data.runs[cursor + 2U])
                                  << 8U;
            cursor = static_cast<uint16_t>(cursor + 3U);
            if (runLength == 0U) {
                break;
            }
            const int32_t runEnd = column + runLength;
            const int32_t drawFirst = std::max(column, firstColumn);
            const int32_t drawLast = std::min(runEnd, lastColumn);
            for (int32_t drawColumn = drawFirst; drawColumn < drawLast;
                 ++drawColumn) {
                uint16_t drawTileId = tileId;
                if (runtime != nullptr && drawTileId < PLATFORMER_BLOCK_TILE_COUNT) {
                    const uint16_t reference =
                        PLATFORMER_BLOCK_REFERENCE_IDS[drawTileId];
                    if (reference == 761U || reference == 809U) {
                        continue;
                    }
                }
                if (runtime != nullptr &&
                    (layer == PlatformerMapLayer::Underground ||
                     layer == PlatformerMapLayer::Foreground)) {
                    const PlatformerTileModification* change =
                        runtime->modificationAt(
                            static_cast<uint16_t>(drawColumn),
                            static_cast<uint8_t>(row));
                    if (change != nullptr) {
                        if (change->state ==
                            PlatformerTileModificationState::Broken) {
                            continue;
                        }
                        if (drawTileId == PLATFORMER_EMPTY_TILE) {
                            drawTileId = 196U;
                        } else {
                            const uint16_t sourceX = drawTileId % 48U;
                            const uint16_t sourceY = drawTileId / 48U;
                            drawTileId = sourceY > 10U ? 724U
                                         : sourceX >= 32U ? 228U
                                         : sourceX >= 16U ? 212U
                                                          : 196U;
                        }
                    }
                }
                if (drawTileId == PLATFORMER_EMPTY_TILE ||
                    skipBlockTile(layer, drawTileId, runtime != nullptr)) {
                    continue;
                }
                drawCachedOrPacked(
                    PLATFORMER_BLOCK_TILES, blockCache, drawTileId,
                    static_cast<int16_t>(drawColumn * TILE - cameraX),
                    static_cast<int16_t>(row * TILE - cameraY), target, width,
                    height, stride);
            }
            column = runEnd;
        }
    }
}

void drawEnemies(const PlatformerCampaignLevel& level, int32_t cameraX,
                 int32_t cameraY, uint16_t* target, uint16_t width,
                 uint16_t height, uint16_t stride,
                 const PlatformerDecodedTileSheet* enemyCache) {
    constexpr int32_t TILE = PLATFORMER_SOURCE_TILE_SIZE;
    const int32_t firstColumn = std::max<int32_t>(0, cameraX / TILE);
    const int32_t firstRow = std::max<int32_t>(0, cameraY / TILE);
    const int32_t lastColumn = std::min<int32_t>(
        level.width, (cameraX + static_cast<int32_t>(width) + TILE - 1) / TILE);
    const int32_t lastRow = std::min<int32_t>(
        level.height, (cameraY + static_cast<int32_t>(height) + TILE - 1) / TILE);
    const PlatformerTileLayer& data =
        level.layers[static_cast<size_t>(PlatformerMapLayer::Enemies)];
    if (data.rowOffsets == nullptr || data.runs == nullptr) {
        return;
    }
    for (int32_t row = firstRow; row < lastRow; ++row) {
        uint16_t cursor = data.rowOffsets[row];
        const uint16_t end = data.rowOffsets[row + 1];
        int32_t column = 0;
        if (cursor > end || end > data.runDataSize) {
            continue;
        }
        while (cursor + 3U <= end && column < lastColumn) {
            const uint8_t runLength = data.runs[cursor];
            const uint16_t tileId =
                static_cast<uint16_t>(data.runs[cursor + 1U]) |
                static_cast<uint16_t>(data.runs[cursor + 2U]) << 8U;
            cursor = static_cast<uint16_t>(cursor + 3U);
            if (runLength == 0U) {
                break;
            }
            const int32_t runEnd = column + runLength;
            if (tileId < PLATFORMER_ENEMY_TILE_COUNT && tileId != 73U &&
                tileId != 79U && tileId != 83U && tileId != 85U &&
                tileId != 91U && tileId != 490U && tileId != 492U &&
                tileId != 496U) {
                for (int32_t drawColumn = std::max(column, firstColumn);
                     drawColumn < std::min(runEnd, lastColumn); ++drawColumn) {
                    drawCachedOrPacked(
                        PLATFORMER_ENEMY_TILES, enemyCache, tileId,
                        static_cast<int16_t>(drawColumn * TILE - cameraX),
                        static_cast<int16_t>(row * TILE - cameraY), target,
                        width, height, stride);
                }
            }
            column = runEnd;
        }
    }
}

}  // namespace

PlatformerBackgroundColor PlatformerTileRenderer::backgroundAt(
    const PlatformerCampaignLevel& level, int32_t cameraX, int32_t cameraY) {
    constexpr int32_t TILE = PLATFORMER_SOURCE_TILE_SIZE;
    const int32_t cameraColumn = std::max<int32_t>(0, cameraX / TILE);
    const int32_t cameraRow = std::max<int32_t>(0, cameraY / TILE);
    BackgroundAnchor best{std::abs(cameraRow - level.cameraStart.y),
                          std::abs(cameraColumn - level.cameraStart.x),
                          level.background};
    for (uint8_t index = 0; index < level.warps.count; ++index) {
        const PlatformerWarpData& warp =
            PLATFORMER_CAMPAIGN_WARPS[level.warps.offset + index];
        if (warp.destinationWorld == 0U && warp.destinationStage == 0U) {
            considerBackgroundAnchor(best, cameraColumn, cameraRow, warp.camera,
                                     warp.background);
        }
    }
    for (uint8_t index = 0; index < level.vines.count; ++index) {
        const PlatformerVineData& vine =
            PLATFORMER_CAMPAIGN_VINES[level.vines.offset + index];
        considerBackgroundAnchor(best, cameraColumn, cameraRow, vine.camera,
                                 vine.background);
    }
    return best.color;
}

void PlatformerTileRenderer::render(
    const PlatformerCampaignLevel& level, int32_t cameraX, int32_t cameraY,
    uint16_t* target, uint16_t width, uint16_t height, uint16_t stride,
    bool showEnemySpawns, const PlatformerDecodedTileSheet* blockCache,
    const PlatformerDecodedTileSheet* enemyCache) {
    if (target == nullptr || width == 0U || height == 0U || stride < width) {
        return;
    }
    fillBackground(backgroundAt(level, cameraX, cameraY) ==
                           PlatformerBackgroundColor::Blue
                       ? SKY_BLUE
                       : 0U,
                   target, width, height, stride);
    drawLayer(level, PlatformerMapLayer::Background, cameraX, cameraY, target,
              width, height, stride, nullptr, blockCache);
    drawLayer(level, PlatformerMapLayer::Underground, cameraX, cameraY, target,
              width, height, stride, nullptr, blockCache);
    drawLayer(level, PlatformerMapLayer::Foreground, cameraX, cameraY, target,
              width, height, stride, nullptr, blockCache);
    if (showEnemySpawns) {
        drawEnemies(level, cameraX, cameraY, target, width, height, stride,
                    enemyCache);
    }
    drawLayer(level, PlatformerMapLayer::AboveForeground, cameraX, cameraY,
              target, width, height, stride, nullptr, blockCache);
}

void PlatformerTileRenderer::render(
    const PlatformerLevelRuntime& runtime, int32_t cameraX, int32_t cameraY,
    uint16_t* target, uint16_t width, uint16_t height, uint16_t stride,
    bool showEnemySpawns, const PlatformerDecodedTileSheet* blockCache,
    const PlatformerDecodedTileSheet* enemyCache) {
    renderBase(runtime, cameraX, cameraY, target, width, height, stride,
               blockCache);
    const PlatformerCampaignLevel* level = runtime.level();
    if (level == nullptr) {
        return;
    }
    if (showEnemySpawns) {
        drawEnemies(*level, cameraX, cameraY, target, width, height, stride,
                    enemyCache);
    }
    drawAboveForeground(runtime, cameraX, cameraY, target, width, height,
                        stride, blockCache);
}

void PlatformerTileRenderer::renderBase(
    const PlatformerLevelRuntime& runtime, int32_t cameraX, int32_t cameraY,
    uint16_t* target, uint16_t width, uint16_t height, uint16_t stride,
    const PlatformerDecodedTileSheet* blockCache) {
    const PlatformerCampaignLevel* level = runtime.level();
    if (level == nullptr || target == nullptr || width == 0U || height == 0U ||
        stride < width) {
        return;
    }
    fillBackground(runtime.activeBackground() ==
                           PlatformerBackgroundColor::Blue
                       ? SKY_BLUE
                       : 0U,
                   target, width, height, stride);
    drawLayer(*level, PlatformerMapLayer::Background, cameraX, cameraY, target,
              width, height, stride, &runtime, blockCache);
    drawLayer(*level, PlatformerMapLayer::Underground, cameraX, cameraY, target,
              width, height, stride, &runtime, blockCache);
    drawLayer(*level, PlatformerMapLayer::Foreground, cameraX, cameraY, target,
              width, height, stride, &runtime, blockCache);
}

void PlatformerTileRenderer::drawAboveForeground(
    const PlatformerLevelRuntime& runtime, int32_t cameraX, int32_t cameraY,
    uint16_t* target, uint16_t width, uint16_t height, uint16_t stride,
    const PlatformerDecodedTileSheet* blockCache) {
    const PlatformerCampaignLevel* level = runtime.level();
    if (level != nullptr) {
        drawLayer(*level, PlatformerMapLayer::AboveForeground, cameraX,
                  cameraY, target, width, height, stride, &runtime,
                  blockCache);
    }
}

void PlatformerTileRenderer::drawTile(
    const PlatformerPackedTileSheet& sheet, uint16_t tileId, int16_t x,
    int16_t y, uint16_t* target, uint16_t width, uint16_t height,
    uint16_t stride, bool flipX) {
    if (target == nullptr || sheet.palette == nullptr ||
        tileId >= sheet.tileCount) {
        return;
    }
    for (uint8_t sourceY = 0; sourceY < PLATFORMER_SOURCE_TILE_SIZE;
         ++sourceY) {
        const int16_t destinationY = static_cast<int16_t>(y + sourceY);
        if (destinationY < 0 || destinationY >= static_cast<int16_t>(height)) {
            continue;
        }
        for (uint8_t sourceX = 0; sourceX < PLATFORMER_SOURCE_TILE_SIZE;
             ++sourceX) {
            const int16_t destinationX = static_cast<int16_t>(x + sourceX);
            if (destinationX < 0 || destinationX >= static_cast<int16_t>(width)) {
                continue;
            }
            const uint8_t sampleX = flipX
                                        ? PLATFORMER_SOURCE_TILE_SIZE - 1U -
                                              sourceX
                                        : sourceX;
            const uint8_t paletteIndex =
                platformerPackedTilePixel(sheet, tileId, sampleX, sourceY);
            if (paletteIndex != 0U && paletteIndex < sheet.paletteSize) {
                target[static_cast<size_t>(destinationY) * stride +
                       destinationX] = sheet.palette[paletteIndex];
            }
        }
    }
}

void PlatformerTileRenderer::drawTile(
    const PlatformerDecodedTileSheet& sheet, uint16_t tileId, int16_t x,
    int16_t y, uint16_t* target, uint16_t width, uint16_t height,
    uint16_t stride, bool flipX) {
    if (sheet.source == nullptr || sheet.pixels == nullptr || target == nullptr ||
        sheet.source->palette == nullptr || tileId >= sheet.source->tileCount ||
        sheet.pixelCount < (static_cast<size_t>(tileId) + 1U) *
                               PIXELS_PER_TILE) {
        return;
    }
    const uint8_t* tile =
        sheet.pixels + static_cast<size_t>(tileId) * PIXELS_PER_TILE;
    for (uint8_t sourceY = 0; sourceY < PLATFORMER_SOURCE_TILE_SIZE;
         ++sourceY) {
        const int16_t destinationY = static_cast<int16_t>(y + sourceY);
        if (destinationY < 0 || destinationY >= static_cast<int16_t>(height)) {
            continue;
        }
        const uint8_t* sourceRow = tile + sourceY * PLATFORMER_SOURCE_TILE_SIZE;
        uint16_t* destination =
            target + static_cast<size_t>(destinationY) * stride;
        for (uint8_t sourceX = 0; sourceX < PLATFORMER_SOURCE_TILE_SIZE;
             ++sourceX) {
            const int16_t destinationX = static_cast<int16_t>(x + sourceX);
            if (destinationX < 0 || destinationX >= static_cast<int16_t>(width)) {
                continue;
            }
            const uint8_t sampleX = flipX
                                        ? PLATFORMER_SOURCE_TILE_SIZE - 1U -
                                              sourceX
                                        : sourceX;
            const uint8_t paletteIndex = sourceRow[sampleX];
            if (paletteIndex != 0U &&
                paletteIndex < sheet.source->paletteSize) {
                destination[destinationX] =
                    sheet.source->palette[paletteIndex];
            }
        }
    }
}

size_t PlatformerTileRenderer::decodedPixelCount(
    const PlatformerPackedTileSheet& sheet) {
    return static_cast<size_t>(sheet.tileCount) * PIXELS_PER_TILE;
}

bool PlatformerTileRenderer::decode(const PlatformerPackedTileSheet& sheet,
                                    uint8_t* target, size_t targetSize,
                                    PlatformerDecodedTileSheet& decoded) {
    const size_t required = decodedPixelCount(sheet);
    decoded = {};
    if (target == nullptr || targetSize < required || sheet.pixels == nullptr) {
        return false;
    }
    for (uint16_t tileId = 0; tileId < sheet.tileCount; ++tileId) {
        uint8_t* tile = target + static_cast<size_t>(tileId) * PIXELS_PER_TILE;
        for (uint8_t y = 0; y < PLATFORMER_SOURCE_TILE_SIZE; ++y) {
            for (uint8_t x = 0; x < PLATFORMER_SOURCE_TILE_SIZE; ++x) {
                tile[static_cast<size_t>(y) * PLATFORMER_SOURCE_TILE_SIZE + x] =
                    platformerPackedTilePixel(sheet, tileId, x, y);
            }
        }
    }
    decoded = {&sheet, target, required};
    return true;
}

}  // namespace pgos
