#include "apps/PlatformerApp.h"

#include "games/PlatformerExtraAssets.generated.h"
#include "games/PlatformerOriginalBackground.generated.h"
#include "games/PlatformerSpriteAssets.generated.h"
#include "services/AudioService.h"
#include "services/BleGamepadService.h"
#include "ui/CanvasDraw.h"
#include "ui/PixelSpriteRenderer.h"
#include "ui/UiRuntime.h"

#include <algorithm>
#include <cmath>

using pgos::drawRect;
using pgos::drawText;

namespace {

// Mario-Level-1's source strip is 224px high with the ground at y=201. The
// shell root starts below the 22px status bar; drawing the strip eight pixels
// above that root puts its original ground line at the same device coordinate
// as Mario's collision geometry (global y≈215).
constexpr int16_t WORLD_Y_OFFSET = -8;
constexpr int16_t TILE_SIZE = pgos::PlatformerEngine::TILE_SIZE;
constexpr int16_t BACKGROUND_CHUNK_WIDTH = 320;

using Sprite = pgos::PixelSprite;

constexpr const Sprite* BACKGROUND_FRAMES[] = {
    &pgos::platformer_original_assets::level_0,
    &pgos::platformer_original_assets::level_1,
    &pgos::platformer_original_assets::level_2,
    &pgos::platformer_original_assets::level_3,
    &pgos::platformer_original_assets::level_4,
    &pgos::platformer_original_assets::level_5,
    &pgos::platformer_original_assets::level_6,
    &pgos::platformer_original_assets::level_7,
    &pgos::platformer_original_assets::level_8,
    &pgos::platformer_original_assets::level_9,
    &pgos::platformer_original_assets::level_10,
};

constexpr const Sprite* BOX_FRAMES[] = {
    &pgos::platformer_assets::CoinBox_1,
    &pgos::platformer_assets::CoinBox_2,
    &pgos::platformer_assets::CoinBox_3,
};

constexpr const Sprite* FIRE_FLOWER_FRAMES[] = {
    &pgos::platformer_extra_assets::fire_flower_1,
    &pgos::platformer_extra_assets::fire_flower_2,
    &pgos::platformer_extra_assets::fire_flower_3,
    &pgos::platformer_extra_assets::fire_flower_4,
};

constexpr const Sprite* STAR_FRAMES[] = {
    &pgos::platformer_extra_assets::star_1,
    &pgos::platformer_extra_assets::star_2,
    &pgos::platformer_extra_assets::star_3,
    &pgos::platformer_extra_assets::star_4,
};

constexpr const Sprite* COIN_FRAMES[] = {
    &pgos::platformer_assets::coin_item_1,
    &pgos::platformer_assets::coin_item_2,
    &pgos::platformer_assets::coin_item_3,
    &pgos::platformer_assets::coin_item_4,
};

constexpr const Sprite* FIREBALL_FRAMES[] = {
    &pgos::platformer_extra_assets::fireball_1,
    &pgos::platformer_extra_assets::fireball_2,
    &pgos::platformer_extra_assets::fireball_3,
    &pgos::platformer_extra_assets::fireball_4,
};

constexpr const Sprite* FIREBALL_EXPLOSION_FRAMES[] = {
    &pgos::platformer_extra_assets::fireball_explode1,
    &pgos::platformer_extra_assets::fireball_explode2,
    &pgos::platformer_extra_assets::fireball_explode3,
};

}  // namespace

AppId PlatformerApp::id() const {
    return AppId::Platformer;
}

const char* PlatformerApp::name() const {
    return "Super Mario";
}

void PlatformerApp::onEnter(AppContext&) {
    root_ = nullptr;
    surface_.reset();
    engine_.reset();
    lastTickMs_ = millis();
    lastRenderMs_ = 0;
    physicsAccumulatorMs_ = 0;
    commandMoveUntilMs_ = 0;
    commandJumpHoldUntilMs_ = 0;
    commandCrouchUntilMs_ = 0;
    commandMoveDirection_ = 0;
    jumpPending_ = false;
    actionHeldLast_ = false;
    gamepadConnectedLast_ = false;
    moveAxisFilter_.reset();
    crouchAxisFilter_.reset();
}

