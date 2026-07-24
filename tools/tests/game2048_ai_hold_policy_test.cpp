#include "games/Game2048AiHoldPolicy.h"

#include <cassert>

using pgos::Game2048AiHoldPolicy;

namespace {

void testLongHoldAndRelease() {
    Game2048AiHoldPolicy policy;
    assert(!policy.sample(true, 100, true));
    assert(!policy.sample(true, 999, true));
    assert(policy.sample(true, 1000, true));
    assert(policy.sample(true, 1200, true));
    assert(!policy.sample(false, 1201, true));
    assert(!policy.sample(true, 1500, true));
    assert(!policy.sample(true, 2399, true));
    assert(policy.sample(true, 2400, true));
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
    return 0;
}
