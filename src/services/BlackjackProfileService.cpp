#include "services/BlackjackProfileService.h"

#include <Preferences.h>

#include <algorithm>
#include <limits>

namespace {

constexpr char NVS_NAMESPACE[] = "pgos_blackjack";

}  // namespace

bool BlackjackProfileService::begin(Stream& output) {
    log_ = &output;
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        output.println(F(
            "[blackjack] profile storage unavailable; using RAM only"));
        return false;
    }

    const uint8_t schema = preferences.getUChar("schema", 0);
    if (schema != SCHEMA_VERSION) {
        preferences.clear();
        preferences.putUChar("schema", SCHEMA_VERSION);
        preferences.putUInt("bankroll", INITIAL_BANKROLL);
    } else {
        bankroll_ = preferences.getUInt("bankroll", INITIAL_BANKROLL);
        handsPlayed_ = preferences.getUInt("hands", 0);
        wins_ = preferences.getUInt("wins", 0);
        losses_ = preferences.getUInt("losses", 0);
        pushes_ = preferences.getUInt("pushes", 0);
        blackjacks_ = preferences.getUInt("blackjacks", 0);
        currentStreak_ = preferences.getUShort("streak", 0);
        bestStreak_ = preferences.getUShort("best", 0);
        bailoutCount_ = preferences.getUShort("bailouts", 0);
        if (bankroll_ > MAX_BANKROLL) {
            bankroll_ = INITIAL_BANKROLL;
        }
    }
    preferences.end();

    output.printf("[blackjack] bankroll=%lu hands=%lu wins=%lu bailouts=%u\n",
                  static_cast<unsigned long>(bankroll_),
                  static_cast<unsigned long>(handsPlayed_),
                  static_cast<unsigned long>(wins_),
                  static_cast<unsigned>(bailoutCount_));
    return true;
}

uint32_t BlackjackProfileService::bankroll() const { return bankroll_; }

uint32_t BlackjackProfileService::handsPlayed() const { return handsPlayed_; }

uint32_t BlackjackProfileService::wins() const { return wins_; }

uint32_t BlackjackProfileService::losses() const { return losses_; }

uint32_t BlackjackProfileService::pushes() const { return pushes_; }

uint32_t BlackjackProfileService::blackjacks() const { return blackjacks_; }

uint16_t BlackjackProfileService::currentStreak() const {
    return currentStreak_;
}

uint16_t BlackjackProfileService::bestStreak() const { return bestStreak_; }

uint16_t BlackjackProfileService::bailoutCount() const {
    return bailoutCount_;
}

bool BlackjackProfileService::beginWager(uint32_t amount) {
    if (amount < MINIMUM_BET || bankroll_ < amount) {
        return false;
    }
    bankroll_ -= amount;
    return true;
}

bool BlackjackProfileService::addWager(uint32_t amount) {
    if (amount == 0 || bankroll_ < amount) {
        return false;
    }
    bankroll_ -= amount;
    return true;
}

void BlackjackProfileService::cancelRound(uint32_t totalWager) {
    bankroll_ = std::min<uint32_t>(MAX_BANKROLL, bankroll_ + totalWager);
}

void BlackjackProfileService::settleRound(pgos::BlackjackOutcome outcome,
                                          uint32_t payout) {
    bankroll_ = std::min<uint32_t>(MAX_BANKROLL, bankroll_ + payout);
    ++handsPlayed_;

    switch (outcome) {
        case pgos::BlackjackOutcome::PlayerBlackjack:
            ++blackjacks_;
            ++wins_;
            currentStreak_ = currentStreak_ ==
                                     std::numeric_limits<uint16_t>::max()
                                 ? currentStreak_
                                 : currentStreak_ + 1U;
            break;
        case pgos::BlackjackOutcome::PlayerWin:
            ++wins_;
            currentStreak_ = currentStreak_ ==
                                     std::numeric_limits<uint16_t>::max()
                                 ? currentStreak_
                                 : currentStreak_ + 1U;
            break;
        case pgos::BlackjackOutcome::DealerWin:
            ++losses_;
            currentStreak_ = 0;
            break;
        case pgos::BlackjackOutcome::Push:
            ++pushes_;
            break;
        default:
            return;
    }
    bestStreak_ = std::max(bestStreak_, currentStreak_);
    save();
}

bool BlackjackProfileService::canClaimBailout() const {
    return bankroll_ < MINIMUM_BET;
}

bool BlackjackProfileService::claimBailout() {
    if (!canClaimBailout()) {
        return false;
    }
    bankroll_ = BAILOUT_BANKROLL;
    if (bailoutCount_ < std::numeric_limits<uint16_t>::max()) {
        ++bailoutCount_;
    }
    save();
    return true;
}

bool BlackjackProfileService::save() const {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        if (log_ != nullptr) {
            log_->println(F("[blackjack] profile save failed"));
        }
        return false;
    }
    preferences.putUChar("schema", SCHEMA_VERSION);
    preferences.putUInt("bankroll", bankroll_);
    preferences.putUInt("hands", handsPlayed_);
    preferences.putUInt("wins", wins_);
    preferences.putUInt("losses", losses_);
    preferences.putUInt("pushes", pushes_);
    preferences.putUInt("blackjacks", blackjacks_);
    preferences.putUShort("streak", currentStreak_);
    preferences.putUShort("best", bestStreak_);
    preferences.putUShort("bailouts", bailoutCount_);
    preferences.end();
    return true;
}