void PlatformerApp::onExit(AppContext&) {
    surface_.reset();
    root_ = nullptr;
    hudFont_ = nullptr;
    overlayFont_ = nullptr;
}

void PlatformerApp::onCommand(const AppCommand& command, AppContext&) {
    const uint32_t nowMs = millis();
    switch (command.type) {
        case AppCommandType::Previous:
            if (engine_.phase() == pgos::PlatformerPhase::Running) {
                jumpPending_ = true;
                commandJumpHoldUntilMs_ = nowMs + 160U;
                invalidate();
            }
            break;
        case AppCommandType::Next:
            if (engine_.phase() == pgos::PlatformerPhase::Running) {
                commandCrouchUntilMs_ = nowMs + 180U;
                invalidate();
            }
            break;
        case AppCommandType::Left:
            commandMoveDirection_ = -1;
            commandMoveUntilMs_ = nowMs + COMMAND_MOVE_MS;
            invalidate();
            break;
        case AppCommandType::Right:
            commandMoveDirection_ = 1;
            commandMoveUntilMs_ = nowMs + COMMAND_MOVE_MS;
            invalidate();
            break;
        case AppCommandType::Activate:
            if (engine_.phase() == pgos::PlatformerPhase::Title ||
                engine_.phase() == pgos::PlatformerPhase::GameOver ||
                engine_.phase() == pgos::PlatformerPhase::Won) {
                engine_.start();
                physicsAccumulatorMs_ = 0;
                lastTickMs_ = nowMs;
                commandJumpHoldUntilMs_ = 0;
                commandCrouchUntilMs_ = 0;
            } else if (engine_.phase() == pgos::PlatformerPhase::Running) {
                jumpPending_ = true;
                commandJumpHoldUntilMs_ = nowMs + 160U;
            } else if (engine_.phase() == pgos::PlatformerPhase::Paused) {
                engine_.togglePause();
                lastTickMs_ = nowMs;
                physicsAccumulatorMs_ = 0;
                commandJumpHoldUntilMs_ = 0;
                commandCrouchUntilMs_ = 0;
            }
            invalidate();
            break;
        case AppCommandType::QuickDrop:
            if (engine_.phase() == pgos::PlatformerPhase::Running ||
                engine_.phase() == pgos::PlatformerPhase::Paused) {
                engine_.togglePause();
                lastTickMs_ = nowMs;
                physicsAccumulatorMs_ = 0;
                jumpPending_ = false;
                commandJumpHoldUntilMs_ = 0;
                commandCrouchUntilMs_ = 0;
                invalidate();
            }
            break;
        case AppCommandType::PlatformerMapTest:
            engine_.startMapTest();
            physicsAccumulatorMs_ = 0;
            lastTickMs_ = nowMs;
            jumpPending_ = false;
            actionHeldLast_ = false;
            invalidate();
            break;
        case AppCommandType::PlatformerMapTestNext:
            engine_.advanceMapTest();
            invalidate();
            break;
        default:
            break;
    }
}

