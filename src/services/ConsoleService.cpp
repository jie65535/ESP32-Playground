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

bool ConsoleService::parseLine(const String& rawLine, AppCommand& command) {
    String line = rawLine;
    line.trim();
    String lower = line;
    lower.toLowerCase();

    if (lower == "up") {
        command.type = AppCommandType::Previous;
    } else if (lower == "down") {
        command.type = AppCommandType::Next;
    } else if (lower == "left") {
        command.type = AppCommandType::Left;
    } else if (lower == "right") {
        command.type = AppCommandType::Right;
    } else if (lower == "ok") {
        command.type = AppCommandType::Activate;
    } else if (lower == "back") {
        command.type = AppCommandType::Back;
    } else if (lower == "home") {
        command.type = AppCommandType::Home;
    } else if (lower == "page system") {
        command.type = AppCommandType::PageSystem;
    } else if (lower == "page time") {
        command.type = AppCommandType::PageTime;
    } else if (lower == "page display") {
        command.type = AppCommandType::PageDisplay;
    } else if (lower == "page settings" || lower == "page display settings") {
        command.type = AppCommandType::PageDisplaySettings;
    } else if (lower == "page sound") {
        command.type = AppCommandType::PageSound;
    } else if (lower == "page rgb") {
        command.type = AppCommandType::PageRgb;
    } else if (lower == "page controller" || lower == "page gamepad") {
        command.type = AppCommandType::PageGamepad;
    } else if (lower == "page console") {
        command.type = AppCommandType::PageConsole;
    } else if (lower == "page network") {
        command.type = AppCommandType::PageNetwork;
    } else if (lower == "page snake" || lower == "game snake") {
        command.type = AppCommandType::PageSnake;
    } else if (lower == "page tetris" || lower == "game tetris") {
        command.type = AppCommandType::PageTetris;
    } else if (lower == "page breakout" || lower == "game breakout") {
        command.type = AppCommandType::PageBreakout;
    } else if (lower == "color_test") {
        command.type = AppCommandType::ColorTest;
    } else if (lower == "screenshot") {
        command.type = AppCommandType::Screenshot;
    } else if (lower.startsWith("screenshot ")) {
        String argument = line.substring(11);
        argument.trim();
        if (!isUnsignedInteger(argument)) {
            command.type = AppCommandType::Unknown;
            command.value = line;
        } else {
            command.type = AppCommandType::Screenshot;
            command.number = argument.toInt();
        }
    } else if (lower == "status") {
        command.type = AppCommandType::Status;
    } else if (lower == "help" || lower == "?") {
        command.type = AppCommandType::Help;
    } else if (lower == "time status") {
        command.type = AppCommandType::TimeStatus;
    } else if (lower.startsWith("time set ")) {
        command.type = AppCommandType::TimeSet;
        command.value = line.substring(9);
    } else if (lower == "i2c scan") {
        command.type = AppCommandType::I2cScan;
    } else if (lower == "gamepad status") {
        command.type = AppCommandType::GamepadStatus;
    } else if (lower == "gamepad rumble") {
        command.type = AppCommandType::GamepadRumble;
    } else if (lower == "gamepad scan") {
        command.type = AppCommandType::GamepadScan;
    } else if (lower == "gamepad stop") {
        command.type = AppCommandType::GamepadStopScan;
    } else if (lower == "gamepad disconnect") {
        command.type = AppCommandType::GamepadDisconnect;
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
    } else if (lower == "server status") {
        command.type = AppCommandType::ServerStatus;
    } else if (lower == "server on") {
        command.type = AppCommandType::ServerOn;
    } else if (lower == "server off") {
        command.type = AppCommandType::ServerOff;
    } else if (lower == "server toggle") {
        command.type = AppCommandType::ServerToggle;
    } else if (lower == "server connect") {
        command.type = AppCommandType::ServerConnect;
    } else if (lower == "server clear") {
        command.type = AppCommandType::ServerClear;
    } else if (lower == "server help") {
        command.type = AppCommandType::ServerHelp;
    } else if (lower == "mirror on") {
        command.type = AppCommandType::MirrorOn;
    } else if (lower == "mirror off") {
        command.type = AppCommandType::MirrorOff;
    } else if (lower == "mirror toggle") {
        command.type = AppCommandType::MirrorToggle;
    } else if (lower == "mirror status") {
        command.type = AppCommandType::MirrorStatus;
    } else if (lower == "bench status") {
        command.type = AppCommandType::BenchStatus;
    } else if (lower == "bench cancel") {
        command.type = AppCommandType::BenchCancel;
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
    } else if (lower.startsWith("server set ")) {
        String argument = line.substring(11);
        const int separator = argument.lastIndexOf(' ');
        if (separator <= 0) {
            command.type = AppCommandType::Unknown;
            command.value = line;
        } else {
            const String host = argument.substring(0, separator);
            const String port = argument.substring(separator + 1);
            if (host.isEmpty() || !isUnsignedInteger(port)) {
                command.type = AppCommandType::Unknown;
                command.value = line;
            } else {
                command.type = AppCommandType::ServerSet;
                command.value = host;
                command.number = port.toInt();
            }
        }
    } else if (lower.startsWith("bench upload ")) {
        String argument = line.substring(13);
        argument.trim();
        if (!isUnsignedInteger(argument)) {
            command.type = AppCommandType::Unknown;
            command.value = line;
        } else {
            command.type = AppCommandType::BenchUpload;
            command.number = argument.toInt();
        }
    } else if (lower.startsWith("bench download ")) {
        String argument = line.substring(15);
        argument.trim();
        if (!isUnsignedInteger(argument)) {
            command.type = AppCommandType::Unknown;
            command.value = line;
        } else {
            command.type = AppCommandType::BenchDownload;
            command.number = argument.toInt();
        }
    } else if (!line.isEmpty()) {
        command.type = AppCommandType::Unknown;
        command.value = line;
    }
    return command.type != AppCommandType::None;
}

void ConsoleService::printHelp(Print& output) {
    output.println(F("Commands: up | down | left | right | ok | back | home"));
    output.println(F("         page system | page time | page display | page settings"));
    output.println(F("         page sound | page rgb | page controller | page console"));
    output.println(F("         page network"));
    output.println(F("         page snake"));
    output.println(F("         page tetris"));
    output.println(F("         page breakout"));
    output.println(F("          color_test | screenshot [request_id] | status | help"));
    output.println(F("Time:     time status | time set YYYY-MM-DD HH:MM:SS"));
    output.println(F("I2C:      i2c scan"));
    output.println(F("Gamepad:  gamepad status | gamepad scan | gamepad stop"));
    output.println(F("          gamepad rumble | gamepad disconnect"));
    output.println(F("Wi-Fi:    wifi scan | wifi select <index> | wifi ssid <name>"));
    output.println(F("          wifi password <password> | wifi open"));
    output.println(F("          wifi status | wifi reconnect | wifi clear"));
    output.println(F("          wifi on | wifi off | wifi toggle | wifi wizard"));
    output.println(F("Server:   server set <host> <port> | server status"));
    output.println(F("          server on | server off | server connect | server clear"));
    output.println(F("Mirror:   mirror on | mirror off | mirror toggle | mirror status"));
    output.println(F("Bench:    bench upload <bytes> | bench download <bytes>"));
    output.println(F("          bench status | bench cancel"));
}
