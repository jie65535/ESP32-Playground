#include "apps/TimeApp.h"

#include "services/TimeService.h"
#include "ui/UiRuntime.h"

namespace {

String statusDescription(const TimeSnapshot& snapshot) {
    if (snapshot.effectiveSource == TimeSource::Ntp) {
        if (snapshot.state == TimeState::Ready) {
            return "NTP 已同步，RTC 正常";
        }
        if (snapshot.state == TimeState::NotFound) {
            return "NTP 已同步，未检测到 RTC";
        }
        return "NTP 已同步，RTC 需检查";
    }
    switch (snapshot.state) {
        case TimeState::Ready:
            return "电池时钟正常";
        case TimeState::VoltageLow:
            return "RTC 电压低，时间可能不准";
        case TimeState::Stopped:
            return "RTC 已停止，请重新设置";
        case TimeState::InvalidData:
            return "日历数据无效，请重新设置";
        case TimeState::ReadError:
            return "I2C 读取失败，正在重试";
        case TimeState::NotFound:
            return "未检测到模块，正在重试";
        case TimeState::BusUnavailable:
        default:
            return "I2C 总线不可用";
    }
}

const char* weekdayText(uint8_t weekday) {
    constexpr const char* NAMES[] = {
        "星期日", "星期一", "星期二", "星期三",
        "星期四", "星期五", "星期六",
    };
    return NAMES[weekday % 7U];
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
    root_ = context.ui.createPageRoot(nullptr, "Clock");

    timeLabel_ = context.ui.createLabel(root_, "--:--:--", 16, 68, 28,
                                        context.ui.accent());
    lv_obj_set_width(timeLabel_, 288);
    lv_obj_set_style_text_align(timeLabel_, LV_TEXT_ALIGN_CENTER, 0);

    dateLabel_ = context.ui.createLabel(root_, "---- -- --", 16, 104, 20,
                                        context.ui.text());
    lv_obj_set_width(dateLabel_, 288);
    lv_obj_set_style_text_align(dateLabel_, LV_TEXT_ALIGN_CENTER, 0);

    weekdayLabel_ = context.ui.createLabel(root_, "等待有效时间", 16, 130,
                                           14, context.ui.muted());
    context.ui.applyBodyFont(weekdayLabel_, true);
    lv_obj_set_width(weekdayLabel_, 288);
    lv_obj_set_style_text_align(weekdayLabel_, LV_TEXT_ALIGN_CENTER, 0);

    statusCard_ = context.ui.createCard(root_, 160, UiIcon::Clock,
                                        "PCF8563 / 0x51", "正在检测模块");
    setCard_ = context.ui.createCard(
        root_, 212, UiIcon::Clock, "设置本地时间",
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
                          ? weekdayText(snapshot.effectiveDateTime.weekday)
                          : "等待有效日历数据");

    const bool timeReady = snapshot.effectiveTimeValid;
    const bool rtcReady = snapshot.state == TimeState::Ready;
    lv_obj_set_style_text_color(timeLabel_,
                                timeReady ? context.ui.accent()
                                          : context.ui.muted(),
                                0);
    lv_obj_set_style_image_recolor(
        statusCard_.iconGraphic,
        selected_ == 0
            ? context.ui.background()
            : rtcReady ? context.ui.accent() : context.ui.text(),
        0);
    const String status = statusDescription(snapshot);
    lv_label_set_text(statusCard_.subtitle, status.c_str());
    const String syncStatus = snapshot.ntpSynced
                                  ? "NTP 已同步，可用 USB 覆盖"
                                  : snapshot.ntpConfigured
                                        ? "等待 NTP，可用 USB 设置"
                                        : "联网后自动 NTP 校时";
    lv_label_set_text(setCard_.subtitle, syncStatus.c_str());
}
