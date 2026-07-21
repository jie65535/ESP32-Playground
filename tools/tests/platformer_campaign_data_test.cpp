#include "games/PlatformerCampaignData.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

using namespace pgos;

int main() {
    assert(platformerCampaignLevel(0, 1) == nullptr);
    assert(platformerCampaignLevel(1, 0) == nullptr);
    assert(platformerCampaignLevel(9, 1) == nullptr);

    size_t nonEmpty[static_cast<size_t>(PlatformerMapLayer::Count)] = {};
    for (uint8_t world = 1; world <= 8; ++world) {
        for (uint8_t stage = 1; stage <= 4; ++stage) {
            const PlatformerCampaignLevel* level =
                platformerCampaignLevel(world, stage);
            assert(level != nullptr);
            assert(level->world == world);
            assert(level->stage == stage);
            assert(level->width >= 160);
            assert(level->height >= 15);
            assert(level->height <= 51);
            // Castle camera bounds intentionally extend a few tiles beyond
            // their CSV width while the ending sequence locks the viewport.
            assert(level->cameraMaximum <= level->width + 7U);

            const uint8_t index =
                static_cast<uint8_t>((world - 1U) * 4U + stage - 1U);
            if (index + 1U < PLATFORMER_CAMPAIGN_LEVEL_COUNT) {
                const PlatformerCampaignLevel& next =
                    PLATFORMER_CAMPAIGN_LEVELS[index + 1U];
                assert(level->nextWorld == next.world);
                assert(level->nextStage == next.stage);
            } else {
                assert(level->nextWorld == 0);
                assert(level->nextStage == 0);
            }

            for (size_t layer = 0;
                 layer < static_cast<size_t>(PlatformerMapLayer::Count);
                 ++layer) {
                for (uint8_t y = 0; y < level->height; ++y) {
                    for (uint16_t x = 0; x < level->width; ++x) {
                        const uint16_t tile = platformerCampaignTileAt(
                            *level, static_cast<PlatformerMapLayer>(layer), x, y);
                        if (tile != PLATFORMER_EMPTY_TILE) {
                            ++nonEmpty[layer];
                        }
                    }
                }
            }
            assert(platformerCampaignTileAt(
                       *level, PlatformerMapLayer::Foreground, level->width, 0) ==
                   PLATFORMER_EMPTY_TILE);
            assert(platformerCampaignTileAt(
                       *level, PlatformerMapLayer::Foreground, 0, level->height) ==
                   PLATFORMER_EMPTY_TILE);
        }
    }

    assert(nonEmpty[static_cast<size_t>(PlatformerMapLayer::Background)] == 17003);
    assert(nonEmpty[static_cast<size_t>(PlatformerMapLayer::Underground)] == 3408);
    assert(nonEmpty[static_cast<size_t>(PlatformerMapLayer::Foreground)] == 21573);
    assert(nonEmpty[static_cast<size_t>(PlatformerMapLayer::AboveForeground)] == 1066);
    assert(nonEmpty[static_cast<size_t>(PlatformerMapLayer::Collectibles)] == 129);
    assert(nonEmpty[static_cast<size_t>(PlatformerMapLayer::Enemies)] == 883);

    const PlatformerCampaignLevel* first = platformerCampaignLevel(1, 1);
    assert(first != nullptr);
    assert(first->width == 224);
    assert(first->height == 33);
    assert(first->playerStart.x == 2 && first->playerStart.y == 12);
    assert(first->warps.count == 2);

    const PlatformerCampaignLevel* repaired = platformerCampaignLevel(3, 1);
    assert(repaired != nullptr);
    assert(repaired->vines.count == 1);
    const PlatformerVineData& vine =
        PLATFORMER_CAMPAIGN_VINES[repaired->vines.offset];
    assert(vine.block.x == 131 && vine.block.y == 20);
    assert(vine.cameraMaximum == 224);
    assert(vine.background == PlatformerBackgroundColor::Black);
    assert(vine.levelType == PlatformerLevelType::Overworld);

    const PlatformerCampaignLevel* finalLevel = platformerCampaignLevel(8, 4);
    assert(finalLevel != nullptr);
    assert(finalLevel->width == 320);
    assert(finalLevel->height == 33);
    assert(finalLevel->warps.count == 8);
    assert(finalLevel->fireBars.count == 5);
    assert(finalLevel->teleports.count == 3);
    const PlatformerTeleportData& loop =
        PLATFORMER_CAMPAIGN_TELEPORTS[finalLevel->teleports.offset + 2U];
    assert(loop.triggerX == 244 && loop.destinationX == 181);
    assert(finalLevel->floatingTexts.count == 5);
    const PlatformerFloatingTextData& ending =
        PLATFORMER_CAMPAIGN_FLOATING_TEXTS[finalLevel->floatingTexts.offset];
    assert(std::strcmp(ending.text, "THANK YOU MARIO") == 0);

    return 0;
}
