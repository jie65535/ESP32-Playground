#include "services/Game2048ProfileService.h"

#include <Preferences.h>

#include <algorithm>

namespace {
constexpr char NVS_NAMESPACE[] = "pgos_2048";
}

bool Game2048ProfileService::begin(Stream& output) {
    log_ = &output;
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        output.println(F("[2048] profile unavailable; using RAM only"));
        return false;
    }
    const uint8_t schema = preferences.getUChar("schema", 0);
    if (schema == SCHEMA_VERSION) {
        bestScore_ = preferences.getUInt("best_score", 0);
        bestTile_ = preferences.getUInt("best_tile", 0);
        gamesPlayed_ = preferences.getUInt("games", 0);
        wins_ = preferences.getUInt("wins", 0);
    } else {
        preferences.clear();
        preferences.putUChar("schema", SCHEMA_VERSION);
    }
    preferences.end();
    output.printf("[2048] best=%lu tile=%lu games=%lu wins=%lu\n",
                  static_cast<unsigned long>(bestScore_),
                  static_cast<unsigned long>(bestTile_),
                  static_cast<unsigned long>(gamesPlayed_),
                  static_cast<unsigned long>(wins_));
    return true;
}

uint32_t Game2048ProfileService::bestScore() const { return bestScore_; }

uint32_t Game2048ProfileService::bestTile() const { return bestTile_; }

uint32_t Game2048ProfileService::gamesPlayed() const { return gamesPlayed_; }

uint32_t Game2048ProfileService::wins() const { return wins_; }

void Game2048ProfileService::observeBest(uint32_t score, uint32_t tile) {
    if (score <= bestScore_ && tile <= bestTile_) {
        return;
    }
    bestScore_ = std::max(bestScore_, score);
    bestTile_ = std::max(bestTile_, tile);
    save();
}

void Game2048ProfileService::recordGame(uint32_t score, uint32_t tile,
                                         bool won) {
    bestScore_ = std::max(bestScore_, score);
    bestTile_ = std::max(bestTile_, tile);
    if (gamesPlayed_ < MAX_SCORE) {
        ++gamesPlayed_;
    }
    if (won && wins_ < MAX_SCORE) {
        ++wins_;
    }
    save();
}

void Game2048ProfileService::save() {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        if (log_ != nullptr) {
            log_->println(F("[2048] profile save failed"));
        }
        return;
    }
    preferences.putUChar("schema", SCHEMA_VERSION);
    preferences.putUInt("best_score", bestScore_);
    preferences.putUInt("best_tile", bestTile_);
    preferences.putUInt("games", gamesPlayed_);
    preferences.putUInt("wins", wins_);
    preferences.end();
}
