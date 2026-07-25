#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include <atomic>

#include "audio/MicrophoneDenoiser.h"
#include "audio/MicrophoneVoiceDetector.h"
#include "audio/MicrophoneVoiceEnhancer.h"

class I2cBusService;

enum class MicrophoneGain : uint8_t {
    Low,
    Normal,
    High,
};

enum class MicrophoneCaptureSource : uint8_t {
    Foreground = 1U << 0U,
    Usb = 1U << 1U,
};

struct MicrophoneSnapshot {
    bool available = false;
    bool captureActive = false;
    bool monitorEnabled = false;
    bool playbackActive = false;
    bool denoiseEnabled = false;
    bool denoiserReady = false;
    bool captureSuppressed = false;
    bool voiceDetectorReady = false;
    bool voiceActive = false;
    bool voiceEnhanceEnabled = false;
    bool voiceLimiting = false;
    MicrophoneGain gain = MicrophoneGain::Normal;
    uint32_t sampleRate = 0;
    uint32_t framesRead = 0;
    uint32_t lastReadMs = 0;
    uint16_t rms = 0;
    uint16_t peak = 0;
    uint16_t backgroundInputRms = 0;
    uint8_t denoiseGainPercent = 100;
    uint16_t voiceGainPercent = 100;
    uint8_t levelPercent = 0;
    uint32_t ringMs = 0;
    uint32_t playbackRemainingMs = 0;
    uint32_t readErrors = 0;
    uint32_t taskStackFree = 0;
};

class AudioService {
public:
    bool begin(Stream& log, I2cBusService& i2c);
    void tick(uint32_t nowMs);

    bool ready() const;
    const String& lastError() const;
    uint8_t volumePercent() const;
    void setVolumePercent(uint8_t percent);
    bool feedbackEnabled() const;
    void setFeedbackEnabled(bool enabled);
    void playFeedback();
    void playGameTone(uint32_t durationMs);
    void playGameTone(uint16_t frequencyHz, uint32_t durationMs);
    void playTestTone();
    MicrophoneSnapshot microphoneSnapshot() const;
    bool microphoneCaptureActive() const;
    void setMicrophoneCaptureRequested(MicrophoneCaptureSource source,
                                       bool requested);
    MicrophoneGain microphoneGain() const;
    bool setMicrophoneGain(MicrophoneGain gain);
    void cycleMicrophoneGain();
    static const char* microphoneGainName(MicrophoneGain gain);
    static bool parseMicrophoneGain(const String& value,
                                    MicrophoneGain& gain);
    bool microphoneDenoiseEnabled() const;
    void setMicrophoneDenoiseEnabled(bool enabled);
    void toggleMicrophoneDenoise();
    bool microphoneVoiceEnhanceEnabled() const;
    void setMicrophoneVoiceEnhanceEnabled(bool enabled);
    void toggleMicrophoneVoiceEnhance();
    bool microphoneMonitorEnabled() const;
    void setMicrophoneMonitorEnabled(bool enabled);
    void toggleMicrophoneMonitor();
    bool playMicrophoneBuffer(uint32_t durationMs);
    void writeMicrophoneRecording(Stream& output, uint32_t durationMs,
                                  uint32_t requestId);
    void printStatus(Print& output) const;

private:
    static constexpr uint8_t AMP_ENABLE_PIN = 1;
    static constexpr uint8_t I2S_MCLK_PIN = 4;
    static constexpr uint8_t I2S_BCLK_PIN = 5;
    static constexpr uint8_t I2S_WS_PIN = 7;
    static constexpr uint8_t I2S_DATA_OUT_PIN = 8;
    static constexpr uint8_t I2S_DATA_IN_PIN = 6;
    static constexpr uint8_t ES8311_ADDRESS = 0x18;
    static constexpr uint32_t SAMPLE_RATE = 8000;
    static constexpr uint16_t TONE_FREQUENCY = 1000;
    static constexpr int16_t TONE_AMPLITUDE = 3500;
    static constexpr size_t DMA_FRAMES = 160;
    static constexpr uint16_t MIC_RMS_FULL_SCALE = 4096;
    static constexpr uint32_t MIC_RECORD_MAX_MS = 5000;
    static constexpr uint32_t MIC_RING_SECONDS = 6;
    static constexpr uint8_t MIC_MONITOR_GAIN_PERCENT = 35;
    static constexpr uint32_t MIC_OUTPUT_SUPPRESS_TAIL_MS = 300;
    static constexpr uint32_t AUDIO_TASK_STACK_BYTES = 8192;
    static constexpr uint8_t DEFAULT_VOLUME_PERCENT = 60;
    static constexpr bool DEFAULT_FEEDBACK_ENABLED = false;
    static constexpr bool DEFAULT_MIC_DENOISE_ENABLED = true;
    static constexpr bool DEFAULT_MIC_VOICE_ENHANCE_ENABLED = true;

