#include "services/MinesweeperProfileService.h"

#include "games/MinesweeperEngine.h"

#include <Preferences.h>

#include <algorithm>

namespace {

constexpr char NVS_NAMESPACE[] = "pgos_mines";

uint16_t maximumCustomMines(uint8_t width, uint8_t height) {
    const uint16_t cells = static_cast<uint16_t>(width) * height;
    return std::min<uint16_t>(static_cast<uint16_t>(cells - 9U),
                              static_cast<uint16_t>(cells * 24U / 100U));
}

}  // namespace

bool MinesweeperProfileService::begin(Stream& output) {
    log_ = &output;
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        output.println(F("[minesweeper] profile unavailable; using RAM only"));
        return false;
    }
    const uint8_t schema = preferences.getUChar("schema", 0);
    if (schema != SCHEMA_VERSION) {
        preferences.clear();
        preferences.putUChar("schema", SCHEMA_VERSION);
    } else {
        pgos::MinesweeperRecordsData data{};
        if (preferences.getBytesLength("records") == sizeof(data)) {
            preferences.getBytes("records", &data, sizeof(data));
            records_.importData(data);
        }
        const uint8_t difficulty = preferences.getUChar("difficulty", 0);
        lastDifficulty_ = difficulty <=
                                  static_cast<uint8_t>(
                                      pgos::MinesweeperDifficulty::Custom)
                              ? static_cast<pgos::MinesweeperDifficulty>(
                                    difficulty)
                              : pgos::MinesweeperDifficulty::Beginner;
        customWidth_ = std::clamp<uint8_t>(preferences.getUChar("custom_w", 12),
                                           8, 30);
        customHeight_ = std::clamp<uint8_t>(
            preferences.getUChar("custom_h", 12), 8, 16);
        const uint16_t maximum = maximumCustomMines(customWidth_, customHeight_);
        customMines_ = std::clamp<uint16_t>(
            preferences.getUShort("custom_m", 24), 1, maximum);
    }
    preferences.end();
    output.printf("[minesweeper] difficulty=%u custom=%ux%u/%u\n",
                  static_cast<unsigned>(lastDifficulty_),
                  static_cast<unsigned>(customWidth_),
                  static_cast<unsigned>(customHeight_),
                  static_cast<unsigned>(customMines_));
    return true;
}

const pgos::MinesweeperRecords& MinesweeperProfileService::records() const {
    return records_;
}

pgos::MinesweeperRecordUpdate MinesweeperProfileService::recordResult(
    pgos::MinesweeperDifficulty difficulty, bool won, uint32_t elapsedMs) {
    const pgos::MinesweeperRecordUpdate update =
        records_.record(difficulty, won, elapsedMs);
    save();
    return update;
}

pgos::MinesweeperDifficulty
MinesweeperProfileService::lastDifficulty() const {
    return lastDifficulty_;
}

uint8_t MinesweeperProfileService::customWidth() const {
    return customWidth_;
}

uint8_t MinesweeperProfileService::customHeight() const {
    return customHeight_;
}

uint16_t MinesweeperProfileService::customMines() const {
    return customMines_;
}

void MinesweeperProfileService::saveSetup(
    pgos::MinesweeperDifficulty difficulty, uint8_t customWidth,
    uint8_t customHeight, uint16_t customMines) {
    const uint8_t width = std::clamp<uint8_t>(customWidth, 8, 30);
    const uint8_t height = std::clamp<uint8_t>(customHeight, 8, 16);
    const uint16_t mines = std::clamp<uint16_t>(
        customMines, 1, maximumCustomMines(width, height));
    if (lastDifficulty_ == difficulty && customWidth_ == width &&
        customHeight_ == height && customMines_ == mines) {
        return;
    }
    lastDifficulty_ = difficulty;
    customWidth_ = width;
    customHeight_ = height;
    customMines_ = mines;
    save();
}

bool MinesweeperProfileService::save() const {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        if (log_ != nullptr) {
            log_->println(F("[minesweeper] profile save failed"));
        }
        return false;
    }
    preferences.putUChar("schema", SCHEMA_VERSION);
    const pgos::MinesweeperRecordsData& data = records_.data();
    preferences.putBytes("records", &data, sizeof(data));
    preferences.putUChar("difficulty",
                         static_cast<uint8_t>(lastDifficulty_));
    preferences.putUChar("custom_w", customWidth_);
    preferences.putUChar("custom_h", customHeight_);
    preferences.putUShort("custom_m", customMines_);
    preferences.end();
    return true;
}
