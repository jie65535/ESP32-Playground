#include "audio/MicrophoneVoiceEnhancer.h"

#include <algorithm>

void MicrophoneVoiceEnhancer::reset() {
    gainQ12_ = GAIN_ONE_Q12;
    inputRms_ = 0;
    limiting_ = false;
}

void MicrophoneVoiceEnhancer::process(int16_t* samples, size_t sampleCount,
                                      bool voiceActive) {
    if (samples == nullptr || sampleCount == 0) {
        return;
    }

    uint64_t squareSum = 0;
    uint32_t peak = 0;
    for (size_t index = 0; index < sampleCount; ++index) {
        const int32_t sample = samples[index];
        const uint32_t magnitude = static_cast<uint32_t>(
            std::min<int32_t>(32767, sample < 0 ? -sample : sample));
        peak = std::max(peak, magnitude);
        squareSum += static_cast<uint64_t>(magnitude) * magnitude;
    }

    const uint32_t rms = integerSquareRoot(squareSum / sampleCount);
    inputRms_ = static_cast<uint16_t>(std::min<uint32_t>(UINT16_MAX, rms));

    const int32_t peakLimitGainQ12 =
        peak == 0
            ? MAX_GAIN_Q12
            : static_cast<int32_t>(std::min<uint32_t>(
                  MAX_GAIN_Q12,
                  (LIMITER_PEAK * GAIN_ONE_Q12) / peak));

    int32_t targetGainQ12 = GAIN_ONE_Q12;
    limiting_ = false;
    if (voiceActive && rms > 0) {
        const int32_t rmsGainQ12 = static_cast<int32_t>(
            std::min<uint32_t>(MAX_GAIN_Q12,
                               (TARGET_RMS * GAIN_ONE_Q12) / rms));
        targetGainQ12 = std::max<int32_t>(
            GAIN_ONE_Q12, std::min(rmsGainQ12, peakLimitGainQ12));
        limiting_ = peakLimitGainQ12 < rmsGainQ12;
    }

    if (!voiceActive) {
        gainQ12_ = GAIN_ONE_Q12;
    }
    int32_t startGainQ12 = std::min(gainQ12_, peakLimitGainQ12);
    if (voiceActive) {
        if (targetGainQ12 > gainQ12_) {
            gainQ12_ += (targetGainQ12 - gainQ12_ + 1) / 2;
        } else {
            gainQ12_ = targetGainQ12;
        }
    }
    gainQ12_ = std::min(gainQ12_, peakLimitGainQ12);
    const int32_t gainDelta = gainQ12_ - startGainQ12;

    for (size_t index = 0; index < sampleCount; ++index) {
        const int32_t gain =
            startGainQ12 +
            static_cast<int32_t>(
                (static_cast<int64_t>(gainDelta) * (index + 1U)) /
                sampleCount);
        const int32_t output =
            (static_cast<int32_t>(samples[index]) * gain + (1 << 11)) >> 12;
        samples[index] = clampPcm16(output);
    }
}

MicrophoneVoiceEnhanceMetrics MicrophoneVoiceEnhancer::metrics() const {
    MicrophoneVoiceEnhanceMetrics value;
    value.inputRms = inputRms_;
    value.gainPercent = static_cast<uint16_t>(std::min<int32_t>(
        400, (gainQ12_ * 100 + GAIN_ONE_Q12 / 2) / GAIN_ONE_Q12));
    value.limiting = limiting_;
    return value;
}

uint32_t MicrophoneVoiceEnhancer::integerSquareRoot(uint64_t value) {
    uint64_t result = 0;
    uint64_t bit = uint64_t{1} << 62;
    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<uint32_t>(result);
}

int16_t MicrophoneVoiceEnhancer::clampPcm16(int32_t value) {
    return static_cast<int16_t>(
        std::max<int32_t>(-32768, std::min<int32_t>(32767, value)));
}
