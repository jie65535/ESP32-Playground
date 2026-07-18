#pragma once

#include "core/AppTypes.h"

class InputRouter {
public:
    static constexpr uint8_t QUEUE_SIZE = 16;

    bool push(const AppCommand& command, InputSource source,
              uint32_t requestId = 0);
    bool poll(RoutedCommand& command);
    uint32_t droppedCount() const;

private:
    RoutedCommand queue_[QUEUE_SIZE];
    uint8_t head_ = 0;
    uint8_t tail_ = 0;
    uint8_t count_ = 0;
    uint32_t droppedCount_ = 0;
};
