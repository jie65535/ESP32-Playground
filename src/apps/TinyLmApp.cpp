#include "apps/TinyLmApp.h"

#include "services/TinyLmService.h"
#include "ui/TinyLmFont.h"

namespace {

struct TinyLmPreset {
    const char* title;
    const char* prompt;
    const uint16_t* ids;
    uint8_t idCount;
};

constexpr uint16_t RABBIT_IDS[] = {407, 262, 4316};
constexpr uint16_t DOG_IDS[] = {407, 262, 3913};
constexpr uint16_t BEAR_IDS[] = {407, 262, 7010};
constexpr uint16_t GIRL_IDS[] = {407, 262, 603};

const TinyLmPreset PRESETS[] = {
    {"森林里的小兔子", "从前，有一只小兔子", RABBIT_IDS,
     static_cast<uint8_t>(sizeof(RABBIT_IDS) / sizeof(RABBIT_IDS[0]))},
    {"爱交朋友的小狗", "从前，有一只小狗", DOG_IDS,
     static_cast<uint8_t>(sizeof(DOG_IDS) / sizeof(DOG_IDS[0]))},
    {"寻找宝物的小熊", "从前，有一只小熊", BEAR_IDS,
     static_cast<uint8_t>(sizeof(BEAR_IDS) / sizeof(BEAR_IDS[0]))},
    {"海边的小女孩", "从前，有一个小女孩", GIRL_IDS,
     static_cast<uint8_t>(sizeof(GIRL_IDS) / sizeof(GIRL_IDS[0]))},
};

static_assert(sizeof(PRESETS) / sizeof(PRESETS[0]) == 4U,
              "TinyLM preset count mismatch");

}  // namespace

AppId TinyLmApp::id() const {
    return AppId::TinyLm;
}

const char* TinyLmApp::name() const {
    return "TinyLM";
}

void TinyLmApp::onEnter(AppContext& context) {
    page_ = Page::Presets;
    renderedSelection_ = -1;
    activeGeneration_ = 0;
    storyText_.reserve(4096);
    resetDecoder();
    (void)context;
}

void TinyLmApp::onExit(AppContext& context) {
    context.tinyLm.unload();
    root_ = nullptr;
    storyPanel_ = nullptr;
    storyLabel_ = nullptr;
    infoLabel_ = nullptr;
    hintLabel_ = nullptr;
    for (UiCard& card : cards_) {
        card = UiCard{};
    }
    storyText_ = String();
    resetDecoder();
}

void TinyLmApp::onCommand(const AppCommand& command, AppContext& context) {
    if (page_ == Page::Generate) {
        if (command.type == AppCommandType::Activate) {
            startSelected(context);
        }
        return;
    }

    if (command.type == AppCommandType::Previous ||
        command.type == AppCommandType::Left) {
        selected_ = selected_ == 0 ? PRESET_COUNT - 1U : selected_ - 1U;
    } else if (command.type == AppCommandType::Next ||
               command.type == AppCommandType::Right) {
        selected_ = static_cast<uint8_t>((selected_ + 1U) % PRESET_COUNT);
    } else if (command.type == AppCommandType::Activate) {
        startSelected(context);
    }
}

void TinyLmApp::onTick(uint32_t nowMs, AppContext& context) {
    if (page_ != Page::Generate) {
        return;
    }
    consumeTokens(context);
    if (storyDirty_ &&
        nowMs - lastViewUpdateMs_ >= VIEW_UPDATE_INTERVAL_MS) {
        lastViewUpdateMs_ = nowMs;
        refreshGenerationView(context);
    }
}

