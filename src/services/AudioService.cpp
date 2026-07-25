#include "services/AudioService.h"

#include <esp_crc.h>
#include <esp_heap_caps.h>
#include <driver/i2s.h>

#include <algorithm>
#include <cmath>

#include "services/I2cBusService.h"

namespace {

constexpr uint8_t RESET_REG = 0x00;
constexpr uint8_t CLOCK_REG01 = 0x01;
constexpr uint8_t CLOCK_REG02 = 0x02;
constexpr uint8_t CLOCK_REG03 = 0x03;
constexpr uint8_t CLOCK_REG04 = 0x04;
constexpr uint8_t CLOCK_REG05 = 0x05;
constexpr uint8_t CLOCK_REG06 = 0x06;
constexpr uint8_t CLOCK_REG07 = 0x07;
constexpr uint8_t CLOCK_REG08 = 0x08;
constexpr uint8_t SDP_INPUT_REG = 0x09;
constexpr uint8_t SDP_OUTPUT_REG = 0x0A;
constexpr uint8_t SYSTEM_REG0D = 0x0D;
constexpr uint8_t SYSTEM_REG0E = 0x0E;
constexpr uint8_t SYSTEM_REG12 = 0x12;
constexpr uint8_t SYSTEM_REG13 = 0x13;
constexpr uint8_t SYSTEM_REG14 = 0x14;
constexpr uint8_t ADC_SCALE_REG = 0x16;
constexpr uint8_t ADC_VOLUME_REG = 0x17;
constexpr uint8_t ADC_EQ_REG = 0x1C;
constexpr uint8_t DAC_MUTE_REG = 0x31;
constexpr uint8_t DAC_VOLUME_REG = 0x32;
constexpr uint8_t DAC_RAMP_REG = 0x37;
constexpr int16_t ES8311_ZERO_DB_REGISTER = 0xBF;
constexpr uint32_t AUDIO_WRITE_STALL_TIMEOUT_MS = 3000UL;

struct __attribute__((packed)) MicrophoneFrameHeader {
    uint8_t magic[4];       // "PGM1"
    uint8_t version;
    uint8_t audioFormat;    // 1 = mono PCM16LE
    uint16_t headerBytes;
    uint32_t sampleRate;
    uint32_t requestedMs;
    uint32_t payloadBytes;
    uint32_t requestId;
    uint32_t sequence;
};

struct __attribute__((packed)) MicrophoneFrameTrailer {
    uint8_t magic[4];       // "PGM2"
    uint32_t requestId;
    uint32_t sequence;
    uint32_t payloadCrc32;
};

static_assert(sizeof(MicrophoneFrameHeader) == 28,
              "Microphone header layout changed");
static_assert(sizeof(MicrophoneFrameTrailer) == 16,
              "Microphone trailer layout changed");

struct VolumeCurvePoint {
    uint8_t percent;
    int16_t gainHalfDb;
};

struct MicrophoneGainProfile {
    MicrophoneGain gain;
    const char* name;
    uint8_t adcScaleRegister;
    uint8_t adcVolumeRegister;
    uint8_t adcScaleDb;
    int16_t adcVolumeHalfDb;
};

// REG16 bit 5 enables ADC_SYNC; bits 2:0 select 6 dB ADC scale steps.
// REG17 uses 0.5 dB steps with 0xBF representing 0 dB.
constexpr MicrophoneGainProfile MICROPHONE_GAIN_PROFILES[] = {
    {MicrophoneGain::Low, "low", 0x24, 0xC8, 24, 9},
    {MicrophoneGain::Normal, "normal", 0x26, 0xD7, 36, 24},
    {MicrophoneGain::High, "high", 0x27, 0xD7, 42, 24},
};

const MicrophoneGainProfile* microphoneGainProfile(MicrophoneGain gain) {
    for (const MicrophoneGainProfile& profile : MICROPHONE_GAIN_PROFILES) {
        if (profile.gain == gain) {
            return &profile;
        }
    }
    return nullptr;
}

// Small-speaker curve built from the AOSP shape, with the lower range lifted
// so this board's speaker remains audible below 30%. ES8311 register 0x32
// uses 0.5 dB steps, with 0xBF representing 0 dB. The top stays at 0 dB to
// avoid the codec's optional +32 dB digital boost.
constexpr VolumeCurvePoint SMALL_SPEAKER_VOLUME_CURVE[] = {
    {1, -82},    // -41.0 dB
    {10, -70},   // -35.0 dB
    {20, -64},   // -32.0 dB
    {30, -50},   // -25.0 dB
    {40, -44},   // -22.0 dB
    {50, -38},   // -19.0 dB
    {60, -34},   // -17.0 dB
    {100, 0},    //   0.0 dB
};

int16_t gainHalfDbForVolumePercent(uint8_t percent) {
    if (percent == 0) {
        return SMALL_SPEAKER_VOLUME_CURVE[0].gainHalfDb;
    }

    for (size_t index = 1;
         index < sizeof(SMALL_SPEAKER_VOLUME_CURVE) /
                     sizeof(SMALL_SPEAKER_VOLUME_CURVE[0]);
         ++index) {
        const VolumeCurvePoint& upper = SMALL_SPEAKER_VOLUME_CURVE[index];
        if (percent > upper.percent) {
            continue;
        }

        const VolumeCurvePoint& lower = SMALL_SPEAKER_VOLUME_CURVE[index - 1];
        const int32_t percentOffset = percent - lower.percent;
        const int32_t percentSpan = upper.percent - lower.percent;
        const int32_t gainSpan = upper.gainHalfDb - lower.gainHalfDb;
        return static_cast<int16_t>(
            lower.gainHalfDb +
            (percentOffset * gainSpan + percentSpan / 2) / percentSpan);
    }

    return SMALL_SPEAKER_VOLUME_CURVE[
        sizeof(SMALL_SPEAKER_VOLUME_CURVE) /
            sizeof(SMALL_SPEAKER_VOLUME_CURVE[0]) - 1]
        .gainHalfDb;
}

uint8_t codecRegisterForVolumePercent(uint8_t percent) {
    if (percent == 0) {
        return 0;
    }
    const int32_t value =
        ES8311_ZERO_DB_REGISTER + gainHalfDbForVolumePercent(percent);
    return static_cast<uint8_t>(constrain(value, 1, ES8311_ZERO_DB_REGISTER));
}

void printHalfDb(Print& output, int16_t halfDb) {
    if (halfDb > 0) {
        output.print('+');
    } else if (halfDb < 0) {
        output.print('-');
        halfDb = -halfDb;
    }
    output.print(halfDb / 2);
    output.print((halfDb & 1) != 0 ? F(".5") : F(".0"));
    output.print(F("dB"));
}

int16_t scaleSample(int16_t sample, uint8_t percent) {
    const int32_t scaled =
        static_cast<int32_t>(sample) * static_cast<int32_t>(percent) / 100;
    return static_cast<int16_t>(constrain(scaled, -32768, 32767));
}

int16_t loudestMono(const int16_t* stereo, size_t frame) {
    const int16_t left = stereo[frame * 2U];
    const int16_t right = stereo[frame * 2U + 1U];
    return std::abs(static_cast<int32_t>(left)) >=
                   std::abs(static_cast<int32_t>(right))
               ? left
               : right;
}

}  // namespace

