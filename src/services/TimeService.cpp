#include "services/TimeService.h"

#include "services/I2cBusService.h"

#include <esp_sntp.h>
#include <time.h>

namespace {

void printTwoDigits(Print& output, uint8_t value) {
    if (value < 10) {
        output.print('0');
    }
    output.print(value);
}

bool isDigitAt(const String& value, size_t index) {
    return index < value.length() && value[index] >= '0' && value[index] <= '9';
}

uint8_t parseTwoDigits(const String& value, size_t index) {
    return static_cast<uint8_t>((value[index] - '0') * 10 +
                                (value[index + 1] - '0'));
}

}  // namespace

constexpr char TimeService::LOCAL_TIMEZONE[];

void TimeService::begin(Print& log, I2cBusService& i2c) {
    log_ = &log;
    i2c_ = &i2c;
    if (!i2c.ready()) {
        setState(TimeState::BusUnavailable, "i2c_unavailable");
        return;
    }
    nextPollMs_ = 0;
    WifiSnapshot noWifi;
    noWifi.state = WifiState::Disabled;
    tick(millis(), noWifi);
}

void TimeService::tick(uint32_t nowMs, const WifiSnapshot& wifi) {
    syncNetworkTime(nowMs, wifi);
    if (i2c_ == nullptr || !i2c_->ready()) {
        setState(TimeState::BusUnavailable, "i2c_unavailable");
        updateEffectiveTime();
        return;
    }
    if (static_cast<int32_t>(nowMs - nextPollMs_) < 0) {
        updateEffectiveTime();
        return;
    }

    const bool refreshed = refresh();
    applyPendingNetworkSync();
    if (refreshed) {
        nextPollMs_ = nowMs + READ_INTERVAL_MS;
    } else if (snapshot_.state == TimeState::NotFound) {
        nextPollMs_ = nowMs + PROBE_RETRY_MS;
    } else {
        nextPollMs_ = nowMs + ERROR_RETRY_MS;
    }
    updateEffectiveTime();
}

TimeSnapshot TimeService::snapshot() const {
    return snapshot_;
}

bool TimeService::ready() const {
    return snapshot_.state == TimeState::Ready;
}

bool TimeService::setDateTime(uint16_t year, uint8_t month, uint8_t day,
                              uint8_t hour, uint8_t minute, uint8_t second) {
    pendingNetworkSync_ = false;
    RtcDateTime dateTime;
    dateTime.year = year;
    dateTime.month = month;
    dateTime.day = day;
    dateTime.weekday = calculateWeekday(year, month, day);
    dateTime.hour = hour;
    dateTime.minute = minute;
    dateTime.second = second;

    if (!isValidDateTime(dateTime)) {
        setState(TimeState::InvalidData, "set_value_invalid");
        return false;
    }
    if (i2c_ == nullptr || !i2c_->ready()) {
        setState(TimeState::BusUnavailable, "i2c_unavailable");
        return false;
    }
    if (!snapshot_.detected && !probe()) {
        ++snapshot_.errorCount;
        setState(TimeState::NotFound, "pcf8563_not_found");
        return false;
    }
    snapshot_.detected = true;
    if (!writeDateTime(dateTime)) {
        ++snapshot_.errorCount;
        setState(TimeState::ReadError, "pcf8563_write_failed");
        nextPollMs_ = millis() + ERROR_RETRY_MS;
        return false;
    }

    nextPollMs_ = 0;
    const bool verified = refresh() && ready();
    nextPollMs_ = millis() + READ_INTERVAL_MS;
    if (verified && log_ != nullptr) {
        log_->println(F("[time] PCF8563 local time updated"));
    }
    return verified;
}

