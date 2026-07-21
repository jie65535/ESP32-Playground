#include "ui/RenderSurface.h"

bool RenderSurface::attach(lv_obj_t* parent, int16_t x, int16_t y,
                            int16_t width, int16_t height,
                            lv_color_t background, DrawCallback callback,
                            void* context) {
    reset();
    if (parent == nullptr || callback == nullptr) {
        return false;
    }

    object_ = lv_obj_create(parent);
    if (object_ == nullptr) {
        return false;
    }
    lv_obj_remove_style_all(object_);
    lv_obj_set_size(object_, width, height);
    lv_obj_set_pos(object_, x, y);
    lv_obj_set_style_bg_color(object_, background, 0);
    lv_obj_set_style_bg_opa(object_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(object_, 0, 0);
    lv_obj_clear_flag(object_, LV_OBJ_FLAG_SCROLLABLE);

    callback_ = callback;
    context_ = context;
    lv_obj_add_event_cb(object_, drawEvent, LV_EVENT_DRAW_MAIN, this);
    return true;
}

void RenderSurface::reset() {
    object_ = nullptr;
    callback_ = nullptr;
    context_ = nullptr;
}

void RenderSurface::invalidate() {
    if (object_ != nullptr) {
        lv_obj_invalidate(object_);
    }
}

lv_obj_t* RenderSurface::object() const {
    return object_;
}

void RenderSurface::drawEvent(lv_event_t* event) {
    auto* surface = static_cast<RenderSurface*>(lv_event_get_user_data(event));
    if (surface != nullptr && surface->callback_ != nullptr) {
        surface->callback_(event, surface->context_);
    }
}
