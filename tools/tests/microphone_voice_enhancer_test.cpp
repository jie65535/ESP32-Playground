#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "audio/MicrophoneVoiceEnhancer.h"

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

void fillTone(std::vector<int16_t>& samples, int amplitude) {
    for (size_t index = 0; index < samples.size(); ++index) {
        samples[index] = static_cast<int16_t>(
            amplitude * std::sin(2.0 * PI * 220.0 * index / 8000.0));
    }
}

}  // namespace

int main() {
    MicrophoneVoiceEnhancer enhancer;
    std::vector<int16_t> frame(FRAME_SAMPLES);
    fillTone(frame, 800);
    enhancer.process(frame.data(), frame.size(), true);
    if (enhancer.metrics().gainPercent <= 100) {
        return EXIT_FAILURE;
    }

    fillTone(frame, 300);
    const double before = rms(frame);
    enhancer.process(frame.data(), frame.size(), false);
    const double after = rms(frame);
    if (std::abs(after - before) > before * 0.02 ||
        enhancer.metrics().gainPercent != 100) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
