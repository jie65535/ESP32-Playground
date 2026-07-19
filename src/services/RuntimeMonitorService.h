#pragma once

#include "core/AppTypes.h"

#include <Arduino.h>

class RuntimeMonitorService {
public:
    enum class Stage : uint8_t {
        Gamepad,
        Wifi,
        Server,
        Mirror,
        Benchmark,
        Time,
        Audio,
        Display,
        Ui,
    };

    void begin(Print& log);
    void beginLoop();
    void endLoop();
    void recordStage(Stage stage, uint32_t elapsedUs);
    void recordFlushMetrics(uint32_t copyUs, uint32_t transferUs,
                            uint32_t wallUs, uint32_t waitUs, uint32_t pixels,
                            uint16_t areas);

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