bool TimeService::setDateTimeText(const String& rawValue) {
    String value = rawValue;
    value.trim();
    if (value.length() != 19 || value[4] != '-' || value[7] != '-' ||
        value[10] != ' ' || value[13] != ':' || value[16] != ':') {
        setState(TimeState::InvalidData, "set_format_invalid");
        return false;
    }
    constexpr size_t DIGIT_POSITIONS[] = {
        0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18,
    };
    for (size_t index : DIGIT_POSITIONS) {
        if (!isDigitAt(value, index)) {
            setState(TimeState::InvalidData, "set_format_invalid");
            return false;
        }
    }

    const uint16_t year = static_cast<uint16_t>(
        (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
        (value[2] - '0') * 10 + (value[3] - '0'));
    return setDateTime(year, parseTwoDigits(value, 5),
                       parseTwoDigits(value, 8), parseTwoDigits(value, 11),
                       parseTwoDigits(value, 14), parseTwoDigits(value, 17));
}

void TimeService::printStatus(Print& output) const {
    output.print(F("[time] state="));
    output.print(stateName(snapshot_.state));
    output.print(F(" rtc=pcf8563 address=0x51 detected="));
    output.print(snapshot_.detected ? F("yes") : F("no"));
    output.print(F(" vl="));
    output.print(snapshot_.voltageLow ? 1 : 0);
    output.print(F(" stop="));
    output.print(snapshot_.stopped ? 1 : 0);
    output.print(F(" reads="));
    output.print(snapshot_.readCount);
    output.print(F(" errors="));
    output.print(snapshot_.errorCount);
    output.print(F(" ntp="));
    output.print(!snapshot_.ntpConfigured
                     ? F("off")
                     : snapshot_.ntpSynced ? F("synced") : F("pending"));
    output.print(F(" ntp_syncs="));
    output.print(snapshot_.ntpSyncCount);
    output.print(F(" source="));
    output.print(sourceName(snapshot_.effectiveSource));
    if (snapshot_.effectiveTimeValid) {
        const RtcDateTime& value = snapshot_.effectiveDateTime;
        output.print(F(" local="));
        output.print(value.year);
        output.print('-');
        printTwoDigits(output, value.month);
        output.print('-');
        printTwoDigits(output, value.day);
        output.print(' ');
        printTwoDigits(output, value.hour);
        output.print(':');
        printTwoDigits(output, value.minute);
        output.print(':');
        printTwoDigits(output, value.second);
        output.print(F(" weekday="));
        output.print(weekdayName(value.weekday));
    }
    if (!snapshot_.lastError.isEmpty()) {
        output.print(F(" error="));
        output.print(snapshot_.lastError);
    }
    output.println();
}

const char* TimeService::stateName(TimeState state) {
    switch (state) {
        case TimeState::BusUnavailable: return "bus_unavailable";
        case TimeState::NotFound: return "not_found";
        case TimeState::ReadError: return "io_error";
        case TimeState::InvalidData: return "invalid";
        case TimeState::VoltageLow: return "voltage_low";
        case TimeState::Stopped: return "stopped";
        case TimeState::Ready: return "ready";
        default: return "unknown";
    }
}

const char* TimeService::sourceName(TimeSource source) {
    switch (source) {
        case TimeSource::Rtc: return "rtc";
        case TimeSource::Ntp: return "ntp";
        case TimeSource::Unavailable:
        default: return "unavailable";
    }
}

const char* TimeService::weekdayName(uint8_t weekday) {
    static constexpr const char* NAMES[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday",
    };
    return weekday < 7 ? NAMES[weekday] : "Unknown";
}

bool TimeService::probe() {
    TwoWire& wire = i2c_->wire();
    wire.beginTransmission(PCF8563_ADDRESS);
    return wire.endTransmission(true) == 0;
}

void TimeService::syncNetworkTime(uint32_t nowMs, const WifiSnapshot& wifi) {
    if (wifi.state != WifiState::Connected) {
        return;
    }
    if (!snapshot_.ntpConfigured) {
        configTzTime(LOCAL_TIMEZONE, "pool.ntp.org", "time.nist.gov",
                     "time.google.com");
        snapshot_.ntpConfigured = true;
        if (log_ != nullptr) {
            log_->print(F("[time] NTP sync requested timezone="));
            log_->println(LOCAL_TIMEZONE);
        }
    }

    const bool due = !snapshot_.ntpSynced ||
                     static_cast<uint32_t>(nowMs - lastNtpSyncMs_) >=
                         NTP_REFRESH_INTERVAL_MS;
    if (!due || sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        return;
    }

    const time_t epoch = time(nullptr);
    struct tm localTime = {};
    if (epoch < 946684800 || localtime_r(&epoch, &localTime) == nullptr) {
        return;
    }

    pendingNetworkSync_ = true;
    snapshot_.ntpSynced = true;
    snapshot_.ntpSyncCount++;
    lastNtpSyncMs_ = nowMs;
    if (log_ != nullptr) {
        log_->println(snapshot_.detected
                          ? F("[time] NTP time received; updating PCF8563")
                          : F("[time] NTP time received; RTC update pending"));
    }
}

void TimeService::applyPendingNetworkSync() {
    if (!pendingNetworkSync_ || !snapshot_.detected) {
        return;
    }
    const time_t epoch = time(nullptr);
    struct tm localTime = {};
    if (epoch < 946684800 || localtime_r(&epoch, &localTime) == nullptr) {
        return;
    }
    pendingNetworkSync_ = false;
    if (!setDateTime(static_cast<uint16_t>(localTime.tm_year + 1900),
                     static_cast<uint8_t>(localTime.tm_mon + 1),
                     static_cast<uint8_t>(localTime.tm_mday),
                     static_cast<uint8_t>(localTime.tm_hour),
                     static_cast<uint8_t>(localTime.tm_min),
                     static_cast<uint8_t>(localTime.tm_sec))) {
        pendingNetworkSync_ = true;
    }
}

void TimeService::updateEffectiveTime() {
    if (snapshot_.ntpSynced) {
        const time_t epoch = time(nullptr);
        struct tm localTime = {};
        if (epoch >= 946684800 && localtime_r(&epoch, &localTime) != nullptr) {
            RtcDateTime value;
            value.year = static_cast<uint16_t>(localTime.tm_year + 1900);
            value.month = static_cast<uint8_t>(localTime.tm_mon + 1);
            value.day = static_cast<uint8_t>(localTime.tm_mday);
            value.weekday = static_cast<uint8_t>(localTime.tm_wday);
            value.hour = static_cast<uint8_t>(localTime.tm_hour);
            value.minute = static_cast<uint8_t>(localTime.tm_min);
            value.second = static_cast<uint8_t>(localTime.tm_sec);
            if (isValidDateTime(value)) {
                snapshot_.effectiveDateTime = value;
                snapshot_.effectiveTimeValid = true;
                snapshot_.effectiveSource = TimeSource::Ntp;
                return;
            }
        }
    }

    if (snapshot_.state == TimeState::Ready && snapshot_.calendarValid) {
        snapshot_.effectiveDateTime = snapshot_.dateTime;
        snapshot_.effectiveTimeValid = true;
        snapshot_.effectiveSource = TimeSource::Rtc;
        return;
    }

    snapshot_.effectiveTimeValid = false;
    snapshot_.effectiveSource = TimeSource::Unavailable;
}

bool TimeService::refresh() {
    if (!snapshot_.detected) {
        if (!probe()) {
            ++snapshot_.errorCount;
            snapshot_.detected = false;
            snapshot_.calendarValid = false;
            snapshot_.voltageLow = false;
            snapshot_.stopped = false;
            snapshot_.centuryBit = false;
            setState(TimeState::NotFound, "pcf8563_not_found");
            return false;
        }
        snapshot_.detected = true;
    }

    uint8_t control = 0;
    uint8_t registers[7] = {};
    if (!readRegisters(CONTROL_STATUS_1_REGISTER, &control, 1) ||
        !readRegisters(VL_SECONDS_REGISTER, registers, sizeof(registers))) {
        ++snapshot_.errorCount;
        snapshot_.detected = false;
        snapshot_.calendarValid = false;
        snapshot_.voltageLow = false;
        snapshot_.stopped = false;
        snapshot_.centuryBit = false;
        setState(TimeState::ReadError, "pcf8563_read_failed");
        return false;
    }
    ++snapshot_.readCount;

    RtcDateTime dateTime;
    uint8_t year = 0;
    const bool decoded =
        decodeBcd(registers[0], 0x7F, dateTime.second) &&
        decodeBcd(registers[1], 0x7F, dateTime.minute) &&
        decodeBcd(registers[2], 0x3F, dateTime.hour) &&
        decodeBcd(registers[3], 0x3F, dateTime.day) &&
        decodeBcd(registers[5], 0x1F, dateTime.month) &&
        decodeBcd(registers[6], 0xFF, year);
    dateTime.weekday = static_cast<uint8_t>(registers[4] & 0x07U);
    dateTime.year = static_cast<uint16_t>(2000U + year);

    snapshot_.dateTime = dateTime;
    snapshot_.voltageLow = (registers[0] & VL_BIT) != 0;
    snapshot_.stopped = (control & STOP_BIT) != 0;
    snapshot_.centuryBit = (registers[5] & 0x80U) != 0;
    snapshot_.calendarValid = decoded && isValidDateTime(dateTime);

    if (snapshot_.centuryBit) {
        snapshot_.calendarValid = false;
        setState(TimeState::InvalidData, "century_bit_set");
    } else if (!snapshot_.calendarValid) {
        setState(TimeState::InvalidData, "calendar_fields_invalid");
    } else if (snapshot_.voltageLow) {
        setState(TimeState::VoltageLow, "clock_integrity_not_guaranteed");
    } else if (snapshot_.stopped) {
        setState(TimeState::Stopped, "rtc_clock_stopped");
    } else {
        setState(TimeState::Ready);
    }
    return true;
}

bool TimeService::readRegisters(uint8_t firstRegister, uint8_t* values,
                                size_t count) {
    if (values == nullptr || count == 0 || count > UINT8_MAX) {
        return false;
    }
    TwoWire& wire = i2c_->wire();
    wire.beginTransmission(PCF8563_ADDRESS);
    wire.write(firstRegister);
    if (wire.endTransmission(false) != 0 ||
        wire.requestFrom(PCF8563_ADDRESS, static_cast<uint8_t>(count),
                         static_cast<uint8_t>(true)) != count) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        if (wire.available() <= 0) {
            return false;
        }
        values[index] = static_cast<uint8_t>(wire.read());
    }
    return true;
}

