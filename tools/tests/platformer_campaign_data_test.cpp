#include "games/PlatformerCampaignData.h"
#include "games/PlatformerEngine.h"
#include "games/PlatformerTileAssets.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

using namespace pgos;

bool isReferenceEnemyLayerTile(uint16_t sourceTile) {
    switch (sourceTile) {
        case 73U:
        case 79U:
        case 83U:
        case 85U:
        case 91U:
        case 490U:
        case 492U:
        case 496U:
            return true;
        default:
            break;
    }
    if (sourceTile >= PLATFORMER_ENEMY_TILE_COUNT) {
        return false;
    }
    switch (PLATFORMER_ENEMY_REFERENCE_IDS[sourceTile]) {
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
        case 62U:
        case 73U:
        case 74U:
        case 75U:
        case 79U:
        case 83U:
        case 91U:
        case 96U:
        case 97U:
        case 457U:
            return true;
        default:
            return false;
    }
}

int main() {
    assert(platformerCampaignLevel(0, 1) == nullptr);
    assert(platformerCampaignLevel(1, 0) == nullptr);
    assert(platformerCampaignLevel(9, 1) == nullptr);

    size_t nonEmpty[static_cast<size_t>(PlatformerMapLayer::Count)] = {};
    size_t cannonCount = 0U;
    uint8_t maximumCannonsInLevel = 0U;
    uint8_t maximumActivatedEnemies = 0U;
    uint8_t maximumVisibleCannons = 0U;
    uint8_t maximumLakitus = 0U;
    uint8_t maximumHammerBros = 0U;
    uint8_t maximumBowsers = 0U;
    uint8_t maximumEnemyPoolDemand = 0U;
    uint8_t maximumHazardPoolDemand = 0U;
    uint8_t maximumDemandWorld = 0U;
    uint8_t maximumDemandStage = 0U;
    uint8_t maximumLevelEnemies = 0U;
    uint8_t maximumLevelEnemyWorld = 0U;
    uint8_t maximumLevelEnemyStage = 0U;
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

            uint8_t levelCannonCount = 0U;
            uint8_t levelEnemyCount = 0U;
            for (size_t layer = 0;
                 layer < static_cast<size_t>(PlatformerMapLayer::Count);
                 ++layer) {
                for (uint8_t y = 0; y < level->height; ++y) {
                    for (uint16_t x = 0; x < level->width; ++x) {
                        const uint16_t tile = platformerCampaignTileAt(
                            *level, static_cast<PlatformerMapLayer>(layer), x, y);
                        if (tile != PLATFORMER_EMPTY_TILE) {
                            ++nonEmpty[layer];
                            const bool cannonLayer =
                                layer == static_cast<size_t>(
                                             PlatformerMapLayer::Underground) ||
                                layer == static_cast<size_t>(
                                             PlatformerMapLayer::Foreground);
                            if (cannonLayer &&
                                tile < PLATFORMER_BLOCK_TILE_COUNT &&
                                PLATFORMER_BLOCK_REFERENCE_IDS[tile] == 63U) {
                                assert(tile == 63U || tile == 79U ||
                                       tile == 95U || tile == 591U);
                                ++cannonCount;
                                ++levelCannonCount;
                            }
                            if (layer == static_cast<size_t>(
                                             PlatformerMapLayer::Enemies)) {
                                assert(isReferenceEnemyLayerTile(tile));
                                if (platformerEnemySourceCreatesEntity(tile)) {
                                    ++levelEnemyCount;
                                }
                            }
                        }
                    }
                }
            }
            assert(levelCannonCount <= 16U);
            if (levelCannonCount > maximumCannonsInLevel) {
                maximumCannonsInLevel = levelCannonCount;
            }
            if (levelEnemyCount > maximumLevelEnemies) {
                maximumLevelEnemies = levelEnemyCount;
                maximumLevelEnemyWorld = world;
                maximumLevelEnemyStage = stage;
            }

            // Enumerate a conservative local envelope around every possible
            // camera tile. Cannons fire only inside the visible 320x240
            // rectangle. One live bullet per visible cannon is conservative
            // because bullets cross it before the five-second timer fires
            // again. Each visible Lakitu gets at least one reserved Spiny.
            for (uint16_t cameraColumn = 0U;
                 cameraColumn < level->width; ++cameraColumn) {
                for (uint8_t cameraRow = 0U; cameraRow < level->height;
                     ++cameraRow) {
                    const int32_t firstColumn =
                        cameraColumn == 0U ? 0 : cameraColumn - 1;
                    const int32_t lastColumn = std::min<int32_t>(
                        level->width - 1,
                        static_cast<int32_t>(cameraColumn) + 22);
                    const int32_t firstRow =
                        cameraRow == 0U ? 0 : cameraRow - 1;
                    const int32_t lastRow = std::min<int32_t>(
                        level->height - 1,
                        static_cast<int32_t>(cameraRow) + 16);
                    uint8_t activatedEnemies = 0U;
                    uint8_t lakitus = 0U;
                    uint8_t hammerBros = 0U;
                    uint8_t bowsers = 0U;
                    for (int32_t y = firstRow; y <= lastRow; ++y) {
                        for (int32_t x = firstColumn; x <= lastColumn; ++x) {
                            const uint16_t source = platformerCampaignTileAt(
                                *level, PlatformerMapLayer::Enemies,
                                static_cast<uint16_t>(x),
                                static_cast<uint8_t>(y));
                            if (!platformerEnemySourceCreatesEntity(source)) {
                                continue;
                            }
                            ++activatedEnemies;
                            const uint16_t reference =
                                PLATFORMER_ENEMY_REFERENCE_IDS[source];
                            lakitus += reference == 50U ? 1U : 0U;
                            hammerBros += reference == 56U ? 1U : 0U;
                            bowsers += reference == 61U ? 1U : 0U;
                        }
                    }

                    uint8_t visibleCannons = 0U;
                    const int32_t visibleLastColumn = std::min<int32_t>(
                        level->width - 1,
                        static_cast<int32_t>(cameraColumn) + 20);
                    const int32_t visibleLastRow = std::min<int32_t>(
                        level->height - 1,
                        static_cast<int32_t>(cameraRow) + 15);
                    for (int32_t y = cameraRow; y <= visibleLastRow; ++y) {
                        for (int32_t x = cameraColumn;
                             x <= visibleLastColumn; ++x) {
                            for (PlatformerMapLayer layer : {
                                     PlatformerMapLayer::Underground,
                                     PlatformerMapLayer::Foreground}) {
                                const uint16_t source =
                                    platformerCampaignTileAt(
                                        *level, layer,
                                        static_cast<uint16_t>(x),
                                        static_cast<uint8_t>(y));
                                if (source < PLATFORMER_BLOCK_TILE_COUNT &&
                                    PLATFORMER_BLOCK_REFERENCE_IDS[source] ==
                                        63U) {
                                    ++visibleCannons;
                                }
                            }
                        }
                    }

                    const uint8_t enemyDemand = static_cast<uint8_t>(
                        activatedEnemies + visibleCannons + lakitus);
                    // A Bowser burst contains at most nine live hammers. A
                    // Hammer Bro owns at most one released hammer at a time.
                    const uint8_t hazardDemand = static_cast<uint8_t>(
                        hammerBros + (bowsers > 0U ? 9U : 0U));
                    maximumActivatedEnemies =
                        std::max(maximumActivatedEnemies, activatedEnemies);
                    maximumVisibleCannons =
                        std::max(maximumVisibleCannons, visibleCannons);
                    maximumLakitus = std::max(maximumLakitus, lakitus);
                    maximumHammerBros =
                        std::max(maximumHammerBros, hammerBros);
                    maximumBowsers = std::max(maximumBowsers, bowsers);
                    maximumHazardPoolDemand =
                        std::max(maximumHazardPoolDemand, hazardDemand);
                    if (enemyDemand > maximumEnemyPoolDemand) {
                        maximumEnemyPoolDemand = enemyDemand;
                        maximumDemandWorld = world;
                        maximumDemandStage = stage;
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
    assert(cannonCount == 31U);
    assert(maximumCannonsInLevel == 13U);
    assert(maximumActivatedEnemies == 9U);
    assert(maximumVisibleCannons == 4U);
    assert(maximumLakitus == 1U);
    assert(maximumHammerBros == 2U);
    assert(maximumBowsers == 1U);
    assert(maximumEnemyPoolDemand == 9U);
    assert(maximumDemandWorld == 3U && maximumDemandStage == 1U);
    assert(maximumLevelEnemies == 57U);
    assert(maximumLevelEnemyWorld == 8U && maximumLevelEnemyStage == 1U);
    assert(maximumLevelEnemies <= PlatformerEngine::MAX_LEVEL_ENEMIES);
    assert(PlatformerEngine::MAX_DYNAMIC_ENEMIES >=
           maximumVisibleCannons + maximumLakitus);
    assert(maximumEnemyPoolDemand <= PlatformerEngine::MAX_ENEMIES);
    assert(maximumHazardPoolDemand == 10U);
    assert(maximumHazardPoolDemand <=
           PlatformerEngine::MAX_ENEMY_HAZARDS);

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
