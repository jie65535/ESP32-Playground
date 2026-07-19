#pragma once

#include "core/AppTypes.h"
#include "ui/BitmapFont.h"

#include <lvgl.h>

class DisplayService;
struct TimeSnapshot;

struct UiCard {
    lv_obj_t* root = nullptr;
    lv_obj_t* icon = nullptr;
    lv_obj_t* iconLabel = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* subtitle = nullptr;
    lv_coord_t normalX = 20;
    lv_coord_t focusedX = 12;
    lv_coord_t normalWidth = 280;
    lv_coord_t focusedWidth = 296;
};

enum class UiPageTransition : uint8_t {
    Forward,
    Backward,
    None,
};

class UiRuntime {
public:
    explicit UiRuntime(DisplayService& display);

    bool begin();
    void tick();
    void refreshNow();
    void pollDisplayFlush();
    bool ready() const;

    lv_obj_t* createPageRoot(const char* eyebrow, const char* title,
                             const char* subtitle = nullptr);
    UiCard createCard(lv_obj_t* parent, int16_t y, const char* symbol,
                      const char* title, const char* subtitle);
    void setCardFocused(UiCard& card, bool focused, bool animated = true);
    void centerFocused(lv_obj_t* scrollable, lv_obj_t* focused,
                       bool animated = true);

    lv_obj_t* createLabel(lv_obj_t* parent, const char* text, int16_t x,
                          int16_t y, uint16_t fontSize, lv_color_t color);
    lv_obj_t* createValueRow(lv_obj_t* parent, const char* label,
                             const char* value, int16_t y,
                             lv_obj_t** valueLabel = nullptr);

    void replacePage(lv_obj_t* page,
                     UiPageTransition transition = UiPageTransition::Forward);
    lv_obj_t* currentPage() const;

    void updateStatus(const WifiSnapshot& wifi, const ServerSnapshot& server,
                      const GamepadSnapshot& gamepad,
                      const TimeSnapshot& time, uint32_t nowMs);

    lv_color_t background() const;
    lv_color_t panel() const;
    lv_color_t panelRaised() const;
    lv_color_t accent() const;
    lv_color_t accentSoft() const;
    lv_color_t text() const;
    lv_color_t muted() const;
    lv_color_t dim() const;
    const lv_font_t* font(uint16_t size) const;
    const lv_font_t* bitmapFont(BitmapFontSize size) const;

private:
    static constexpr uint16_t DRAW_BUFFER_LINES = 40;

    DisplayService& display_;
    uint8_t* drawBuffer_ = nullptr;
    uint8_t* drawBuffer2_ = nullptr;
    lv_display_t* lvDisplay_ = nullptr;
    lv_obj_t* statusBar_ = nullptr;
    lv_obj_t* statusWifiIcon_ = nullptr;
    lv_obj_t* statusServerIcon_ = nullptr;
    lv_obj_t* statusBluetoothIcon_ = nullptr;
    lv_obj_t* statusTimeLabel_ = nullptr;
    lv_obj_t* activePage_ = nullptr;
    lv_obj_t* previousPage_ = nullptr;
    uint32_t lastTickMs_ = 0;
    uint32_t lastStatusMs_ = 0;
    bool ready_ = false;
    bool asyncFlush_ = false;

    static void displayFlush(lv_display_t* display, const lv_area_t* area,
                             uint8_t* pixels);
    static void displayFlushWait(lv_display_t* display);
    static void setObjX(void* object, int32_t value);
    static void setObjWidth(void* object, int32_t value);
    static void setObjScrollY(void* object, int32_t value);
    static void onPageTransitionFinished(lv_anim_t* animation);

    void createStatusBar();
    void finishPageTransition();
    void animateCard(UiCard& card, bool focused);
};