void PlatformerApp::onTick(uint32_t nowMs, AppContext& context) {
    if (surface_.object() == nullptr) {
        return;
    }

    const uint32_t elapsedMs =
        lastTickMs_ == 0 ? 0 : std::min<uint32_t>(nowMs - lastTickMs_,
                                                   MAX_FRAME_MS);
    lastTickMs_ = nowMs;

    const pgos::PlatformerPhase phase = engine_.phase();
    const bool simulationActive =
        phase == pgos::PlatformerPhase::Running ||
        phase == pgos::PlatformerPhase::Dying ||
        phase == pgos::PlatformerPhase::Flagpole ||
        phase == pgos::PlatformerPhase::CastleWalk ||
        phase == pgos::PlatformerPhase::TimeBonus;
    bool simulationChanged = false;
    if (simulationActive && elapsedMs != 0) {
        physicsAccumulatorMs_ = std::min<uint32_t>(
            physicsAccumulatorMs_ + elapsedMs, MAX_FRAME_MS);
        const GamepadSnapshot gamepad = context.gamepad.snapshot();
        if (gamepad.connected != gamepadConnectedLast_) {
            moveAxisFilter_.reset();
            crouchAxisFilter_.reset();
            gamepadConnectedLast_ = gamepad.connected;
        }
        while (physicsAccumulatorMs_ >= PHYSICS_STEP_MS &&
               engine_.phase() != pgos::PlatformerPhase::Paused &&
               engine_.phase() != pgos::PlatformerPhase::Title &&
               engine_.phase() != pgos::PlatformerPhase::GameOver &&
               engine_.phase() != pgos::PlatformerPhase::Won) {
            pgos::PlatformerInput input;
            input.moveAxis = sampleMove(gamepad, nowMs);
            input.jumpPressed = jumpPending_;
            input.jumpHeld = sampleJumpHeld(gamepad, nowMs);
            input.crouchHeld = sampleCrouchHeld(gamepad, nowMs);
            const bool actionHeld = sampleActionHeld(gamepad);
            input.actionPressed = actionHeld && !actionHeldLast_;
            input.actionHeld = actionHeld;
            jumpPending_ = false;
            actionHeldLast_ = actionHeld;
            engine_.step(static_cast<float>(PHYSICS_STEP_MS) / 1000.0F,
                         input);
            physicsAccumulatorMs_ -= PHYSICS_STEP_MS;
            simulationChanged = true;
        }
    } else if (!simulationActive) {
        physicsAccumulatorMs_ = 0;
        actionHeldLast_ = false;
    }

    if (simulationChanged &&
        (lastRenderMs_ == 0 || nowMs - lastRenderMs_ >= FRAME_RENDER_INTERVAL_MS)) {
        lastRenderMs_ = nowMs;
        invalidate();
    }

    consumeEvents(context);
}

lv_obj_t* PlatformerApp::onCreateView(AppContext& context) {
    root_ = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT);
    lv_obj_set_pos(root_, 0, 22);
    lv_obj_set_style_bg_color(root_, context.ui.background(), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    skyColor_ = lv_color_hex(0x72C8F4);
    skyDarkColor_ = lv_color_hex(0x4B92C5);
    textColor_ = context.ui.text();
    mutedColor_ = context.ui.muted();
    accentColor_ = context.ui.accent();
    hudFont_ = context.ui.font(12);
    overlayFont_ = context.ui.font(20);

    surface_.attach(root_, 0, 0, GAME_SURFACE_WIDTH, GAME_SURFACE_HEIGHT,
                     context.ui.background(), drawEvent, this);
    invalidate();
    return root_;
}

void PlatformerApp::onUpdateView(AppContext&) {
    invalidate();
}

void PlatformerApp::drawEvent(lv_event_t* event, void* context) {
    auto* app = static_cast<PlatformerApp*>(context);
    if (app != nullptr) {
        app->draw(event);
    }
}