bool TimeService::writeRegister(uint8_t address, uint8_t value) {
    TwoWire& wire = i2c_->wire();
    wire.beginTransmission(PCF8563_ADDRESS);
    wire.write(address);
    wire.write(value);
    return wire.endTransmission(true) == 0;
}

bool TimeService::writeDateTime(const RtcDateTime& dateTime) {
    if (!writeRegister(CONTROL_STATUS_1_REGISTER, STOP_BIT)) {
        return false;
    }

    TwoWire& wire = i2c_->wire();
    wire.beginTransmission(PCF8563_ADDRESS);
    wire.write(VL_SECONDS_REGISTER);
    wire.write(encodeBcd(dateTime.second));
    wire.write(encodeBcd(dateTime.minute));
    wire.write(encodeBcd(dateTime.hour));
    wire.write(encodeBcd(dateTime.day));
    wire.write(dateTime.weekday);
    wire.write(encodeBcd(dateTime.month));  // C=0: project epoch is 2000.
    wire.write(encodeBcd(static_cast<uint8_t>(dateTime.year - 2000U)));
    const bool written = wire.endTransmission(true) == 0;
    const bool restarted = writeRegister(CONTROL_STATUS_1_REGISTER, 0x00);
    return written && restarted;
}

void TimeService::setState(TimeState state, const char* error) {
    const String nextError = error == nullptr ? String() : String(error);
    const bool changed = snapshot_.state != state ||
                         snapshot_.lastError != nextError;
    snapshot_.state = state;
    snapshot_.lastError = nextError;
    if (changed && log_ != nullptr) {
        printStatus(*log_);
    }
}

