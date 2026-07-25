#include <cstddef>
#include <cstdint>
#include <cstdlib>

#define private public
#include "audio/MicrophoneVoiceDetector.h"
#undef private

extern "C" {

Fvad* fvad_new(void) {
    return nullptr;
}

void fvad_free(Fvad*) {}
void fvad_reset(Fvad*) {}

int fvad_set_mode(Fvad*, int) {
    return 0;
}

int fvad_set_sample_rate(Fvad*, int) {
    return 0;
}

int fvad_process(Fvad*, const int16_t*, size_t) {
    return 0;
}

}  // extern "C"

int main() {
    MicrophoneVoiceDetector detector;

    for (int frame = 0; frame < 4; ++frame) {
        detector.updateDecision(true, 649);
    }
    if (detector.metrics().voiceActive ||
        detector.metrics().speechRunFrames != 0) {
        return EXIT_FAILURE;
    }

    detector.updateDecision(true, 650);
    detector.updateDecision(true, 650);
    if (detector.metrics().voiceActive ||
        detector.metrics().speechRunFrames != 2) {
        return EXIT_FAILURE;
    }

    detector.updateDecision(false, 650);
    if (detector.metrics().speechRunFrames != 0) {
        return EXIT_FAILURE;
    }

    detector.updateDecision(true, 650);
    detector.updateDecision(true, 650);
    detector.updateDecision(true, 650);
    if (!detector.metrics().voiceActive ||
        detector.metrics().hangoverFrames != 6) {
        return EXIT_FAILURE;
    }

    for (int frame = 0; frame < 6; ++frame) {
        detector.updateDecision(false, 650);
    }
    if (!detector.metrics().voiceActive ||
        detector.metrics().hangoverFrames != 0) {
        return EXIT_FAILURE;
    }

    detector.updateDecision(false, 650);
    return detector.metrics().voiceActive ? EXIT_FAILURE : EXIT_SUCCESS;
}
