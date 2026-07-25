#pragma once

#include <stddef.h>
#include <stdint.h>

struct Fvad;

struct MicrophoneVoiceDetectionMetrics {
    bool ready = false;
    bool rawVoice = false;
    bool voiceActive = false;
    uint8_t speechRunFrames = 0;
    uint8_t hangoverFrames = 0;
};

class MicrophoneVoiceDetector {
public:
    MicrophoneVoiceDetector() = default;
    ~MicrophoneVoiceDetector();

    MicrophoneVoiceDetector(const MicrophoneVoiceDetector&) = delete;
    MicrophoneVoiceDetector& operator=(const MicrophoneVoiceDetector&) =
        delete;

    bool begin();
    void end();
    bool reset();
    MicrophoneVoiceDetectionMetrics process(const int16_t* samples,
                                            size_t sampleCount);
    MicrophoneVoiceDetectionMetrics metrics() const;

private:
    static constexpr uint32_t SAMPLE_RATE = 8000;
    static constexpr size_t FRAME_SAMPLES = 160;
    static constexpr int AGGRESSIVENESS_MODE = 3;
    static constexpr uint8_t START_SPEECH_FRAMES = 3;
    static constexpr uint8_t HANGOVER_FRAMES = 6;
    static constexpr uint16_t MIN_VOICE_RMS = 650;

    Fvad* vad_ = nullptr;
    int16_t frame_[FRAME_SAMPLES] = {};
    size_t frameSamples_ = 0;
    uint8_t speechRunFrames_ = 0;
    uint8_t hangoverFrames_ = 0;
    bool rawVoice_ = false;
    bool voiceActive_ = false;
    bool ready_ = false;

    void updateDecision(bool rawVoice, uint16_t rms);
    static uint16_t calculateRms(const int16_t* samples,
                                 size_t sampleCount);
};
