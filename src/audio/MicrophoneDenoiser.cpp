#include "audio/MicrophoneDenoiser.h"

#include <algorithm>

void MicrophoneDenoiser::reset() {
    previousInput_ = 0;
    previousHighPass_ = 0;
    noiseRmsQ8_ = 0;
    gainQ15_ = GAIN_ONE_Q15;
    bootstrapFramesRemaining_ = BOOTSTRAP_FRAMES;
    voiceActive_ = false;
}

void MicrophoneDenoiser::process(int16_t* samples, size_t sampleCount) {
    if (samples == nullptr || sampleCount == 0) {
        return;
    }

    uint64_t squareSum = 0;
    for (size_t index = 0; index < sampleCount; ++index) {
        const int32_t input = samples[index];
        const int32_t highPass =
            input - previousInput_ +
            ((HPF_ALPHA_Q15 * previousHighPass_ + (1 << 14)) >> 15);
        previousInput_ = input;
        previousHighPass_ = clampPcm16(highPass);
        samples[index] = static_cast<int16_t>(previousHighPass_);

        const uint32_t magnitude = static_cast<uint32_t>(
            std::min<int32_t>(32767, previousHighPass_ < 0
                                         ? -previousHighPass_
                                         : previousHighPass_));
        squareSum += static_cast<uint64_t>(magnitude) * magnitude;
    }

    const uint32_t rms = integerSquareRoot(squareSum / sampleCount);
    const uint32_t rmsQ8 = std::max<uint32_t>(1, rms) << 8;

    if (noiseRmsQ8_ == 0) {
        noiseRmsQ8_ = rmsQ8;
    } else if (bootstrapFramesRemaining_ > 0) {
        if (rmsQ8 < noiseRmsQ8_) {
            const uint32_t difference = noiseRmsQ8_ - rmsQ8;
            noiseRmsQ8_ -= std::max<uint32_t>(1, difference / 8U);
        } else if (rmsQ8 <= noiseRmsQ8_ * 2U) {
            const uint32_t difference = rmsQ8 - noiseRmsQ8_;
            noiseRmsQ8_ += std::max<uint32_t>(1, difference / 32U);
        }
    } else if (rmsQ8 < noiseRmsQ8_) {
        const uint32_t difference = noiseRmsQ8_ - rmsQ8;
        noiseRmsQ8_ -= std::max<uint32_t>(1, difference / 64U);
    } else if (rmsQ8 <= noiseRmsQ8_ * 5U / 2U) {
        const uint32_t difference = rmsQ8 - noiseRmsQ8_;
        if (difference > 0) {
            noiseRmsQ8_ += std::max<uint32_t>(1, difference / 128U);
        }
    } else {
        const uint32_t difference = rmsQ8 - noiseRmsQ8_;
        noiseRmsQ8_ += std::max<uint32_t>(1, difference / 1024U);
    }

    if (bootstrapFramesRemaining_ > 0) {
        --bootstrapFramesRemaining_;
    }

    const uint32_t noiseRms = std::max<uint32_t>(1, noiseRmsQ8_ >> 8);
    const uint32_t closeThreshold = std::max<uint32_t>(16, noiseRms * 5U / 4U);
    const uint32_t openThreshold =
        std::max<uint32_t>(closeThreshold + 1U, noiseRms * 5U / 2U);

    int32_t targetGainQ15 = GAIN_ONE_Q15;
    if (bootstrapFramesRemaining_ == 0 && rms <= closeThreshold) {
        targetGainQ15 = MIN_GAIN_Q15;
    } else if (bootstrapFramesRemaining_ == 0 && rms < openThreshold) {
        targetGainQ15 =
            MIN_GAIN_Q15 +
            static_cast<int32_t>(
                (static_cast<uint64_t>(rms - closeThreshold) *
                 (GAIN_ONE_Q15 - MIN_GAIN_Q15)) /
                (openThreshold - closeThreshold));
    }

    const int32_t startGainQ15 = gainQ15_;
    if (targetGainQ15 > gainQ15_) {
        gainQ15_ += (targetGainQ15 - gainQ15_ + 1) / 2;
    } else if (targetGainQ15 < gainQ15_) {
        gainQ15_ -= (gainQ15_ - targetGainQ15 + 7) / 8;
    }
    voiceActive_ = targetGainQ15 >= (GAIN_ONE_Q15 * 3 / 4);

    const int32_t gainDelta = gainQ15_ - startGainQ15;
    for (size_t index = 0; index < sampleCount; ++index) {
        const int32_t gain =
            startGainQ15 +
            static_cast<int32_t>(
                (static_cast<int64_t>(gainDelta) * (index + 1U)) /
                sampleCount);
        const int32_t output =
            (static_cast<int32_t>(samples[index]) * gain + (1 << 14)) >> 15;
        samples[index] = clampPcm16(output);
    }
}

MicrophoneDenoiseMetrics MicrophoneDenoiser::metrics() const {
    MicrophoneDenoiseMetrics value;
    value.noiseRms = static_cast<uint16_t>(
        std::min<uint32_t>(UINT16_MAX, noiseRmsQ8_ >> 8));
    value.gainPercent = static_cast<uint8_t>(std::min<int32_t>(
        100, (gainQ15_ * 100 + GAIN_ONE_Q15 / 2) / GAIN_ONE_Q15));
    value.voiceActive = voiceActive_;
    return value;
}

uint32_t MicrophoneDenoiser::integerSquareRoot(uint64_t value) {
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

int16_t MicrophoneDenoiser::clampPcm16(int32_t value) {
    return static_cast<int16_t>(
        std::max<int32_t>(-32768, std::min<int32_t>(32767, value)));
}
