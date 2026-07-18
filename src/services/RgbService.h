#pragma once

#include <Arduino.h>

enum class RgbEffect : uint8_t {
    Solid,
    Breathe,
    Rainbow,
    Pulse,
    Sparkle,
    Count,
};

struct RgbSnapshot {
    bool enabled = false;
    RgbEffect effect = RgbEffect::Rainbow;
    uint8_t colorIndex = 0;
    uint8_t brightnessPercent = 40;
    uint8_t speedIndex = 1;
    uint8_t outputRed = 0;
    uint8_t outputGreen = 0;
    uint8_t outputBlue = 0;
};

class RgbService {
public:
    static constexpr uint8_t EFFECT_COUNT =
        static_cast<uint8_t>(RgbEffect::Count);
    static constexpr uint8_t PALETTE_COUNT = 7;
    static constexpr uint8_t SPEED_COUNT = 3;

    void begin(Print& log);
    void tick(uint32_t nowMs);

    bool enabled() const;
    void setEnabled(bool enabled);
    RgbEffect effect() const;
    void setEffect(RgbEffect effect);
    uint8_t colorIndex() const;
    void setColorIndex(uint8_t index);
    uint8_t brightnessPercent() const;
    void setBrightnessPercent(uint8_t percent);
    uint8_t speedIndex() const;
    void setSpeedIndex(uint8_t index);

    RgbSnapshot snapshot() const;
    void printStatus(Print& output) const;

    static const char* effectName(RgbEffect effect);
    static const char* paletteName(uint8_t index);
    static const char* speedName(uint8_t index);
    static void paletteRgb(uint8_t index, uint8_t& red, uint8_t& green,
                           uint8_t& blue);

private:
    static constexpr uint8_t RGB_PIN = 42;
    static constexpr uint32_t FRAME_INTERVAL_MS = 10;
    static constexpr uint8_t BRIGHTNESS_RAMP_STEP_PERCENT = 3;

    Print* log_ = nullptr;
    bool ready_ = false;
    bool enabled_ = false;
    bool dirty_ = true;
    RgbEffect effect_ = RgbEffect::Rainbow;
    uint8_t colorIndex_ = 0;
    uint8_t brightnessPercent_ = 40;
    uint8_t appliedBrightnessPercent_ = 40;
    uint8_t speedIndex_ = 1;
    uint8_t outputRed_ = 0;
    uint8_t outputGreen_ = 0;
    uint8_t outputBlue_ = 0;
    uint32_t effectStartedMs_ = 0;
    uint32_t lastFrameMs_ = 0;
    uint32_t randomState_ = 0x42A5C39DU;

    void restartEffect();
    void render(uint32_t nowMs);
    void writeOutput(uint8_t red, uint8_t green, uint8_t blue);
    static uint8_t scale8(uint8_t value, uint16_t scale);
    static uint8_t trianglePulse(uint16_t position, uint16_t start,
                                 uint16_t width);
    static void hsvToRgb(uint16_t hue, uint8_t& red, uint8_t& green,
                         uint8_t& blue);
};