lv_obj_t* TinyLmApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot(
        nullptr, "TinyLM", "5.69M 参数 · INT4 2.95 MB · 本地运行");

    for (uint8_t index = 0; index < PRESET_COUNT; ++index) {
        cards_[index] = context.ui.createCard(
            root_,
            UiRuntime::CARD_START_WITH_SUBTITLE_Y +
                static_cast<int16_t>(index) * UiRuntime::CARD_STEP_Y,
            UiIcon::Sparkles, PRESETS[index].title, PRESETS[index].prompt);
    }

    storyPanel_ = lv_obj_create(root_);
    lv_obj_remove_style_all(storyPanel_);
    lv_obj_set_pos(storyPanel_, 12, 64);
    lv_obj_set_size(storyPanel_, 296, 116);
    lv_obj_set_style_radius(storyPanel_, 8, 0);
    lv_obj_set_style_bg_color(storyPanel_, context.ui.panel(), 0);
    lv_obj_set_style_bg_opa(storyPanel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(storyPanel_, 2, 0);
    lv_obj_set_style_border_color(storyPanel_, context.ui.panelRaised(), 0);
    lv_obj_set_style_pad_all(storyPanel_, 8, 0);
    lv_obj_set_scroll_dir(storyPanel_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(storyPanel_, LV_SCROLLBAR_MODE_OFF);

    storyLabel_ = lv_label_create(storyPanel_);
    lv_obj_set_width(storyLabel_, 276);
    lv_obj_set_pos(storyLabel_, 0, 0);
    lv_label_set_long_mode(storyLabel_, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(storyLabel_, TinyLmFont::font(), 0);
    lv_obj_set_style_text_color(storyLabel_, context.ui.text(), 0);
    lv_obj_set_style_text_line_space(storyLabel_, 2, 0);
    lv_label_set_text(storyLabel_, "");

    infoLabel_ = context.ui.createBodyLabel(
        root_, "准备生成", 16, 185, context.ui.muted());
    hintLabel_ = context.ui.createBodyLabel(
        root_, "A 重新生成    B 返回列表", 16, 202, context.ui.dim());

    lv_obj_add_flag(storyPanel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(infoLabel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(hintLabel_, LV_OBJ_FLAG_HIDDEN);
    return root_;
}

void TinyLmApp::onUpdateView(AppContext& context) {
    if (root_ == nullptr) {
        return;
    }
    if (page_ == Page::Generate) {
        refreshGenerationView(context);
        return;
    }
    if (renderedSelection_ == static_cast<int8_t>(selected_)) {
        return;
    }
    for (uint8_t index = 0; index < PRESET_COUNT; ++index) {
        context.ui.setCardFocused(cards_[index], index == selected_);
    }
    context.ui.centerFocused(root_, cards_[selected_].root);
    renderedSelection_ = static_cast<int8_t>(selected_);
}

bool TinyLmApp::onBack(AppContext& context) {
    if (page_ != Page::Generate) {
        return false;
    }
    context.tinyLm.stop();
    showPresets(context);
    return true;
}

void TinyLmApp::startSelected(AppContext& context) {
    const TinyLmPreset& preset = PRESETS[selected_];
    storyText_ = preset.prompt;
    resetDecoder();
    storyDirty_ = true;
    activeGeneration_ = context.tinyLm.start(
        preset.ids, preset.idCount, MAX_GENERATED_TOKENS);
    showGeneration();
    refreshGenerationView(context);
}

void TinyLmApp::showPresets(AppContext& context) {
    page_ = Page::Presets;
    activeGeneration_ = 0;
    storyDirty_ = false;
    resetDecoder();
    lv_obj_add_flag(storyPanel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(infoLabel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(hintLabel_, LV_OBJ_FLAG_HIDDEN);
    for (UiCard& card : cards_) {
        lv_obj_remove_flag(card.root, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_scroll_to_y(root_, 0, LV_ANIM_OFF);
    renderedSelection_ = -1;
    onUpdateView(context);
}

void TinyLmApp::showGeneration() {
    page_ = Page::Generate;
    for (UiCard& card : cards_) {
        lv_obj_add_flag(card.root, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(storyPanel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(infoLabel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(hintLabel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_y(root_, 0, LV_ANIM_OFF);
}

void TinyLmApp::consumeTokens(AppContext& context) {
    TinyLmTokenEvent event;
    uint8_t bytes[64];
    while (context.tinyLm.pollToken(event)) {
        if (event.generation != activeGeneration_) {
            continue;
        }
        const size_t count =
            context.tinyLm.decodeToken(event.token, bytes, sizeof(bytes));
        for (size_t index = 0; index < count; ++index) {
            appendByte(bytes[index]);
        }
        storyDirty_ = true;
    }
}

void TinyLmApp::appendByte(uint8_t value) {
    if (utf8Expected_ == 0) {
        if (value < 0x80U) {
            appendCodepoint(value);
        } else if ((value & 0xE0U) == 0xC0U) {
            utf8Codepoint_ = value & 0x1FU;
            utf8Expected_ = 1;
            utf8Minimum_ = 0x80U;
        } else if ((value & 0xF0U) == 0xE0U) {
            utf8Codepoint_ = value & 0x0FU;
            utf8Expected_ = 2;
            utf8Minimum_ = 0x800U;
        } else if ((value & 0xF8U) == 0xF0U) {
            utf8Codepoint_ = value & 0x07U;
            utf8Expected_ = 3;
            utf8Minimum_ = 0x10000U;
        } else {
            appendCodepoint(0xFFFDU);
        }
        return;
    }

    if ((value & 0xC0U) != 0x80U) {
        appendCodepoint(0xFFFDU);
        resetDecoder();
        appendByte(value);
        return;
    }

    utf8Codepoint_ = (utf8Codepoint_ << 6U) | (value & 0x3FU);
    if (--utf8Expected_ != 0) {
        return;
    }
    const uint32_t codepoint = utf8Codepoint_;
    const uint32_t minimum = utf8Minimum_;
    resetDecoder();
    if (codepoint < minimum || codepoint > 0x10FFFFU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
        appendCodepoint(0xFFFDU);
    } else {
        appendCodepoint(codepoint);
    }
}

void TinyLmApp::appendCodepoint(uint32_t codepoint) {
    if (codepoint == '\r') {
        return;
    }
    if (codepoint != '\n' && !TinyLmFont::supports(codepoint)) {
        codepoint = 0xFFFDU;
    }
    char encoded[5] = {};
    uint8_t length = 0;
    if (codepoint < 0x80U) {
        encoded[length++] = static_cast<char>(codepoint);
    } else if (codepoint < 0x800U) {
        encoded[length++] = static_cast<char>(0xC0U | (codepoint >> 6U));
        encoded[length++] = static_cast<char>(0x80U | (codepoint & 0x3FU));
    } else if (codepoint < 0x10000U) {
        encoded[length++] = static_cast<char>(0xE0U | (codepoint >> 12U));
        encoded[length++] =
            static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU));
        encoded[length++] = static_cast<char>(0x80U | (codepoint & 0x3FU));
    } else {
        encoded[length++] = static_cast<char>(0xF0U | (codepoint >> 18U));
        encoded[length++] =
            static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU));
        encoded[length++] =
            static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU));
        encoded[length++] = static_cast<char>(0x80U | (codepoint & 0x3FU));
    }
    storyText_.concat(encoded, length);
}

void TinyLmApp::resetDecoder() {
    utf8Codepoint_ = 0;
    utf8Minimum_ = 0;
    utf8Expected_ = 0;
}

void TinyLmApp::refreshGenerationView(AppContext& context) {
    if (storyLabel_ == nullptr || page_ != Page::Generate) {
        return;
    }
    const TinyLmSnapshot snapshot = context.tinyLm.snapshot();
    if (activeGeneration_ == 0 || !snapshot.available) {
        lv_label_set_text(storyLabel_,
                          "模型分区不可用，请先写入中文 model.bin。");
        lv_label_set_text(infoLabel_, snapshot.error[0] == '\0'
                                          ? "TinyLM 不可用"
                                          : snapshot.error);
    } else {
        lv_label_set_text(storyLabel_, storyText_.c_str());
        const float tokensPerSecond =
            snapshot.computeUs == 0
                ? 0.0F
                : static_cast<float>(snapshot.generatedTokens) * 1000000.0F /
                      static_cast<float>(snapshot.computeUs);
        char status[96];
        if (snapshot.state == TinyLmState::Loading) {
            snprintf(status, sizeof(status), "正在加载模型…");
        } else if (snapshot.state == TinyLmState::Complete) {
            snprintf(status, sizeof(status), "完成 · %u token · %.1f tok/s",
                     static_cast<unsigned>(snapshot.generatedTokens),
                     tokensPerSecond);
        } else if (snapshot.state == TinyLmState::Error) {
            snprintf(status, sizeof(status), "错误 · %s", snapshot.error);
        } else {
            snprintf(status, sizeof(status), "%u / %u token · %.1f tok/s",
                     static_cast<unsigned>(snapshot.generatedTokens),
                     static_cast<unsigned>(snapshot.maxTokens),
                     tokensPerSecond);
        }
        lv_label_set_text(infoLabel_, status);
    }
    lv_obj_update_layout(storyPanel_);
    lv_obj_scroll_to_y(storyPanel_, LV_COORD_MAX, LV_ANIM_OFF);
    storyDirty_ = false;
}
