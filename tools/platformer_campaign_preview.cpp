#include "games/PlatformerCampaignData.h"
#include "games/PlatformerTileRenderer.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr uint16_t WIDTH = 320;
constexpr uint16_t HEIGHT = 218;
uint16_t frame[static_cast<size_t>(WIDTH) * HEIGHT] = {};

uint8_t expand5(uint16_t value) {
    return static_cast<uint8_t>((value << 3U) | (value >> 2U));
}

uint8_t expand6(uint16_t value) {
    return static_cast<uint8_t>((value << 2U) | (value >> 4U));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 6) {
        std::fprintf(stderr, "usage: %s WORLD STAGE CAMERA_TILE_X CAMERA_TILE_Y OUTPUT.ppm\n",
                     argv[0]);
        return 2;
    }
    const int world = std::atoi(argv[1]);
    const int stage = std::atoi(argv[2]);
    const int cameraX = std::atoi(argv[3]) * 16;
    const int cameraY = std::atoi(argv[4]) * 16;
    const pgos::PlatformerCampaignLevel* level = pgos::platformerCampaignLevel(
        static_cast<uint8_t>(world), static_cast<uint8_t>(stage));
    if (level == nullptr) {
        std::fprintf(stderr, "invalid level %d-%d\n", world, stage);
        return 2;
    }

    pgos::PlatformerTileRenderer::render(*level, cameraX, cameraY, frame,
                                         WIDTH, HEIGHT, WIDTH, true);
    std::FILE* output = std::fopen(argv[5], "wb");
    if (output == nullptr) {
        std::perror(argv[5]);
        return 1;
    }
    std::fprintf(output, "P6\n%u %u\n255\n", WIDTH, HEIGHT);
    for (uint16_t pixel : frame) {
        const uint8_t rgb[] = {
            expand5(static_cast<uint16_t>((pixel >> 11U) & 0x1FU)),
            expand6(static_cast<uint16_t>((pixel >> 5U) & 0x3FU)),
            expand5(static_cast<uint16_t>(pixel & 0x1FU)),
        };
        std::fwrite(rgb, sizeof(rgb), 1, output);
    }
    std::fclose(output);
    return 0;
}
