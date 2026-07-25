#include "apps/MicrophoneSettingsApp.h"

#include "services/AudioService.h"
#include "ui/UiRuntime.h"

#include <cstring>

namespace {

String choiceText(const char* value) {
    return String("< ") + value + " >";
}

String msText(uint32_t value) {
    return String(value) + " ms";
}

}  // namespace

AppId MicrophoneSettingsApp::id() const {
    return AppId::MicrophoneSettings;
}

const char* MicrophoneSettingsApp::name() const {
    return "Microphone";
}

void MicrophoneSettingsApp::onEnter(AppContext&) {
    selected_ = 0;
    renderedSelected_ = -1;
    renderedSignature_ = "";
    lastRefreshMs_ = 0;
}

void MicrophoneSettingsApp::onExit(AppContext&) {
    root_ = nullptr;
    memset(rows_, 0, sizeof(rows_));
    memset(valueLabels_, 0, sizeof(valueLabels_));
    levelBar_ = nullptr;
    renderedSignature_ = "";
}

void MicrophoneSettingsApp::onCommand(const AppCommand& command,
                                      AppContext& context) {
    switch (command.type) {
        case AppCommandType::Previous:
            selected_ = selected_ == 0 ? ITEM_COUNT - 1U : selected_ - 1U;
            break;
        case AppCommandType::Next:
            selected_ = static_cast<uint8_t>((selected_ + 1U) % ITEM_COUNT);
            break;
        case AppCommandType::Left:
        case AppCommandType::Right:
        case AppCommandType::Activate:
            activateSelected(context);
            break;
        case AppCommandType::MicStatus:
            context.audio.printStatus(context.console);
            break;
        case AppCommandType::MicGain: {
            MicrophoneGain gain = MicrophoneGain::Normal;
            if (!AudioService::parseMicrophoneGain(command.value, gain) ||
                !context.audio.setMicrophoneGain(gain)) {
                context.console.println(
                    F("[mic] usage: mic gain low|normal|high"));
            }
            break;
        }
        case AppCommandType::MicDenoiseOn:
            context.audio.setMicrophoneDenoiseEnabled(true);
            break;
        case AppCommandType::MicDenoiseOff:
            context.audio.setMicrophoneDenoiseEnabled(false);
            break;
        case AppCommandType::MicDenoiseToggle:
            context.audio.toggleMicrophoneDenoise();
            break;
        case AppCommandType::MicVoiceOn:
            context.audio.setMicrophoneVoiceEnhanceEnabled(true);
            break;
        case AppCommandType::MicVoiceOff:
            context.audio.setMicrophoneVoiceEnhanceEnabled(false);
            break;
        case AppCommandType::MicVoiceToggle:
            context.audio.toggleMicrophoneVoiceEnhance();
            break;
        case AppCommandType::MicMonitorOn:
            context.audio.setMicrophoneMonitorEnabled(true);
            break;
        case AppCommandType::MicMonitorOff:
            context.audio.setMicrophoneMonitorEnabled(false);
            break;
        case AppCommandType::MicMonitorToggle:
            context.audio.toggleMicrophoneMonitor();
            break;
        case AppCommandType::MicPlayback:
            if (!context.audio.playMicrophoneBuffer(
                    command.number > 0 ? static_cast<uint32_t>(command.number)
                                       : PREVIEW_PLAYBACK_MS)) {
                context.console.println(F("[mic] playback unavailable"));
            }
            break;
        default:
            break;
    }
}

void MicrophoneSettingsApp::onTick(uint32_t nowMs, AppContext& context) {
    if (root_ == nullptr || nowMs - lastRefreshMs_ < 120U) {
        return;
    }
    lastRefreshMs_ = nowMs;
    onUpdateView(context);
}