bool TimeService::decodeBcd(uint8_t raw, uint8_t mask, uint8_t& value) {
    const uint8_t encoded = static_cast<uint8_t>(raw & mask);
    const uint8_t ones = static_cast<uint8_t>(encoded & 0x0FU);
    const uint8_t tens = static_cast<uint8_t>((encoded >> 4U) & 0x0FU);
    if (ones > 9 || tens > 9) {
        return false;
    }
    value = static_cast<uint8_t>(tens * 10U + ones);
    return true;
}

uint8_t TimeService::encodeBcd(uint8_t value) {
    return static_cast<uint8_t>(((value / 10U) << 4U) | (value % 10U));
}

bool TimeService::isValidDateTime(const RtcDateTime& dateTime) {
    return dateTime.year >= 2000 && dateTime.year <= 2099 &&
           dateTime.month >= 1 && dateTime.month <= 12 &&
           dateTime.day >= 1 &&
           dateTime.day <= daysInMonth(dateTime.year, dateTime.month) &&
           dateTime.weekday <= 6 && dateTime.hour <= 23 &&
           dateTime.minute <= 59 && dateTime.second <= 59;
}

uint8_t TimeService::daysInMonth(uint16_t year, uint8_t month) {
    static constexpr uint8_t DAYS[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if (month < 1 || month > 12) {
        return 0;
    }
    const bool leap = year % 4U == 0U &&
                      (year % 100U != 0U || year % 400U == 0U);
    return static_cast<uint8_t>(DAYS[month - 1U] +
                                (month == 2 && leap ? 1U : 0U));
}

uint8_t TimeService::calculateWeekday(uint16_t year, uint8_t month,
                                      uint8_t day) {
    static constexpr uint8_t OFFSETS[] = {
        0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4,
    };
    if (month < 1 || month > 12) {
        return 0;
    }
    uint16_t adjustedYear = year;
    if (month < 3) {
        --adjustedYear;
    }
    return static_cast<uint8_t>(
        (adjustedYear + adjustedYear / 4U - adjustedYear / 100U +
         adjustedYear / 400U + OFFSETS[month - 1U] + day) % 7U);
}
