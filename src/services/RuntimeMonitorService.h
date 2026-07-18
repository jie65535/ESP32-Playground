#pragma once

#include "core/AppTypes.h"

#include <Arduino.h>

class RuntimeMonitorService {
public:
    void begin(Print& log);
    void beginLoop();
    void endLoop();

    RuntimeSnapshot snapshot() const;
    void printStatus(Print& output) const;

private:
    Print* log_ = nullptr;
    RuntimeSnapshot snapshot_;
    uint64_t loopStartedUs_ = 0;
    uint64_t windowStartedUs_ = 0;
    uint64_t windowBusyUs_ = 0;
    uint32_t lastResourceSampleMs_ = 0;

    void sampleResources(uint32_t nowMs);
};