lv_obj_t* MicrophoneSettingsApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot(nullptr, "Microphone");

    rows_[0] = context.ui.createCard(root_, UiRuntime::CARD_START_Y,
                                     UiIcon::Mic, "采集状态",
                                     "ES8311 ADC / GPIO6");
    rows_[1] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + UiRuntime::CARD_STEP_Y,
        UiIcon::Activity, "实时电平", "RMS / Peak");
    rows_[2] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + 2 * UiRuntime::CARD_STEP_Y,
        UiIcon::Sliders, "输入增益", "low / normal / high");
    rows_[3] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + 3 * UiRuntime::CARD_STEP_Y,
        UiIcon::AudioWaveform, "轻量降噪", "高通 / 自适应扩展器");
    rows_[4] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + 4 * UiRuntime::CARD_STEP_Y,
        UiIcon::Sparkles, "人声增强", "语音 AGC / 峰值限幅");
    rows_[5] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + 5 * UiRuntime::CARD_STEP_Y,
        UiIcon::Headphones, "衰减耳返", "低增益监听");
    rows_[6] = context.ui.createCard(
        root_, UiRuntime::CARD_START_Y + 6 * UiRuntime::CARD_STEP_Y,
        UiIcon::Play, "回放缓存", "播放最近 3 秒");

    for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
        valueLabels_[index] = context.ui.createLabel(
            rows_[index].root, "--", 194, 12, 12, context.ui.text());
        context.ui.applyBodyFont(valueLabels_[index]);
        lv_obj_set_width(valueLabels_[index], 78);
        lv_label_set_long_mode(valueLabels_[index], LV_LABEL_LONG_CLIP);
        lv_obj_align(valueLabels_[index], LV_ALIGN_RIGHT_MID, -9, 0);
    }

    levelBar_ = lv_bar_create(rows_[1].root);
    lv_obj_remove_style_all(levelBar_);
    lv_obj_set_size(levelBar_, 78, 5);
    lv_obj_align(levelBar_, LV_ALIGN_BOTTOM_RIGHT, -9, -6);
    lv_bar_set_range(levelBar_, 0, 100);
    lv_bar_set_value(levelBar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(levelBar_, 3, 0);
    lv_obj_set_style_bg_color(levelBar_, context.ui.panelRaised(), 0);
    lv_obj_set_style_bg_opa(levelBar_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(levelBar_, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(levelBar_, context.ui.accent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(levelBar_, LV_OPA_COVER, LV_PART_INDICATOR);

    return root_;
}

void MicrophoneSettingsApp::onUpdateView(AppContext& context) {
    if (root_ == nullptr) {
        return;
    }

    if (renderedSelected_ != static_cast<int8_t>(selected_)) {
        for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
            context.ui.setCardFocused(rows_[index], index == selected_);
        }
        context.ui.centerFocused(root_, rows_[selected_].root);
        renderedSelected_ = static_cast<int8_t>(selected_);
    }

    const MicrophoneSnapshot mic = context.audio.microphoneSnapshot();
    const String signature = statusSignature(mic);
    if (signature == renderedSignature_) {
        return;
    }
    renderedSignature_ = signature;

    lv_label_set_text(valueLabels_[0], mic.available ? "Ready" : "不可用");
    const String level = String(mic.levelPercent) + "%";
    lv_label_set_text(valueLabels_[1], level.c_str());
    lv_label_set_text(valueLabels_[2],
                      mic.available
                          ? choiceText(AudioService::microphoneGainName(
                                           mic.gain))
                                .c_str()
                          : "不可用");
    lv_label_set_text(valueLabels_[3],
                      mic.available
                          ? choiceText(mic.denoiseEnabled ? "开" : "关").c_str()
                          : "不可用");
    lv_label_set_text(valueLabels_[4],
                      mic.available
                          ? choiceText(mic.voiceEnhanceEnabled ? "开" : "关")
                                .c_str()
                          : "不可用");
    lv_label_set_text(valueLabels_[5],
                      mic.available
                          ? choiceText(mic.monitorEnabled ? "开" : "关").c_str()
                          : "不可用");
    lv_label_set_text(
        valueLabels_[6],
        mic.playbackActive ? msText(mic.playbackRemainingMs).c_str()
                           : (mic.ringMs > 0 ? "播放" : "等待"));
    if (levelBar_ != nullptr) {
        lv_bar_set_value(levelBar_, mic.levelPercent, LV_ANIM_OFF);
    }

    const String levelSubtitle =
        String("rms ") + mic.rms + " / peak " + mic.peak;
    lv_label_set_text(rows_[1].subtitle, levelSubtitle.c_str());

    const String denoiseSubtitle =
        mic.denoiseEnabled
            ? String("noise ") + mic.noiseRms + " / gain " +
                  mic.denoiseGainPercent + "%"
            : String("高通 / 自适应扩展器");
    lv_label_set_text(rows_[3].subtitle, denoiseSubtitle.c_str());

    const String voiceSubtitle =
        mic.voiceEnhanceEnabled
            ? String(mic.voiceDetectorReady
                         ? (mic.voiceActive ? "vad voice" : "vad wait")
                         : "vad error") +
                  " / gain " + mic.voiceGainPercent + "%" +
                  (mic.voiceLimiting ? " / limit" : "")
            : String("语音 AGC / 峰值限幅");
    lv_label_set_text(rows_[4].subtitle, voiceSubtitle.c_str());

    const String statusSubtitle =
        mic.available
            ? String(mic.sampleRate) + " Hz, errors " + mic.readErrors
            : String("初始化失败");
    lv_label_set_text(rows_[0].subtitle, statusSubtitle.c_str());
}

void MicrophoneSettingsApp::activateSelected(AppContext& context) {
    switch (selected_) {
        case 0:
        case 1:
            context.audio.printStatus(context.console);
            break;
        case 2:
            context.audio.cycleMicrophoneGain();
            break;
        case 3:
            context.audio.toggleMicrophoneDenoise();
            break;
        case 4:
            context.audio.toggleMicrophoneVoiceEnhance();
            break;
        case 5:
            context.audio.toggleMicrophoneMonitor();
            break;
        case 6:
            if (!context.audio.playMicrophoneBuffer(PREVIEW_PLAYBACK_MS)) {
                context.console.println(F("[mic] playback unavailable"));
            }
            break;
        default:
            break;
    }
}

String MicrophoneSettingsApp::statusSignature(
    const MicrophoneSnapshot& mic) const {
    String value;
    value.reserve(96);
    value += mic.available ? '1' : '0';
    value += '|';
    value += mic.monitorEnabled ? '1' : '0';
    value += '|';
    value += mic.denoiseEnabled ? '1' : '0';
    value += '|';
    value += mic.voiceDetectorReady ? '1' : '0';
    value += '|';
    value += mic.voiceActive ? '1' : '0';
    value += '|';
    value += mic.voiceEnhanceEnabled ? '1' : '0';
    value += '|';
    value += mic.voiceLimiting ? '1' : '0';
    value += '|';
    value += mic.playbackActive ? '1' : '0';
    value += '|';
    value += static_cast<uint8_t>(mic.gain);
    value += '|';
    value += mic.sampleRate;
    value += '|';
    value += mic.rms;
    value += '|';
    value += mic.peak;
    value += '|';
    value += mic.noiseRms;
    value += '|';
    value += mic.denoiseGainPercent;
    value += '|';
    value += mic.voiceGainPercent;
    value += '|';
    value += mic.levelPercent;
    value += '|';
    value += mic.ringMs;
    value += '|';
    value += mic.playbackRemainingMs;
    value += '|';
    value += mic.readErrors;
    return value;
}
