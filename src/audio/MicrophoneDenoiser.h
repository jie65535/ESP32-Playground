#pragma once

#include <stddef.h>
#include <stdint.h>

struct MicrophoneDenoiseMetrics {
    uint16_t noiseRms = 0;
    uint8_t gainPercent = 100;
    bool voiceActive = false;
};

class MicrophoneDenoiser {
public:
    void reset();
    void process(int16_t* samples, size_t sampleCount);
    MicrophoneDenoiseMetrics metrics() const;

private:
    static constexpr int32_t HPF_ALPHA_Q15 = 30293;
    static constexpr int32_t GAIN_ONE_Q15 = 32768;
    static constexpr int32_t MIN_GAIN_Q15 = 6554;
    static constexpr uint8_t BOOTSTRAP_FRAMES = 32;

    int32_t previousInput_ = 0;
    int32_t previousHighPass_ = 0;
    uint32_t noiseRmsQ8_ = 0;
    int32_t gainQ15_ = GAIN_ONE_Q15;
    uint8_t bootstrapFramesRemaining_ = BOOTSTRAP_FRAMES;
    bool voiceActive_ = false;

    static uint32_t integerSquareRoot(uint64_t value);
    static int16_t clampPcm16(int32_t value);
};
