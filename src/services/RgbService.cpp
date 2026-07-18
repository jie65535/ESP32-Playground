#include "services/RgbService.h"

#include <esp32-hal-rgb-led.h>

namespace {

struct PaletteColor {
    const char* name;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

constexpr PaletteColor PALETTE[] = {
    {"Cyan", 0, 190, 255},
    {"Violet", 170, 55, 255},
    {"Rose", 255, 45, 105},
    {"Amber", 255, 135, 18},
    {"Green", 55, 255, 105},
    {"Blue", 35, 95, 255},
    {"White", 255, 255, 255},
};

constexpr uint32_t EFFECT_PERIODS_MS[] = {6000, 3200, 1600};
constexpr uint32_t SPARKLE_INTERVALS_MS[] = {120, 65, 35};

}  // namespace

void RgbService::begin(Print& log) {
    log_ = &log;
    effectStartedMs_ = millis();
    writeOutput(0, 0, 0);
    ready_ = true;
    log_->println(F("[rgb] GPIO42 WS2812 ready; power=off"));
}

void RgbService::tick(uint32_t nowMs) {
    if (!ready_) {
        return;
    }

    if (!enabled_) {
        if (dirty_ || outputRed_ != 0 || outputGreen_ != 0 ||
            outputBlue_ != 0) {
            dirty_ = false;
            writeOutput(0, 0, 0);
        }
        return;
    }

    uint32_t intervalMs = FRAME_INTERVAL_MS;
    if (effect_ == RgbEffect::Solid) {
        intervalMs = UINT32_MAX;
    } else if (effect_ == RgbEffect::Sparkle) {
        intervalMs = SPARKLE_INTERVALS_MS[speedIndex_];
    }
    if (!dirty_ && nowMs - lastFrameMs_ < intervalMs) {
        return;
    }

    dirty_ = false;
    lastFrameMs_ = nowMs;
    render(nowMs);
}

bool RgbService::enabled() const {
    return enabled_;
}

void RgbService::setEnabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    restartEffect();
}

RgbEffect RgbService::effect() const {
    return effect_;
}

void RgbService::setEffect(RgbEffect effect) {
    if (effect >= RgbEffect::Count || effect_ == effect) {
        return;
    }
    effect_ = effect;
    restartEffect();
}

uint8_t RgbService::colorIndex() const {
    return colorIndex_;
}

void RgbService::setColorIndex(uint8_t index) {
    index %= PALETTE_COUNT;
    if (colorIndex_ == index) {
        return;
    }
    colorIndex_ = index;
    dirty_ = true;
}

uint8_t RgbService::brightnessPercent() const {
    return brightnessPercent_;
}

void RgbService::setBrightnessPercent(uint8_t percent) {
    percent = constrain(percent, static_cast<uint8_t>(1),
                        static_cast<uint8_t>(100));
    if (brightnessPercent_ == percent) {
        return;
    }
    brightnessPercent_ = percent;
    dirty_ = true;
}

uint8_t RgbService::speedIndex() const {
    return speedIndex_;
}

void RgbService::setSpeedIndex(uint8_t index) {
    index %= SPEED_COUNT;
    if (speedIndex_ == index) {
        return;
    }
    speedIndex_ = index;
    restartEffect();
}

RgbSnapshot RgbService::snapshot() const {
    RgbSnapshot value;
    value.enabled = enabled_;
    value.effect = effect_;
    value.colorIndex = colorIndex_;
    value.brightnessPercent = brightnessPercent_;
    value.speedIndex = speedIndex_;
    value.outputRed = outputRed_;
    value.outputGreen = outputGreen_;
    value.outputBlue = outputBlue_;
    return value;
}

void RgbService::printStatus(Print& output) const {
    const RgbSnapshot value = snapshot();
    output.print(F("[rgb] state="));
    output.print(value.enabled ? F("on") : F("off"));
    output.print(F(" effect="));
    output.print(effectName(value.effect));
    output.print(F(" color="));
    output.print(paletteName(value.colorIndex));
    output.print(F(" brightness="));
    output.print(value.brightnessPercent);
    output.print(F("% speed="));
    output.print(speedName(value.speedIndex));
    output.print(F(" output="));
    output.print(value.outputRed);
    output.print(',');
    output.print(value.outputGreen);
    output.print(',');
    output.println(value.outputBlue);
}

const char* RgbService::effectName(RgbEffect effect) {
    switch (effect) {
        case RgbEffect::Solid: return "Solid";
        case RgbEffect::Breathe: return "Breathe";
        case RgbEffect::Rainbow: return "Rainbow";
        case RgbEffect::Pulse: return "Heartbeat";
        case RgbEffect::Sparkle: return "Sparkle";
        default: return "Unknown";
    }
}

