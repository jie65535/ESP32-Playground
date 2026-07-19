#pragma once

#include <Arduino.h>
#include <Wire.h>

class I2cBusService {
public:
    static constexpr uint8_t SCL_PIN = 15;
    static constexpr uint8_t SDA_PIN = 16;
    static constexpr uint32_t FREQUENCY_HZ = 400000U;

    bool begin(Print& log);
    bool ready() const;
    TwoWire& wire();
    uint8_t scan(Print& output);
    void printStatus(Print& output) const;

private:
    bool ready_ = false;
};
