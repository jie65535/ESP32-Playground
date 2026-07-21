#include "games/PlatformerCampaignData.h"
#include "games/PlatformerTileAssets.h"
#include "games/PlatformerTileRenderer.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace pgos;

namespace {

constexpr uint16_t WIDTH = 320;
constexpr uint16_t HEIGHT = 218;
uint16_t frame[static_cast<size_t>(WIDTH) * HEIGHT] = {};

uint32_t checksum() {
    uint32_t value = 2166136261U;
    for (uint16_t pixel : frame) {
        value ^= pixel;
        value *= 16777619U;
    }
    return value;
}

}  // namespace

int main(int argc, char** argv) {
    assert(PLATFORMER_BLOCK_TILES.tileCount == PLATFORMER_BLOCK_TILE_COUNT);
    assert(PLATFORMER_BLOCK_TILES.paletteSize == 26);
    assert(PLATFORMER_BLOCK_TILES.bitsPerPixel == 5);
    assert(PLATFORMER_ENEMY_TILES.tileCount == PLATFORMER_ENEMY_TILE_COUNT);
    assert(PLATFORMER_ENEMY_TILES.paletteSize == 15);
    assert(PLATFORMER_ENEMY_TILES.bitsPerPixel == 4);
    assert(PLATFORMER_PLAYER_TILES.tileCount == PLATFORMER_PLAYER_TILE_COUNT);
    assert(PLATFORMER_PLAYER_TILES.paletteSize == 18);
    assert(PLATFORMER_PLAYER_TILES.bitsPerPixel == 5);

    assert(PLATFORMER_BLOCK_REFERENCE_IDS[592] == 337);
    assert(PLATFORMER_BLOCK_REFERENCE_IDS[593] == 101);
    assert(PLATFORMER_ENEMY_REFERENCE_IDS[38] == 38);
    assert(PLATFORMER_ENEMY_REFERENCE_IDS[143] == 38);

    const PlatformerCampaignLevel* first = platformerCampaignLevel(1, 1);
    assert(first != nullptr);
    assert(PlatformerTileRenderer::backgroundAt(*first, 0, 0) ==
           PlatformerBackgroundColor::Blue);
    PlatformerTileRenderer::render(*first, 0, 0, frame, WIDTH, HEIGHT,
                                   WIDTH, false);
    const uint32_t firstChecksum = checksum();
    std::vector<uint8_t> blockPixels(
        PlatformerTileRenderer::decodedPixelCount(PLATFORMER_BLOCK_TILES));
    std::vector<uint8_t> enemyPixels(
        PlatformerTileRenderer::decodedPixelCount(PLATFORMER_ENEMY_TILES));
    PlatformerDecodedTileSheet blockCache;
    PlatformerDecodedTileSheet enemyCache;
    assert(PlatformerTileRenderer::decode(
        PLATFORMER_BLOCK_TILES, blockPixels.data(), blockPixels.size(),
        blockCache));
    assert(PlatformerTileRenderer::decode(
        PLATFORMER_ENEMY_TILES, enemyPixels.data(), enemyPixels.size(),
        enemyCache));
    PlatformerTileRenderer::render(*first, 0, 0, frame, WIDTH, HEIGHT,
                                   WIDTH, false, &blockCache, &enemyCache);
    assert(checksum() == firstChecksum);
    PlatformerTileRenderer::render(*first, 256, 0, frame, WIDTH, HEIGHT,
                                   WIDTH, false);
    const uint32_t enemyAreaChecksum = checksum();
    PlatformerTileRenderer::render(*first, 256, 0, frame, WIDTH, HEIGHT,
                                   WIDTH, true);
    const uint32_t firstWithEnemiesChecksum = checksum();
    assert(enemyAreaChecksum != firstWithEnemiesChecksum);
    PlatformerTileRenderer::render(*first, 256, 0, frame, WIDTH, HEIGHT,
                                   WIDTH, true, &blockCache, &enemyCache);
    assert(checksum() == firstWithEnemiesChecksum);

    const PlatformerCampaignLevel* finalLevel = platformerCampaignLevel(8, 4);
    assert(finalLevel != nullptr);
    PlatformerTileRenderer::render(*finalLevel, 248 * 16, 18 * 16, frame,
                                   WIDTH, HEIGHT, WIDTH, true);
    const uint32_t underwaterChecksum = checksum();
    if (argc > 1 && std::strcmp(argv[1], "--probe") == 0) {
        std::printf("%u %u %u %u\n", firstChecksum, enemyAreaChecksum,
                    firstWithEnemiesChecksum, underwaterChecksum);
    }
    assert(underwaterChecksum != firstChecksum);
    assert(firstChecksum == 732514888U);
    assert(enemyAreaChecksum == 935641337U);
    assert(firstWithEnemiesChecksum == 3000685141U);
    assert(underwaterChecksum == 3948382147U);

    const PlatformerCampaignLevel* underground =
        platformerCampaignLevel(1, 2);
    assert(underground != nullptr);
    assert(PlatformerTileRenderer::backgroundAt(*underground, 0, 0) ==
           PlatformerBackgroundColor::Blue);
    assert(PlatformerTileRenderer::backgroundAt(*underground, 0, 13 * 16) ==
           PlatformerBackgroundColor::Black);
    PlatformerTileRenderer::render(*underground, 0, 13 * 16, frame, WIDTH,
                                   HEIGHT, WIDTH, false);
    assert(frame[static_cast<size_t>(100) * WIDTH + 100] == 0U);

    return 0;
}
