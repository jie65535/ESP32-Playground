#include "apps/BlackjackApp.h"

#include "services/AudioService.h"
#include "services/BlackjackProfileService.h"
#include "services/BleGamepadService.h"
#include "services/RgbService.h"
#include "services/TimeService.h"
#include "ui/CanvasDraw.h"
#include "ui/UiRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

using pgos::drawRect;
using pgos::drawText;

namespace {

constexpr uint32_t BET_OPTIONS[] = {10, 50, 100, 0};
constexpr const char* SUIT_SYMBOLS[] = {"♠", "♥", "♦", "♣"};

float easeOutCubic(float value) {
    const float inverse = 1.0F - value;
    return 1.0F - inverse * inverse * inverse;
}

int16_t interpolate(int16_t from, int16_t to, float progress) {
    return static_cast<int16_t>(
        std::lround(from + (to - from) * progress));
}

}  // namespace

AppId BlackjackApp::id() const { return AppId::Blackjack; }

const char* BlackjackApp::name() const { return "Blackjack"; }

void BlackjackApp::onEnter(AppContext& context) {
    root_ = nullptr;
    surface_ = nullptr;
    phase_ = ViewPhase::Intro;
    eventActive_ = false;
    wagerActive_ = false;
    bailoutAnimating_ = false;
    holeRevealed_ = false;
    doubleAvailable_ = false;
    visiblePlayerCards_ = 0;
    visibleDealerCards_ = 0;
    betSelection_ = 0;
    actionSelection_ = 0;
    analogXSign_ = 0;
    wagerDebited_ = 0;
    settlementFromBankroll_ = context.blackjackProfile.bankroll();
    settlementToBankroll_ = settlementFromBankroll_;
    profileWins_ = context.blackjackProfile.wins();
    profileBlackjacks_ = context.blackjackProfile.blackjacks();
    profileBestStreak_ = context.blackjackProfile.bestStreak();
    engine_.reset(static_cast<uint32_t>(micros()) ^
                  static_cast<uint32_t>(context.blackjackProfile.bankroll()));
}

void BlackjackApp::onExit(AppContext& context) {
    if (wagerActive_ && wagerDebited_ != 0) {
        context.blackjackProfile.cancelRound(wagerDebited_);
    }
    wagerActive_ = false;
    wagerDebited_ = 0;
    root_ = nullptr;
    surface_ = nullptr;
    hudFont_ = nullptr;
    bodyFont_ = nullptr;
    strongBodyFont_ = nullptr;
    rankFont_ = nullptr;
    titleFont_ = nullptr;
}

void BlackjackApp::onCommand(const AppCommand& command, AppContext& context) {
    if (eventActive_ || bailoutAnimating_) {
        return;
    }

    switch (command.type) {
        case AppCommandType::Previous:
        case AppCommandType::Left:
            moveSelection(-1, context);
            break;
        case AppCommandType::Next:
        case AppCommandType::Right:
            moveSelection(1, context);
            break;
        case AppCommandType::QuickDrop:
            if (phase_ == ViewPhase::PlayerTurn) {
                executeAction(PlayerAction::Stand, context, millis());
            }
            break;
        case AppCommandType::Activate:
            if (phase_ == ViewPhase::Intro) {
                if (context.blackjackProfile.canClaimBailout()) {
                    phase_ = ViewPhase::Bailout;
                } else {
                    enterBetting(context);
                }
            } else if (phase_ == ViewPhase::Betting) {
                startRound(context, millis());
            } else if (phase_ == ViewPhase::PlayerTurn) {
                executeAction(selectedAction(context), context, millis());
            } else if (phase_ == ViewPhase::RoundOver) {
                if (context.blackjackProfile.canClaimBailout()) {
                    phase_ = ViewPhase::Bailout;
                } else {
                    enterBetting(context);
                }
            } else if (phase_ == ViewPhase::Bailout) {
                claimBailout(context, millis());
            }
            invalidate();
            break;
        default:
            break;
    }
}

void BlackjackApp::onTick(uint32_t nowMs, AppContext& context) {
    if (surface_ == nullptr) {
        return;
    }

    sampleAnalog(context.gamepad.snapshot(), context);
    if (bailoutAnimating_) {
        if (nowMs - bailoutStartedMs_ >= BAILOUT_ANIMATION_MS) {
            bailoutAnimating_ = false;
            enterBetting(context);
        }
        invalidate();
        return;
    }

    if (!eventActive_) {
        return;
    }
    if (nowMs - eventStartedMs_ >= eventDurationMs_) {
        finishActiveEvent(context, nowMs);
    }
    invalidate();
}

