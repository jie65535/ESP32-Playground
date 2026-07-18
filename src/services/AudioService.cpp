#include "services/AudioService.h"

#include <Wire.h>
#include <driver/i2s.h>

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
constexpr uint8_t DAC_MUTE_REG = 0x31;
constexpr uint8_t DAC_VOLUME_REG = 0x32;
constexpr uint8_t DAC_RAMP_REG = 0x37;
constexpr int16_t ES8311_ZERO_DB_REGISTER = 0xBF;

struct VolumeCurvePoint {
    uint8_t percent;
    int16_t gainHalfDb;
};

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

}  // namespace

bool AudioService::begin(Stream& log) {
    log_ = &log;
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
    }

    if (!Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000U)) {
        lastError_ = "i2c_init_failed";
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

    i2s_zero_dma_buffer(I2S_NUM_1);
    if (xTaskCreate(taskEntry, "pgos_audio", 3072, this, 2, &taskHandle_) !=
        pdPASS) {
        lastError_ = "audio_task_failed";
        setCodecMuted(true);
        i2s_driver_uninstall(I2S_NUM_1);
        i2sInstalled_ = false;
        return false;
    }

    ready_ = true;
    digitalWrite(AMP_ENABLE_PIN, LOW);  // Active-low amplifier on.
    log.println(F("[audio] ES8311/I2S ready"));
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
    if (feedbackEnabled_) {
        requestTone(45U);
    }
}

void AudioService::playTestTone() {
    requestTone(300U);
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
}

bool AudioService::initializeI2s() {
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
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
    pins.data_in_num = I2S_PIN_NO_CHANGE;
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

bool AudioService::writeRegister(uint8_t address, uint8_t value) {
    Wire.beginTransmission(ES8311_ADDRESS);
    Wire.write(address);
    Wire.write(value);
    return Wire.endTransmission(true) == 0;
}

bool AudioService::readRegister(uint8_t address, uint8_t& value) {
    Wire.beginTransmission(ES8311_ADDRESS);
    Wire.write(address);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom(ES8311_ADDRESS, static_cast<uint8_t>(1),
                         static_cast<uint8_t>(true)) != 1) {
        return false;
    }
    value = static_cast<uint8_t>(Wire.read());
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
    if (!ready_ || volumePercent_ == 0) {
        return;
    }
    const uint32_t requestedUntil = millis() + durationMs;
    const uint32_t currentUntil = toneUntilMs_.load(std::memory_order_relaxed);
    if (static_cast<int32_t>(requestedUntil - currentUntil) > 0) {
        toneUntilMs_.store(requestedUntil, std::memory_order_relaxed);
    }
}

void AudioService::saveSettings() {
    settingsDirty_ = false;
    if (!preferencesReady_) {
        return;
    }
    preferences_.putUChar("volume", volumePercent_);
    preferences_.putBool("feedback", feedbackEnabled_);
    if (log_ != nullptr) {
        log_->println(F("[audio] settings saved"));
    }
}

void AudioService::run() {
    int16_t samples[DMA_FRAMES * 2U];
    uint32_t phase = 0;
    constexpr uint32_t samplesPerPeriod = SAMPLE_RATE / TONE_FREQUENCY;
    constexpr uint32_t halfPeriod = samplesPerPeriod / 2U;

    while (true) {
        const uint32_t nowMs = millis();
        const uint32_t untilMs = toneUntilMs_.load(std::memory_order_relaxed);
        const bool toneOn = static_cast<int32_t>(untilMs - nowMs) > 0;
        for (size_t frame = 0; frame < DMA_FRAMES; ++frame) {
            const int16_t sample = toneOn
                                       ? (phase < halfPeriod ? TONE_AMPLITUDE
                                                             : -TONE_AMPLITUDE)
                                       : 0;
            samples[frame * 2U] = sample;
            samples[frame * 2U + 1U] = sample;
            phase = toneOn ? (phase + 1U) % samplesPerPeriod : 0;
        }

        size_t bytesWritten = 0;
        const esp_err_t result =
            i2s_write(I2S_NUM_1, samples, sizeof(samples), &bytesWritten,
                      pdMS_TO_TICKS(100));
        if (result != ESP_OK || bytesWritten != sizeof(samples)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void AudioService::taskEntry(void* context) {
    static_cast<AudioService*>(context)->run();
}
