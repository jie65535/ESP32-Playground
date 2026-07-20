#include "services/GamepadPolicy.h"

#include <cassert>

using pgos::GamepadActivitySample;
using pgos::GamepadActivityTracker;
using pgos::GamepadReconnectScheduler;

GamepadActivitySample neutralSample() {
    return GamepadActivitySample{};
}

int main() {
    GamepadActivityTracker tracker;
    GamepadActivitySample sample = neutralSample();
    assert(!tracker.update(sample));

    sample.axes[0] = 18;
    sample.triggers[0] = 12;
    assert(!tracker.update(sample));
    sample.axes[0] = -22;
    sample.triggers[0] = 20;
    assert(!tracker.update(sample));

    sample.axes[0] = 160;
    assert(tracker.update(sample));
    assert(tracker.update(sample));
    sample.axes[0] = 0;
    assert(tracker.update(sample));
    sample.axes[0] = 0;
    assert(!tracker.update(sample));

    sample.triggers[0] = 180;
    assert(tracker.update(sample));
    assert(tracker.update(sample));
    sample.triggers[0] = 0;
    assert(tracker.update(sample));

    sample = neutralSample();
    tracker.reset();
    sample.axes[0] = 140;
    assert(!tracker.update(sample));
    assert(!tracker.update(sample));
    sample.axes[0] = 145;
    assert(!tracker.update(sample));

    GamepadReconnectScheduler scheduler;
    scheduler.scheduleAfterUnexpectedDisconnect(1000);
    assert(scheduler.due(1000));
    scheduler.scheduleAfterScan(1000);
    assert(!scheduler.due(1000));
    assert(scheduler.remainingMs(1000) == 20000UL);
    assert(scheduler.due(21000));
    scheduler.scheduleAfterIntentionalDisconnect(30000);
    assert(!scheduler.due(59999));
    assert(scheduler.due(60000));
    scheduler.cancel();
    assert(!scheduler.scheduled());
    return 0;
}
