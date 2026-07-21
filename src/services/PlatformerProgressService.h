#pragma once

#include <Arduino.h>

class PlatformerProgressService final {
public:
    bool begin(Stream& output);

    uint8_t continueWorld() const;
    uint8_t continueStage() const;
    uint32_t bestScore() const;
    bool completed() const;

    bool recordCourseClear(uint8_t world, uint8_t stage,
                           uint8_t nextWorld, uint8_t nextStage,
                           uint32_t score);

private:
    static constexpr uint8_t SCHEMA_VERSION = 1;

    uint8_t continueWorld_ = 1;
    uint8_t continueStage_ = 1;
    uint32_t bestScore_ = 0;
    bool completed_ = false;

    bool save() const;
    static uint8_t levelIndex(uint8_t world, uint8_t stage);
};
