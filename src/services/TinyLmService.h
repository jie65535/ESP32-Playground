#pragma once

#include <Arduino.h>

enum class TinyLmState : uint8_t {
    Unavailable,
    Idle,
    Loading,
    Generating,
    Complete,
    Error,
};

struct TinyLmSnapshot {
    TinyLmState state = TinyLmState::Unavailable;
    bool available = false;
    uint32_t generation = 0;
    uint16_t generatedTokens = 0;
    uint16_t maxTokens = 0;
    uint64_t elapsedUs = 0;
    uint64_t computeUs = 0;
    uint32_t freePsram = 0;
    char error[64] = {};
};

struct TinyLmTokenEvent {
    uint32_t generation = 0;
    uint16_t token = 0;
};

class TinyLmService {
public:
    bool begin(Print& log);
    uint32_t start(const uint16_t* promptIds, size_t promptCount,
                   uint16_t maxTokens = 200);
    void stop();
    void unload();

    bool pollToken(TinyLmTokenEvent& event);
    size_t decodeToken(uint16_t token, uint8_t* output,
                       size_t capacity) const;
    TinyLmSnapshot snapshot() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};
