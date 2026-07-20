#pragma once

#include <cstddef>
#include <cstdint>

namespace pgos {

enum class BlackjackHandOwner : uint8_t {
    Player,
    Dealer,
};

enum class BlackjackPhase : uint8_t {
    Idle,
    PlayerTurn,
    DealerTurn,
    RoundOver,
};

enum class BlackjackOutcome : uint8_t {
    None,
    PlayerBlackjack,
    PlayerWin,
    DealerWin,
    Push,
};

enum class BlackjackEventType : uint8_t {
    ShoeShuffled,
    CardDealt,
    HoleRevealed,
    RoundSettled,
};

struct BlackjackCard {
    uint8_t rank = 1;
    uint8_t suit = 0;
};

struct BlackjackHandValue {
    uint8_t total = 0;
    bool soft = false;
    bool blackjack = false;
    bool bust = false;
};

struct BlackjackEvent {
    BlackjackEventType type = BlackjackEventType::ShoeShuffled;
    BlackjackHandOwner owner = BlackjackHandOwner::Player;
    BlackjackCard card{};
    uint8_t cardIndex = 0;
    bool faceUp = true;
    BlackjackOutcome outcome = BlackjackOutcome::None;
    uint32_t payout = 0;
};

class BlackjackEngine {
public:
    static constexpr uint8_t MAX_HAND_CARDS = 12;
    static constexpr uint8_t SHOE_SIZE = 52;
    static constexpr uint8_t RESHUFFLE_THRESHOLD = 15;

    void reset(uint32_t seed);
    bool startRound(uint32_t wager);
    bool hit();
    bool stand();
    bool doubleDown();

    BlackjackPhase phase() const;
    BlackjackOutcome outcome() const;
    uint32_t wager() const;
    uint32_t payout() const;
    bool canDoubleDown() const;
    uint8_t cardsRemaining() const;

    uint8_t playerCardCount() const;
    uint8_t dealerCardCount() const;
    BlackjackCard playerCard(uint8_t index) const;
    BlackjackCard dealerCard(uint8_t index) const;
    BlackjackHandValue playerValue() const;
    BlackjackHandValue dealerValue() const;

    bool pollEvent(BlackjackEvent& event);

    static BlackjackHandValue evaluate(const BlackjackCard* cards,
                                        uint8_t count);

#if defined(PGOS_BLACKJACK_TESTING)
    void setShoeForTesting(const BlackjackCard* cards, uint8_t count);
#endif

private:
    static constexpr uint8_t EVENT_QUEUE_SIZE = 24;

    BlackjackCard shoe_[SHOE_SIZE] = {};
    BlackjackCard player_[MAX_HAND_CARDS] = {};
    BlackjackCard dealer_[MAX_HAND_CARDS] = {};
    BlackjackEvent events_[EVENT_QUEUE_SIZE] = {};
    uint8_t shoeIndex_ = 0;
    uint8_t shoeCount_ = 0;
    uint8_t playerCount_ = 0;
    uint8_t dealerCount_ = 0;
    uint8_t eventHead_ = 0;
    uint8_t eventTail_ = 0;
    uint8_t eventCount_ = 0;
    uint32_t randomState_ = 0x21ACCAFEU;
    uint32_t wager_ = 0;
    uint32_t payout_ = 0;
    BlackjackPhase phase_ = BlackjackPhase::Idle;
    BlackjackOutcome outcome_ = BlackjackOutcome::None;
    bool shoeReady_ = false;
    bool holeRevealed_ = false;

    uint32_t nextRandom();
    void shuffleShoe();
    bool drawTo(BlackjackHandOwner owner, bool faceUp);
    void revealHole();
    void playDealer();
    void settle(BlackjackOutcome outcome);
    bool queueEvent(const BlackjackEvent& event);
};

}  // namespace pgos