const char* RgbService::paletteName(uint8_t index) {
    return PALETTE[index % PALETTE_COUNT].name;
}

const char* RgbService::speedName(uint8_t index) {
    switch (index % SPEED_COUNT) {
        case 0: return "Slow";
        case 1: return "Normal";
        case 2: return "Fast";
        default: return "Normal";
    }
}

void RgbService::paletteRgb(uint8_t index, uint8_t& red, uint8_t& green,
                            uint8_t& blue) {
    const PaletteColor& color = PALETTE[index % PALETTE_COUNT];
    red = color.red;
    green = color.green;
    blue = color.blue;
}

void RgbService::restartEffect() {
    effectStartedMs_ = millis();
    lastFrameMs_ = 0;
    dirty_ = true;
}

void RgbService::render(uint32_t nowMs) {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    paletteRgb(colorIndex_, red, green, blue);

    const uint32_t elapsedMs = nowMs - effectStartedMs_;
    uint8_t effectScale = 255;
    switch (effect_) {
        case RgbEffect::Solid:
            break;

        case RgbEffect::Breathe: {
            const uint32_t periodMs = EFFECT_PERIODS_MS[speedIndex_];
            const uint16_t phase = static_cast<uint16_t>(
                (elapsedMs % periodMs) * 512U / periodMs);
            const uint16_t triangle = phase <= 255U ? phase : 511U - phase;
            const uint16_t eased = (triangle * triangle + 255U) / 255U;
            effectScale = static_cast<uint8_t>(12U + eased * 243U / 255U);
            break;
        }

        case RgbEffect::Rainbow: {
            const uint32_t periodMs = EFFECT_PERIODS_MS[speedIndex_];
            const uint16_t hue = static_cast<uint16_t>(
                (elapsedMs % periodMs) * 1536U / periodMs);
            hsvToRgb(hue, red, green, blue);
            break;
        }

        case RgbEffect::Pulse: {
            const uint32_t periodMs = EFFECT_PERIODS_MS[speedIndex_] / 2U;
            const uint16_t position = static_cast<uint16_t>(
                (elapsedMs % periodMs) * 1000U / periodMs);
            const uint8_t first = trianglePulse(position, 0, 150);
            const uint8_t second = trianglePulse(position, 190, 240);
            effectScale = max<uint8_t>(first, second);
            break;
        }

        case RgbEffect::Sparkle:
            randomState_ = randomState_ * 1664525U + 1013904223U;
            effectScale = static_cast<uint8_t>(
                38U + ((randomState_ >> 24U) * 217U) / 255U);
            break;

        default:
            effectScale = 0;
            break;
    }

    const uint16_t masterScale = static_cast<uint16_t>(
        effectScale * brightnessPercent_ / 100U);
    writeOutput(scale8(red, masterScale), scale8(green, masterScale),
                scale8(blue, masterScale));
}

void RgbService::writeOutput(uint8_t red, uint8_t green, uint8_t blue) {
    if (red == outputRed_ && green == outputGreen_ && blue == outputBlue_ &&
        ready_) {
        return;
    }
    neopixelWrite(RGB_PIN, red, green, blue);
    outputRed_ = red;
    outputGreen_ = green;
    outputBlue_ = blue;
}

uint8_t RgbService::scale8(uint8_t value, uint16_t scale) {
    return static_cast<uint8_t>((static_cast<uint16_t>(value) * scale + 127U) /
                                255U);
}

uint8_t RgbService::trianglePulse(uint16_t position, uint16_t start,
                                  uint16_t width) {
    if (position < start || position >= start + width || width < 2) {
        return 0;
    }
    const uint16_t phase = static_cast<uint16_t>(
        (position - start) * 510U / width);
    return static_cast<uint8_t>(phase <= 255U ? phase : 510U - phase);
}

void RgbService::hsvToRgb(uint16_t hue, uint8_t& red, uint8_t& green,
                          uint8_t& blue) {
    hue %= 1536U;
    const uint8_t region = static_cast<uint8_t>(hue >> 8U);
    const uint8_t offset = static_cast<uint8_t>(hue & 0xffU);
    const uint8_t rising = offset;
    const uint8_t falling = static_cast<uint8_t>(255U - offset);

    switch (region) {
        case 0: red = 255; green = rising; blue = 0; break;
        case 1: red = falling; green = 255; blue = 0; break;
        case 2: red = 0; green = 255; blue = rising; break;
        case 3: red = 0; green = falling; blue = 255; break;
        case 4: red = rising; green = 0; blue = 255; break;
        default: red = 255; green = 0; blue = falling; break;
    }
}
