#pragma once

#include "core/AppTypes.h"

#include <Arduino.h>

class I2cBusService;

enum class TimeState : uint8_t {
    BusUnavailable,
    NotFound,
    ReadError,
    InvalidData,
    VoltageLow,
    Stopped,
    Ready,
};

enum class TimeSource : uint8_t {
    Unavailable,
    Rtc,
    Ntp,
};

struct RtcDateTime {
    uint16_t year = 2000;
    uint8_t month = 1;
    uint8_t day = 1;
    uint8_t weekday = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
};

struct TimeSnapshot {
    TimeState state = TimeState::BusUnavailable;
    RtcDateTime dateTime;
    RtcDateTime effectiveDateTime;
    bool detected = false;
    bool calendarValid = false;
    bool effectiveTimeValid = false;
    TimeSource effectiveSource = TimeSource::Unavailable;
    bool voltageLow = false;
    bool stopped = false;
    bool centuryBit = false;
    bool ntpConfigured = false;
    bool ntpSynced = false;
    uint32_t ntpSyncCount = 0;
    uint32_t readCount = 0;
    uint32_t errorCount = 0;
    String lastError;
};

class TimeService {
public:
    static constexpr uint8_t PCF8563_ADDRESS = 0x51;

    void begin(Print& log, I2cBusService& i2c);
    void tick(uint32_t nowMs, const WifiSnapshot& wifi);
    TimeSnapshot snapshot() const;
    bool ready() const;
    bool setDateTime(uint16_t year, uint8_t month, uint8_t day,
                     uint8_t hour, uint8_t minute, uint8_t second);
    bool setDateTimeText(const String& value);
    void printStatus(Print& output) const;

    static const char* stateName(TimeState state);
    static const char* sourceName(TimeSource source);
    static const char* weekdayName(uint8_t weekday);

private:
    static constexpr uint8_t CONTROL_STATUS_1_REGISTER = 0x00;
    static constexpr uint8_t VL_SECONDS_REGISTER = 0x02;
    static constexpr uint8_t STOP_BIT = 0x20;
    static constexpr uint8_t VL_BIT = 0x80;
    static constexpr uint32_t READ_INTERVAL_MS = 1000U;
    static constexpr uint32_t ERROR_RETRY_MS = 2000U;
    static constexpr uint32_t PROBE_RETRY_MS = 5000U;
    static constexpr uint32_t NTP_REFRESH_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
    static constexpr char LOCAL_TIMEZONE[] = "CST-8";

    Print* log_ = nullptr;
    I2cBusService* i2c_ = nullptr;
    TimeSnapshot snapshot_;
    uint32_t nextPollMs_ = 0;
    bool pendingNetworkSync_ = false;
    uint32_t lastNtpSyncMs_ = 0;

    bool probe();
    bool refresh();
    bool readRegisters(uint8_t firstRegister, uint8_t* values, size_t count);
    bool writeRegister(uint8_t address, uint8_t value);
    bool writeDateTime(const RtcDateTime& dateTime);
    void syncNetworkTime(uint32_t nowMs, const WifiSnapshot& wifi);
    void applyPendingNetworkSync();
    void updateEffectiveTime();
    void setState(TimeState state, const char* error = nullptr);

    static bool decodeBcd(uint8_t raw, uint8_t mask, uint8_t& value);
    static uint8_t encodeBcd(uint8_t value);
    static bool isValidDateTime(const RtcDateTime& dateTime);
    static uint8_t daysInMonth(uint16_t year, uint8_t month);
    static uint8_t calculateWeekday(uint16_t year, uint8_t month, uint8_t day);
};
