#pragma once

#include <stddef.h>
#include <stdint.h>

struct MicrophoneVoiceEnhanceMetrics {
    uint16_t inputRms = 0;
    uint16_t gainPercent = 100;
    bool limiting = false;
};

class MicrophoneVoiceEnhancer {
public:
    void reset();
    void process(int16_t* samples, size_t sampleCount, bool voiceActive);
    MicrophoneVoiceEnhanceMetrics metrics() const;

private:
    static constexpr int32_t GAIN_ONE_Q12 = 4096;
    static constexpr int32_t MAX_GAIN_Q12 = GAIN_ONE_Q12 * 4;
    static constexpr uint32_t TARGET_RMS = 3000;
    static constexpr uint32_t LIMITER_PEAK = 30000;

    int32_t gainQ12_ = GAIN_ONE_Q12;
    uint16_t inputRms_ = 0;
    bool limiting_ = false;

    static uint32_t integerSquareRoot(uint64_t value);
    static int16_t clampPcm16(int32_t value);
};
