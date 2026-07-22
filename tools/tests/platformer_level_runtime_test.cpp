#include "games/PlatformerLevelRuntime.h"
#include "games/PlatformerTileAssets.h"

#include <algorithm>
#include <cassert>

using namespace pgos;

int main() {
    PlatformerLevelRuntime runtime;
    assert(!runtime.load(0, 1));
    assert(runtime.load(1, 1));
    assert(runtime.widthPixels() == 224U * 16U);
    assert(runtime.heightPixels() == 33U * 16U);
    assert(runtime.goalColumn() == 198);
    assert(runtime.axeColumn() == -1);

    assert(runtime.isSolid(0, 13));
    assert(!runtime.isSolid(69, 13));  // first overworld pit
    assert(runtime.rectHitsSolid(32.0F, 208.0F, 16.0F, 16.0F));
    assert(!runtime.rectHitsSolid(69.0F * 16.0F, 208.0F, 16.0F, 16.0F));

    const PlatformerRuntimeTile question = runtime.tile(16, 9);
    assert(question.kind == PlatformerRuntimeTileKind::Question);
    assert(question.reward == PlatformerRuntimeReward::Coin);
    const PlatformerRuntimeTile mushroom = runtime.tile(21, 9);
    assert(mushroom.kind == PlatformerRuntimeTileKind::Question);
    assert(mushroom.reward == PlatformerRuntimeReward::Mushroom);
    runtime.updateAnimations(0.0F, 0.0F, 320U, 240U);
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId + 1U);
    assert(runtime.displaySourceId(21, 9, mushroom.sourceId) ==
           mushroom.sourceId);
    for (uint8_t frame = 0U; frame < 7U; ++frame) {
        runtime.updateAnimations(0.0F, 0.0F, 320U, 240U);
    }
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId + 1U);
    runtime.updateAnimations(0.0F, 0.0F, 320U, 240U);
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId + 2U);
    runtime.updateAnimations(32.0F, 0.0F, 320U, 240U);
    assert(runtime.displaySourceId(21, 9, mushroom.sourceId) ==
           mushroom.sourceId + 1U);

    runtime.setAnimationFrame(1U);
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId + 1U);
    runtime.setAnimationFrame(8U);
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId + 1U);
    runtime.setAnimationFrame(9U);
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId + 2U);
    runtime.setAnimationFrame(17U);
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId + 3U);
    runtime.setAnimationFrame(25U);
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId);
    runtime.setAnimationFrame(57U);
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId);
    runtime.setAnimationFrame(58U);
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId + 1U);
    const PlatformerBlockHitResult questionHit = runtime.hitBlock(16, 9, false);
    assert(questionHit.accepted && questionHit.changed && !questionHit.broken);
    assert(runtime.displaySourceId(16, 9, question.sourceId) ==
           question.sourceId);
    assert(runtime.isSolid(16, 9));
    assert(!runtime.hitBlock(16, 9, false).accepted);
    assert(runtime.startBlockBump(16, 9));
    assert(!runtime.startBlockBump(16, 9));
    constexpr int8_t EXPECTED_BUMP_OFFSETS[] = {-2, -3, -4, -5,
                                                -4, -3, -2, 0};
    for (int8_t expected : EXPECTED_BUMP_OFFSETS) {
        runtime.updateBlockBumps();
        assert(runtime.blockBumpOffset(16, 9) == expected);
    }
    assert(runtime.blockBump(0) == nullptr);

    // World 1-1's hidden 1UP block is pass-through until hit, then becomes a
    // normal used block in both point and rectangle collision queries.
    const PlatformerRuntimeTile hidden = runtime.tile(64, 8);
    assert(hidden.kind == PlatformerRuntimeTileKind::Hidden);
    assert(hidden.reward == PlatformerRuntimeReward::OneUp);
    assert(!runtime.isSolid(64, 8));
    assert(!runtime.rectHitsSolid(64.0F * 16.0F, 8.0F * 16.0F,
                                  16.0F, 16.0F));
    const PlatformerBlockHitResult hiddenHit = runtime.hitBlock(64, 8, false);
    assert(hiddenHit.accepted && hiddenHit.changed && !hiddenHit.broken);
    assert(runtime.isSolid(64, 8));
    assert(runtime.rectHitsSolid(64.0F * 16.0F, 8.0F * 16.0F,
                                 16.0F, 16.0F));

    const PlatformerRuntimeTile brick = runtime.tile(20, 9);
    assert(brick.kind == PlatformerRuntimeTileKind::Brick);
    assert(runtime.hitBlock(20, 9, false).accepted);
    assert(runtime.modificationAt(20, 9) == nullptr);
    assert(runtime.startBlockBump(20, 9));
    runtime.updateBlockBumps();
    assert(runtime.blockBumpOffset(20, 9) == -2);
    const PlatformerBlockHitResult broken = runtime.hitBlock(20, 9, true);
    assert(broken.accepted && broken.broken);
    assert(!runtime.isSolid(20, 9));

    assert(runtime.load(2, 1));
    assert(runtime.trampolineCount() > 0U);
    const PlatformerTrampolineRuntimeState* trampoline =
        runtime.trampoline(0U);
    assert(trampoline != nullptr);
    assert(PLATFORMER_BLOCK_REFERENCE_IDS[trampoline->sourceId] == 346U);
    assert(runtime.isSolid(trampoline->column, trampoline->row));
    assert(runtime.displaySourceId(trampoline->column, trampoline->row,
                                   trampoline->sourceId) ==
           trampoline->sourceId);
    const PlatformerRuntimeTile trampolineBottom =
        runtime.tile(trampoline->column,
                     static_cast<uint8_t>(trampoline->row + 1U));
    assert(trampolineBottom.sourceId < PLATFORMER_BLOCK_TILE_COUNT);
    assert(PLATFORMER_BLOCK_REFERENCE_IDS[trampolineBottom.sourceId] == 394U);
    assert(runtime.setTrampolineState(0U, 1U, 1U, true));
    assert(!runtime.isSolid(trampoline->column, trampoline->row));
    assert(runtime.displaySourceId(trampoline->column, trampoline->row,
                                   trampoline->sourceId) ==
           trampoline->sourceId + 1U);
    assert(runtime.displaySourceId(
               trampoline->column,
               static_cast<uint8_t>(trampoline->row + 1U),
               trampolineBottom.sourceId) == trampolineBottom.sourceId + 1U);
    runtime.resetChanges();
    trampoline = runtime.trampoline(0U);
    assert(trampoline != nullptr && !trampoline->activated &&
           trampoline->visualState == 0U &&
           runtime.isSolid(trampoline->column, trampoline->row));

    assert(runtime.load(4, 2));
    assert(runtime.heightPixels() == 51U * 16U);
    assert(runtime.tile(90, 3).kind == PlatformerRuntimeTileKind::Coin);
    assert(!runtime.isSolid(90, 3));
    assert(runtime.collectCoin(90, 3));
    assert(!runtime.collectCoin(90, 3));

    assert(runtime.load(8, 4));
    assert(runtime.goalColumn() == -1);
    assert(runtime.axeColumn() == 301);

    uint8_t maximumAnimatedTiles = 0U;
    for (uint8_t world = 1U; world <= 8U; ++world) {
        for (uint8_t stage = 1U; stage <= 4U; ++stage) {
            assert(runtime.load(world, stage));
            assert(runtime.animatedTileCount() <=
                   PlatformerLevelRuntime::MAX_ANIMATED_TILES);
            maximumAnimatedTiles =
                std::max(maximumAnimatedTiles, runtime.animatedTileCount());
        }
    }
    assert(maximumAnimatedTiles == 104U);
    assert(runtime.level()->warps.count == 8);
    assert(runtime.level()->teleports.count == 3);
    return 0;
}
