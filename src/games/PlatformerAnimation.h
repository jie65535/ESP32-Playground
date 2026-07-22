#pragma once

#include <cstdint>

namespace pgos {

constexpr uint8_t platformerReferenceAnimationFrame(uint32_t ageFrames,
                                                     uint8_t frameDelay,
                                                     uint8_t frameCount) {
    if (ageFrames == 0U || frameDelay == 0U || frameCount == 0U) {
        return 0U;
    }
    return static_cast<uint8_t>(
        (((ageFrames - 1U) / frameDelay) + 1U) % frameCount);
}

constexpr uint8_t platformerReferencePausedAnimationFrame(
    uint32_t ageFrames, uint8_t frameDelay, uint8_t frameCount,
    uint8_t pauseFrames) {
    if (ageFrames == 0U || frameDelay == 0U || frameCount <= 1U) {
        return 0U;
    }
    const uint32_t activeFrames =
        static_cast<uint32_t>(frameCount - 1U) * frameDelay;
    const uint32_t cycleFrames =
        static_cast<uint32_t>(frameCount) * frameDelay + pauseFrames;
    const uint32_t cycleFrame = (ageFrames - 1U) % cycleFrames + 1U;
    if (cycleFrame > activeFrames) {
        return 0U;
    }
    return static_cast<uint8_t>((cycleFrame - 1U) / frameDelay + 1U);
}

constexpr uint32_t platformerAnimationAge(uint32_t currentFrame,
                                          uint32_t bornFrame) {
    return currentFrame >= bornFrame ? currentFrame - bornFrame : 0U;
}

inline void platformerAdvanceReferenceAnimation(uint8_t& currentFrame,
                                                 uint8_t& frameTimer,
                                                 uint8_t frameDelay,
                                                 uint8_t frameCount) {
    if (frameDelay == 0U || frameCount == 0U) {
        currentFrame = 0U;
        frameTimer = 0U;
        return;
    }
    if (frameTimer > 0U) {
        --frameTimer;
    }
    if (frameTimer == 0U) {
        currentFrame = static_cast<uint8_t>((currentFrame + 1U) % frameCount);
        frameTimer = frameDelay;
    }
}

}  // namespace pgos
