#pragma once

#include "core/AppTypes.h"

#include <Arduino.h>

class ConsoleService {
public:
    static constexpr size_t COMMAND_BUFFER_SIZE = 160;

    void begin(Stream& io);
    bool poll(AppCommand& command);
    static void printHelp(Print& output);

private:
    Stream* io_ = nullptr;
    String buffer_;

    bool parseLine(const String& line, AppCommand& command) const;
};
