#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include <atomic>

class I2cBusService;

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
    void playTestTone();
    void printStatus(Print& output) const;

private:
    static constexpr uint8_t AMP_ENABLE_PIN = 1;
    static constexpr uint8_t I2S_MCLK_PIN = 4;
    static constexpr uint8_t I2S_BCLK_PIN = 5;
    static constexpr uint8_t I2S_WS_PIN = 7;
    static constexpr uint8_t I2S_DATA_OUT_PIN = 8;
    static constexpr uint8_t ES8311_ADDRESS = 0x18;
    static constexpr uint32_t SAMPLE_RATE = 8000;
    static constexpr uint16_t TONE_FREQUENCY = 1000;
    static constexpr int16_t TONE_AMPLITUDE = 3500;
    static constexpr size_t DMA_FRAMES = 128;
    static constexpr uint8_t DEFAULT_VOLUME_PERCENT = 60;
    static constexpr bool DEFAULT_FEEDBACK_ENABLED = false;

    Stream* log_ = nullptr;
    I2cBusService* i2c_ = nullptr;
    Preferences preferences_;
    TaskHandle_t taskHandle_ = nullptr;
    std::atomic<uint32_t> toneUntilMs_{0};
    bool ready_ = false;
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
    bool writeRegister(uint8_t address, uint8_t value);
    bool readRegister(uint8_t address, uint8_t& value);
    bool setCodecMuted(bool muted);
    bool applyCodecVolume();
    void requestTone(uint32_t durationMs);
    void saveSettings();
    void run();
    static void taskEntry(void* context);
};
