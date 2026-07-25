#include "audio/MicrophoneDenoiser.h"

#include <algorithm>

#include "speex/speex_preprocess.h"

MicrophoneDenoiser::~MicrophoneDenoiser() {
    end();
}

bool MicrophoneDenoiser::begin() {
    end();
    state_ = speex_preprocess_state_init(FRAME_SAMPLES, SAMPLE_RATE);
    if (state_ == nullptr) {
        return false;
    }

    int enabled = 1;
    int disabled = 0;
    int suppressionDb = NOISE_SUPPRESS_DB;
    ready_ =
        speex_preprocess_ctl(state_, SPEEX_PREPROCESS_SET_DENOISE,
                             &enabled) == 0 &&
        speex_preprocess_ctl(state_, SPEEX_PREPROCESS_SET_VAD,
                             &disabled) == 0 &&
        speex_preprocess_ctl(state_, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS,
                             &suppressionDb) == 0;
    if (!ready_) {
        end();
        return false;
    }

    reset();
    return true;
}

void MicrophoneDenoiser::end() {
    if (state_ != nullptr) {
        speex_preprocess_state_destroy(state_);
        state_ = nullptr;
    }
    ready_ = false;
    reset();
}

void MicrophoneDenoiser::reset() {
    previousInput_ = 0;
    previousHighPass_ = 0;
    backgroundInputRmsQ8_ = 0;
    gainPercent_ = 100;
}

void MicrophoneDenoiser::process(int16_t* samples, size_t sampleCount) {
    if (samples == nullptr || sampleCount == 0) {
        return;
    }

    for (size_t index = 0; index < sampleCount; ++index) {
        const int32_t input = samples[index];
        const int32_t highPass =
            input - previousInput_ +
            ((HPF_ALPHA_Q15 * previousHighPass_ + (1 << 14)) >> 15);
        previousInput_ = input;
        previousHighPass_ = clampPcm16(highPass);
        samples[index] = static_cast<int16_t>(previousHighPass_);
    }

    const uint32_t inputRms = calculateRms(samples, sampleCount);
    if (ready_ && sampleCount >= FRAME_SAMPLES) {
        size_t offset = 0;
        while (offset + FRAME_SAMPLES <= sampleCount) {
            (void)speex_preprocess_run(state_, samples + offset);
            offset += FRAME_SAMPLES;
        }
    }

    const uint32_t outputRms = calculateRms(samples, sampleCount);
    gainPercent_ = static_cast<uint8_t>(std::min<uint32_t>(
        100U, inputRms == 0 ? 100U : (outputRms * 100U) / inputRms));
    const uint32_t rmsQ8 = std::max<uint32_t>(1, inputRms) << 8;
    if (backgroundInputRmsQ8_ == 0) {
        backgroundInputRmsQ8_ = rmsQ8;
    } else if (rmsQ8 < backgroundInputRmsQ8_) {
        backgroundInputRmsQ8_ -= std::max<uint32_t>(
            1U, (backgroundInputRmsQ8_ - rmsQ8) / 16U);
    } else {
        backgroundInputRmsQ8_ += std::max<uint32_t>(
            1U, (rmsQ8 - backgroundInputRmsQ8_) / 128U);
    }
}

MicrophoneDenoiseMetrics MicrophoneDenoiser::metrics() const {
    MicrophoneDenoiseMetrics value;
    value.backgroundInputRms = static_cast<uint16_t>(
        std::min<uint32_t>(UINT16_MAX, backgroundInputRmsQ8_ >> 8));
    value.gainPercent = gainPercent_;
    return value;
}

bool MicrophoneDenoiser::ready() const {
    return ready_;
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

uint32_t MicrophoneDenoiser::calculateRms(const int16_t* samples,
                                          size_t sampleCount) {
    if (samples == nullptr || sampleCount == 0) {
        return 0;
    }
    uint64_t squareSum = 0;
    for (size_t index = 0; index < sampleCount; ++index) {
        const int32_t sample = samples[index];
        const uint32_t magnitude = static_cast<uint32_t>(
            std::min<int32_t>(32767, sample < 0 ? -sample : sample));
        squareSum += static_cast<uint64_t>(magnitude) * magnitude;
    }
    return integerSquareRoot(squareSum / sampleCount);
}

int16_t MicrophoneDenoiser::clampPcm16(int32_t value) {
    return static_cast<int16_t>(
        std::max<int32_t>(-32768, std::min<int32_t>(32767, value)));
}