lv_obj_t* BlackjackApp::onCreateView(AppContext& context) {
    root_ = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, SURFACE_WIDTH, SURFACE_HEIGHT);
    lv_obj_set_pos(root_, 0, 22);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    surface_ = lv_obj_create(root_);
    lv_obj_remove_style_all(surface_);
    lv_obj_set_size(surface_, SURFACE_WIDTH, SURFACE_HEIGHT);
    lv_obj_set_pos(surface_, 0, 0);
    lv_obj_clear_flag(surface_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(surface_, drawEvent, LV_EVENT_DRAW_MAIN, this);

    hudFont_ = context.ui.font(12);
    bodyFont_ = context.ui.bitmapFont(BitmapFontSize::Small12);
    strongBodyFont_ = context.ui.bitmapFont(BitmapFontSize::Bold12);
    rankFont_ = context.ui.font(20);
    titleFont_ = context.ui.font(24);

    const TimeSnapshot time = context.time.snapshot();
    const bool night = time.effectiveTimeValid &&
                       (time.effectiveDateTime.hour >= 18 ||
                        time.effectiveDateTime.hour < 6);
    feltColor_ = lv_color_hex(night ? 0x063A34 : 0x0A5748);
    feltRaisedColor_ = lv_color_hex(night ? 0x0A4B43 : 0x11705C);
    headerColor_ = lv_color_hex(night ? 0x052A27 : 0x073B33);
    cardColor_ = lv_color_hex(0xF4F0E4);
    cardInkColor_ = lv_color_hex(0x17202A);
    redSuitColor_ = lv_color_hex(0xC9414A);
    goldColor_ = lv_color_hex(0xE6B94A);
    mutedColor_ = lv_color_hex(0xA9C4BC);
    panelColor_ = lv_color_hex(0x092D29);
    textColor_ = lv_color_hex(0xF7F5ED);
    invalidate();
    return root_;
}

void BlackjackApp::onUpdateView(AppContext&) { invalidate(); }

void BlackjackApp::drawEvent(lv_event_t* event) {
    auto* app = static_cast<BlackjackApp*>(lv_event_get_user_data(event));
    if (app != nullptr && lv_event_get_target_obj(event) == app->surface_) {
        app->draw(event);
    }
}

void BlackjackApp::draw(lv_event_t* event) {
    lv_obj_t* object = lv_event_get_target_obj(event);
    lv_layer_t* layer = lv_event_get_layer(event);
    if (object == nullptr || layer == nullptr) {
        return;
    }

    lv_area_t surfaceArea;
    lv_obj_get_coords(object, &surfaceArea);
    const uint32_t nowMs = millis();
    drawRect(layer, surfaceArea, feltColor_);

    lv_area_t hud = surfaceArea;
    hud.y2 = hud.y1 + HUD_HEIGHT - 1;
    drawRect(layer, hud, headerColor_);
    lv_area_t titleArea = hud;
    titleArea.x1 += 8;
    titleArea.x2 = titleArea.x1 + 105;
    drawText(layer, "BLACKJACK", titleArea, textColor_, hudFont_,
             LV_TEXT_ALIGN_LEFT);

    char money[40];
    lv_snprintf(money, sizeof(money), "筹码 %lu",
                static_cast<unsigned long>(displayedBankroll(nowMs)));
    lv_area_t moneyArea = hud;
    moneyArea.x1 = surfaceArea.x2 - 150;
    moneyArea.x2 -= 8;
    drawText(layer, money, moneyArea, goldColor_, strongBodyFont_,
             LV_TEXT_ALIGN_RIGHT);

    if (phase_ == ViewPhase::Intro) {
        drawIntro(layer, surfaceArea);
        return;
    }
    if (phase_ == ViewPhase::Bailout) {
        drawBailout(layer, surfaceArea, nowMs);
        return;
    }

    drawTable(layer, surfaceArea, nowMs);
    if (phase_ == ViewPhase::Betting) {
        drawBetting(layer, surfaceArea);
    } else if (phase_ == ViewPhase::PlayerTurn) {
        drawActions(layer, surfaceArea);
    } else if (phase_ == ViewPhase::RoundOver ||
               (eventActive_ &&
                activeEvent_.type == pgos::BlackjackEventType::RoundSettled)) {
        drawOutcome(layer, surfaceArea, nowMs);
    } else if (phase_ == ViewPhase::Animating) {
        lv_area_t status = surfaceArea;
        status.x1 += 70;
        status.x2 -= 70;
        status.y1 = surfaceArea.y1 + ACTION_BAR_Y + 7;
        status.y2 = surfaceArea.y2 - 6;
        const char* text = activeEvent_.type ==
                                   pgos::BlackjackEventType::ShoeShuffled
                               ? "洗牌中"
                               : activeEvent_.owner ==
                                         pgos::BlackjackHandOwner::Dealer &&
                                     holeRevealed_
                                 ? "庄家行动"
                                 : "发牌中";
        drawText(layer, text, status, mutedColor_, bodyFont_);
    }
}

