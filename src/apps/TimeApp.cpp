#include "apps/TimeApp.h"

#include "services/TimeService.h"
#include "ui/UiRuntime.h"

namespace {

String statusDescription(const TimeSnapshot& snapshot) {
    if (snapshot.effectiveSource == TimeSource::Ntp) {
        if (snapshot.state == TimeState::Ready) {
            return "NTP active / PCF8563 is ready";
        }
        if (snapshot.state == TimeState::NotFound) {
            return "NTP active / PCF8563 not found";
        }
        return "NTP active / RTC needs attention";
    }
    switch (snapshot.state) {
        case TimeState::Ready:
            return "Battery-backed local clock is valid";
        case TimeState::VoltageLow:
            return "VL set; clock integrity is not guaranteed";
        case TimeState::Stopped:
            return "RTC clock stopped; set local time";
        case TimeState::InvalidData:
            return "Calendar fields invalid; set local time";
        case TimeState::ReadError:
            return "I2C read failed; retrying automatically";
        case TimeState::NotFound:
            return "Module not found; retrying every 5 seconds";
        case TimeState::BusUnavailable:
        default:
            return "Shared I2C bus is unavailable";
    }
}

}  // namespace

AppId TimeApp::id() const {
    return AppId::Time;
}

const char* TimeApp::name() const {
    return "Time";
}

void TimeApp::onEnter(AppContext&) {
    selected_ = 0;
    renderedSelected_ = -1;
}

void TimeApp::onExit(AppContext&) {
    root_ = nullptr;
    timeLabel_ = nullptr;
    dateLabel_ = nullptr;
    weekdayLabel_ = nullptr;
    statusCard_ = UiCard{};
    setCard_ = UiCard{};
    renderedSelected_ = -1;
}

void TimeApp::onCommand(const AppCommand& command, AppContext&) {
    if (command.type == AppCommandType::Previous) {
        selected_ = selected_ == 0 ? 1 : 0;
    } else if (command.type == AppCommandType::Next) {
        selected_ = static_cast<uint8_t>((selected_ + 1U) % 2U);
    }
}

void TimeApp::onTick(uint32_t, AppContext&) {}

lv_obj_t* TimeApp::onCreateView(AppContext& context) {
    root_ = context.ui.createPageRoot("TIME / PCF8563", "Clock",
                                      "Local wall time / no timezone stored");

    timeLabel_ = context.ui.createLabel(root_, "--:--:--", 16, 68, 28,
                                        context.ui.accent());
    lv_obj_set_width(timeLabel_, 288);
    lv_obj_set_style_text_align(timeLabel_, LV_TEXT_ALIGN_CENTER, 0);

    dateLabel_ = context.ui.createLabel(root_, "---- -- --", 16, 104, 20,
                                        context.ui.text());
    lv_obj_set_width(dateLabel_, 288);
    lv_obj_set_style_text_align(dateLabel_, LV_TEXT_ALIGN_CENTER, 0);

    weekdayLabel_ = context.ui.createLabel(root_, "Waiting for RTC", 16, 130,
                                           14, context.ui.muted());
    lv_obj_set_width(weekdayLabel_, 288);
    lv_obj_set_style_text_align(weekdayLabel_, LV_TEXT_ALIGN_CENTER, 0);

    statusCard_ = context.ui.createCard(root_, 160, LV_SYMBOL_LOOP,
                                        "PCF8563 / 0x51", "Detecting module");
    setCard_ = context.ui.createCard(
        root_, 210, LV_SYMBOL_EDIT, "Set local time",
        "USB: time set YYYY-MM-DD HH:MM:SS");
    return root_;
}

void TimeApp::onUpdateView(AppContext& context) {
    if (root_ == nullptr) {
        return;
    }

    if (renderedSelected_ != static_cast<int8_t>(selected_)) {
        context.ui.setCardFocused(statusCard_, selected_ == 0);
        context.ui.setCardFocused(setCard_, selected_ == 1);
        if (renderedSelected_ >= 0) {
            context.ui.centerFocused(
                root_, selected_ == 0 ? statusCard_.root : setCard_.root);
        }
        renderedSelected_ = static_cast<int8_t>(selected_);
    }

    const TimeSnapshot snapshot = context.time.snapshot();
    char timeText[9] = "--:--:--";
    char dateText[11] = "---- -- --";
    if (snapshot.effectiveTimeValid) {
        snprintf(timeText, sizeof(timeText), "%02u:%02u:%02u",
                 snapshot.effectiveDateTime.hour,
                 snapshot.effectiveDateTime.minute,
                 snapshot.effectiveDateTime.second);
        snprintf(dateText, sizeof(dateText), "%04u-%02u-%02u",
                 snapshot.effectiveDateTime.year,
                 snapshot.effectiveDateTime.month,
                 snapshot.effectiveDateTime.day);
    }
    lv_label_set_text(timeLabel_, timeText);
    lv_label_set_text(dateLabel_, dateText);
    lv_label_set_text(weekdayLabel_,
                      snapshot.effectiveTimeValid
                          ? TimeService::weekdayName(
                                snapshot.effectiveDateTime.weekday)
                          : "Waiting for valid calendar data");

    const bool timeReady = snapshot.effectiveTimeValid;
    const bool rtcReady = snapshot.state == TimeState::Ready;
    lv_obj_set_style_text_color(timeLabel_,
                                timeReady ? context.ui.accent()
                                          : context.ui.muted(),
                                0);
    lv_label_set_text(statusCard_.iconLabel,
                      rtcReady ? LV_SYMBOL_OK
                            : snapshot.state == TimeState::NotFound
                                  ? LV_SYMBOL_CLOSE
                                  : LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(
        statusCard_.iconLabel,
        selected_ == 0
            ? context.ui.background()
            : rtcReady ? context.ui.accent() : context.ui.text(),
                                0);
    const String status = statusDescription(snapshot);
    lv_label_set_text(statusCard_.subtitle, status.c_str());
    const String syncStatus = snapshot.ntpSynced
                                  ? "NTP synced / USB override available"
                                  : snapshot.ntpConfigured
                                        ? "NTP pending / USB time set available"
                                        : "Wi-Fi NTP auto / USB time set available";
    lv_label_set_text(setCard_.subtitle, syncStatus.c_str());
}
