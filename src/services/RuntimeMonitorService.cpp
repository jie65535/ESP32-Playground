#include "services/RuntimeMonitorService.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void RuntimeMonitorService::begin(Print& log) {
    log_ = &log;
    windowStartedUs_ = esp_timer_get_time();
    sampleResources(millis());
    if (log_ != nullptr) {
        log_->println(F("[runtime] monitor ready; main-loop duty is sampled"));
    }
}

void RuntimeMonitorService::beginLoop() {
    loopStartedUs_ = esp_timer_get_time();
    if (windowStartedUs_ == 0) {
        windowStartedUs_ = loopStartedUs_;
    }
}

void RuntimeMonitorService::endLoop() {
    const uint64_t nowUs = esp_timer_get_time();
    if (loopStartedUs_ != 0 && nowUs >= loopStartedUs_) {
        snapshot_.mainLoopWorkUs = static_cast<uint32_t>(
            min<uint64_t>(nowUs - loopStartedUs_, UINT32_MAX));
        windowBusyUs_ += nowUs - loopStartedUs_;
    }

    const uint64_t windowUs = nowUs - windowStartedUs_;
    if (windowUs >= 1000000U) {
        const uint64_t busy = min<uint64_t>(windowBusyUs_, windowUs);
        snapshot_.mainLoopBusyPercent = static_cast<uint8_t>(
            min<uint64_t>(100U, busy * 100U / windowUs));
        windowStartedUs_ = nowUs;
        windowBusyUs_ = 0;
    }

    const uint32_t nowMs = millis();
    if (nowMs - lastResourceSampleMs_ >= 500U) {
        sampleResources(nowMs);
    }
}

void RuntimeMonitorService::recordStage(Stage stage, uint32_t elapsedUs) {
    switch (stage) {
        case Stage::Wifi: snapshot_.lastWifiUs = elapsedUs; break;
        case Stage::Server: snapshot_.lastServerUs = elapsedUs; break;
        case Stage::Mirror: snapshot_.lastMirrorUs = elapsedUs; break;
        case Stage::Benchmark: snapshot_.lastBenchmarkUs = elapsedUs; break;
        case Stage::Audio: snapshot_.lastAudioUs = elapsedUs; break;
        case Stage::Display: snapshot_.lastDisplayUs = elapsedUs; break;
        case Stage::Ui: snapshot_.lastUiUs = elapsedUs; break;
    }
}

void RuntimeMonitorService::recordFlushMetrics(uint32_t copyUs,
                                                uint32_t transferUs,
                                                uint32_t pixels,
                                                uint16_t areas) {
    snapshot_.lastFlushCopyUs = copyUs;
    snapshot_.lastFlushTransferUs = transferUs;
    snapshot_.lastFlushPixels = pixels;
    snapshot_.lastFlushAreas = areas;
}

RuntimeSnapshot RuntimeMonitorService::snapshot() const {
    return snapshot_;
}

void RuntimeMonitorService::printStatus(Print& output) const {
    output.print(F("[runtime] main_loop="));
    output.print(snapshot_.mainLoopBusyPercent);
    output.print(F("% work_us="));
    output.print(snapshot_.mainLoopWorkUs);
    output.print(F(" heap="));
    output.print(snapshot_.freeHeap);
    output.print('/');
    output.print(snapshot_.heapSize);
    output.print(F(" psram="));
    output.print(snapshot_.freePsram);
    output.print('/');
    output.print(snapshot_.psramSize);
    output.print(F(" tasks="));
    output.print(snapshot_.taskCount);
    output.print(F(" stages_us wifi="));
    output.print(snapshot_.lastWifiUs);
    output.print(F(" server="));
    output.print(snapshot_.lastServerUs);
    output.print(F(" mirror="));
    output.print(snapshot_.lastMirrorUs);
    output.print(F(" display="));
    output.print(snapshot_.lastDisplayUs);
    output.print(F(" ui="));
    output.print(snapshot_.lastUiUs);
    output.print(F(" flush_us spi="));
    output.print(snapshot_.lastFlushTransferUs);
    output.print(F(" copy="));
    output.print(snapshot_.lastFlushCopyUs);
    output.print(F(" areas="));
    output.print(snapshot_.lastFlushAreas);
    output.print(F(" pixels="));
    output.println(snapshot_.lastFlushPixels);
}

void RuntimeMonitorService::sampleResources(uint32_t nowMs) {
    lastResourceSampleMs_ = nowMs;
    snapshot_.freeHeap = ESP.getFreeHeap();
    snapshot_.heapSize = ESP.getHeapSize();
    snapshot_.minimumFreeHeap = ESP.getMinFreeHeap();
    snapshot_.freePsram = ESP.getFreePsram();
    snapshot_.psramSize = ESP.getPsramSize();
    snapshot_.flashSize = ESP.getFlashChipSize();
    snapshot_.sketchSize = ESP.getSketchSize();
    snapshot_.freeSketchSpace = ESP.getFreeSketchSpace();
    snapshot_.taskCount = uxTaskGetNumberOfTasks();
}
