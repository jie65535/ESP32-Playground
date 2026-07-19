#include "services/I2cBusService.h"

bool I2cBusService::begin(Print& log) {
    ready_ = Wire.begin(SDA_PIN, SCL_PIN, FREQUENCY_HZ);
    if (ready_) {
        log.printf("[i2c] ready sda=%u scl=%u frequency=%luHz\n",
                   static_cast<unsigned>(SDA_PIN),
                   static_cast<unsigned>(SCL_PIN),
                   static_cast<unsigned long>(FREQUENCY_HZ));
    } else {
        log.println(F("[i2c] initialization failed"));
    }
    return ready_;
}

bool I2cBusService::ready() const {
    return ready_;
}

TwoWire& I2cBusService::wire() {
    return Wire;
}

uint8_t I2cBusService::scan(Print& output) {
    if (!ready_) {
        output.println(F("[i2c] scan unavailable; bus is not initialized"));
        return 0;
    }
    output.println(F("[i2c] scan start"));
    uint8_t found = 0;
    for (uint8_t address = 1; address < 0x78; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission(true) == 0) {
            output.print(F("[i2c] found address=0x"));
            if (address < 0x10) {
                output.print('0');
            }
            output.println(address, HEX);
            ++found;
        }
    }
    output.print(F("[i2c] scan complete devices="));
    output.println(found);
    return found;
}

void I2cBusService::printStatus(Print& output) const {
    output.print(F("[i2c] state="));
    output.print(ready_ ? F("ready") : F("unavailable"));
    output.print(F(" sda="));
    output.print(SDA_PIN);
    output.print(F(" scl="));
    output.print(SCL_PIN);
    output.print(F(" frequency="));
    output.print(FREQUENCY_HZ);
    output.println(F("Hz"));
}