void PlatformerApp::draw(lv_event_t* event) {
    lv_obj_t* object = lv_event_get_target_obj(event);
    lv_layer_t* layer = lv_event_get_layer(event);
    if (object == nullptr || layer == nullptr) {
        return;
    }

    lv_area_t surfaceArea;
    lv_obj_get_coords(object, &surfaceArea);
    drawRect(layer, surfaceArea, skyColor_);
    const pgos::PlatformerSnapshot state = engine_.snapshot();
    const uint32_t animationMs = millis();
    const auto isVisibleHorizontally = [&surfaceArea](int32_t x,
                                                      int32_t width) {
        return x + width > surfaceArea.x1 && x <= surfaceArea.x2;
    };

    lv_area_t world = surfaceArea;
    world.y1 += WORLD_Y_OFFSET;
    drawRect(layer, world, skyColor_);

    const int16_t camera = static_cast<int16_t>(std::lround(state.cameraX));

    constexpr uint8_t BACKGROUND_FRAME_COUNT =
        sizeof(BACKGROUND_FRAMES) / sizeof(BACKGROUND_FRAMES[0]);
    const int16_t firstBackground = std::max<int16_t>(
        0, static_cast<int16_t>(camera / BACKGROUND_CHUNK_WIDTH));
    const int16_t lastBackground = std::min<int16_t>(
        BACKGROUND_FRAME_COUNT - 1,
        static_cast<int16_t>((camera + GAME_SURFACE_WIDTH - 1) /
                             BACKGROUND_CHUNK_WIDTH));
    for (int16_t index = firstBackground; index <= lastBackground; ++index) {
        pgos::PixelSpriteRenderer::draw(
            layer, *BACKGROUND_FRAMES[index],
            world.x1 + index * BACKGROUND_CHUNK_WIDTH - camera, world.y1);
    }

    for (uint8_t index = 0; index < state.totalBoxes; ++index) {
        const pgos::PlatformerBox box = engine_.box(index);
        const int16_t boxX = world.x1 + box.x - camera;
        if (boxX + TILE_SIZE < surfaceArea.x1 || boxX > surfaceArea.x2) {
            continue;
        }
        if (!box.visible) {
            if (box.opened &&
                box.type == pgos::PlatformerObjectType::CoinBrick) {
                const int16_t boxY = world.y1 + box.y;
                lv_area_t clearedBrick = {
                    static_cast<lv_coord_t>(boxX),
                    static_cast<lv_coord_t>(boxY),
                    static_cast<lv_coord_t>(boxX + TILE_SIZE - 1),
                    static_cast<lv_coord_t>(boxY + TILE_SIZE - 1),
                };
                drawRect(layer, clearedBrick, lv_color_hex(0x6380E8));
            }
            continue;
        }
        const pgos::PixelSprite* boxSprite = nullptr;
        if (box.opened) {
            boxSprite = &pgos::platformer_assets::empty;
        } else if (box.type == pgos::PlatformerObjectType::CoinBrick) {
            boxSprite = &pgos::platformer_assets::bricks;
        } else {
            boxSprite = BOX_FRAMES[(animationMs / 160U) % 3U];
        }
        pgos::PixelSpriteRenderer::draw(
            layer, *boxSprite, boxX, world.y1 + box.y + box.bumpOffset);
    }

    for (uint8_t index = 0; index < state.powerupCount; ++index) {
        const pgos::PlatformerPowerup powerup = engine_.powerup(index);
        if (!powerup.active) {
            continue;
        }
        const pgos::PixelSprite* sprite = &pgos::platformer_assets::mushroom;
        switch (powerup.kind) {
            case pgos::PlatformerPowerupKind::Mushroom:
                break;
            case pgos::PlatformerPowerupKind::OneUp:
                sprite = &pgos::platformer_extra_assets::one_up;
                break;
            case pgos::PlatformerPowerupKind::FireFlower: {
                sprite = FIRE_FLOWER_FRAMES[(animationMs / 70U) % 4U];
                break;
            }
            case pgos::PlatformerPowerupKind::Star: {
                sprite = STAR_FRAMES[(animationMs / 70U) % 4U];
                break;
            }
        }
        const int16_t x = world.x1 +
                          static_cast<int16_t>(std::lround(powerup.x)) -
                          camera;
        if (!isVisibleHorizontally(x, sprite->width)) {
            continue;
        }
        pgos::PixelSpriteRenderer::draw(
            layer, *sprite, x,
            world.y1 + static_cast<int16_t>(std::lround(powerup.y)));
    }

    const pgos::PixelSprite& coinSprite =
        *COIN_FRAMES[(animationMs / 120U) % 4U];
    const int16_t goalX = world.x1 +
                          static_cast<int16_t>(std::lround(engine_.goalX())) -
                          camera;
    lv_area_t pole = {
        static_cast<lv_coord_t>(goalX),
        static_cast<lv_coord_t>(world.y1 +
                                pgos::PLATFORMER_LEVEL_1_1.flagTopY),
        static_cast<lv_coord_t>(goalX + 2),
        static_cast<lv_coord_t>(
            world.y1 + pgos::PLATFORMER_LEVEL_1_1.flagPoleBottomY),
    };
    drawRect(layer, pole, textColor_);
    pgos::PixelSpriteRenderer::draw(
        layer, pgos::platformer_extra_assets::pole_flag, goalX - 16,
        world.y1 + static_cast<int16_t>(std::lround(state.flagY)));

    if (state.phase == pgos::PlatformerPhase::TimeBonus ||
        state.phase == pgos::PlatformerPhase::Won) {
        const int16_t castleFlagX = world.x1 +
                                    static_cast<int16_t>(std::lround(engine_.castleX())) -
                                    camera - 11;
        const int16_t castleFlagRise =
            state.phase == pgos::PlatformerPhase::Won
                ? 14
                : std::min<int16_t>(14, state.phaseElapsedMs / 35U);
        const int16_t castleFlagY = world.y1 + 120 - castleFlagRise;
        pgos::PixelSpriteRenderer::draw(
            layer, pgos::platformer_extra_assets::castle_flag, castleFlagX,
            castleFlagY);
    }

    for (uint8_t index = 0; index < state.enemyCount; ++index) {
        const pgos::PlatformerEnemyState enemy = engine_.enemy(index);
        if (!enemy.active) {
            continue;
        }
        const int16_t x = world.x1 +
                          static_cast<int16_t>(std::lround(enemy.x)) - camera;
        const int16_t y = world.y1 +
                          static_cast<int16_t>(std::lround(enemy.y));
        if (x + static_cast<int16_t>(std::ceil(enemy.width)) < surfaceArea.x1 ||
            x > surfaceArea.x2) {
            continue;
        }
        if (enemy.motion == pgos::PlatformerEnemyMotion::ShellIdle ||
            enemy.motion == pgos::PlatformerEnemyMotion::ShellSliding) {
            const pgos::PixelSprite& shell =
                pgos::platformer_extra_assets::koopa_shell;
            pgos::PixelSpriteRenderer::draw(
                layer, shell, x,
                y + static_cast<int16_t>(std::ceil(enemy.height)) -
                    static_cast<int16_t>(shell.height),
                1, enemy.facingLeft);
        } else if (enemy.type == pgos::PlatformerEnemyType::Koopa) {
            const pgos::PixelSprite& koopaSprite =
                ((animationMs / 180U) & 1U) == 0U
                    ? pgos::platformer_assets::koopa_1
                    : pgos::platformer_assets::koopa_2;
            // The converted Koopa frames face left by default, just like the
            // reference game's sprite sheet.  Mirror only while travelling
            // right; passing facingLeft here reverses the visual direction.
            // Enemy coordinates are collision-box tops.  Keep the legacy
            // 17x32 converted Koopa art aligned by its feet; the reference
            // sprite itself is 16x24, so this also remains correct if the
            // asset is regenerated at its native size.
            const int16_t koopaYOffset = static_cast<int16_t>(
                koopaSprite.height > static_cast<uint16_t>(std::ceil(enemy.height))
                    ? koopaSprite.height -
                          static_cast<uint16_t>(std::ceil(enemy.height))
                    : 0U);
            pgos::PixelSpriteRenderer::draw(layer, koopaSprite, x,
                                            y - koopaYOffset, 1,
                                            !enemy.facingLeft);
        } else {
            const pgos::PixelSprite& goombaSprite =
                enemy.motion == pgos::PlatformerEnemyMotion::Squashed
                    ? pgos::platformer_assets::goomba_flat
                    : ((animationMs / 180U) & 1U) == 0U
                          ? pgos::platformer_assets::goomba_1
                          : pgos::platformer_assets::goomba_2;
            pgos::PixelSpriteRenderer::draw(layer, goombaSprite, x, y,
                                            1, enemy.facingLeft);
        }
    }

    for (uint8_t index = 0; index < state.projectileCount; ++index) {
        const pgos::PlatformerProjectile projectile = engine_.projectile(index);
        if (!projectile.active) {
            continue;
        }
        const pgos::PixelSprite* projectileSprite = nullptr;
        if (projectile.exploding) {
            projectileSprite = FIREBALL_EXPLOSION_FRAMES[std::min<uint8_t>(
                2, static_cast<uint8_t>(projectile.ageMs / 40U))];
        } else {
            projectileSprite = FIREBALL_FRAMES[(animationMs / 80U) % 4U];
        }
        const int16_t x = world.x1 +
                          static_cast<int16_t>(std::lround(projectile.x)) -
                          camera;
        if (!isVisibleHorizontally(x, projectileSprite->width)) {
            continue;
        }
        pgos::PixelSpriteRenderer::draw(
            layer, *projectileSprite, x,
            world.y1 + static_cast<int16_t>(std::lround(projectile.y)));
    }

    for (uint8_t index = 0; index < state.effectCount; ++index) {
        const pgos::PlatformerEffect effect = engine_.effect(index);
        if (!effect.active) {
            continue;
        }
        const int16_t x = world.x1 +
                          static_cast<int16_t>(std::lround(effect.x)) - camera;
        const int16_t y = world.y1 +
                          static_cast<int16_t>(std::lround(effect.y));
        if (!isVisibleHorizontally(x, 32)) {
            continue;
        }
        if (effect.kind == pgos::PlatformerEffectKind::RisingCoin) {
            pgos::PixelSpriteRenderer::draw(layer, coinSprite, x, y);
        } else if (effect.kind == pgos::PlatformerEffectKind::BrickPiece) {
            pgos::PixelSpriteRenderer::draw(
                layer, pgos::platformer_extra_assets::brick_piece, x, y, 1,
                effect.vx < 0.0F);
        } else if (effect.kind == pgos::PlatformerEffectKind::Score) {
            char points[8];
            lv_snprintf(points, sizeof(points), "%u",
                        static_cast<unsigned>(effect.value));
            lv_area_t scoreArea = {
                static_cast<lv_coord_t>(x - 12),
                static_cast<lv_coord_t>(y - 2),
                static_cast<lv_coord_t>(x + 28),
                static_cast<lv_coord_t>(y + 12),
            };
            drawText(layer, points, scoreArea, textColor_, hudFont_);
        }
    }

    const int16_t playerX = world.x1 +
                            static_cast<int16_t>(std::lround(state.playerX)) -
                            camera;
    const int16_t playerY = world.y1 +
                            static_cast<int16_t>(std::lround(state.playerY));
    const pgos::PixelSprite* playerSprite = nullptr;
    if (state.phase == pgos::PlatformerPhase::Dying) {
        playerSprite = &pgos::platformer_extra_assets::mario_death;
    } else if (state.phase == pgos::PlatformerPhase::Flagpole) {
        const bool secondFrame = ((animationMs / 70U) & 1U) != 0U;
        if (state.playerBig) {
            playerSprite = secondFrame
                               ? &pgos::platformer_extra_assets::mario_big_flag2
                               : &pgos::platformer_extra_assets::mario_big_flag1;
        } else {
            playerSprite = secondFrame
                               ? &pgos::platformer_extra_assets::mario_flag2
                               : &pgos::platformer_extra_assets::mario_flag1;
        }
    } else if (state.playerFire) {
        if (state.playerCrouching) {
            playerSprite = &pgos::platformer_extra_assets::mario_fire_crouch;
        } else if (!state.grounded) {
            playerSprite = &pgos::platformer_extra_assets::mario_fire_jump;
        } else if (state.playerSkidding) {
            playerSprite = &pgos::platformer_extra_assets::mario_fire_skid;
        } else if (std::abs(state.playerVx) > 8.0F) {
            const uint8_t runFrame = static_cast<uint8_t>((animationMs / 90U) % 3U);
            playerSprite = runFrame == 0U
                               ? &pgos::platformer_extra_assets::mario_fire_run1
                               : runFrame == 1U
                                     ? &pgos::platformer_extra_assets::mario_fire_run2
                                     : &pgos::platformer_extra_assets::mario_fire_run3;
        } else {
            playerSprite = &pgos::platformer_extra_assets::mario_fire_idle;
        }
    } else if (state.playerBig) {
        if (state.playerCrouching) {
            playerSprite = &pgos::platformer_extra_assets::mario_big_crouch;
        } else if (state.playerSkidding) {
            playerSprite = &pgos::platformer_extra_assets::mario_big_skid;
        } else if (!state.grounded) {
            playerSprite = &pgos::platformer_assets::mario_big_jump;
        } else if (std::abs(state.playerVx) > 8.0F) {
            const uint8_t runFrame = static_cast<uint8_t>((animationMs / 90U) % 3U);
            playerSprite = runFrame == 0U
                               ? &pgos::platformer_assets::mario_big_run1
                               : runFrame == 1U
                                     ? &pgos::platformer_assets::mario_big_run2
                                     : &pgos::platformer_assets::mario_big_run3;
        } else {
            playerSprite = &pgos::platformer_assets::mario_big_idle;
        }
    } else if (state.playerSkidding) {
        playerSprite = &pgos::platformer_extra_assets::mario_skid;
    } else if (!state.grounded) {
        playerSprite = &pgos::platformer_assets::mario_jump;
    } else if (std::abs(state.playerVx) > 8.0F) {
        const uint8_t runFrame = static_cast<uint8_t>((animationMs / 90U) % 3U);
        playerSprite = runFrame == 0U
                           ? &pgos::platformer_assets::mario_run1
                           : runFrame == 1U
                                 ? &pgos::platformer_assets::mario_run2
                                 : &pgos::platformer_assets::mario_run3;
    } else {
        playerSprite = &pgos::platformer_assets::mario_idle;
    }
    const bool flickerHidden = state.playerInvincible &&
                               ((animationMs / 65U) % 4U) == 0U;
    if (state.playerVisible && !flickerHidden && playerSprite != nullptr) {
        pgos::PixelSpriteRenderer::draw(
            layer, *playerSprite, playerX, playerY, 1,
            state.playerFacingLeft);
    }

    lv_area_t hud = surfaceArea;
    hud.y2 = hud.y1 + HUD_HEIGHT - 1;
    drawRect(layer, hud, skyDarkColor_);
    char hudText[64];
    lv_snprintf(hudText, sizeof(hudText),
                "M%u %06lu  x%02u  1-1  T%03u",
                static_cast<unsigned>(state.lives),
                static_cast<unsigned long>(state.score % 1000000UL),
                static_cast<unsigned>(state.coinsCollected % 100U),
                static_cast<unsigned>(state.timeRemaining));
    lv_area_t hudTextArea = hud;
    hudTextArea.x1 += 7;
    hudTextArea.x2 -= 7;
    drawText(layer, hudText, hudTextArea, textColor_, hudFont_,
             LV_TEXT_ALIGN_LEFT);

    if (state.phase == pgos::PlatformerPhase::Dying) {
        const char* reason = "MISS";
        switch (state.deathReason) {
            case pgos::PlatformerDeathReason::Time:
                reason = "TIME UP";
                break;
            case pgos::PlatformerDeathReason::Enemy:
                reason = "ENEMY HIT";
                break;
            case pgos::PlatformerDeathReason::Fall:
                reason = "FELL";
                break;
            case pgos::PlatformerDeathReason::None:
                break;
        }
        lv_area_t reasonArea = surfaceArea;
        reasonArea.y1 += HUD_HEIGHT + 5;
        reasonArea.y2 = reasonArea.y1 + 22;
        drawText(layer, reason, reasonArea, textColor_, overlayFont_);
    }

    const bool showOverlay = state.phase == pgos::PlatformerPhase::Title ||
                             state.phase == pgos::PlatformerPhase::Paused ||
                             state.phase == pgos::PlatformerPhase::GameOver ||
                             state.phase == pgos::PlatformerPhase::Won;
    if (showOverlay) {
        lv_area_t overlay = world;
        overlay.x1 += 28;
        overlay.x2 -= 28;
        overlay.y1 += 46;
        overlay.y2 = overlay.y1 + 86;
        drawRect(layer, overlay, lv_color_hex(0x101722), 10, LV_OPA_90);

        const char* title = "PRESS A TO START";
        const char* subtitle = "A JUMP   Y / RT RUN";
        if (state.phase == pgos::PlatformerPhase::Paused) {
            title = "PAUSED";
            subtitle = "X RESUME   B EXIT";
        } else if (state.phase == pgos::PlatformerPhase::GameOver) {
            title = "GAME OVER";
            subtitle = "A RESTART   B EXIT";
        } else if (state.phase == pgos::PlatformerPhase::Won) {
            title = "STAGE CLEAR";
            subtitle = "A REPLAY   B EXIT";
        }

        lv_area_t titleArea = overlay;
        titleArea.y1 += 11;
        titleArea.y2 = titleArea.y1 + 26;
        drawText(layer, title, titleArea,
                 state.phase == pgos::PlatformerPhase::Won ? accentColor_
                                                           : textColor_,
                 overlayFont_);
        lv_area_t subtitleArea = overlay;
        subtitleArea.y1 = overlay.y2 - 25;
        subtitleArea.y2 = overlay.y2 - 7;
        drawText(layer, subtitle, subtitleArea, mutedColor_, hudFont_);
    }
}