void BlackjackApp::drawTable(lv_layer_t* layer, const lv_area_t& surfaceArea,
                             uint32_t nowMs) {
    lv_area_t inner = surfaceArea;
    inner.x1 += 5;
    inner.x2 -= 5;
    inner.y1 += HUD_HEIGHT + 3;
    inner.y2 -= 45;
    drawRect(layer, inner, feltRaisedColor_, 7, LV_OPA_30);

    if (phase_ == ViewPhase::Betting) {
        drawShoe(layer, surfaceArea);
        return;
    }

    char valueText[32];
    lv_area_t dealerLabel = surfaceArea;
    dealerLabel.x1 += 10;
    dealerLabel.x2 = dealerLabel.x1 + 100;
    dealerLabel.y1 += 27;
    dealerLabel.y2 = dealerLabel.y1 + 16;
    if (!holeRevealed_ && visibleDealerCards_ >= 2) {
        lv_snprintf(valueText, sizeof(valueText), "庄家  ?");
    } else {
        pgos::BlackjackCard cards[pgos::BlackjackEngine::MAX_HAND_CARDS];
        for (uint8_t index = 0; index < visibleDealerCards_; ++index) {
            cards[index] = engine_.dealerCard(index);
        }
        const pgos::BlackjackHandValue value =
            pgos::BlackjackEngine::evaluate(cards, visibleDealerCards_);
        lv_snprintf(valueText, sizeof(valueText), "庄家  %u",
                    static_cast<unsigned>(value.total));
    }
    drawText(layer, valueText, dealerLabel, textColor_, strongBodyFont_,
             LV_TEXT_ALIGN_LEFT);

    lv_area_t playerLabel = dealerLabel;
    playerLabel.y1 = surfaceArea.y1 + 97;
    playerLabel.y2 = playerLabel.y1 + 16;
    pgos::BlackjackCard playerCards[pgos::BlackjackEngine::MAX_HAND_CARDS];
    for (uint8_t index = 0; index < visiblePlayerCards_; ++index) {
        playerCards[index] = engine_.playerCard(index);
    }
    const pgos::BlackjackHandValue player =
        pgos::BlackjackEngine::evaluate(playerCards, visiblePlayerCards_);
    if (player.soft && player.total >= 11) {
        lv_snprintf(valueText, sizeof(valueText), "玩家  %u / %u",
                    static_cast<unsigned>(player.total - 10U),
                    static_cast<unsigned>(player.total));
    } else {
        lv_snprintf(valueText, sizeof(valueText), "玩家  %u",
                    static_cast<unsigned>(player.total));
    }
    drawText(layer, valueText, playerLabel,
             player.bust ? redSuitColor_ : textColor_, strongBodyFont_,
             LV_TEXT_ALIGN_LEFT);

    drawHand(layer, surfaceArea, pgos::BlackjackHandOwner::Dealer, nowMs);
    drawHand(layer, surfaceArea, pgos::BlackjackHandOwner::Player, nowMs);
    drawShoe(layer, surfaceArea);
}

void BlackjackApp::drawIntro(lv_layer_t* layer,
                             const lv_area_t& surfaceArea) const {
    const pgos::BlackjackCard ace{1, 0};
    const pgos::BlackjackCard king{13, 1};
    drawCard(layer, ace, surfaceArea.x1 + 108, surfaceArea.y1 + 48, true);
    drawCard(layer, king, surfaceArea.x1 + 166, surfaceArea.y1 + 48, true);

    lv_area_t title = surfaceArea;
    title.y1 += 112;
    title.y2 = title.y1 + 31;
    drawText(layer, "BLACKJACK", title, goldColor_, titleFont_);
    lv_area_t subtitle = title;
    subtitle.y1 += 32;
    subtitle.y2 = subtitle.y1 + 18;
    drawText(layer, "二十一点 · A 开始", subtitle, textColor_, bodyFont_);

    char stats[64];
    lv_snprintf(stats, sizeof(stats), "胜 %lu  Blackjack %lu  最佳连胜 %u",
                static_cast<unsigned long>(profileWins_),
                static_cast<unsigned long>(profileBlackjacks_),
                static_cast<unsigned>(profileBestStreak_));
    lv_area_t statsArea = surfaceArea;
    statsArea.y1 = surfaceArea.y2 - 31;
    statsArea.y2 = surfaceArea.y2 - 10;
    drawText(layer, stats, statsArea, mutedColor_, bodyFont_);
}

void BlackjackApp::drawBetting(lv_layer_t* layer,
                               const lv_area_t& surfaceArea) const {
    lv_area_t prompt = surfaceArea;
    prompt.y1 = surfaceArea.y1 + 67;
    prompt.y2 = prompt.y1 + 23;
    drawText(layer, "选择下注", prompt, textColor_, strongBodyFont_);

    const uint32_t availableBankroll =
        settlementToBankroll_ - settlementToBankroll_ % 10U;
    for (uint8_t index = 0; index < 4; ++index) {
        const int16_t x = surfaceArea.x1 + 8 + index * 77;
        lv_area_t button = {
            static_cast<lv_coord_t>(x),
            static_cast<lv_coord_t>(surfaceArea.y1 + 103),
            static_cast<lv_coord_t>(x + 69),
            static_cast<lv_coord_t>(surfaceArea.y1 + 137),
        };
        const bool enabled = index == 3 || BET_OPTIONS[index] <= availableBankroll;
        const bool selected = enabled && index == betSelection_;
        drawRect(layer, button, selected ? goldColor_ : panelColor_, 5);
        button.x1 += 2;
        button.x2 -= 2;
        button.y1 += 2;
        button.y2 -= 2;
        drawRect(layer, button, selected ? headerColor_ : feltColor_, 4);
        char label[16];
        if (index == 3) {
            lv_snprintf(label, sizeof(label), "全押");
        } else {
            lv_snprintf(label, sizeof(label), "%lu",
                        static_cast<unsigned long>(BET_OPTIONS[index]));
        }
        drawText(layer, label, button,
                 !enabled ? mutedColor_ : selected ? goldColor_ : textColor_,
                 selected ? strongBodyFont_ : bodyFont_);
    }

    lv_area_t hint = surfaceArea;
    hint.y1 = surfaceArea.y2 - 31;
    hint.y2 = surfaceArea.y2 - 8;
    drawText(layer, "左右选择 · A 发牌", hint, mutedColor_, bodyFont_);
}

