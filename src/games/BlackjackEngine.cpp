#include "games/BlackjackEngine.h"

#include <algorithm>
#include <limits>

namespace pgos {

void BlackjackEngine::reset(uint32_t seed) {
    randomState_ = seed == 0 ? 0x21ACCAFEU : seed;
    shoeIndex_ = 0;
    shoeCount_ = 0;
    playerCount_ = 0;
    dealerCount_ = 0;
    eventHead_ = 0;
    eventTail_ = 0;
    eventCount_ = 0;
    wager_ = 0;
    payout_ = 0;
    phase_ = BlackjackPhase::Idle;
    outcome_ = BlackjackOutcome::None;
    shoeReady_ = false;
    holeRevealed_ = false;
}

bool BlackjackEngine::startRound(uint32_t wager) {
    if (wager == 0 || eventCount_ != 0 ||
        (phase_ != BlackjackPhase::Idle &&
         phase_ != BlackjackPhase::RoundOver)) {
        return false;
    }

    playerCount_ = 0;
    dealerCount_ = 0;
    wager_ = wager;
    payout_ = 0;
    outcome_ = BlackjackOutcome::None;
    holeRevealed_ = false;

    if (!shoeReady_ || cardsRemaining() < RESHUFFLE_THRESHOLD) {
        shuffleShoe();
        BlackjackEvent shuffle;
        shuffle.type = BlackjackEventType::ShoeShuffled;
        queueEvent(shuffle);
    }

    if (!drawTo(BlackjackHandOwner::Player, true) ||
        !drawTo(BlackjackHandOwner::Dealer, true) ||
        !drawTo(BlackjackHandOwner::Player, true) ||
        !drawTo(BlackjackHandOwner::Dealer, false)) {
        phase_ = BlackjackPhase::Idle;
        return false;
    }

    const BlackjackHandValue player = playerValue();
    const BlackjackHandValue dealer = dealerValue();
    if (player.blackjack || dealer.blackjack) {
        revealHole();
        if (player.blackjack && dealer.blackjack) {
            settle(BlackjackOutcome::Push);
        } else if (player.blackjack) {
            settle(BlackjackOutcome::PlayerBlackjack);
        } else {
            settle(BlackjackOutcome::DealerWin);
        }
    } else {
        phase_ = BlackjackPhase::PlayerTurn;
    }
    return true;
}

bool BlackjackEngine::hit() {
    if (phase_ != BlackjackPhase::PlayerTurn ||
        !drawTo(BlackjackHandOwner::Player, true)) {
        return false;
    }

    const BlackjackHandValue value = playerValue();
    if (value.bust) {
        revealHole();
        settle(BlackjackOutcome::DealerWin);
    } else if (value.total == 21) {
        playDealer();
    }
    return true;
}

bool BlackjackEngine::stand() {
    if (phase_ != BlackjackPhase::PlayerTurn) {
        return false;
    }
    playDealer();
    return true;
}

bool BlackjackEngine::doubleDown() {
    if (!canDoubleDown() ||
        wager_ > std::numeric_limits<uint32_t>::max() / 2U) {
        return false;
    }
    wager_ *= 2U;
    if (!drawTo(BlackjackHandOwner::Player, true)) {
        wager_ /= 2U;
        return false;
    }

    if (playerValue().bust) {
        revealHole();
        settle(BlackjackOutcome::DealerWin);
    } else {
        playDealer();
    }
    return true;
}

BlackjackPhase BlackjackEngine::phase() const { return phase_; }

BlackjackOutcome BlackjackEngine::outcome() const { return outcome_; }

uint32_t BlackjackEngine::wager() const { return wager_; }

uint32_t BlackjackEngine::payout() const { return payout_; }

bool BlackjackEngine::canDoubleDown() const {
    return phase_ == BlackjackPhase::PlayerTurn && playerCount_ == 2;
}

uint8_t BlackjackEngine::cardsRemaining() const {
    return shoeCount_ > shoeIndex_
               ? static_cast<uint8_t>(shoeCount_ - shoeIndex_)
               : 0;
}

uint8_t BlackjackEngine::playerCardCount() const { return playerCount_; }

uint8_t BlackjackEngine::dealerCardCount() const { return dealerCount_; }

BlackjackCard BlackjackEngine::playerCard(uint8_t index) const {
    return index < playerCount_ ? player_[index] : BlackjackCard{};
}

BlackjackCard BlackjackEngine::dealerCard(uint8_t index) const {
    return index < dealerCount_ ? dealer_[index] : BlackjackCard{};
}

BlackjackHandValue BlackjackEngine::playerValue() const {
    return evaluate(player_, playerCount_);
}

BlackjackHandValue BlackjackEngine::dealerValue() const {
    return evaluate(dealer_, dealerCount_);
}

bool BlackjackEngine::pollEvent(BlackjackEvent& event) {
    if (eventCount_ == 0) {
        return false;
    }
    event = events_[eventHead_];
    eventHead_ = static_cast<uint8_t>((eventHead_ + 1U) % EVENT_QUEUE_SIZE);
    --eventCount_;
    return true;
}

BlackjackHandValue BlackjackEngine::evaluate(const BlackjackCard* cards,
                                             uint8_t count) {
    BlackjackHandValue value;
    if (cards == nullptr || count == 0) {
        return value;
    }

    uint16_t total = 0;
    uint8_t aces = 0;
    for (uint8_t index = 0; index < count; ++index) {
        const uint8_t rank = cards[index].rank;
        if (rank == 1) {
            total += 11;
            ++aces;
        } else if (rank >= 10) {
            total += 10;
        } else {
            total += rank;
        }
    }
    while (total > 21 && aces > 0) {
        total -= 10;
        --aces;
    }

    value.total = static_cast<uint8_t>(std::min<uint16_t>(total, 255));
    value.soft = aces > 0;
    value.blackjack = count == 2 && total == 21;
    value.bust = total > 21;
    return value;
}

uint32_t BlackjackEngine::nextRandom() {
    randomState_ = randomState_ * 1664525U + 1013904223U;
    return randomState_;
}

void BlackjackEngine::shuffleShoe() {
    uint8_t index = 0;
    for (uint8_t suit = 0; suit < 4; ++suit) {
        for (uint8_t rank = 1; rank <= 13; ++rank) {
            shoe_[index++] = BlackjackCard{rank, suit};
        }
    }
    for (uint8_t upper = SHOE_SIZE - 1U; upper > 0; --upper) {
        const uint8_t target = static_cast<uint8_t>(nextRandom() % (upper + 1U));
        std::swap(shoe_[upper], shoe_[target]);
    }
    shoeIndex_ = 0;
    shoeCount_ = SHOE_SIZE;
    shoeReady_ = true;
}

bool BlackjackEngine::drawTo(BlackjackHandOwner owner, bool faceUp) {
    if (!shoeReady_ || shoeIndex_ >= shoeCount_) {
        return false;
    }

    BlackjackCard* hand = owner == BlackjackHandOwner::Player ? player_ : dealer_;
    uint8_t& count = owner == BlackjackHandOwner::Player ? playerCount_ : dealerCount_;
    if (count >= MAX_HAND_CARDS) {
        return false;
    }

    const BlackjackCard card = shoe_[shoeIndex_++];
    hand[count] = card;
    BlackjackEvent event;
    event.type = BlackjackEventType::CardDealt;
    event.owner = owner;
    event.card = card;
    event.cardIndex = count;
    event.faceUp = faceUp;
    ++count;
    return queueEvent(event);
}

void BlackjackEngine::revealHole() {
    if (holeRevealed_ || dealerCount_ < 2) {
        return;
    }
    holeRevealed_ = true;
    BlackjackEvent event;
    event.type = BlackjackEventType::HoleRevealed;
    event.owner = BlackjackHandOwner::Dealer;
    event.card = dealer_[1];
    event.cardIndex = 1;
    queueEvent(event);
}

void BlackjackEngine::playDealer() {
    phase_ = BlackjackPhase::DealerTurn;
    revealHole();
    while (dealerValue().total < 17) {
        if (!drawTo(BlackjackHandOwner::Dealer, true)) {
            break;
        }
    }

    const BlackjackHandValue player = playerValue();
    const BlackjackHandValue dealer = dealerValue();
    if (player.bust) {
        settle(BlackjackOutcome::DealerWin);
    } else if (dealer.bust || player.total > dealer.total) {
        settle(BlackjackOutcome::PlayerWin);
    } else if (player.total < dealer.total) {
        settle(BlackjackOutcome::DealerWin);
    } else {
        settle(BlackjackOutcome::Push);
    }
}

void BlackjackEngine::settle(BlackjackOutcome outcome) {
    outcome_ = outcome;
    switch (outcome) {
        case BlackjackOutcome::PlayerBlackjack:
            payout_ = wager_ > std::numeric_limits<uint32_t>::max() / 5U
                          ? std::numeric_limits<uint32_t>::max()
                          : (wager_ * 5U) / 2U;
            break;
        case BlackjackOutcome::PlayerWin:
            payout_ = wager_ > std::numeric_limits<uint32_t>::max() / 2U
                          ? std::numeric_limits<uint32_t>::max()
                          : wager_ * 2U;
            break;
        case BlackjackOutcome::Push:
            payout_ = wager_;
            break;
        default:
            payout_ = 0;
            break;
    }
    phase_ = BlackjackPhase::RoundOver;

    BlackjackEvent event;
    event.type = BlackjackEventType::RoundSettled;
    event.outcome = outcome_;
    event.payout = payout_;
    queueEvent(event);
}

bool BlackjackEngine::queueEvent(const BlackjackEvent& event) {
    if (eventCount_ >= EVENT_QUEUE_SIZE) {
        return false;
    }
    events_[eventTail_] = event;
    eventTail_ = static_cast<uint8_t>((eventTail_ + 1U) % EVENT_QUEUE_SIZE);
    ++eventCount_;
    return true;
}

#if defined(PGOS_BLACKJACK_TESTING)
void BlackjackEngine::setShoeForTesting(const BlackjackCard* cards,
                                        uint8_t count) {
    count = std::min<uint8_t>(count, SHOE_SIZE);
    for (uint8_t index = 0; index < count; ++index) {
        shoe_[index] = cards[index];
    }
    shoeIndex_ = 0;
    shoeCount_ = count;
    shoeReady_ = true;
}
#endif

}  // namespace pgos
