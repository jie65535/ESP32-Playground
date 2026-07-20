#pragma once

#include <cstdint>

namespace pgos {

struct GamepadActivitySample {
    uint16_t buttons = 0;
    uint8_t dpad = 0;
    uint8_t miscButtons = 0;
    int16_t axes[4] = {};
    uint16_t triggers[2] = {};
};

class GamepadActivityTracker {
public:
    static constexpr int16_t AXIS_ENTER_THRESHOLD = 128;
    static constexpr int16_t AXIS_EXIT_THRESHOLD = 80;
    static constexpr int16_t AXIS_MOTION_THRESHOLD = 48;
    static constexpr uint16_t TRIGGER_ENTER_THRESHOLD = 96;
    static constexpr uint16_t TRIGGER_EXIT_THRESHOLD = 48;
    static constexpr uint16_t TRIGGER_MOTION_THRESHOLD = 32;

    void reset() {
        initialized_ = false;
        previous_ = GamepadActivitySample{};
        for (uint8_t index = 0; index < 4; ++index) {
            axisStates_[index] = 0;
            axisEngaged_[index] = false;
        }
        for (uint8_t index = 0; index < 2; ++index) {
            triggerStates_[index] = false;
            triggerEngaged_[index] = false;
        }
    }

    bool update(const GamepadActivitySample& sample) {
        if (!initialized_) {
            initialized_ = true;
            previous_ = sample;
            for (uint8_t index = 0; index < 4; ++index) {
                axisStates_[index] = axisState(sample.axes[index], 0);
            }
            for (uint8_t index = 0; index < 2; ++index) {
                triggerStates_[index] = triggerState(sample.triggers[index], false);
            }
            return sample.buttons != 0 || sample.dpad != 0 ||
                   sample.miscButtons != 0;
        }

        bool active = sample.buttons != previous_.buttons ||
                      sample.dpad != previous_.dpad ||
                      sample.miscButtons != previous_.miscButtons ||
                      sample.buttons != 0 || sample.dpad != 0 ||
                      sample.miscButtons != 0;

        for (uint8_t index = 0; index < 4; ++index) {
            const int8_t nextState = axisState(sample.axes[index], axisStates_[index]);
            const bool moved = absoluteDifference(sample.axes[index],
                                                  previous_.axes[index]) >=
                               AXIS_MOTION_THRESHOLD;
            const bool changed = nextState != axisStates_[index];
            if (nextState == 0) {
                axisEngaged_[index] = false;
            } else if (moved || changed) {
                axisEngaged_[index] = true;
            }
            active = active || moved || changed || axisEngaged_[index];
            axisStates_[index] = nextState;
        }

        for (uint8_t index = 0; index < 2; ++index) {
            const bool nextState = triggerState(sample.triggers[index],
                                                triggerStates_[index]);
            const bool moved = absoluteDifference(sample.triggers[index],
                                                  previous_.triggers[index]) >=
                               TRIGGER_MOTION_THRESHOLD;
            const bool changed = nextState != triggerStates_[index];
            if (!nextState) {
                triggerEngaged_[index] = false;
            } else if (moved || changed) {
                triggerEngaged_[index] = true;
            }
            active = active || moved || changed || triggerEngaged_[index];
            triggerStates_[index] = nextState;
        }

        previous_ = sample;
        return active;
    }

private:
    bool initialized_ = false;
    GamepadActivitySample previous_;
    int8_t axisStates_[4] = {};
    bool axisEngaged_[4] = {};
    bool triggerStates_[2] = {};
    bool triggerEngaged_[2] = {};

    static int8_t axisState(int16_t value, int8_t previousState) {
        if (previousState < 0 && value <= -AXIS_EXIT_THRESHOLD) {
            return -1;
        }
        if (previousState > 0 && value >= AXIS_EXIT_THRESHOLD) {
            return 1;
        }
        if (value <= -AXIS_ENTER_THRESHOLD) {
            return -1;
        }
        if (value >= AXIS_ENTER_THRESHOLD) {
            return 1;
        }
        return 0;
    }

    static bool triggerState(uint16_t value, bool previousState) {
        return previousState ? value >= TRIGGER_EXIT_THRESHOLD
                             : value >= TRIGGER_ENTER_THRESHOLD;
    }

    template <typename Value>
    static uint32_t absoluteDifference(Value left, Value right) {
        const int32_t difference = static_cast<int32_t>(left) -
                                   static_cast<int32_t>(right);
        return static_cast<uint32_t>(difference < 0 ? -difference : difference);
    }
};

class GamepadReconnectScheduler {
public:
    static constexpr uint32_t SCAN_WINDOW_MS = 10000UL;
    static constexpr uint32_t SCAN_PAUSE_MS = 20000UL;
    static constexpr uint32_t INTENTIONAL_DISCONNECT_DELAY_MS = 30000UL;

    void cancel() {
        scheduled_ = false;
        deadlineMs_ = 0;
    }

    void scheduleAfterIntentionalDisconnect(uint32_t nowMs) {
        schedule(nowMs, INTENTIONAL_DISCONNECT_DELAY_MS);
    }

    void scheduleAfterUnexpectedDisconnect(uint32_t nowMs) {
        schedule(nowMs, 0);
    }

    void scheduleAfterScan(uint32_t nowMs) {
        schedule(nowMs, SCAN_PAUSE_MS);
    }

    bool scheduled() const {
        return scheduled_;
    }

    bool due(uint32_t nowMs) const {
        return scheduled_ &&
               static_cast<int32_t>(nowMs - deadlineMs_) >= 0;
    }

    uint32_t remainingMs(uint32_t nowMs) const {
        if (!scheduled_ || due(nowMs)) {
            return 0;
        }
        return deadlineMs_ - nowMs;
    }

private:
    bool scheduled_ = false;
    uint32_t deadlineMs_ = 0;

    void schedule(uint32_t nowMs, uint32_t delayMs) {
        scheduled_ = true;
        deadlineMs_ = nowMs + delayMs;
    }
};

}
