#include "games/PlatformerCampaignData.h"

namespace pgos {

const PlatformerCampaignLevel* platformerCampaignLevel(uint8_t world,
                                                        uint8_t stage) {
    if (world < 1U || world > 8U || stage < 1U || stage > 4U) {
        return nullptr;
    }
    const uint8_t index = static_cast<uint8_t>((world - 1U) * 4U + stage - 1U);
    const PlatformerCampaignLevel& level = PLATFORMER_CAMPAIGN_LEVELS[index];
    return level.world == world && level.stage == stage ? &level : nullptr;
}

uint16_t platformerCampaignTileAt(const PlatformerCampaignLevel& level,
                                  PlatformerMapLayer layer, uint16_t x,
                                  uint8_t y) {
    const size_t layerIndex = static_cast<size_t>(layer);
    if (layerIndex >= static_cast<size_t>(PlatformerMapLayer::Count) ||
        x >= level.width || y >= level.height) {
        return PLATFORMER_EMPTY_TILE;
    }

    const PlatformerTileLayer& tileLayer = level.layers[layerIndex];
    if (tileLayer.rowOffsets == nullptr || tileLayer.runs == nullptr) {
        return PLATFORMER_EMPTY_TILE;
    }

    uint16_t cursor = tileLayer.rowOffsets[y];
    const uint16_t end = tileLayer.rowOffsets[static_cast<uint16_t>(y) + 1U];
    uint16_t column = 0;
    if (end > tileLayer.runDataSize || cursor > end) {
        return PLATFORMER_EMPTY_TILE;
    }

    while (cursor + 3U <= end) {
        const uint8_t runLength = tileLayer.runs[cursor];
        const uint16_t tile = static_cast<uint16_t>(tileLayer.runs[cursor + 1U]) |
                              static_cast<uint16_t>(
                                  static_cast<uint16_t>(tileLayer.runs[cursor + 2U]) << 8U);
        if (runLength == 0U) {
            return PLATFORMER_EMPTY_TILE;
        }
        if (x < static_cast<uint16_t>(column + runLength)) {
            return tile;
        }
        column = static_cast<uint16_t>(column + runLength);
        cursor = static_cast<uint16_t>(cursor + 3U);
    }
    return PLATFORMER_EMPTY_TILE;
}

}  // namespace pgos
