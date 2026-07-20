#define PGOS_BLACKJACK_TESTING 1

#include "games/BlackjackEngine.h"

#include <cassert>
#include <cstdint>
#include <vector>

using pgos::BlackjackCard;
using pgos::BlackjackEngine;
using pgos::BlackjackEvent;
using pgos::BlackjackEventType;
using pgos::BlackjackOutcome;

namespace {

BlackjackCard card(uint8_t rank, uint8_t suit = 0) {
    return BlackjackCard{rank, suit};
}

void loadShoe(BlackjackEngine& engine,
              std::initializer_list<BlackjackCard> opening) {
    std::vector<BlackjackCard> shoe(opening);
    while (shoe.size() < 20) {
        shoe.push_back(card(static_cast<uint8_t>(2 + shoe.size() % 8), 3));
    }
    engine.setShoeForTesting(shoe.data(), static_cast<uint8_t>(shoe.size()));
}

void testHandValues() {
    const BlackjackCard soft17[] = {card(1), card(6)};
    auto value = BlackjackEngine::evaluate(soft17, 2);
    assert(value.total == 17 && value.soft && !value.blackjack && !value.bust);

    const BlackjackCard hard17[] = {card(1), card(6), card(10)};
    value = BlackjackEngine::evaluate(hard17, 3);
    assert(value.total == 17 && !value.soft && !value.bust);

    const BlackjackCard natural[] = {card(1), card(13)};
    value = BlackjackEngine::evaluate(natural, 2);
    assert(value.total == 21 && value.soft && value.blackjack);

    const BlackjackCard bust[] = {card(10), card(12), card(2)};
    value = BlackjackEngine::evaluate(bust, 3);
    assert(value.total == 22 && value.bust);
}

void testNaturalBlackjackPayoutAndEvents() {
    BlackjackEngine engine;
    engine.reset(1);
    loadShoe(engine, {card(1), card(9), card(13), card(7)});
    assert(engine.startRound(10));
    assert(engine.outcome() == BlackjackOutcome::PlayerBlackjack);
    assert(engine.payout() == 25);

    BlackjackEvent event;
    uint8_t dealt = 0;
    bool hiddenDealerCard = false;
    bool revealed = false;
    bool settled = false;
    while (engine.pollEvent(event)) {
        if (event.type == BlackjackEventType::CardDealt) {
            ++dealt;
            hiddenDealerCard |= !event.faceUp;
        } else if (event.type == BlackjackEventType::HoleRevealed) {
            revealed = true;
        } else if (event.type == BlackjackEventType::RoundSettled) {
            settled = event.outcome == BlackjackOutcome::PlayerBlackjack &&
                      event.payout == 25;
        }
    }
    assert(dealt == 4 && hiddenDealerCard && revealed && settled);
}

void testDealerNaturalWins() {
    BlackjackEngine engine;
    engine.reset(2);
    loadShoe(engine, {card(10), card(1), card(9), card(13)});
    assert(engine.startRound(50));
    assert(engine.outcome() == BlackjackOutcome::DealerWin);
    assert(engine.payout() == 0);
}

void testPlayerBustsOnHit() {
    BlackjackEngine engine;
    engine.reset(3);
    loadShoe(engine, {card(10), card(9), card(6), card(7), card(10)});
    assert(engine.startRound(10));
    BlackjackEvent event;
    while (engine.pollEvent(event)) {}
    assert(engine.hit());
    assert(engine.playerValue().bust);
    assert(engine.outcome() == BlackjackOutcome::DealerWin);
}

void testDealerDrawsAndBusts() {
    BlackjackEngine engine;
    engine.reset(4);
    loadShoe(engine, {card(10), card(6), card(8), card(9), card(10)});
    assert(engine.startRound(10));
    BlackjackEvent event;
    while (engine.pollEvent(event)) {}
    assert(engine.stand());
    assert(engine.dealerCardCount() == 3);
    assert(engine.dealerValue().bust);
    assert(engine.outcome() == BlackjackOutcome::PlayerWin);
    assert(engine.payout() == 20);
}

void testDealerStandsOnSoft17() {
    BlackjackEngine engine;
    engine.reset(5);
    loadShoe(engine, {card(10), card(1), card(7), card(6), card(10)});
    assert(engine.startRound(10));
    BlackjackEvent event;
    while (engine.pollEvent(event)) {}
    assert(engine.stand());
    assert(engine.dealerCardCount() == 2);
    assert(engine.dealerValue().soft);
    assert(engine.outcome() == BlackjackOutcome::Push);
    assert(engine.payout() == 10);
}

void testDoubleDown() {
    BlackjackEngine engine;
    engine.reset(6);
    loadShoe(engine, {card(5), card(10), card(6), card(7), card(10)});
    assert(engine.startRound(10));
    BlackjackEvent event;
    while (engine.pollEvent(event)) {}
    assert(engine.canDoubleDown());
    assert(engine.doubleDown());
    assert(engine.wager() == 20);
    assert(engine.playerValue().total == 21);
    assert(engine.outcome() == BlackjackOutcome::PlayerWin);
    assert(engine.payout() == 40);
}

}  // namespace

int main() {
    testHandValues();
    testNaturalBlackjackPayoutAndEvents();
    testDealerNaturalWins();
    testPlayerBustsOnHit();
    testDealerDrawsAndBusts();
    testDealerStandsOnSoft17();
    testDoubleDown();
    return 0;
}
