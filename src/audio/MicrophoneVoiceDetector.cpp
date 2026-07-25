#include "audio/MicrophoneVoiceDetector.h"

#include <algorithm>
#include <cstring>

#include "third_party/libfvad/include/fvad.h"

MicrophoneVoiceDetector::~MicrophoneVoiceDetector() {
    end();
}

void MicrophoneVoiceDetector::end() {
    if (vad_ != nullptr) {
        fvad_free(vad_);
        vad_ = nullptr;
    }
    frameSamples_ = 0;
    speechRunFrames_ = 0;
    hangoverFrames_ = 0;
    rawVoice_ = false;
    voiceActive_ = false;
    ready_ = false;
}

bool MicrophoneVoiceDetector::begin() {
    return reset();
}

bool MicrophoneVoiceDetector::reset() {
    frameSamples_ = 0;
    speechRunFrames_ = 0;
    hangoverFrames_ = 0;
    rawVoice_ = false;
    voiceActive_ = false;

    if (vad_ == nullptr) {
        vad_ = fvad_new();
    }
    if (vad_ == nullptr) {
        ready_ = false;
        return false;
    }

    fvad_reset(vad_);
    ready_ = fvad_set_sample_rate(vad_, SAMPLE_RATE) == 0 &&
             fvad_set_mode(vad_, AGGRESSIVENESS_MODE) == 0;
    return ready_;
}

MicrophoneVoiceDetectionMetrics MicrophoneVoiceDetector::process(
    const int16_t* samples, size_t sampleCount) {
    if (samples == nullptr || sampleCount == 0 || !ready_) {
        return metrics();
    }

    size_t sampleIndex = 0;
    while (sampleIndex < sampleCount) {
        const size_t copyCount = std::min(
            FRAME_SAMPLES - frameSamples_, sampleCount - sampleIndex);
        std::memcpy(frame_ + frameSamples_, samples + sampleIndex,
                    copyCount * sizeof(int16_t));
        frameSamples_ += copyCount;
        sampleIndex += copyCount;

        if (frameSamples_ != FRAME_SAMPLES) {
            continue;
        }

        const int result = fvad_process(vad_, frame_, FRAME_SAMPLES);
        frameSamples_ = 0;
        if (result < 0) {
            ready_ = false;
            rawVoice_ = false;
            voiceActive_ = false;
            break;
        }
        updateDecision(result > 0, calculateRms(frame_, FRAME_SAMPLES));
    }

    return metrics();
}

MicrophoneVoiceDetectionMetrics MicrophoneVoiceDetector::metrics() const {
    MicrophoneVoiceDetectionMetrics value;
    value.ready = ready_;
    value.rawVoice = rawVoice_;
    value.voiceActive = voiceActive_;
    value.speechRunFrames = speechRunFrames_;
    value.hangoverFrames = hangoverFrames_;
    return value;
}

void MicrophoneVoiceDetector::updateDecision(bool rawVoice, uint16_t rms) {
    rawVoice_ = rawVoice;
    if (rawVoice && rms >= MIN_VOICE_RMS) {
        if (speechRunFrames_ < START_SPEECH_FRAMES) {
            ++speechRunFrames_;
        }
        if (speechRunFrames_ >= START_SPEECH_FRAMES) {
            voiceActive_ = true;
            hangoverFrames_ = HANGOVER_FRAMES;
        }
        return;
    }

    speechRunFrames_ = 0;
    if (voiceActive_ && hangoverFrames_ > 0) {
        --hangoverFrames_;
    } else {
        voiceActive_ = false;
    }
}

uint16_t MicrophoneVoiceDetector::calculateRms(const int16_t* samples,
                                                size_t sampleCount) {
    uint64_t squareSum = 0;
    for (size_t index = 0; index < sampleCount; ++index) {
        const int32_t sample = samples[index];
        squareSum += static_cast<uint64_t>(sample * sample);
    }
    uint64_t value = squareSum / sampleCount;
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
    return static_cast<uint16_t>(
        std::min<uint64_t>(UINT16_MAX, result));
}
