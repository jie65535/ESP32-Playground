#include "services/PlatformerProgressService.h"

#include <Preferences.h>

namespace {

constexpr char NVS_NAMESPACE[] = "pgos_mario";

bool validLevel(uint8_t world, uint8_t stage) {
    return world >= 1U && world <= 8U && stage >= 1U && stage <= 4U;
}

}  // namespace

bool PlatformerProgressService::begin(Stream& output) {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        output.println(F(
            "[platformer] progress storage unavailable; using RAM only"));
        return false;
    }
    const uint8_t schema = preferences.getUChar("schema", 0U);
    if (schema != SCHEMA_VERSION) {
        preferences.clear();
        preferences.putUChar("schema", SCHEMA_VERSION);
    } else {
        continueWorld_ = preferences.getUChar("world", 1U);
        continueStage_ = preferences.getUChar("stage", 1U);
        bestScore_ = preferences.getUInt("best", 0U);
        completed_ = preferences.getBool("complete", false);
        if (!validLevel(continueWorld_, continueStage_)) {
            continueWorld_ = 1U;
            continueStage_ = 1U;
        }
    }
    preferences.end();
    output.printf("[platformer] continue=%u-%u best=%lu complete=%s\n",
                  static_cast<unsigned>(continueWorld_),
                  static_cast<unsigned>(continueStage_),
                  static_cast<unsigned long>(bestScore_),
                  completed_ ? "yes" : "no");
    return true;
}

uint8_t PlatformerProgressService::continueWorld() const {
    return continueWorld_;
}

uint8_t PlatformerProgressService::continueStage() const {
    return continueStage_;
}

uint32_t PlatformerProgressService::bestScore() const {
    return bestScore_;
}

bool PlatformerProgressService::completed() const {
    return completed_;
}

bool PlatformerProgressService::recordCourseClear(
    uint8_t world, uint8_t stage, uint8_t nextWorld, uint8_t nextStage,
    uint32_t score) {
    bool changed = false;
    if (score > bestScore_) {
        bestScore_ = score;
        changed = true;
    }
    if (!validLevel(nextWorld, nextStage)) {
        if (world == 8U && stage == 4U && !completed_) {
            completed_ = true;
            changed = true;
        }
    } else if (levelIndex(nextWorld, nextStage) >
               levelIndex(continueWorld_, continueStage_)) {
        continueWorld_ = nextWorld;
        continueStage_ = nextStage;
        changed = true;
    }
    return !changed || save();
}

bool PlatformerProgressService::save() const {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        return false;
    }
    preferences.putUChar("schema", SCHEMA_VERSION);
    preferences.putUChar("world", continueWorld_);
    preferences.putUChar("stage", continueStage_);
    preferences.putUInt("best", bestScore_);
    preferences.putBool("complete", completed_);
    preferences.end();
    return true;
}

uint8_t PlatformerProgressService::levelIndex(uint8_t world, uint8_t stage) {
    return validLevel(world, stage)
               ? static_cast<uint8_t>((world - 1U) * 4U + stage)
               : 0U;
}
