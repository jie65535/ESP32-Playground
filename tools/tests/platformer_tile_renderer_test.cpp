#include "games/PlatformerCampaignData.h"
#include "games/PlatformerTileAssets.h"
#include "games/PlatformerTileRenderer.h"

#include <algorithm>
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
    assert(platformerEnemySourceCreatesEntity(38U));
    assert(platformerEnemySourceCreatesEntity(143U));
    assert(!platformerEnemySourceCreatesEntity(75U));
    assert(!platformerEnemySourceCreatesEntity(184U));
    assert(!platformerEnemySourceCreatesEntity(492U));

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

    PlatformerLevelRuntime trampolineRuntime;
    assert(trampolineRuntime.load(2, 1));
    const PlatformerTrampolineRuntimeState* trampoline =
        trampolineRuntime.trampoline(0U);
    assert(trampoline != nullptr);
    const int32_t trampolineCameraX = std::max<int32_t>(
        0, static_cast<int32_t>(trampoline->column) * 16 - WIDTH / 2);
    const int32_t trampolineCameraY = std::max<int32_t>(
        0, static_cast<int32_t>(trampoline->row) * 16 - HEIGHT / 2);
    PlatformerTileRenderer::renderBase(
        trampolineRuntime, trampolineCameraX, trampolineCameraY, frame,
        WIDTH, HEIGHT, WIDTH, &blockCache);
    const uint32_t extendedTrampolineChecksum = checksum();
    assert(trampolineRuntime.setTrampolineState(0U, 1U, 1U, true));
    PlatformerTileRenderer::renderBase(
        trampolineRuntime, trampolineCameraX, trampolineCameraY, frame,
        WIDTH, HEIGHT, WIDTH, &blockCache);
    assert(checksum() != extendedTrampolineChecksum);

    PlatformerLevelRuntime bumpRuntime;
    assert(bumpRuntime.load(1, 1));
    assert(bumpRuntime.hitBlock(16, 9, false).accepted);
    PlatformerTileRenderer::renderBase(bumpRuntime, 192, 64, frame, WIDTH,
                                       HEIGHT, WIDTH, &blockCache);
    const uint32_t usedBlockChecksum = checksum();
    assert(bumpRuntime.startBlockBump(16, 9));
    bumpRuntime.updateBlockBumps();
    PlatformerTileRenderer::renderBase(bumpRuntime, 192, 64, frame, WIDTH,
                                       HEIGHT, WIDTH, &blockCache);
    assert(checksum() != usedBlockChecksum);
    for (uint8_t step = 1U; step < 8U; ++step) {
        bumpRuntime.updateBlockBumps();
    }
    PlatformerTileRenderer::renderBase(bumpRuntime, 192, 64, frame, WIDTH,
                                       HEIGHT, WIDTH, &blockCache);
    assert(checksum() == usedBlockChecksum);

    uint16_t normalTile[16U * 16U] = {};
    uint16_t flippedPacked[16U * 16U] = {};
    uint16_t flippedDecoded[16U * 16U] = {};
    PlatformerTileRenderer::drawTile(PLATFORMER_ENEMY_TILES, 38U, 0, 0,
                                     normalTile, 16U, 16U, 16U);
    PlatformerTileRenderer::drawTile(PLATFORMER_ENEMY_TILES, 38U, 0, 0,
                                     flippedPacked, 16U, 16U, 16U, false,
                                     true);
    PlatformerTileRenderer::drawTile(enemyCache, 38U, 0, 0,
                                     flippedDecoded, 16U, 16U, 16U, false,
                                     true);
    assert(std::memcmp(flippedPacked, flippedDecoded,
                       sizeof(flippedPacked)) == 0);
    for (uint8_t y = 0U; y < 16U; ++y) {
        for (uint8_t x = 0U; x < 16U; ++x) {
            assert(flippedPacked[y * 16U + x] ==
                   normalTile[(15U - y) * 16U + x]);
        }
    }

    return 0;
}
