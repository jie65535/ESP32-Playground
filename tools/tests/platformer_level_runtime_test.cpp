#include "games/PlatformerLevelRuntime.h"

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
    const PlatformerBlockHitResult questionHit = runtime.hitBlock(16, 9, false);
    assert(questionHit.accepted && questionHit.changed && !questionHit.broken);
    assert(runtime.isSolid(16, 9));
    assert(!runtime.hitBlock(16, 9, false).accepted);

    const PlatformerRuntimeTile mushroom = runtime.tile(21, 9);
    assert(mushroom.kind == PlatformerRuntimeTileKind::Question);
    assert(mushroom.reward == PlatformerRuntimeReward::Mushroom);

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
    const PlatformerBlockHitResult broken = runtime.hitBlock(20, 9, true);
    assert(broken.accepted && broken.broken);
    assert(!runtime.isSolid(20, 9));

    assert(runtime.load(4, 2));
    assert(runtime.heightPixels() == 51U * 16U);
    assert(runtime.tile(90, 3).kind == PlatformerRuntimeTileKind::Coin);
    assert(!runtime.isSolid(90, 3));
    assert(runtime.collectCoin(90, 3));
    assert(!runtime.collectCoin(90, 3));

    assert(runtime.load(8, 4));
    assert(runtime.goalColumn() == -1);
    assert(runtime.axeColumn() == 301);
    assert(runtime.level()->warps.count == 8);
    assert(runtime.level()->teleports.count == 3);
    return 0;
}
