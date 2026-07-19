#include "services/TetrisScoreService.h"

#include <Preferences.h>

void TetrisScoreService::begin(Stream& output) {
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false)) {
        output.println(F("[tetris] score storage unavailable"));
        return;
    }
    const uint8_t schema = preferences.getUChar("schema", 0);
    if (schema != SCHEMA_VERSION) {
        for (auto& score : scores_) {
            score = 0;
        }
        preferences.clear();
        preferences.putUChar("schema", SCHEMA_VERSION);
    } else {
        for (uint8_t index = 0; index < SCORE_COUNT; ++index) {
            char key[8];
            snprintf(key, sizeof(key), "score%u", static_cast<unsigned>(index));
            scores_[index] = preferences.getUShort(key, 0);
        }
    }
    preferences.end();
}

uint16_t TetrisScoreService::bestScore() const {
    return scores_[0];
}

uint16_t TetrisScoreService::scoreAt(uint8_t index) const {
    return index < SCORE_COUNT ? scores_[index] : 0;
}

void TetrisScoreService::recordScore(uint16_t score) {
    if (score == 0 || score <= scores_[SCORE_COUNT - 1U]) {
        return;
    }
    uint8_t position = SCORE_COUNT - 1U;
    while (position > 0 && score > scores_[position - 1U]) {
        scores_[position] = scores_[position - 1U];
        --position;
    }
    scores_[position] = score;

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false)) {
        return;
    }
    preferences.putUChar("schema", SCHEMA_VERSION);
    for (uint8_t index = 0; index < SCORE_COUNT; ++index) {
        char key[8];
        snprintf(key, sizeof(key), "score%u", static_cast<unsigned>(index));
        preferences.putUShort(key, scores_[index]);
    }
    preferences.end();
}
