#pragma once

#include "games/BlackjackEngine.h"

#include <Arduino.h>

class BlackjackProfileService {
public:
    static constexpr uint32_t INITIAL_BANKROLL = 1000;
    static constexpr uint32_t BAILOUT_BANKROLL = 500;
    static constexpr uint16_t MINIMUM_BET = 10;

    bool begin(Stream& output);

    uint32_t bankroll() const;
    uint32_t handsPlayed() const;
    uint32_t wins() const;
    uint32_t losses() const;
    uint32_t pushes() const;
    uint32_t blackjacks() const;
    uint16_t currentStreak() const;
    uint16_t bestStreak() const;
    uint16_t bailoutCount() const;

    bool beginWager(uint32_t amount);
    bool addWager(uint32_t amount);
    void cancelRound(uint32_t totalWager);
    void settleRound(pgos::BlackjackOutcome outcome, uint32_t payout);
    bool canClaimBailout() const;
    bool claimBailout();

private:
    static constexpr uint8_t SCHEMA_VERSION = 4;
    static constexpr uint32_t MAX_BANKROLL = 9999999U;

    Stream* log_ = nullptr;
    uint32_t bankroll_ = INITIAL_BANKROLL;
    uint32_t handsPlayed_ = 0;
    uint32_t wins_ = 0;
    uint32_t losses_ = 0;
    uint32_t pushes_ = 0;
    uint32_t blackjacks_ = 0;
    uint16_t currentStreak_ = 0;
    uint16_t bestStreak_ = 0;
    uint16_t bailoutCount_ = 0;

    bool save() const;
};
