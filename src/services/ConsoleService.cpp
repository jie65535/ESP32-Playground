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
    } else if (lower == "flag" || lower == "x") {
        command.type = AppCommandType::QuickDrop;
    } else if (lower == "pause") {
        command.type = AppCommandType::Pause;
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
    } else if (lower == "page mic" || lower == "page microphone") {
        command.type = AppCommandType::PageMicrophone;
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
    } else if (lower == "page platformer" || lower == "game platformer") {
        command.type = AppCommandType::PagePlatformer;
    } else if (lower == "platformer maptest" ||
               lower == "game platformer maptest") {
        command.type = AppCommandType::PlatformerMapTest;
    } else if (lower.startsWith("platformer maptest ")) {
        String argument = lower.substring(19);
        argument.trim();
        const int separator = argument.indexOf('-');
        if (separator <= 0) {
            command.type = AppCommandType::Unknown;
            command.value = line;
        } else {
            const String worldText = argument.substring(0, separator);
            const String stageText = argument.substring(separator + 1);
            if (!isUnsignedInteger(worldText) ||
                !isUnsignedInteger(stageText)) {
                command.type = AppCommandType::Unknown;
                command.value = line;
                return true;
            }
            const int world = worldText.toInt();
            const int stage = stageText.toInt();
            if (world < 1 || world > 8 || stage < 1 || stage > 4) {
                command.type = AppCommandType::Unknown;
                command.value = line;
            } else {
                command.type = AppCommandType::PlatformerMapTest;
                command.number = (world - 1) * 4 + (stage - 1);
            }
        }
    } else if (lower == "platformer mapnext") {
        command.type = AppCommandType::PlatformerMapTestNext;
    } else if (lower == "page blackjack" || lower == "game blackjack") {
        command.type = AppCommandType::PageBlackjack;
    } else if (lower == "page minesweeper" || lower == "game minesweeper" ||
               lower == "page mines") {
        command.type = AppCommandType::PageMinesweeper;
    } else if (lower == "page 2048" || lower == "game 2048") {
        command.type = AppCommandType::PageGame2048;
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
    } else if (lower == "mic status") {
        command.type = AppCommandType::MicStatus;
    } else if (lower == "mic capture on") {
        command.type = AppCommandType::MicCaptureOn;
    } else if (lower == "mic capture off") {
        command.type = AppCommandType::MicCaptureOff;
    } else if (lower == "mic gain") {
        command.type = AppCommandType::MicGain;
    } else if (lower.startsWith("mic gain ")) {
        String gain = lower.substring(9);
        gain.trim();
        if (gain != "low" && gain != "normal" && gain != "high") {
            command.type = AppCommandType::Unknown;
            command.value = line;
        } else {
            command.type = AppCommandType::MicGain;
            command.value = gain;
        }
    } else if (lower == "mic denoise on") {
        command.type = AppCommandType::MicDenoiseOn;
    } else if (lower == "mic denoise off") {
        command.type = AppCommandType::MicDenoiseOff;
    } else if (lower == "mic denoise toggle") {
        command.type = AppCommandType::MicDenoiseToggle;
    } else if (lower == "mic voice on") {
        command.type = AppCommandType::MicVoiceOn;
    } else if (lower == "mic voice off") {
        command.type = AppCommandType::MicVoiceOff;
    } else if (lower == "mic voice toggle") {
        command.type = AppCommandType::MicVoiceToggle;
    } else if (lower == "mic record") {
        command.type = AppCommandType::MicRecord;
        command.number = 3000;
    } else if (lower.startsWith("mic record ")) {
        String argument = line.substring(11);
        argument.trim();
        const int separator = argument.indexOf(' ');
        String duration = argument;
        String correlationId;
        if (separator >= 0) {
            duration = argument.substring(0, separator);
            correlationId = argument.substring(separator + 1);
            correlationId.trim();
        }
        if (!isUnsignedInteger(duration) ||
            (!correlationId.isEmpty() &&
             !isUnsignedInteger(correlationId))) {
            command.type = AppCommandType::Unknown;
            command.value = line;
        } else {
            command.type = AppCommandType::MicRecord;
            command.number = duration.toInt();
            command.value = correlationId;
        }
    } else if (lower == "mic monitor on") {
        command.type = AppCommandType::MicMonitorOn;
    } else if (lower == "mic monitor off") {
        command.type = AppCommandType::MicMonitorOff;
    } else if (lower == "mic monitor toggle") {
        command.type = AppCommandType::MicMonitorToggle;
    } else if (lower == "mic playback") {
        command.type = AppCommandType::MicPlayback;
        command.number = 3000;
    } else if (lower.startsWith("mic playback ")) {
        String argument = line.substring(13);
        argument.trim();
        if (!isUnsignedInteger(argument)) {
            command.type = AppCommandType::Unknown;
            command.value = line;
        } else {
            command.type = AppCommandType::MicPlayback;
            command.number = argument.toInt();
        }
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
    output.println(F("Commands: up | down | left | right | ok | flag | pause"));
    output.println(F("          back | home"));
    output.println(F("         page system | page time | page display | page settings"));
    output.println(F("         page sound | page mic | page rgb | page controller"));
    output.println(F("         page console | page network"));
    output.println(F("         page snake"));
    output.println(F("         page tetris"));
    output.println(F("         page breakout"));
    output.println(F("         page platformer"));
    output.println(F("         platformer maptest [world-stage]"));
    output.println(F("         platformer mapnext"));
    output.println(F("         page blackjack"));
    output.println(F("         page minesweeper"));
    output.println(F("         page 2048"));
    output.println(F("          color_test | screenshot [request_id] | status | help"));
    output.println(F("Time:     time status | time set YYYY-MM-DD HH:MM:SS"));
    output.println(F("I2C:      i2c scan"));
    output.println(F("Mic:      mic status | mic capture on|off"));
    output.println(F("          mic gain low|normal|high"));
    output.println(F("          mic denoise on|off|toggle"));
    output.println(F("          mic voice on|off|toggle"));
    output.println(F("          mic record [250-5000 ms]"));
    output.println(F("          mic monitor on|off|toggle | mic playback [ms]"));
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