bool AudioService::begin(Stream& log, I2cBusService& i2c) {
    log_ = &log;
    i2c_ = &i2c;
    pinMode(AMP_ENABLE_PIN, OUTPUT);
    digitalWrite(AMP_ENABLE_PIN, HIGH);  // Active-low amplifier stays off.

    preferencesReady_ = preferences_.begin("pgos_audio", false);
    if (preferencesReady_) {
        volumePercent_ = preferences_.getUChar(
            "volume", DEFAULT_VOLUME_PERCENT);
        if (volumePercent_ > 100) {
            volumePercent_ = DEFAULT_VOLUME_PERCENT;
        }
        feedbackEnabled_ = preferences_.getBool(
            "feedback", DEFAULT_FEEDBACK_ENABLED);
        micDenoiseEnabled_.store(
            preferences_.getBool("mic_denoise",
                                 DEFAULT_MIC_DENOISE_ENABLED),
            std::memory_order_relaxed);
        micVoiceEnhanceEnabled_.store(
            preferences_.getBool("mic_voice",
                                 DEFAULT_MIC_VOICE_ENHANCE_ENABLED),
            std::memory_order_relaxed);
    } else {
        micDenoiseEnabled_.store(DEFAULT_MIC_DENOISE_ENABLED,
                                 std::memory_order_relaxed);
        micVoiceEnhanceEnabled_.store(
            DEFAULT_MIC_VOICE_ENHANCE_ENABLED, std::memory_order_relaxed);
    }

    if (!i2c.ready()) {
        lastError_ = "i2c_unavailable";
        return false;
    }
    if (!initializeI2s()) {
        lastError_ = "i2s_init_failed";
        return false;
    }
    if (!initializeCodec()) {
        lastError_ = "es8311_init_failed";
        i2s_driver_uninstall(I2S_NUM_1);
        i2sInstalled_ = false;
        return false;
    }

    micRingSamples_ = SAMPLE_RATE * MIC_RING_SECONDS;
    micRing_ = static_cast<int16_t*>(heap_caps_malloc(
        micRingSamples_ * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (micRing_ == nullptr) {
        micRing_ = static_cast<int16_t*>(
            malloc(micRingSamples_ * sizeof(int16_t)));
    }
    if (micRing_ != nullptr) {
        memset(micRing_, 0, micRingSamples_ * sizeof(int16_t));
    } else {
        log.println(F("[audio] microphone ring buffer allocation failed"));
    }

    const bool denoiserRequested = microphoneDenoiseEnabled();
    const bool denoiserAllocated = micDenoiser_.begin();
    const bool denoiserReady = denoiserRequested && denoiserAllocated;
    micDenoiseActive_ = denoiserRequested;
    micDenoiserReady_.store(denoiserReady, std::memory_order_relaxed);
    if (denoiserRequested) {
        log.println(denoiserReady
                        ? F("[mic] SpeexDSP denoiser ready in PSRAM")
                        : F("[mic] SpeexDSP denoiser unavailable"));
    }

    const bool voiceRequested = microphoneVoiceEnhanceEnabled();
    const bool voiceDetectorAllocated = micVoiceDetector_.begin();
    const bool voiceDetectorReady =
        denoiserReady && voiceRequested && voiceDetectorAllocated;
    micVoiceEnhanceActive_ = voiceRequested;
    micVoiceDetectorReady_.store(voiceDetectorReady,
                                 std::memory_order_relaxed);
    if (voiceRequested && !voiceDetectorReady) {
        log.println(F("[mic] WebRTC VAD unavailable; voice AGC bypassed"));
    }

    i2s_zero_dma_buffer(I2S_NUM_1);
    if (xTaskCreate(taskEntry, "pgos_audio", AUDIO_TASK_STACK_BYTES, this, 2,
                    &taskHandle_) != pdPASS) {
        lastError_ = "audio_task_failed";
        setCodecMuted(true);
        i2s_driver_uninstall(I2S_NUM_1);
        i2sInstalled_ = false;
        return false;
    }

    ready_ = true;
    digitalWrite(AMP_ENABLE_PIN, LOW);  // Active-low amplifier on.
    log.println(F("[audio] ES8311/I2S ready; microphone ADC on GPIO6"));
    return true;
}

void AudioService::tick(uint32_t nowMs) {
    if (settingsDirty_ &&
        static_cast<int32_t>(nowMs - settingsSaveDueMs_) >= 0) {
        saveSettings();
    }
}

bool AudioService::ready() const {
    return ready_;
}

const String& AudioService::lastError() const {
    return lastError_;
}

uint8_t AudioService::volumePercent() const {
    return volumePercent_;
}

void AudioService::setVolumePercent(uint8_t percent) {
    percent = constrain(percent, static_cast<uint8_t>(0),
                         static_cast<uint8_t>(100));
    if (percent == volumePercent_) {
        return;
    }
    volumePercent_ = percent;
    if (ready_ && !applyCodecVolume()) {
        lastError_ = "volume_write_failed";
    }
    settingsDirty_ = true;
    settingsSaveDueMs_ = millis() + 750U;
}

bool AudioService::feedbackEnabled() const {
    return feedbackEnabled_;
}

void AudioService::setFeedbackEnabled(bool enabled) {
    if (feedbackEnabled_ == enabled) {
        return;
    }
    feedbackEnabled_ = enabled;
    settingsDirty_ = true;
    settingsSaveDueMs_ = millis() + 750U;
}

void AudioService::playFeedback() {
    const uint32_t nowMs = millis();
    const uint32_t gameUntilMs =
        gameToneUntilMs_.load(std::memory_order_relaxed);
    if (feedbackEnabled_ &&
        static_cast<int32_t>(gameUntilMs - nowMs) <= 0) {
        requestTone(45U);
    }
}

void AudioService::playGameTone(uint32_t durationMs) {
    requestGameTone(TONE_FREQUENCY, durationMs);
}

void AudioService::playGameTone(uint16_t frequencyHz, uint32_t durationMs) {
    requestGameTone(frequencyHz, durationMs);
}

void AudioService::playTestTone() {
    requestTone(300U);
}

bool AudioService::microphoneCaptureActive() const {
    return micCaptureEnabled_.load(std::memory_order_relaxed);
}

void AudioService::setMicrophoneCaptureRequested(
    MicrophoneCaptureSource source, bool requested) {
    const uint8_t sourceMask = static_cast<uint8_t>(source);
    if (requested) {
        micCaptureRequests_.fetch_or(sourceMask, std::memory_order_relaxed);
    } else {
        micCaptureRequests_.fetch_and(static_cast<uint8_t>(~sourceMask),
                                      std::memory_order_relaxed);
    }
    updateMicrophoneCaptureState();
}

MicrophoneGain AudioService::microphoneGain() const {
    const MicrophoneGain gain = static_cast<MicrophoneGain>(
        micGain_.load(std::memory_order_relaxed));
    return microphoneGainProfile(gain) != nullptr ? gain
                                                   : MicrophoneGain::Normal;
}

bool AudioService::setMicrophoneGain(MicrophoneGain gain) {
    if (microphoneGainProfile(gain) == nullptr) {
        return false;
    }

    const MicrophoneGain previous = microphoneGain();
    if (gain == previous) {
        return true;
    }

    micGain_.store(static_cast<uint8_t>(gain), std::memory_order_relaxed);
    if (microphoneConfigured_ && !applyMicrophoneGain()) {
        micGain_.store(static_cast<uint8_t>(previous),
                       std::memory_order_relaxed);
        (void)applyMicrophoneGain();
        lastError_ = "mic_gain_write_failed";
        return false;
    }

    if (log_ != nullptr) {
        log_->print(F("[mic] gain="));
        log_->println(microphoneGainName(gain));
    }
    return true;
}

void AudioService::cycleMicrophoneGain() {
    const uint8_t next =
        (static_cast<uint8_t>(microphoneGain()) + 1U) %
        static_cast<uint8_t>(sizeof(MICROPHONE_GAIN_PROFILES) /
                             sizeof(MICROPHONE_GAIN_PROFILES[0]));
    (void)setMicrophoneGain(static_cast<MicrophoneGain>(next));
}

const char* AudioService::microphoneGainName(MicrophoneGain gain) {
    const MicrophoneGainProfile* profile = microphoneGainProfile(gain);
    return profile != nullptr ? profile->name : "normal";
}

bool AudioService::parseMicrophoneGain(const String& value,
                                       MicrophoneGain& gain) {
    String normalized = value;
    normalized.trim();
    normalized.toLowerCase();
    for (const MicrophoneGainProfile& profile : MICROPHONE_GAIN_PROFILES) {
        if (normalized == profile.name) {
            gain = profile.gain;
            return true;
        }
    }
    return false;
}

bool AudioService::microphoneDenoiseEnabled() const {
    return micDenoiseEnabled_.load(std::memory_order_relaxed);
}

void AudioService::setMicrophoneDenoiseEnabled(bool enabled) {
    if (!enabled) {
        micVoiceEnhanceEnabled_.store(false, std::memory_order_relaxed);
    }
    const bool previous =
        micDenoiseEnabled_.exchange(enabled, std::memory_order_relaxed);
    if (previous == enabled) {
        return;
    }
    settingsDirty_ = true;
    settingsSaveDueMs_ = millis() + 750U;
    if (log_ != nullptr) {
        log_->print(F("[mic] denoise="));
        log_->println(enabled ? F("on") : F("off"));
    }
}

void AudioService::toggleMicrophoneDenoise() {
    setMicrophoneDenoiseEnabled(!microphoneDenoiseEnabled());
}

bool AudioService::microphoneVoiceEnhanceEnabled() const {
    return micVoiceEnhanceEnabled_.load(std::memory_order_relaxed);
}

void AudioService::setMicrophoneVoiceEnhanceEnabled(bool enabled) {
    if (enabled) {
        setMicrophoneDenoiseEnabled(true);
    }
    const bool previous =
        micVoiceEnhanceEnabled_.exchange(enabled, std::memory_order_relaxed);
    if (previous == enabled) {
        return;
    }
    settingsDirty_ = true;
    settingsSaveDueMs_ = millis() + 750U;
    if (log_ != nullptr) {
        log_->print(F("[mic] voice_enhance="));
        log_->println(enabled ? F("on") : F("off"));
    }
}

void AudioService::toggleMicrophoneVoiceEnhance() {
    setMicrophoneVoiceEnhanceEnabled(!microphoneVoiceEnhanceEnabled());
}

bool AudioService::microphoneMonitorEnabled() const {
    return micMonitorEnabled_.load(std::memory_order_relaxed);
}

void AudioService::setMicrophoneMonitorEnabled(bool enabled) {
    if (!ready_ || !microphoneConfigured_) {
        enabled = false;
    }
    micMonitorEnabled_.store(enabled, std::memory_order_relaxed);
    updateMicrophoneCaptureState();
}

void AudioService::toggleMicrophoneMonitor() {
    setMicrophoneMonitorEnabled(!microphoneMonitorEnabled());
}

bool AudioService::playMicrophoneBuffer(uint32_t durationMs) {
    if (!ready_ || !microphoneConfigured_ || micRing_ == nullptr ||
        micRingSamples_ == 0 || volumePercent_ == 0) {
        return false;
    }
    durationMs = constrain(durationMs, static_cast<uint32_t>(250U),
                           MIC_RECORD_MAX_MS);
    const uint32_t requestedSamples =
        min<uint32_t>((durationMs * SAMPLE_RATE) / 1000U,
                      static_cast<uint32_t>(micRingSamples_));
    const uint32_t totalSamples =
        micTotalSamples_.load(std::memory_order_acquire);
    const uint32_t availableSamples =
        min<uint32_t>(totalSamples, static_cast<uint32_t>(micRingSamples_));
    const uint32_t sampleCount = min(requestedSamples, availableSamples);
    if (sampleCount == 0) {
        return false;
    }

    const uint32_t writeIndex =
        micWriteIndex_.load(std::memory_order_acquire);
    const uint32_t readIndex =
        (writeIndex + static_cast<uint32_t>(micRingSamples_) - sampleCount) %
        static_cast<uint32_t>(micRingSamples_);
    micPlaybackReadIndex_.store(readIndex, std::memory_order_release);
    micPlaybackSamplesRemaining_.store(sampleCount,
                                       std::memory_order_release);
    return true;
}

MicrophoneSnapshot AudioService::microphoneSnapshot() const {
    MicrophoneSnapshot value;
    value.available = ready_ && microphoneConfigured_;
    value.captureActive = microphoneCaptureActive();
    value.gain = microphoneGain();
    value.denoiseEnabled = microphoneDenoiseEnabled();
    value.denoiserReady =
        micDenoiserReady_.load(std::memory_order_relaxed);
    value.captureSuppressed =
        micCaptureSuppressed_.load(std::memory_order_relaxed);
    value.voiceDetectorReady =
        micVoiceDetectorReady_.load(std::memory_order_relaxed);
    value.voiceActive = micVoiceActive_.load(std::memory_order_relaxed);
    value.voiceEnhanceEnabled = microphoneVoiceEnhanceEnabled();
    value.voiceLimiting = micVoiceLimiting_.load(std::memory_order_relaxed);
    value.monitorEnabled = microphoneMonitorEnabled();
    const uint32_t playbackSamples =
        micPlaybackSamplesRemaining_.load(std::memory_order_acquire);
    value.playbackActive = playbackSamples > 0;
    value.sampleRate = SAMPLE_RATE;
    value.framesRead = micFramesRead_.load(std::memory_order_relaxed);
    value.lastReadMs = micLastReadMs_.load(std::memory_order_relaxed);
    value.rms = micRms_.load(std::memory_order_relaxed);
    value.peak = micPeak_.load(std::memory_order_relaxed);
    value.backgroundInputRms =
        micBackgroundInputRms_.load(std::memory_order_relaxed);
    value.denoiseGainPercent =
        micDenoiseGainPercent_.load(std::memory_order_relaxed);
    value.voiceGainPercent =
        micVoiceGainPercent_.load(std::memory_order_relaxed);
    value.levelPercent = micLevelPercent_.load(std::memory_order_relaxed);
    const uint32_t totalSamples =
        micTotalSamples_.load(std::memory_order_acquire);
    const uint32_t availableSamples =
        micRingSamples_ == 0
            ? 0
            : min<uint32_t>(totalSamples,
                            static_cast<uint32_t>(micRingSamples_));
    value.ringMs = (availableSamples * 1000U) / SAMPLE_RATE;
    value.playbackRemainingMs = (playbackSamples * 1000U) / SAMPLE_RATE;
    value.readErrors = micReadErrors_.load(std::memory_order_relaxed);
    value.taskStackFree = micTaskStackFree_.load(std::memory_order_relaxed);
    return value;
}

void AudioService::writeMicrophoneRecording(Stream& output,
                                            uint32_t durationMs,
                                            uint32_t requestId) {
    durationMs = constrain(durationMs, static_cast<uint32_t>(250U),
                           MIC_RECORD_MAX_MS);
    const uint32_t requestedSamples =
        min<uint32_t>((durationMs * SAMPLE_RATE) / 1000U,
                      static_cast<uint32_t>(micRingSamples_));
    const bool temporaryCapture = !microphoneCaptureActive();
    if (temporaryCapture && ready_ && microphoneConfigured_ && micRing_ != nullptr &&
        micRingSamples_ > 0) {
        setMicrophoneCaptureRequested(MicrophoneCaptureSource::Usb, true);
        const uint32_t startMs = millis();
        while (micTotalSamples_.load(std::memory_order_acquire) <
                   requestedSamples &&
               millis() - startMs < durationMs + 250U) {
            delay(10);
        }
    }

    uint32_t sampleCount = 0;
    int16_t* capture = nullptr;
    if (ready_ && microphoneConfigured_ && micRing_ != nullptr &&
        micRingSamples_ > 0) {
        const uint32_t totalSamples =
            micTotalSamples_.load(std::memory_order_acquire);
        const uint32_t availableSamples =
            min<uint32_t>(totalSamples, static_cast<uint32_t>(micRingSamples_));
        sampleCount = min(requestedSamples, availableSamples);
        if (sampleCount > 0) {
            capture = static_cast<int16_t*>(heap_caps_malloc(
                sampleCount * sizeof(int16_t),
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (capture == nullptr) {
                capture = static_cast<int16_t*>(
                    malloc(sampleCount * sizeof(int16_t)));
            }
            if (capture == nullptr) {
                sampleCount = 0;
                lastError_ = "mic_record_buffer_failed";
            }
        }
        if (capture != nullptr) {
            const uint32_t writeIndex =
                micWriteIndex_.load(std::memory_order_acquire);
            uint32_t sourceIndex =
                (writeIndex + static_cast<uint32_t>(micRingSamples_) -
                 sampleCount) %
                static_cast<uint32_t>(micRingSamples_);
            for (uint32_t index = 0; index < sampleCount; ++index) {
                capture[index] = micRing_[sourceIndex];
                sourceIndex = (sourceIndex + 1U) %
                              static_cast<uint32_t>(micRingSamples_);
            }
        }
    }

    auto writeBytes = [&](const uint8_t* data, size_t length) -> bool {
        size_t written = 0;
        uint32_t stalledSinceMs = 0;
        while (written < length) {
            const size_t step = output.write(data + written, length - written);
            if (step == 0) {
                if (stalledSinceMs == 0) {
                    stalledSinceMs = millis();
                } else if (millis() - stalledSinceMs >=
                           AUDIO_WRITE_STALL_TIMEOUT_MS) {
                    return false;
                }
                delay(1);
                continue;
            }
            stalledSinceMs = 0;
            written += step;
        }
        return true;
    };

    const uint32_t sequence = ++micRecordSequence_;
    MicrophoneFrameHeader header = {
        {'P', 'G', 'M', '1'},
        1,
        1,
        static_cast<uint16_t>(sizeof(MicrophoneFrameHeader)),
        SAMPLE_RATE,
        durationMs,
        sampleCount * static_cast<uint32_t>(sizeof(int16_t)),
        requestId,
        sequence,
    };
    if (!writeBytes(reinterpret_cast<const uint8_t*>(&header),
                    sizeof(header))) {
        if (capture != nullptr) {
            heap_caps_free(capture);
        }
        if (temporaryCapture) {
            setMicrophoneCaptureRequested(MicrophoneCaptureSource::Usb,
                                          false);
        }
        return;
    }
    output.flush();

    uint8_t chunk[512];
    uint32_t payloadCrc = 0;
    uint32_t offset = 0;
    while (offset < sampleCount) {
        const uint32_t count =
            min<uint32_t>(sampleCount - offset, sizeof(chunk) / 2U);
        for (uint32_t index = 0; index < count; ++index) {
            const uint16_t value =
                static_cast<uint16_t>(capture[offset + index]);
            chunk[index * 2U] = static_cast<uint8_t>(value & 0xFFU);
            chunk[index * 2U + 1U] = static_cast<uint8_t>(value >> 8U);
        }
        const size_t byteCount = count * 2U;
        payloadCrc = esp_crc32_le(payloadCrc, chunk,
                                  static_cast<uint32_t>(byteCount));
        if (!writeBytes(chunk, byteCount)) {
            heap_caps_free(capture);
            if (temporaryCapture) {
                setMicrophoneCaptureRequested(MicrophoneCaptureSource::Usb,
                                              false);
            }
            return;
        }
        offset += count;
        yield();
    }

    MicrophoneFrameTrailer trailer = {
        {'P', 'G', 'M', '2'}, requestId, sequence, payloadCrc};
    (void)writeBytes(reinterpret_cast<const uint8_t*>(&trailer),
                     sizeof(trailer));
    output.flush();
    if (capture != nullptr) {
        heap_caps_free(capture);
    }
    if (temporaryCapture) {
        setMicrophoneCaptureRequested(MicrophoneCaptureSource::Usb, false);
    }
}

void AudioService::printStatus(Print& output) const {
    output.print(F("[audio] state="));
    output.print(ready_ ? F("ready") : F("unavailable"));
    output.print(F(" volume="));
    output.print(volumePercent_);
    output.print(F("% curve=small-speaker"));
    if (volumePercent_ == 0) {
        output.print(F(" gain=mute"));
    } else {
        output.print(F(" gain="));
        printHalfDb(output, gainHalfDbForVolumePercent(volumePercent_));
    }
    output.print(F(" feedback="));
    output.print(feedbackEnabled_ ? F("on") : F("off"));
    if (!lastError_.isEmpty()) {
        output.print(F(" error="));
        output.print(lastError_);
    }
    output.println();

    const MicrophoneSnapshot mic = microphoneSnapshot();
    output.print(F("[mic] state="));
    output.print(mic.available ? F("ready") : F("unavailable"));
    output.print(F(" route=ES8311-ADC/GPIO6 sample_rate="));
    output.print(mic.sampleRate);
    const MicrophoneGainProfile* gainProfile =
        microphoneGainProfile(mic.gain);
    if (gainProfile != nullptr) {
        output.print(F("Hz gain="));
        output.print(gainProfile->name);
        output.print(F(" pga=30dB adc_scale="));
        output.print(gainProfile->adcScaleDb);
        output.print(F("dB adc_volume="));
        printHalfDb(output, gainProfile->adcVolumeHalfDb);
    }
    output.print(F(" rms="));
    output.print(mic.rms);
    output.print(F(" peak="));
    output.print(mic.peak);
    output.print(F(" level="));
    output.print(mic.levelPercent);
    output.print(F("% frames="));
    output.print(mic.framesRead);
    output.print(F(" ring_ms="));
    output.print(mic.ringMs);
    output.print(F(" capture="));
    output.print(mic.captureActive ? F("active") : F("idle"));
    output.print(F(" capture_gate="));
    output.print(mic.captureSuppressed ? F("closed") : F("open"));
    output.print(F(" monitor="));
    output.print(mic.monitorEnabled ? F("on") : F("off"));
    output.print(F(" denoise="));
    output.print(mic.denoiseEnabled ? F("on") : F("off"));
    output.print(F(" ns="));
    output.print(!mic.denoiseEnabled
                     ? F("bypass")
                     : (!mic.captureActive
                            ? F("idle")
                            : (mic.denoiserReady ? F("speex")
                                                 : F("unavailable"))));
    output.print(F(" background_input_rms="));
    output.print(mic.backgroundInputRms);
    output.print(F(" denoise_gain="));
    output.print(mic.denoiseGainPercent);
    output.print(F("% vad="));
    output.print(!mic.voiceEnhanceEnabled
                     ? F("bypass")
                     : (!mic.captureActive
                            ? F("idle")
                            : (mic.voiceDetectorReady ? F("webrtc3")
                                                      : F("unavailable"))));
    output.print(F(" voice="));
    output.print(mic.voiceActive ? F("yes") : F("no"));
    output.print(F(" voice_enhance="));
    output.print(mic.voiceEnhanceEnabled ? F("on") : F("off"));
    output.print(F(" voice_gain="));
    output.print(mic.voiceGainPercent);
    output.print(F("% limiting="));
    output.print(mic.voiceLimiting ? F("yes") : F("no"));
    output.print(F(" playback_ms="));
    output.print(mic.playbackRemainingMs);
    output.print(F(" last_ms="));
    output.print(mic.lastReadMs);
    output.print(F(" errors="));
    output.print(mic.readErrors);
    output.print(F(" task_stack_free="));
    output.println(mic.taskStackFree);
}

bool AudioService::initializeI2s() {
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX |
                                          I2S_MODE_RX);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 4;
    config.dma_buf_len = DMA_FRAMES;
    config.use_apll = true;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = SAMPLE_RATE * 384U;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    config.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;

    if (i2s_driver_install(I2S_NUM_1, &config, 0, nullptr) != ESP_OK) {
        return false;
    }
    i2sInstalled_ = true;

    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_MCLK_PIN;
    pins.bck_io_num = I2S_BCLK_PIN;
    pins.ws_io_num = I2S_WS_PIN;
    pins.data_out_num = I2S_DATA_OUT_PIN;
    pins.data_in_num = I2S_DATA_IN_PIN;
    if (i2s_set_pin(I2S_NUM_1, &pins) != ESP_OK) {
        i2s_driver_uninstall(I2S_NUM_1);
        i2sInstalled_ = false;
        return false;
    }
    return true;
}

bool AudioService::initializeCodec() {
    if (!writeRegister(RESET_REG, 0x1F)) {
        return false;
    }
    delay(20);
    if (!writeRegister(RESET_REG, 0x00) ||
        !writeRegister(RESET_REG, 0x80) || !configureFixed8kClock()) {
        return false;
    }

    uint8_t reset = 0;
    if (!readRegister(RESET_REG, reset) ||
        !writeRegister(RESET_REG, static_cast<uint8_t>(reset & 0xBF)) ||
        !writeRegister(SDP_INPUT_REG, 0x0C) ||
        !writeRegister(SDP_OUTPUT_REG, 0x0C) ||
        !writeRegister(SYSTEM_REG0D, 0x01) ||
        !writeRegister(SYSTEM_REG0E, 0x02) ||
        !writeRegister(SYSTEM_REG12, 0x00) ||
        !writeRegister(SYSTEM_REG13, 0x10) ||
        !writeRegister(ADC_EQ_REG, 0x6A) ||
        !configureMicrophone() ||
        !writeRegister(DAC_RAMP_REG, 0x08) || !applyCodecVolume()) {
        return false;
    }
    return true;
}

bool AudioService::configureFixed8kClock() {
    if (!writeRegister(CLOCK_REG01, 0x3F)) {
        return false;
    }
    uint8_t value = 0;
    if (!readRegister(CLOCK_REG02, value) ||
        !writeRegister(CLOCK_REG02, static_cast<uint8_t>(value & 0x07)) ||
        !writeRegister(CLOCK_REG03, 0x10) ||
        !writeRegister(CLOCK_REG04, 0x10) ||
        !writeRegister(CLOCK_REG05, 0x00) ||
        !readRegister(CLOCK_REG06, value)) {
        return false;
    }
    value = static_cast<uint8_t>((value & 0xC0) | 0x03);
    if (!writeRegister(CLOCK_REG06, value) ||
        !readRegister(CLOCK_REG07, value) ||
        !writeRegister(CLOCK_REG07, static_cast<uint8_t>(value & 0xC0)) ||
        !writeRegister(CLOCK_REG08, 0xFF)) {
        return false;
    }
    return true;
}

bool AudioService::configureMicrophone() {
    // SYSTEM14 selects the analog MIC input and the maximum 30 dB PGA.
    if (!writeRegister(SYSTEM_REG14, 0x1A) || !applyMicrophoneGain()) {
        microphoneConfigured_ = false;
        return false;
    }
    microphoneConfigured_ = true;
    return true;
}

bool AudioService::applyMicrophoneGain() {
    const MicrophoneGainProfile* profile =
        microphoneGainProfile(microphoneGain());
    return profile != nullptr &&
           writeRegister(ADC_SCALE_REG, profile->adcScaleRegister) &&
           writeRegister(ADC_VOLUME_REG, profile->adcVolumeRegister);
}

bool AudioService::writeRegister(uint8_t address, uint8_t value) {
    TwoWire& wire = i2c_->wire();
    wire.beginTransmission(ES8311_ADDRESS);
    wire.write(address);
    wire.write(value);
    return wire.endTransmission(true) == 0;
}

bool AudioService::readRegister(uint8_t address, uint8_t& value) {
    TwoWire& wire = i2c_->wire();
    wire.beginTransmission(ES8311_ADDRESS);
    wire.write(address);
    if (wire.endTransmission(false) != 0 ||
        wire.requestFrom(ES8311_ADDRESS, static_cast<uint8_t>(1),
                         static_cast<uint8_t>(true)) != 1) {
        return false;
    }
    value = static_cast<uint8_t>(wire.read());
    return true;
}

bool AudioService::setCodecMuted(bool muted) {
    uint8_t value = 0;
    if (!readRegister(DAC_MUTE_REG, value)) {
        return false;
    }
    if (muted) {
        value = static_cast<uint8_t>(value | (1U << 6U) | (1U << 5U));
    } else {
        value = static_cast<uint8_t>(value & ~((1U << 6U) | (1U << 5U)));
    }
    return writeRegister(DAC_MUTE_REG, value);
}

bool AudioService::applyCodecVolume() {
    const uint8_t value = codecRegisterForVolumePercent(volumePercent_);
    return writeRegister(DAC_VOLUME_REG, value) &&
           setCodecMuted(volumePercent_ == 0);
}

void AudioService::requestTone(uint32_t durationMs) {
    requestTone(TONE_FREQUENCY, durationMs);
}

void AudioService::requestTone(uint16_t frequencyHz, uint32_t durationMs) {
    if (!ready_ || volumePercent_ == 0) {
        return;
    }
    frequencyHz = constrain(frequencyHz, static_cast<uint16_t>(80),
                            static_cast<uint16_t>(2000));
    toneFrequencyHz_.store(frequencyHz, std::memory_order_relaxed);
    const uint32_t requestedUntil = millis() + durationMs;
    const uint32_t currentUntil = toneUntilMs_.load(std::memory_order_relaxed);
    if (static_cast<int32_t>(requestedUntil - currentUntil) > 0) {
        toneUntilMs_.store(requestedUntil, std::memory_order_relaxed);
    }
}

void AudioService::requestGameTone(uint16_t frequencyHz,
                                   uint32_t durationMs) {
    if (!ready_ || volumePercent_ == 0) {
        return;
    }
    frequencyHz = constrain(frequencyHz, static_cast<uint16_t>(80),
                            static_cast<uint16_t>(2000));
    toneFrequencyHz_.store(frequencyHz, std::memory_order_relaxed);

    const uint32_t nowMs = millis();
    const uint32_t requestedUntil = nowMs + durationMs;
    uint32_t gameUntilMs =
        gameToneUntilMs_.load(std::memory_order_relaxed);
    if (static_cast<int32_t>(gameUntilMs - nowMs) <= 0 ||
        static_cast<int32_t>(requestedUntil - gameUntilMs) > 0) {
        gameUntilMs = requestedUntil;
        gameToneUntilMs_.store(gameUntilMs, std::memory_order_relaxed);
    }
    // A game cue must replace any lower-priority shell feedback already in
    // flight, rather than only changing its pitch and inheriting its tail.
    toneUntilMs_.store(gameUntilMs, std::memory_order_relaxed);
}

void AudioService::saveSettings() {
    settingsDirty_ = false;
    if (!preferencesReady_) {
        return;
    }
    preferences_.putUChar("volume", volumePercent_);
    preferences_.putBool("feedback", feedbackEnabled_);
    preferences_.putBool("mic_denoise", microphoneDenoiseEnabled());
    preferences_.putBool("mic_voice", microphoneVoiceEnhanceEnabled());
    if (log_ != nullptr) {
        log_->println(F("[audio] settings saved"));
    }
}

void AudioService::updateMicrophoneCaptureState() {
    const bool requested =
        micCaptureRequests_.load(std::memory_order_relaxed) != 0U ||
        microphoneMonitorEnabled();
    const bool active = microphoneCaptureActive();
    if (requested == active) {
        return;
    }

    if (requested) {
        micWriteIndex_.store(0, std::memory_order_relaxed);
        micTotalSamples_.store(0, std::memory_order_relaxed);
        micPlaybackSamplesRemaining_.store(0, std::memory_order_relaxed);
        resetMicrophoneCaptureMetrics();
        micCaptureEnabled_.store(true, std::memory_order_release);
    } else {
        micCaptureEnabled_.store(false, std::memory_order_release);
        resetMicrophoneCaptureMetrics();
    }

    if (log_ != nullptr) {
        log_->print(F("[mic] capture="));
        log_->println(requested ? F("active") : F("idle"));
    }
}

void AudioService::resetMicrophoneCaptureMetrics() {
    micLastReadMs_.store(0, std::memory_order_relaxed);
    micRms_.store(0, std::memory_order_relaxed);
    micPeak_.store(0, std::memory_order_relaxed);
    micBackgroundInputRms_.store(0, std::memory_order_relaxed);
    micDenoiseGainPercent_.store(100, std::memory_order_relaxed);
    micVoiceActive_.store(false, std::memory_order_relaxed);
    micVoiceGainPercent_.store(100, std::memory_order_relaxed);
    micVoiceLimiting_.store(false, std::memory_order_relaxed);
    micLevelPercent_.store(0, std::memory_order_relaxed);
    micCaptureSuppressed_.store(false, std::memory_order_relaxed);
    micCaptureSuppressUntilMs_.store(0, std::memory_order_relaxed);
}

void AudioService::resetMicrophoneProcessors() {
    micDenoiser_.reset();
    (void)micVoiceDetector_.reset();
    micVoiceEnhancer_.reset();
    micDenoiseActive_ = false;
    micVoiceEnhanceActive_ = false;
    micDenoiserReady_.store(false, std::memory_order_relaxed);
    micVoiceDetectorReady_.store(false, std::memory_order_relaxed);
    micBackgroundInputRms_.store(0, std::memory_order_relaxed);
    micDenoiseGainPercent_.store(100, std::memory_order_relaxed);
    micVoiceActive_.store(false, std::memory_order_relaxed);
    micVoiceGainPercent_.store(100, std::memory_order_relaxed);
    micVoiceLimiting_.store(false, std::memory_order_relaxed);
}

size_t AudioService::processMicrophoneSamples(const int16_t* samples,
                                              size_t sampleCount,
                                              int16_t* output,
                                              size_t outputCapacity) {
    if (samples == nullptr || output == nullptr || sampleCount < 2U ||
        outputCapacity == 0) {
        return 0;
    }

    const size_t frames = min(sampleCount / 2U, outputCapacity);
    for (size_t frame = 0; frame < frames; ++frame) {
        output[frame] = loudestMono(samples, frame);
    }

    const uint32_t nowMs = millis();
    const uint32_t suppressUntilMs =
        micCaptureSuppressUntilMs_.load(std::memory_order_relaxed);
    const bool suppressCapture =
        static_cast<int32_t>(suppressUntilMs - nowMs) > 0;
    micCaptureSuppressed_.store(suppressCapture, std::memory_order_relaxed);

    const bool denoiseEnabled = microphoneDenoiseEnabled();
    if (suppressCapture) {
        if (micDenoiseActive_ || micVoiceEnhanceActive_ ||
            micVoiceDetectorReady_.load(std::memory_order_relaxed)) {
            resetMicrophoneProcessors();
        }
    } else if (denoiseEnabled != micDenoiseActive_) {
        bool denoiserReady = micDenoiser_.ready();
        if (denoiseEnabled) {
            if (!denoiserReady) {
                denoiserReady = micDenoiser_.begin();
            }
            if (log_ != nullptr) {
                log_->println(denoiserReady
                                  ? F("[mic] SpeexDSP denoiser ready")
                                  : F("[mic] SpeexDSP denoiser unavailable"));
            }
        }
        micDenoiserReady_.store(denoiseEnabled && denoiserReady,
                                std::memory_order_relaxed);
        (void)micVoiceDetector_.reset();
        micVoiceDetectorReady_.store(false, std::memory_order_relaxed);
        micVoiceEnhancer_.reset();
        micVoiceEnhanceActive_ = false;
        micDenoiseActive_ = denoiseEnabled;
    }
    const bool denoiserReady =
        !suppressCapture && denoiseEnabled && micDenoiser_.ready();
    if (denoiserReady) {
        micDenoiser_.process(output, frames);
        const MicrophoneDenoiseMetrics metrics = micDenoiser_.metrics();
        micBackgroundInputRms_.store(metrics.backgroundInputRms,
                                     std::memory_order_relaxed);
        micDenoiseGainPercent_.store(metrics.gainPercent,
                                     std::memory_order_relaxed);

        const bool voiceEnhanceEnabled = microphoneVoiceEnhanceEnabled();
        if (voiceEnhanceEnabled != micVoiceEnhanceActive_) {
            micVoiceEnhancer_.reset();
            const bool voiceDetectorAllocated = micVoiceDetector_.reset();
            const bool voiceDetectorReady =
                voiceEnhanceEnabled && voiceDetectorAllocated;
            micVoiceDetectorReady_.store(voiceDetectorReady,
                                         std::memory_order_relaxed);
            if (voiceEnhanceEnabled && !voiceDetectorReady && log_ != nullptr) {
                log_->println(
                    F("[mic] WebRTC VAD unavailable; voice AGC bypassed"));
            }
            micVoiceEnhanceActive_ = voiceEnhanceEnabled;
        }
        bool voiceActive = false;
        if (voiceEnhanceEnabled &&
            micVoiceDetectorReady_.load(std::memory_order_relaxed)) {
            const MicrophoneVoiceDetectionMetrics detection =
                micVoiceDetector_.process(output, frames);
            micVoiceDetectorReady_.store(detection.ready,
                                         std::memory_order_relaxed);
            voiceActive = detection.voiceActive;
        }
        micVoiceActive_.store(voiceActive, std::memory_order_relaxed);
        if (voiceEnhanceEnabled) {
            micVoiceEnhancer_.process(output, frames, voiceActive);
            const MicrophoneVoiceEnhanceMetrics voiceMetrics =
                micVoiceEnhancer_.metrics();
            micVoiceGainPercent_.store(voiceMetrics.gainPercent,
                                       std::memory_order_relaxed);
            micVoiceLimiting_.store(voiceMetrics.limiting,
                                    std::memory_order_relaxed);
        } else {
            micVoiceGainPercent_.store(100, std::memory_order_relaxed);
            micVoiceLimiting_.store(false, std::memory_order_relaxed);
        }
    } else {
        micBackgroundInputRms_.store(0, std::memory_order_relaxed);
        micDenoiseGainPercent_.store(100, std::memory_order_relaxed);
        micVoiceActive_.store(false, std::memory_order_relaxed);
        micVoiceGainPercent_.store(100, std::memory_order_relaxed);
        micVoiceLimiting_.store(false, std::memory_order_relaxed);
        micVoiceEnhanceActive_ = false;
    }

    uint64_t squareSum = 0;
    uint16_t peak = 0;
    uint32_t writeIndex = micWriteIndex_.load(std::memory_order_relaxed);

    for (size_t frame = 0; frame < frames; ++frame) {
        const int16_t mono = output[frame];
        const uint16_t magnitude =
            static_cast<uint16_t>(min<int32_t>(
                32767, std::abs(static_cast<int32_t>(mono))));
        peak = max<uint16_t>(peak, magnitude);
        squareSum += static_cast<uint32_t>(magnitude) * magnitude;

        if (!suppressCapture && micRing_ != nullptr && micRingSamples_ > 0) {
            micRing_[writeIndex] = mono;
            writeIndex =
                (writeIndex + 1U) % static_cast<uint32_t>(micRingSamples_);
        }
    }

    const uint32_t rms = static_cast<uint32_t>(
        sqrt(static_cast<double>(squareSum) / static_cast<double>(frames)));
    const uint8_t level = static_cast<uint8_t>(min<uint32_t>(
        100U, (rms * 100U + MIC_RMS_FULL_SCALE / 2U) /
                  MIC_RMS_FULL_SCALE));

    if (!suppressCapture && micRing_ != nullptr && micRingSamples_ > 0) {
        micWriteIndex_.store(writeIndex, std::memory_order_release);
        micTotalSamples_.fetch_add(static_cast<uint32_t>(frames),
                                   std::memory_order_release);
    }
    micFramesRead_.fetch_add(static_cast<uint32_t>(frames),
                             std::memory_order_relaxed);
    micLastReadMs_.store(millis(), std::memory_order_relaxed);
    micRms_.store(static_cast<uint16_t>(min<uint32_t>(rms, UINT16_MAX)),
                  std::memory_order_relaxed);
    micPeak_.store(peak, std::memory_order_relaxed);
    micLevelPercent_.store(level, std::memory_order_relaxed);
    return frames;
}

void AudioService::run() {
    int16_t samples[DMA_FRAMES * 2U];
    int16_t microphoneSamples[DMA_FRAMES * 2U];
    int16_t microphoneMono[DMA_FRAMES];
    uint32_t phase = 0;
    uint16_t previousFrequency = 0;
    bool captureWasActive = false;

    while (true) {
        size_t microphoneFrameCount = 0;
        const bool captureActive =
            microphoneConfigured_ && microphoneCaptureActive();
        if (captureActive) {
            size_t bytesRead = 0;
            const esp_err_t readResult =
                i2s_read(I2S_NUM_1, microphoneSamples,
                         sizeof(microphoneSamples), &bytesRead,
                         pdMS_TO_TICKS(25));
            if (readResult == ESP_OK &&
                bytesRead == sizeof(microphoneSamples)) {
                microphoneFrameCount = processMicrophoneSamples(
                    microphoneSamples, bytesRead / sizeof(int16_t),
                    microphoneMono, DMA_FRAMES);
            } else if (readResult == ESP_OK && bytesRead > 0) {
                micReadErrors_.fetch_add(1, std::memory_order_relaxed);
            } else if (readResult != ESP_ERR_TIMEOUT) {
                micReadErrors_.fetch_add(1, std::memory_order_relaxed);
            }
        } else if (captureWasActive) {
            resetMicrophoneProcessors();
        }
        captureWasActive = captureActive;

        const uint32_t nowMs = millis();
        const uint32_t untilMs = toneUntilMs_.load(std::memory_order_relaxed);
        const bool toneOn = static_cast<int32_t>(untilMs - nowMs) > 0;
        uint32_t playbackRemaining =
            micPlaybackSamplesRemaining_.load(std::memory_order_acquire);
        uint32_t playbackReadIndex =
            micPlaybackReadIndex_.load(std::memory_order_acquire);
        const bool monitorOn =
            micMonitorEnabled_.load(std::memory_order_relaxed) &&
            microphoneFrameCount > 0 && volumePercent_ > 0;
        const bool playbackOutputOn = playbackRemaining > 0 &&
                                      micRing_ != nullptr &&
                                      micRingSamples_ > 0;
        const bool localOutputOn = toneOn || playbackOutputOn || monitorOn;
        if (localOutputOn) {
            micCaptureSuppressUntilMs_.store(
                nowMs + MIC_OUTPUT_SUPPRESS_TAIL_MS,
                std::memory_order_relaxed);
            micCaptureSuppressed_.store(true, std::memory_order_relaxed);
        }
        const uint16_t frequency =
            toneFrequencyHz_.load(std::memory_order_relaxed);
        if (frequency != previousFrequency) {
            phase = 0;
            previousFrequency = frequency;
        }
        const uint32_t samplesPerPeriod =
            std::max<uint32_t>(2U, SAMPLE_RATE / frequency);
        const uint32_t halfPeriod = samplesPerPeriod / 2U;
        for (size_t frame = 0; frame < DMA_FRAMES; ++frame) {
            int16_t sample = 0;
            if (toneOn) {
                sample = phase < halfPeriod ? TONE_AMPLITUDE
                                            : -TONE_AMPLITUDE;
            } else if (playbackRemaining > 0 && micRing_ != nullptr &&
                       micRingSamples_ > 0) {
                sample = micRing_[playbackReadIndex];
                playbackReadIndex =
                    (playbackReadIndex + 1U) %
                    static_cast<uint32_t>(micRingSamples_);
                playbackRemaining--;
            } else if (monitorOn && frame < microphoneFrameCount) {
                sample = scaleSample(microphoneMono[frame],
                                     MIC_MONITOR_GAIN_PERCENT);
            }
            samples[frame * 2U] = sample;
            samples[frame * 2U + 1U] = sample;
            phase = toneOn ? (phase + 1U) % samplesPerPeriod : 0;
        }
        micPlaybackReadIndex_.store(playbackReadIndex,
                                    std::memory_order_release);
        micPlaybackSamplesRemaining_.store(playbackRemaining,
                                           std::memory_order_release);

        size_t bytesWritten = 0;
        const esp_err_t result =
            i2s_write(I2S_NUM_1, samples, sizeof(samples), &bytesWritten,
                      pdMS_TO_TICKS(100));
        if (result != ESP_OK || bytesWritten != sizeof(samples)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        micTaskStackFree_.store(
            static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr)),
            std::memory_order_relaxed);
    }
}

void AudioService::taskEntry(void* context) {
    static_cast<AudioService*>(context)->run();
}
