#pragma once

#include <lvgl.h>

class RenderSurface final {
public:
    using DrawCallback = void (*)(lv_event_t* event, void* context);

    bool attach(lv_obj_t* parent, int16_t x, int16_t y, int16_t width,
                int16_t height, lv_color_t background, DrawCallback callback,
                void* context);
    void reset();
    void invalidate();

    lv_obj_t* object() const;

private:
    lv_obj_t* object_ = nullptr;
    DrawCallback callback_ = nullptr;
    void* context_ = nullptr;

    static void drawEvent(lv_event_t* event);
};