float PlatformerApp::sampleMove(const GamepadSnapshot& gamepad,
                                uint32_t nowMs) {
    if (gamepad.connected) {
        if ((gamepad.dpad & GamepadDpadLeft) != 0) {
            return -1.0F;
        }
        if ((gamepad.dpad & GamepadDpadRight) != 0) {
            return 1.0F;
        }
        const int16_t axis = moveAxisFilter_.update(gamepad.axisX);
        if (axis != 0) {
            return std::clamp(static_cast<float>(axis) / 512.0F, -1.0F,
                              1.0F);
        }
    }
    if (static_cast<int32_t>(nowMs - commandMoveUntilMs_) < 0) {
        return static_cast<float>(commandMoveDirection_);
    }
    return 0.0F;
}

bool PlatformerApp::sampleJumpHeld(const GamepadSnapshot& gamepad,
                                    uint32_t nowMs) const {
    if (gamepad.connected &&
        ((gamepad.buttons & GamepadButtonA) != 0 ||
         (gamepad.dpad & GamepadDpadUp) != 0)) {
        return true;
    }
    return static_cast<int32_t>(nowMs - commandJumpHoldUntilMs_) < 0;
}

bool PlatformerApp::sampleCrouchHeld(const GamepadSnapshot& gamepad,
                                     uint32_t nowMs) {
    if (gamepad.connected) {
        if ((gamepad.dpad & GamepadDpadDown) != 0) {
            return true;
        }
        if (crouchAxisFilter_.update(gamepad.axisY) > 0) {
            return true;
        }
    }
    return static_cast<int32_t>(nowMs - commandCrouchUntilMs_) < 0;
}

