#include "services/SnakeScoreService.h"

bool SnakeScoreService::begin(Stream& log) {
    log_ = &log;
    preferencesReady_ = preferences_.begin("pgos_snake", false);
    if (!preferencesReady_) {
        log.println(F("[snake] score storage unavailable; using RAM only"));
        return false;
    }

    const uint8_t schema = preferences_.getUChar("schema", SETTINGS_SCHEMA);
    if (schema != SETTINGS_SCHEMA) {
        preferences_.clear();
        preferences_.putUChar("schema", SETTINGS_SCHEMA);
        bestScore_ = DEFAULT_BEST_SCORE;
        return true;
    }

    for (uint8_t index = 0; index < SCORE_COUNT; ++index) {
        char key[8];
        snprintf(key, sizeof(key), "score%u", static_cast<unsigned>(index));
        scores_[index] = preferences_.getUShort(key, DEFAULT_BEST_SCORE);
    }
    bestScore_ = scores_[0];
    return true;
}

uint16_t SnakeScoreService::bestScore() const {
    return bestScore_;
}

uint16_t SnakeScoreService::scoreAt(uint8_t index) const {
    return index < SCORE_COUNT ? scores_[index] : DEFAULT_BEST_SCORE;
}

bool SnakeScoreService::recordScore(uint16_t score) {
    if (score == 0 || score <= scores_[SCORE_COUNT - 1U]) {
        return false;
    }

    uint8_t insertAt = SCORE_COUNT - 1U;
    while (insertAt > 0 && score > scores_[insertAt - 1U]) {
        scores_[insertAt] = scores_[insertAt - 1U];
        --insertAt;
    }
    scores_[insertAt] = score;
    bestScore_ = scores_[0];
    if (preferencesReady_) {
        for (uint8_t index = 0; index < SCORE_COUNT; ++index) {
            char key[8];
            snprintf(key, sizeof(key), "score%u", static_cast<unsigned>(index));
            preferences_.putUShort(key, scores_[index]);
        }
    }
    return true;
}