void BlackjackApp::drawBailout(lv_layer_t* layer,
                               const lv_area_t& surfaceArea,
                               uint32_t nowMs) const {
    lv_area_t panel = surfaceArea;
    panel.x1 += 34;
    panel.x2 -= 34;
    panel.y1 += 54;
    panel.y2 -= 35;
    drawRect(layer, panel, panelColor_, 8, LV_OPA_90);

    lv_area_t title = panel;
    title.y1 += 16;
    title.y2 = title.y1 + 26;
    drawText(layer, bailoutAnimating_ ? "补助已到账" : "资金不足", title,
             bailoutAnimating_ ? goldColor_ : textColor_, strongBodyFont_);
    lv_area_t detail = panel;
    detail.y1 = title.y2 + 10;
    detail.y2 = detail.y1 + 20;
    drawText(layer,
             bailoutAnimating_ ? "PGOS 补助  +500"
                               : "领取 PGOS 补助 500",
             detail, mutedColor_, bodyFont_);
    lv_area_t hint = panel;
    hint.y1 = panel.y2 - 26;
    hint.y2 = panel.y2 - 8;
    drawText(layer, bailoutAnimating_ ? "财务部门已批准" : "A 确认",
             hint, textColor_, bodyFont_);

    if (bailoutAnimating_) {
        const float progress = std::min(
            1.0F, static_cast<float>(nowMs - bailoutStartedMs_) /
                      static_cast<float>(BAILOUT_ANIMATION_MS));
        for (uint8_t index = 0; index < 5; ++index) {
            const float delayed = std::max(
                0.0F, std::min(1.0F, progress * 1.5F - index * 0.1F));
            const int16_t x = surfaceArea.x1 + 128 + index * 16;
            const int16_t y = interpolate(surfaceArea.y1 + 30,
                                          surfaceArea.y1 + 151,
                                          easeOutCubic(delayed));
            drawChip(layer, x, y, index % 2 == 0 ? goldColor_ : redSuitColor_);
        }
    }
}

void BlackjackApp::drawActions(lv_layer_t* layer,
                               const lv_area_t& surfaceArea) const {
    static constexpr const char* LABELS[] = {"要牌", "停牌", "双倍"};
    const uint8_t count = doubleAvailable_ ? 3 : 2;
    for (uint8_t index = 0; index < 3; ++index) {
        const int16_t x = surfaceArea.x1 + 8 + index * 103;
        lv_area_t button = {
            static_cast<lv_coord_t>(x),
            static_cast<lv_coord_t>(surfaceArea.y1 + ACTION_BAR_Y + 5),
            static_cast<lv_coord_t>(x + 95),
            static_cast<lv_coord_t>(surfaceArea.y2 - 6),
        };
        const bool enabled = index < count;
        const bool selected = enabled && index == actionSelection_;
        drawRect(layer, button, selected ? goldColor_ : panelColor_, 5);
        button.x1 += 2;
        button.x2 -= 2;
        button.y1 += 2;
        button.y2 -= 2;
        drawRect(layer, button, selected ? headerColor_ : feltColor_, 4);
        drawText(layer, LABELS[index], button,
                 !enabled ? mutedColor_ : selected ? goldColor_ : textColor_,
                 selected ? strongBodyFont_ : bodyFont_);
    }
}

