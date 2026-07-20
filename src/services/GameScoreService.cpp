#include "services/GameScoreService.h"

#include <Preferences.h>

GameScoreService::GameScoreService(const char* nvsNamespace,
                                   uint8_t schemaVersion,
                                   const char* logTag)
    : nvsNamespace_(nvsNamespace),
      logTag_(logTag),
      schemaVersion_(schemaVersion) {}

bool GameScoreService::begin(Stream& output) {
    Preferences preferences;
    if (!preferences.begin(nvsNamespace_, false)) {
        output.printf("[%s] score storage unavailable; using RAM only\n",
                      logTag_);
        return false;
    }

    const bool hasSchema = preferences.isKey("schema");
    const uint8_t storedSchema =
        preferences.getUChar("schema", schemaVersion_);
    if (hasSchema && storedSchema != schemaVersion_) {
        preferences.clear();
        for (uint16_t& score : scores_) {
            score = 0;
        }
    } else {
        for (uint8_t index = 0; index < SCORE_COUNT; ++index) {
            char key[8];
            snprintf(key, sizeof(key), "score%u",
                     static_cast<unsigned>(index));
            scores_[index] = preferences.getUShort(key, 0);
        }
    }
    preferences.putUChar("schema", schemaVersion_);
    preferences.end();
    return true;
}

uint16_t GameScoreService::bestScore() const {
    return scores_[0];
}

uint16_t GameScoreService::scoreAt(uint8_t index) const {
    return index < SCORE_COUNT ? scores_[index] : 0;
}

bool GameScoreService::recordScore(uint16_t score) {
    if (score == 0 || score <= scores_[SCORE_COUNT - 1U]) {
        return false;
    }

    uint8_t position = SCORE_COUNT - 1U;
    while (position > 0 && score > scores_[position - 1U]) {
        scores_[position] = scores_[position - 1U];
        --position;
    }
    scores_[position] = score;
    save();
    return true;
}

bool GameScoreService::save() const {
    Preferences preferences;
    if (!preferences.begin(nvsNamespace_, false)) {
        return false;
    }
    preferences.putUChar("schema", schemaVersion_);
    for (uint8_t index = 0; index < SCORE_COUNT; ++index) {
        char key[8];
        snprintf(key, sizeof(key), "score%u",
                 static_cast<unsigned>(index));
        preferences.putUShort(key, scores_[index]);
    }
    preferences.end();
    return true;
}
