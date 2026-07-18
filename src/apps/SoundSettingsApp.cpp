#include "apps/SoundSettingsApp.h"

#include "services/AudioService.h"
#include "ui/UiRuntime.h"

namespace {

String choiceText(const char* value) {
    return String("< ") + value + " >";
}

}  // namespace

AppId SoundSettingsApp::id() const {
    return AppId::SoundSettings;
}

const char* SoundSettingsApp::name() const {
    return "Sound";
}

void SoundSettingsApp::onEnter(AppContext& context) {
    selected_ = 0;
    renderedSelected_ = -1;
    renderedVolume_ = 0;
    renderedFeedback_ = false;
    renderedReady_ = !context.audio.ready();
}

void SoundSettingsApp::onExit(AppContext&) {
    root_ = nullptr;
    memset(rows_, 0, sizeof(rows_));
    memset(valueLabels_, 0, sizeof(valueLabels_));
}

void SoundSettingsApp::onCommand(const AppCommand& command,
                                 AppContext& context) {
    switch (command.type) {
        case AppCommandType::Previous:
            selected_ = selected_ == 0 ? SETTING_COUNT - 1U : selected_ - 1U;
            break;
        case AppCommandType::Next:
            selected_ = static_cast<uint8_t>((selected_ + 1U) % SETTING_COUNT);
            break;
        case AppCommandType::Left:
            adjustSelected(-1, context);
            break;
        case AppCommandType::Right:
            adjustSelected(1, context);
            break;
        case AppCommandType::Activate:
            if (selected_ == 2) {
                context.audio.playTestTone();
            } else {
                adjustSelected(1, context);
            }
            break;
        default:
            break;
    }
}

void SoundSettingsApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* SoundSettingsApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot("SOUND / AUDIO", "Sound");

    rows_[0] = context.ui.createCard(root_, 58, LV_SYMBOL_VOLUME_MAX,
                                     "Volume", "small-speaker loudness curve");
    rows_[1] = context.ui.createCard(root_, 108, LV_SYMBOL_BELL,
                                     "Feedback sound", "Short navigation tone");
    rows_[2] = context.ui.createCard(root_, 158, LV_SYMBOL_PLAY,
                                     "Test tone", "Play a short speaker check");

    for (uint8_t index = 0; index < SETTING_COUNT; ++index) {
        valueLabels_[index] = context.ui.createLabel(
            rows_[index].root, index == 0 ? "< 60% >" : index == 1 ? "< Off >"
                                                                     : "Play",
            194, 12, 12, context.ui.text());
        lv_obj_set_width(valueLabels_[index], 78);
        lv_label_set_long_mode(valueLabels_[index], LV_LABEL_LONG_CLIP);
        lv_obj_align(valueLabels_[index], LV_ALIGN_RIGHT_MID, -9, 0);
    }
    return root_;
}

void SoundSettingsApp::onUpdateView(AppContext& context) {
    if (root_ == nullptr) {
        return;
    }

    AudioService& audio = context.audio;
    if (renderedSelected_ != static_cast<int8_t>(selected_)) {
        for (uint8_t index = 0; index < SETTING_COUNT; ++index) {
            context.ui.setCardFocused(rows_[index], index == selected_);
        }
        context.ui.centerFocused(root_, rows_[selected_].root);
        renderedSelected_ = static_cast<int8_t>(selected_);
    }

    const uint8_t volume = audio.volumePercent();
    const bool feedback = audio.feedbackEnabled();
    const bool ready = audio.ready();
    if (renderedVolume_ != volume || renderedReady_ != ready) {
        const String value = ready ? volumeText(audio) : "Unavailable";
        lv_label_set_text(valueLabels_[0], value.c_str());
        renderedVolume_ = volume;
    }
    if (renderedFeedback_ != feedback || renderedReady_ != ready) {
        const String value = ready ? choiceText(feedback ? "On" : "Off")
                                   : "Unavailable";
        lv_label_set_text(valueLabels_[1], value.c_str());
        renderedFeedback_ = feedback;
    }
    if (renderedReady_ != ready) {
        const String value = ready ? "Play" : "Unavailable";
        lv_label_set_text(valueLabels_[2], value.c_str());
        renderedReady_ = ready;
    }
}

void SoundSettingsApp::adjustSelected(int8_t delta, AppContext& context) {
    if (selected_ == 0) {
        cycleVolume(delta, context);
    } else if (selected_ == 1) {
        context.audio.setFeedbackEnabled(!context.audio.feedbackEnabled());
    } else if (delta > 0) {
        context.audio.playTestTone();
    }
}

void SoundSettingsApp::cycleVolume(int8_t delta, AppContext& context) {
    int16_t volume = context.audio.volumePercent();
    volume += delta * 10;
    if (volume < 0) {
        volume = 100;
    } else if (volume > 100) {
        volume = 0;
    }
    context.audio.setVolumePercent(static_cast<uint8_t>(volume));
}

String SoundSettingsApp::volumeText(const AudioService& audio) const {
    return String("< ") + audio.volumePercent() + "% >";
}