void BlackjackApp::drawOutcome(lv_layer_t* layer,
                               const lv_area_t& surfaceArea,
                               uint32_t nowMs) const {
    lv_area_t banner = surfaceArea;
    banner.x1 += 7;
    banner.x2 -= 7;
    banner.y1 = surfaceArea.y1 + ACTION_BAR_Y + 2;
    banner.y2 = surfaceArea.y2 - 4;
    drawRect(layer, banner, panelColor_, 6, LV_OPA_90);

    char result[48];
    const uint32_t wager = engine_.wager();
    const uint32_t payout = engine_.payout();
    switch (engine_.outcome()) {
        case pgos::BlackjackOutcome::PlayerBlackjack:
            lv_snprintf(result, sizeof(result), "BLACKJACK  +%lu",
                        static_cast<unsigned long>(payout - wager));
            break;
        case pgos::BlackjackOutcome::PlayerWin:
            lv_snprintf(result, sizeof(result), "获胜  +%lu",
                        static_cast<unsigned long>(payout - wager));
            break;
        case pgos::BlackjackOutcome::DealerWin:
            lv_snprintf(result, sizeof(result), "庄家获胜  -%lu",
                        static_cast<unsigned long>(wager));
            break;
        default:
            lv_snprintf(result, sizeof(result), "平局  退回 %lu",
                        static_cast<unsigned long>(wager));
            break;
    }
    lv_area_t resultArea = banner;
    resultArea.x2 -= 88;
    drawText(layer, result, resultArea,
             engine_.outcome() == pgos::BlackjackOutcome::DealerWin
                 ? redSuitColor_
                 : goldColor_,
             strongBodyFont_);
    lv_area_t hint = banner;
    hint.x1 = banner.x2 - 83;
    drawText(layer, eventActive_ ? "结算中" : "A 下一局", hint,
             textColor_, bodyFont_);

    if (eventActive_) {
        const float progress = activeProgress(nowMs);
        const bool loss = engine_.outcome() ==
                          pgos::BlackjackOutcome::DealerWin;
        for (uint8_t index = 0; index < 4; ++index) {
            const int16_t x = surfaceArea.x1 + 132 + index * 18;
            const int16_t y = loss
                                  ? interpolate(surfaceArea.y1 + 166,
                                                surfaceArea.y1 + 48, progress)
                                  : interpolate(surfaceArea.y1 + 48,
                                                surfaceArea.y1 + 166, progress);
            drawChip(layer, x, y,
                     index % 2 == 0 ? goldColor_ : redSuitColor_);
        }
    }
}

void BlackjackApp::drawShoe(lv_layer_t* layer,
                            const lv_area_t& surfaceArea) const {
    const int16_t x = surfaceArea.x2 - 35;
    const int16_t y = surfaceArea.y1 + 31;
    for (int8_t offset = 2; offset >= 0; --offset) {
        lv_area_t card = {
            static_cast<lv_coord_t>(x - offset * 2),
            static_cast<lv_coord_t>(y + offset * 2),
            static_cast<lv_coord_t>(x + 25 - offset * 2),
            static_cast<lv_coord_t>(y + 37 + offset * 2),
        };
        drawRect(layer, card, offset == 0 ? redSuitColor_ : cardColor_, 3);
    }
    const uint8_t segments = static_cast<uint8_t>(
        (engine_.cardsRemaining() * 4U + 51U) / 52U);
    for (uint8_t index = 0; index < 4; ++index) {
        lv_area_t mark = {
            static_cast<lv_coord_t>(x + 5 + index * 5),
            static_cast<lv_coord_t>(y + 30),
            static_cast<lv_coord_t>(x + 7 + index * 5),
            static_cast<lv_coord_t>(y + 32),
        };
        drawRect(layer, mark, index < segments ? goldColor_ : panelColor_, 1);
    }
}

void BlackjackApp::drawHand(lv_layer_t* layer,
                            const lv_area_t& surfaceArea,
                            pgos::BlackjackHandOwner owner,
                            uint32_t nowMs) const {
    const bool player = owner == pgos::BlackjackHandOwner::Player;
    const uint8_t visible = player ? visiblePlayerCards_ : visibleDealerCards_;
    const int16_t y = surfaceArea.y1 +
                      (player ? PLAYER_CARD_Y : DEALER_CARD_Y);
    const bool dealing = eventActive_ &&
                         activeEvent_.type ==
                             pgos::BlackjackEventType::CardDealt &&
                         activeEvent_.owner == owner;
    const float progress = dealing ? easeOutCubic(activeProgress(nowMs)) : 1.0F;
    const uint8_t layoutCount = dealing ? visible + 1U : visible;

    for (uint8_t index = 0; index < visible; ++index) {
        const int16_t fromX = cardX(visible, index);
        const int16_t toX = cardX(layoutCount, index);
        const int16_t x = surfaceArea.x1 +
                          interpolate(fromX, toX, progress);
        const pgos::BlackjackCard card =
            player ? engine_.playerCard(index) : engine_.dealerCard(index);
        bool faceUp = player || index != 1 || holeRevealed_;
        int16_t width = CARD_WIDTH;
        if (!player && eventActive_ &&
            activeEvent_.type == pgos::BlackjackEventType::HoleRevealed &&
            index == 1) {
            const float flip = activeProgress(nowMs);
            if (flip < 0.5F) {
                width = std::max<int16_t>(2, static_cast<int16_t>(
                    std::lround(CARD_WIDTH * (1.0F - flip * 2.0F))));
                faceUp = false;
            } else {
                width = std::max<int16_t>(2, static_cast<int16_t>(
                    std::lround(CARD_WIDTH * ((flip - 0.5F) * 2.0F))));
                faceUp = true;
            }
        }
        drawCard(layer, card, x + (CARD_WIDTH - width) / 2, y, faceUp,
                 width);
    }

    if (dealing) {
        const int16_t originX = surfaceArea.x2 - 35;
        const int16_t originY = surfaceArea.y1 + 31;
        const int16_t targetX = surfaceArea.x1 +
                                cardX(layoutCount, activeEvent_.cardIndex);
        const int16_t x = interpolate(originX, targetX, progress);
        const int16_t animatedY = interpolate(originY, y, progress);
        drawCard(layer, activeEvent_.card, x, animatedY,
                 activeEvent_.faceUp);
    }
}

