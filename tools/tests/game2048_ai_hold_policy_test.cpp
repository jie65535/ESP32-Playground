#include "games/Game2048AiHoldPolicy.h"

#include <cassert>

using pgos::Game2048AiHoldPolicy;
using pgos::Game2048AiPacing;

namespace {

void testLongHoldAndRelease() {
    Game2048AiHoldPolicy policy;
    assert(!policy.sample(true, 100, true));
    assert(!policy.sample(true, 999, true));
    assert(policy.sample(true, 1000, true));
    assert(policy.activeDurationMs(1000) == 0);
    assert(policy.activeDurationMs(1600) == 600);
    assert(policy.sample(true, 1200, true));
    assert(!policy.sample(false, 1201, true));
    assert(!policy.sample(true, 1500, true));
    assert(!policy.sample(true, 2399, true));
    assert(policy.sample(true, 2400, true));
}

void testPacingAcceleratesSmoothlyAndResets() {
    assert(Game2048AiPacing::moveIntervalMs(0) ==
           Game2048AiPacing::START_INTERVAL_MS);
    assert(Game2048AiPacing::animationMs(0) ==
           Game2048AiPacing::START_ANIMATION_MS);
    assert(Game2048AiPacing::moveIntervalMs(3000) <
           Game2048AiPacing::moveIntervalMs(0));
    assert(Game2048AiPacing::moveIntervalMs(6000) <
           Game2048AiPacing::moveIntervalMs(3000));
    assert(Game2048AiPacing::moveIntervalMs(9000) <
           Game2048AiPacing::moveIntervalMs(6000));
    assert(Game2048AiPacing::moveIntervalMs(Game2048AiPacing::RAMP_MS) ==
           Game2048AiPacing::MIN_INTERVAL_MS);
    assert(Game2048AiPacing::animationMs(Game2048AiPacing::RAMP_MS) ==
           Game2048AiPacing::MIN_ANIMATION_MS);
    assert(Game2048AiPacing::moveIntervalMs(Game2048AiPacing::RAMP_MS * 2U) ==
           Game2048AiPacing::MIN_INTERVAL_MS);
}

void testEnteringWhileHeldRequiresRelease() {
    Game2048AiHoldPolicy policy;
    policy.reset(true);
    assert(!policy.sample(true, 900, true));
    assert(!policy.sample(false, 901, true));
    assert(!policy.sample(true, 902, true));
    assert(!policy.sample(true, 1801, true));
    assert(policy.sample(true, 1802, true));
}

void testIneligibleStateDoesNotArmHeldButton() {
    Game2048AiHoldPolicy policy;
    assert(!policy.sample(true, 0, false));
    assert(!policy.sample(true, 1000, true));
    assert(!policy.sample(false, 1001, true));
    assert(!policy.sample(true, 1002, true));
    assert(policy.sample(true, 1902, true));
}

}  // namespace

int main() {
    testLongHoldAndRelease();
    testEnteringWhileHeldRequiresRelease();
    testIneligibleStateDoesNotArmHeldButton();
    testPacingAcceleratesSmoothlyAndResets();
    return 0;
}