bool PlatformerApp::sampleActionHeld(const GamepadSnapshot& gamepad) const {
    if (!gamepad.connected) {
        return false;
    }
    constexpr uint16_t ACTION_BUTTONS =
        GamepadButtonY | GamepadButtonShoulderR | GamepadButtonTriggerR;
    return (gamepad.buttons & ACTION_BUTTONS) != 0 ||
           gamepad.throttle >= 96U;
}

void PlatformerApp::consumeEvents(AppContext& context) {
    pgos::PlatformerEvent event;
    const bool gamepadConnected = context.gamepad.snapshot().connected;
    while (engine_.pollEvent(event)) {
        switch (event.type) {
            case pgos::PlatformerEventType::CoinBoxHit:
                context.audio.playGameTone(520);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(55, 40, 60);
                }
                break;
            case pgos::PlatformerEventType::BrickBroken:
                context.audio.playGameTone(110);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(95, 115, 85);
                }
                break;
            case pgos::PlatformerEventType::PowerupAppeared:
                context.audio.playGameTone(120);
                break;
            case pgos::PlatformerEventType::PowerupCollected:
                context.audio.playGameTone(680);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(90, 70, 100);
                }
                break;
            case pgos::PlatformerEventType::PlayerHurt:
                context.audio.playGameTone(180);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(170, 135, 180);
                }
                break;
            case pgos::PlatformerEventType::EnemyStomped:
                context.audio.playGameTone(70);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(70, 95, 90);
                }
                break;
            case pgos::PlatformerEventType::ShellKicked:
            case pgos::PlatformerEventType::EnemyDefeated:
                context.audio.playGameTone(80);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(90, 110, 75);
                }
                break;
            case pgos::PlatformerEventType::FireballShot:
                context.audio.playGameTone(55);
                break;
            case pgos::PlatformerEventType::OneUp:
                context.audio.playGameTone(420);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(120, 80, 150);
                }
                break;
            case pgos::PlatformerEventType::TimeWarning:
                context.audio.playGameTone(650);
                break;
            case pgos::PlatformerEventType::PlayerDied:
                context.audio.playGameTone(240);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(220, 160, 240);
                }
                break;
            case pgos::PlatformerEventType::LifeRestarted:
                break;
            case pgos::PlatformerEventType::ReachedGoal:
                context.audio.playGameTone(360);
                if (gamepadConnected) {
                    context.gamepad.requestRumble(260, 220, 255);
                }
                break;
            case pgos::PlatformerEventType::CourseClear:
                context.audio.playGameTone(700);
                break;
            case pgos::PlatformerEventType::Jumped:
                break;
        }
    }
}

void PlatformerApp::invalidate() {
    surface_.invalidate();
}
