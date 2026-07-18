#include "core/InputRouter.h"

bool InputRouter::push(const AppCommand& command, InputSource source,
                       uint32_t requestId) {
    if (count_ >= QUEUE_SIZE) {
        droppedCount_++;
        return false;
    }
    queue_[tail_].command = command;
    queue_[tail_].source = source;
    queue_[tail_].requestId = requestId;
    tail_ = static_cast<uint8_t>((tail_ + 1U) % QUEUE_SIZE);
    count_++;
    return true;
}

bool InputRouter::poll(RoutedCommand& command) {
    if (count_ == 0) {
        return false;
    }
    command = queue_[head_];
    head_ = static_cast<uint8_t>((head_ + 1U) % QUEUE_SIZE);
    count_--;
    return true;
}

uint32_t InputRouter::droppedCount() const {
    return droppedCount_;
}