void BlackjackApp::drawCard(lv_layer_t* layer,
                            const pgos::BlackjackCard& card, int16_t x,
                            int16_t y, bool faceUp, int16_t width) const {
    if (width < 2) {
        return;
    }
    lv_area_t shadow = {
        static_cast<lv_coord_t>(x + 2),
        static_cast<lv_coord_t>(y + 3),
        static_cast<lv_coord_t>(x + width + 1),
        static_cast<lv_coord_t>(y + CARD_HEIGHT + 2),
    };
    drawRect(layer, shadow, headerColor_, 4, LV_OPA_60);
    lv_area_t body = {
        static_cast<lv_coord_t>(x),
        static_cast<lv_coord_t>(y),
        static_cast<lv_coord_t>(x + width - 1),
        static_cast<lv_coord_t>(y + CARD_HEIGHT - 1),
    };
    drawRect(layer, body, cardColor_, 4);
    if (width < 12) {
        return;
    }

    if (!faceUp) {
        lv_area_t border = body;
        border.x1 += 4;
        border.x2 -= 4;
        border.y1 += 4;
        border.y2 -= 4;
        drawRect(layer, border, goldColor_, 3);
        border.x1 += 2;
        border.x2 -= 2;
        border.y1 += 2;
        border.y2 -= 2;
        drawRect(layer, border, redSuitColor_, 2);
        drawText(layer, "PG", border, cardColor_, hudFont_);
        return;
    }

    const bool red = card.suit == 1 || card.suit == 2;
    const lv_color_t ink = red ? redSuitColor_ : cardInkColor_;
    char rank[4];
    if (card.rank == 1) {
        lv_snprintf(rank, sizeof(rank), "A");
    } else if (card.rank == 11) {
        lv_snprintf(rank, sizeof(rank), "J");
    } else if (card.rank == 12) {
        lv_snprintf(rank, sizeof(rank), "Q");
    } else if (card.rank == 13) {
        lv_snprintf(rank, sizeof(rank), "K");
    } else {
        lv_snprintf(rank, sizeof(rank), "%u",
                    static_cast<unsigned>(card.rank));
    }

    lv_area_t rankArea = body;
    rankArea.x1 += 4;
    rankArea.x2 -= 2;
    rankArea.y1 += 2;
    rankArea.y2 = rankArea.y1 + 22;
    drawText(layer, rank, rankArea, ink, rankFont_, LV_TEXT_ALIGN_LEFT);
    lv_area_t suitArea = body;
    suitArea.x1 += 3;
    suitArea.x2 -= 3;
    suitArea.y1 += 31;
    suitArea.y2 -= 8;
    drawText(layer, SUIT_SYMBOLS[card.suit % 4U], suitArea, ink,
             strongBodyFont_);
}

void BlackjackApp::drawChip(lv_layer_t* layer, int16_t x, int16_t y,
                            lv_color_t color) const {
    lv_area_t outer = {
        static_cast<lv_coord_t>(x - 5), static_cast<lv_coord_t>(y - 5),
        static_cast<lv_coord_t>(x + 5), static_cast<lv_coord_t>(y + 5),
    };
    drawRect(layer, outer, cardColor_, LV_RADIUS_CIRCLE);
    outer.x1 += 2;
    outer.x2 -= 2;
    outer.y1 += 2;
    outer.y2 -= 2;
    drawRect(layer, outer, color, LV_RADIUS_CIRCLE);
}

void BlackjackApp::enterBetting(AppContext& context) {
    (void)context;
    phase_ = ViewPhase::Betting;
    visiblePlayerCards_ = 0;
    visibleDealerCards_ = 0;
    holeRevealed_ = false;
    actionSelection_ = 0;
    betSelection_ = 0;
    settlementFromBankroll_ = context.blackjackProfile.bankroll();
    settlementToBankroll_ = settlementFromBankroll_;
    invalidate();
}

void BlackjackApp::startRound(AppContext& context, uint32_t nowMs) {
    const uint32_t wager = selectedBet(context);
    if (!context.blackjackProfile.beginWager(wager)) {
        phase_ = ViewPhase::Bailout;
        invalidate();
        return;
    }
    wagerDebited_ = wager;
    wagerActive_ = true;
    settlementFromBankroll_ = context.blackjackProfile.bankroll();
    settlementToBankroll_ = settlementFromBankroll_;
    visiblePlayerCards_ = 0;
    visibleDealerCards_ = 0;
    holeRevealed_ = false;
    if (!engine_.startRound(wager)) {
        context.blackjackProfile.cancelRound(wager);
        wagerActive_ = false;
        wagerDebited_ = 0;
        return;
    }
    phase_ = ViewPhase::Animating;
    startNextEvent(context, nowMs);
    invalidate();
}

