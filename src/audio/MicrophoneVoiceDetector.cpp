#include "audio/MicrophoneVoiceDetector.h"

#include <algorithm>
#include <cstring>

#include "third_party/libfvad/include/fvad.h"

MicrophoneVoiceDetector::~MicrophoneVoiceDetector() {
    if (vad_ != nullptr) {
        fvad_free(vad_);
    }
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
        updateDecision(result > 0);
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

void MicrophoneVoiceDetector::updateDecision(bool rawVoice) {
    rawVoice_ = rawVoice;
    if (rawVoice) {
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
