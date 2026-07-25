#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "audio/MicrophoneDenoiser.h"

namespace {

constexpr size_t FRAME_SAMPLES = 160;
constexpr double PI = 3.14159265358979323846;

double rms(const std::vector<int16_t>& samples) {
    double sum = 0.0;
    for (const int16_t sample : samples) {
        sum += static_cast<double>(sample) * sample;
    }
    return std::sqrt(sum / samples.size());
}

void fillNoise(std::vector<int16_t>& frame, uint32_t& state,
               int32_t amplitude) {
    for (int16_t& sample : frame) {
        state = state * 1664525U + 1013904223U;
        sample = static_cast<int16_t>(
            (static_cast<int32_t>(state >> 16U) - 32768) * amplitude /
            32768);
    }
}

}  // namespace

int main() {
    MicrophoneDenoiser denoiser;
    if (!denoiser.begin() || !denoiser.ready()) {
        return EXIT_FAILURE;
    }

    uint32_t randomState = 0x12345678U;
    std::vector<int16_t> frame(FRAME_SAMPLES);
    double inputNoiseRms = 0.0;
    double outputNoiseRms = 0.0;
    for (size_t frameIndex = 0; frameIndex < 150; ++frameIndex) {
        fillNoise(frame, randomState, 900);
        inputNoiseRms = rms(frame);
        denoiser.process(frame.data(), frame.size());
        outputNoiseRms = rms(frame);
    }
    if (!(outputNoiseRms < inputNoiseRms * 0.55)) {
        return EXIT_FAILURE;
    }

    double inputSpeechRms = 0.0;
    double outputSpeechRms = 0.0;
    for (size_t frameIndex = 0; frameIndex < 25; ++frameIndex) {
        fillNoise(frame, randomState, 900);
        for (size_t index = 0; index < frame.size(); ++index) {
            const size_t sampleIndex = frameIndex * FRAME_SAMPLES + index;
            const int32_t speech = static_cast<int32_t>(
                5000.0 * std::sin(2.0 * PI * 220.0 * sampleIndex / 8000.0));
            frame[index] = static_cast<int16_t>(std::clamp<int32_t>(
                static_cast<int32_t>(frame[index]) + speech, -32768, 32767));
        }
        inputSpeechRms = rms(frame);
        denoiser.process(frame.data(), frame.size());
        outputSpeechRms = rms(frame);
    }
    if (!(outputSpeechRms > inputSpeechRms * 0.55)) {
        return EXIT_FAILURE;
    }

    denoiser.end();
    return denoiser.ready() ? EXIT_FAILURE : EXIT_SUCCESS;
}