void BlackjackApp::executeAction(PlayerAction action, AppContext& context,
                                 uint32_t nowMs) {
    bool accepted = false;
    if (action == PlayerAction::Hit) {
        accepted = engine_.hit();
    } else if (action == PlayerAction::Stand) {
        accepted = engine_.stand();
    } else {
        const uint32_t additional = engine_.wager();
        if (context.blackjackProfile.addWager(additional)) {
            accepted = engine_.doubleDown();
            if (accepted) {
                wagerDebited_ += additional;
            } else {
                context.blackjackProfile.cancelRound(additional);
            }
        }
    }
    if (accepted) {
        phase_ = ViewPhase::Animating;
        startNextEvent(context, nowMs);
        invalidate();
    }
}

void BlackjackApp::startNextEvent(AppContext& context, uint32_t nowMs) {
    pgos::BlackjackEvent event;
    if (engine_.pollEvent(event)) {
        beginEvent(event, context, nowMs);
        return;
    }

    eventActive_ = false;
    if (engine_.phase() == pgos::BlackjackPhase::PlayerTurn) {
        phase_ = ViewPhase::PlayerTurn;
        actionSelection_ = 0;
        doubleAvailable_ = engine_.canDoubleDown() &&
                           context.blackjackProfile.bankroll() >=
                               engine_.wager();
    } else if (engine_.phase() == pgos::BlackjackPhase::RoundOver) {
        phase_ = ViewPhase::RoundOver;
    }
}

void BlackjackApp::finishActiveEvent(AppContext& context, uint32_t nowMs) {
    switch (activeEvent_.type) {
        case pgos::BlackjackEventType::CardDealt:
            if (activeEvent_.owner == pgos::BlackjackHandOwner::Player) {
                visiblePlayerCards_ = std::max<uint8_t>(
                    visiblePlayerCards_, activeEvent_.cardIndex + 1U);
            } else {
                visibleDealerCards_ = std::max<uint8_t>(
                    visibleDealerCards_, activeEvent_.cardIndex + 1U);
            }
            break;
        case pgos::BlackjackEventType::HoleRevealed:
            holeRevealed_ = true;
            break;
        case pgos::BlackjackEventType::RoundSettled:
            phase_ = ViewPhase::RoundOver;
            break;
        default:
            break;
    }
    eventActive_ = false;
    startNextEvent(context, nowMs);
}

void BlackjackApp::beginEvent(const pgos::BlackjackEvent& event,
                              AppContext& context, uint32_t nowMs) {
    activeEvent_ = event;
    eventActive_ = true;
    eventStartedMs_ = nowMs;
    phase_ = ViewPhase::Animating;
    switch (event.type) {
        case pgos::BlackjackEventType::ShoeShuffled:
            eventDurationMs_ = SHUFFLE_ANIMATION_MS;
            context.audio.playGameTone(55);
            break;
        case pgos::BlackjackEventType::CardDealt:
            eventDurationMs_ = DEAL_ANIMATION_MS;
            context.audio.playGameTone(18);
            break;
        case pgos::BlackjackEventType::HoleRevealed:
            eventDurationMs_ = FLIP_ANIMATION_MS;
            context.audio.playGameTone(40);
            break;
        case pgos::BlackjackEventType::RoundSettled:
            eventDurationMs_ = SETTLE_ANIMATION_MS;
            settleProfile(context);
            playOutcomeFeedback(context);
            break;
    }
}

void BlackjackApp::settleProfile(AppContext& context) {
    if (!wagerActive_) {
        return;
    }
    settlementFromBankroll_ = context.blackjackProfile.bankroll();
    context.blackjackProfile.settleRound(engine_.outcome(), engine_.payout());
    settlementToBankroll_ = context.blackjackProfile.bankroll();
    profileWins_ = context.blackjackProfile.wins();
    profileBlackjacks_ = context.blackjackProfile.blackjacks();
    profileBestStreak_ = context.blackjackProfile.bestStreak();
    wagerActive_ = false;
    wagerDebited_ = 0;
}

void BlackjackApp::playOutcomeFeedback(AppContext& context) const {
    const bool connected = context.gamepad.snapshot().connected;
    switch (engine_.outcome()) {
        case pgos::BlackjackOutcome::PlayerBlackjack:
            context.audio.playGameTone(240);
            context.rgb.flashFeedback(255, 185, 45, 650);
            if (connected) {
                context.gamepad.requestRumble(320, 140, 220);
            }
            break;
        case pgos::BlackjackOutcome::PlayerWin:
            context.audio.playGameTone(150);
            context.rgb.flashFeedback(255, 185, 45, 420);
            if (connected) {
                context.gamepad.requestRumble(180, 90, 155);
            }
            break;
        case pgos::BlackjackOutcome::DealerWin:
            context.audio.playGameTone(120);
            context.rgb.flashFeedback(210, 35, 45, 360);
            if (connected) {
                context.gamepad.requestRumble(220, 175, 230);
            }
            break;
        case pgos::BlackjackOutcome::Push:
            context.audio.playGameTone(70);
            context.rgb.flashFeedback(35, 150, 180, 240);
            break;
        default:
            break;
    }
}