    Stream* log_ = nullptr;
    I2cBusService* i2c_ = nullptr;
    Preferences preferences_;
    TaskHandle_t taskHandle_ = nullptr;
    std::atomic<uint32_t> toneUntilMs_{0};
    std::atomic<uint32_t> gameToneUntilMs_{0};
    std::atomic<uint16_t> toneFrequencyHz_{TONE_FREQUENCY};
    std::atomic<uint32_t> micFramesRead_{0};
    std::atomic<uint32_t> micLastReadMs_{0};
    std::atomic<uint16_t> micRms_{0};
    std::atomic<uint16_t> micPeak_{0};
    std::atomic<uint16_t> micBackgroundInputRms_{0};
    std::atomic<uint8_t> micDenoiseGainPercent_{100};
    std::atomic<bool> micDenoiserReady_{false};
    std::atomic<bool> micVoiceDetectorReady_{false};
    std::atomic<bool> micVoiceActive_{false};
    std::atomic<uint16_t> micVoiceGainPercent_{100};
    std::atomic<bool> micVoiceLimiting_{false};
    std::atomic<uint8_t> micLevelPercent_{0};
    std::atomic<uint32_t> micReadErrors_{0};
    std::atomic<uint32_t> micTotalSamples_{0};
    std::atomic<uint32_t> micWriteIndex_{0};
    std::atomic<uint32_t> micCaptureSuppressUntilMs_{0};
    std::atomic<bool> micCaptureSuppressed_{false};
    std::atomic<uint32_t> micTaskStackFree_{0};
    std::atomic<uint8_t> micCaptureRequests_{0};
    std::atomic<bool> micCaptureEnabled_{false};
    std::atomic<uint8_t> micGain_{
        static_cast<uint8_t>(MicrophoneGain::Normal)};
    std::atomic<bool> micDenoiseEnabled_{false};
    std::atomic<bool> micVoiceEnhanceEnabled_{false};
    std::atomic<bool> micMonitorEnabled_{false};
    std::atomic<uint32_t> micPlaybackReadIndex_{0};
    std::atomic<uint32_t> micPlaybackSamplesRemaining_{0};
    int16_t* micRing_ = nullptr;
    size_t micRingSamples_ = 0;
    uint32_t micRecordSequence_ = 0;
    MicrophoneDenoiser micDenoiser_;
    MicrophoneVoiceDetector micVoiceDetector_;
    MicrophoneVoiceEnhancer micVoiceEnhancer_;
    bool micDenoiseActive_ = false;
    bool micVoiceEnhanceActive_ = false;
    bool ready_ = false;
    bool microphoneConfigured_ = false;
    bool preferencesReady_ = false;
    bool settingsDirty_ = false;
    bool i2sInstalled_ = false;
    uint8_t volumePercent_ = DEFAULT_VOLUME_PERCENT;
    bool feedbackEnabled_ = DEFAULT_FEEDBACK_ENABLED;
    uint32_t settingsSaveDueMs_ = 0;
    String lastError_;

    bool initializeI2s();
    bool initializeCodec();
    bool configureFixed8kClock();
    bool configureMicrophone();
    bool applyMicrophoneGain();
    bool writeRegister(uint8_t address, uint8_t value);
    bool readRegister(uint8_t address, uint8_t& value);
    bool setCodecMuted(bool muted);
    bool applyCodecVolume();
    void requestTone(uint32_t durationMs);
    void requestTone(uint16_t frequencyHz, uint32_t durationMs);
    void requestGameTone(uint16_t frequencyHz, uint32_t durationMs);
    void saveSettings();
    void updateMicrophoneCaptureState();
    void resetMicrophoneCaptureMetrics();
    void resetMicrophoneProcessors();
    size_t processMicrophoneSamples(const int16_t* samples,
                                    size_t sampleCount, int16_t* output,
                                    size_t outputCapacity);
    void run();
    static void taskEntry(void* context);
};
