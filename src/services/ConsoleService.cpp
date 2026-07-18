#include "services/ConsoleService.h"

namespace {

bool isUnsignedInteger(const String& value) {
    if (value.isEmpty()) {
        return false;
    }
    for (size_t index = 0; index < value.length(); ++index) {
        if (value[index] < '0' || value[index] > '9') {
            return false;
        }
    }
    return true;
}

}  // namespace

void ConsoleService::begin(Stream& io) {
    io_ = &io;
    buffer_.reserve(COMMAND_BUFFER_SIZE);
}

bool ConsoleService::poll(AppCommand& command) {
    command = AppCommand{};
    if (io_ == nullptr) {
        return false;
    }

    while (io_->available() > 0) {
        const char character = static_cast<char>(io_->read());
        if (character == '\r' || character == '\n') {
            if (buffer_.isEmpty()) {
                continue;
            }
            const String line = buffer_;
            buffer_ = "";
            return parseLine(line, command);
        }
        if (buffer_.length() < COMMAND_BUFFER_SIZE) {
            buffer_ += character;
        } else {
            buffer_ = "";
            if (io_ != nullptr) {
                io_->println(F("[console] command discarded: too long"));
            }
        }
    }
    return false;
}

bool ConsoleService::parseLine(const String& rawLine, AppCommand& command) const {
    String line = rawLine;
    line.trim();
    String lower = line;
    lower.toLowerCase();

    if (lower == "up") {
        command.type = AppCommandType::Previous;
    } else if (lower == "down") {
        command.type = AppCommandType::Next;
    } else if (lower == "ok") {
        command.type = AppCommandType::Activate;
    } else if (lower == "page system") {
        command.type = AppCommandType::PageSystem;
    } else if (lower == "page display") {
        command.type = AppCommandType::PageDisplay;
    } else if (lower == "page network") {
        command.type = AppCommandType::PageNetwork;
    } else if (lower == "color_test") {
        command.type = AppCommandType::ColorTest;
    } else if (lower == "screenshot") {
        command.type = AppCommandType::Screenshot;
    } else if (lower == "status") {
        command.type = AppCommandType::Status;
    } else if (lower == "help" || lower == "?") {
        command.type = AppCommandType::Help;
    } else if (lower == "wifi scan") {
        command.type = AppCommandType::WifiScan;
    } else if (lower == "wifi status") {
        command.type = AppCommandType::WifiStatus;
    } else if (lower == "wifi reconnect") {
        command.type = AppCommandType::WifiReconnect;
    } else if (lower == "wifi clear") {
        command.type = AppCommandType::WifiClear;
    } else if (lower == "wifi on") {
        command.type = AppCommandType::WifiOn;
    } else if (lower == "wifi off") {
        command.type = AppCommandType::WifiOff;
    } else if (lower == "wifi toggle") {
        command.type = AppCommandType::WifiToggle;
    } else if (lower == "wifi wizard") {
        command.type = AppCommandType::WifiWizard;
    } else if (lower == "wifi wizard cancel") {
        command.type = AppCommandType::WifiWizardCancel;
    } else if (lower == "wifi help") {
        command.type = AppCommandType::WifiHelp;
    } else if (lower.startsWith("wifi select ")) {
        String argument = line.substring(12);
        argument.trim();
        if (!isUnsignedInteger(argument)) {
            command.type = AppCommandType::Unknown;
            command.value = line;
        } else {
            command.type = AppCommandType::WifiSelect;
            command.number = argument.toInt();
        }
    } else if (lower.startsWith("wifi ssid ")) {
        command.type = AppCommandType::WifiSsid;
        command.value = line.substring(10);
    } else if (lower.startsWith("wifi password ")) {
        command.type = AppCommandType::WifiPassword;
        command.value = line.substring(14);
    } else if (lower == "wifi open") {
        command.type = AppCommandType::WifiOpen;
    } else if (!line.isEmpty()) {
        command.type = AppCommandType::Unknown;
        command.value = line;
    }
    return command.type != AppCommandType::None;
}

void ConsoleService::printHelp(Print& output) {
    output.println(F("Commands: up | down | ok | page system | page display | page network"));
    output.println(F("          color_test | screenshot | status | help"));
    output.println(F("Wi-Fi:    wifi scan | wifi select <index> | wifi ssid <name>"));
    output.println(F("          wifi password <password> | wifi open"));
    output.println(F("          wifi status | wifi reconnect | wifi clear"));
    output.println(F("          wifi on | wifi off | wifi toggle | wifi wizard"));
}
