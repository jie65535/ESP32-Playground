#include "games/Game2048InputPolicy.h"

#include <cassert>
#include <cstdint>

using pgos::Game2048Direction;
using pgos::Game2048InputPolicy;

namespace {

void testHeldStickWaitsPastMoveAnimation() {
    Game2048InputPolicy policy;
    auto decision = policy.sample(400, 0, 0, true);
    assert(decision.triggered &&
           decision.direction == Game2048Direction::Right);

    assert(!policy.sample(400, 0, 100, false).triggered);
    assert(!policy.sample(400, 0, 146, true).triggered);
    assert(!policy.sample(400, 0, 299, true).triggered);
    decision = policy.sample(400, 0, 300, true);
    assert(decision.triggered &&
           decision.direction == Game2048Direction::Right);
    assert(!policy.sample(400, 0, 539, true).triggered);
    assert(policy.sample(400, 0, 540, true).triggered);
}

void testReleaseAndNewDirectionTriggerImmediately() {
    Game2048InputPolicy policy;
    assert(policy.sample(400, 0, 10, true).triggered);
    assert(!policy.sample(0, 0, 40, true).triggered);
    const auto decision = policy.sample(-400, 0, 50, true);
    assert(decision.triggered &&
           decision.direction == Game2048Direction::Left);
}

void testNearDiagonalNoiseKeepsDominantAxis() {
    Game2048InputPolicy policy;
    auto decision = policy.sample(320, 300, 0, true);
    assert(decision.triggered &&
           decision.direction == Game2048Direction::Right);
    assert(!policy.sample(285, 310, 100, true).triggered);
    decision = policy.sample(285, 310, 300, true);
    assert(decision.triggered &&
           decision.direction == Game2048Direction::Right);
    decision = policy.sample(190, -400, 320, true);
    assert(decision.triggered &&
           decision.direction == Game2048Direction::Up);
}

void testBlockedDirectionChangeStartsItsOwnDelay() {
    Game2048InputPolicy policy;
    assert(policy.sample(400, 0, 0, true).triggered);
    assert(!policy.sample(0, -400, 100, false).triggered);
    assert(!policy.sample(0, -400, 399, true).triggered);
    const auto decision = policy.sample(0, -400, 400, true);
    assert(decision.triggered &&
           decision.direction == Game2048Direction::Up);
}

}  // namespace

int main() {
    testHeldStickWaitsPastMoveAnimation();
    testReleaseAndNewDirectionTriggerImmediately();
    testNearDiagonalNoiseKeepsDominantAxis();
    testBlockedDirectionChangeStartsItsOwnDelay();
    return 0;
}
