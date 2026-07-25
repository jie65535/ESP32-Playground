#pragma once

#include <stddef.h>
#include <stdint.h>

struct MicrophoneDenoiseMetrics {
    uint16_t backgroundInputRms = 0;
    uint8_t gainPercent = 100;
};

struct SpeexPreprocessState_;

class MicrophoneDenoiser {
public:
    MicrophoneDenoiser() = default;
    ~MicrophoneDenoiser();

    MicrophoneDenoiser(const MicrophoneDenoiser&) = delete;
    MicrophoneDenoiser& operator=(const MicrophoneDenoiser&) = delete;

    bool begin();
    void end();
    void reset();
    void process(int16_t* samples, size_t sampleCount);
    MicrophoneDenoiseMetrics metrics() const;
    bool ready() const;

private:
    static constexpr int32_t HPF_ALPHA_Q15 = 30293;
    static constexpr uint32_t SAMPLE_RATE = 8000;
    static constexpr size_t FRAME_SAMPLES = 160;
    static constexpr int NOISE_SUPPRESS_DB = -22;

    SpeexPreprocessState_* state_ = nullptr;
    int32_t previousInput_ = 0;
    int32_t previousHighPass_ = 0;
    uint32_t backgroundInputRmsQ8_ = 0;
    uint8_t gainPercent_ = 100;
    bool ready_ = false;

    static uint32_t integerSquareRoot(uint64_t value);
    static uint32_t calculateRms(const int16_t* samples, size_t sampleCount);
    static int16_t clampPcm16(int32_t value);
};