void BlackjackApp::claimBailout(AppContext& context, uint32_t nowMs) {
    bailoutFromBankroll_ = context.blackjackProfile.bankroll();
    if (!context.blackjackProfile.claimBailout()) {
        enterBetting(context);
        return;
    }
    bailoutAnimating_ = true;
    bailoutStartedMs_ = nowMs;
    context.audio.playGameTone(190);
    context.rgb.flashFeedback(255, 185, 45, 560);
    if (context.gamepad.snapshot().connected) {
        context.gamepad.requestRumble(220, 80, 150);
    }
}

void BlackjackApp::sampleAnalog(const GamepadSnapshot& gamepad,
                                AppContext& context) {
    int8_t sign = 0;
    if (gamepad.connected && gamepad.axisX <= -ANALOG_THRESHOLD) {
        sign = -1;
    } else if (gamepad.connected && gamepad.axisX >= ANALOG_THRESHOLD) {
        sign = 1;
    }
    if (sign != 0 && analogXSign_ == 0 && !eventActive_ &&
        !bailoutAnimating_) {
        moveSelection(sign, context);
        context.audio.playFeedback();
    }
    analogXSign_ = sign;
}

void BlackjackApp::moveSelection(int8_t direction, AppContext& context) {
    if (phase_ == ViewPhase::Betting) {
        const uint32_t available = context.blackjackProfile.bankroll() -
                                   context.blackjackProfile.bankroll() % 10U;
        do {
            betSelection_ = static_cast<uint8_t>(
                (betSelection_ + 4 + (direction < 0 ? -1 : 1)) % 4);
        } while (betSelection_ < 3 && BET_OPTIONS[betSelection_] > available);
        invalidate();
    } else if (phase_ == ViewPhase::PlayerTurn) {
        const uint8_t count = availableActionCount(context);
        actionSelection_ = static_cast<uint8_t>(
            (actionSelection_ + count + (direction < 0 ? -1 : 1)) % count);
        invalidate();
    }
}

uint8_t BlackjackApp::availableActionCount(const AppContext& context) const {
    return engine_.canDoubleDown() &&
                   context.blackjackProfile.bankroll() >= engine_.wager()
               ? 3
               : 2;
}

BlackjackApp::PlayerAction BlackjackApp::selectedAction(
    const AppContext& context) const {
    const uint8_t count = availableActionCount(context);
    const uint8_t selected = actionSelection_ < count ? actionSelection_ : 0;
    return static_cast<PlayerAction>(selected);
}

uint32_t BlackjackApp::selectedBet(const AppContext& context) const {
    const uint32_t bankroll = context.blackjackProfile.bankroll();
    const uint32_t available = bankroll - bankroll % 10U;
    if (betSelection_ >= 3) {
        return available;
    }
    return std::min<uint32_t>(available, BET_OPTIONS[betSelection_]);
}

uint32_t BlackjackApp::displayedBankroll(uint32_t nowMs) const {
    if (bailoutAnimating_) {
        const float progress = std::min(
            1.0F, static_cast<float>(nowMs - bailoutStartedMs_) /
                      static_cast<float>(BAILOUT_ANIMATION_MS));
        return static_cast<uint32_t>(std::lround(
            bailoutFromBankroll_ +
            (BlackjackProfileService::BAILOUT_BANKROLL -
             bailoutFromBankroll_) *
                easeOutCubic(progress)));
    }
    if (eventActive_ &&
        activeEvent_.type == pgos::BlackjackEventType::RoundSettled) {
        const float progress = easeOutCubic(activeProgress(nowMs));
        return static_cast<uint32_t>(std::lround(
            settlementFromBankroll_ +
            (settlementToBankroll_ - settlementFromBankroll_) * progress));
    }
    return settlementToBankroll_;
}

float BlackjackApp::activeProgress(uint32_t nowMs) const {
    if (!eventActive_ || eventDurationMs_ == 0) {
        return 1.0F;
    }
    return std::min(
        1.0F, static_cast<float>(nowMs - eventStartedMs_) /
                  static_cast<float>(eventDurationMs_));
}

int16_t BlackjackApp::cardX(uint8_t count, uint8_t index) const {
    if (count <= 1) {
        return (SURFACE_WIDTH - CARD_WIDTH) / 2;
    }
    const int16_t available = 244;
    const int16_t spacing = std::max<int16_t>(
        18, std::min<int16_t>(47, (available - CARD_WIDTH) / (count - 1U)));
    const int16_t width = CARD_WIDTH + spacing * (count - 1U);
    return static_cast<int16_t>((SURFACE_WIDTH - width) / 2 + index * spacing);
}

void BlackjackApp::invalidate() {
    if (surface_ != nullptr) {
        lv_obj_invalidate(surface_);
    }
}
