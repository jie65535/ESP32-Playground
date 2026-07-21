#include "ui/PixelSpriteRenderer.h"

#include <algorithm>

namespace pgos {

namespace {

// A descriptor referenced by an LVGL draw task must not be recycled before
// that frame has been submitted. Platformer draws fewer than 24 unique images
// per frame, so 32 entries leave headroom without reserving one descriptor for
// every asset that might be visited over the lifetime of the app.
constexpr uint8_t IMAGE_CACHE_SIZE = 32;

struct ImageCacheEntry {
    const PixelSprite* sprite = nullptr;
    lv_image_dsc_t image{};
};

ImageCacheEntry imageCache[IMAGE_CACHE_SIZE] = {};
uint8_t imageCacheCount = 0;
uint8_t nextImageCacheSlot = 0;

}  // namespace

lv_color_t PixelSpriteRenderer::colorFromRgb565(uint16_t value) {
    lv_color16_t packed;
    packed.red = static_cast<uint16_t>((value >> 11) & 0x1FU);
    packed.green = static_cast<uint16_t>((value >> 5) & 0x3FU);
    packed.blue = static_cast<uint16_t>(value & 0x1FU);
    return lv_color16_to_color(packed);
}

const lv_image_dsc_t* PixelSpriteRenderer::imageDescriptor(
    const PixelSprite& sprite) {
    for (uint8_t index = 0; index < imageCacheCount; ++index) {
        if (imageCache[index].sprite == &sprite) {
            return &imageCache[index].image;
        }
    }

    uint8_t slot = nextImageCacheSlot;
    if (imageCacheCount < IMAGE_CACHE_SIZE) {
        slot = imageCacheCount++;
    } else {
        nextImageCacheSlot = static_cast<uint8_t>(
            (nextImageCacheSlot + 1U) % IMAGE_CACHE_SIZE);
    }
    ImageCacheEntry& entry = imageCache[slot];
    entry.sprite = &sprite;
    entry.image = lv_image_dsc_t{};
    entry.image.header.magic = LV_IMAGE_HEADER_MAGIC;
    entry.image.header.cf = sprite.opaque ? LV_COLOR_FORMAT_RGB565
                                          : LV_COLOR_FORMAT_RGB565A8;
    entry.image.header.flags = 0;
    entry.image.header.w = sprite.width;
    entry.image.header.h = sprite.height;
    entry.image.header.stride = static_cast<uint16_t>(sprite.width * 2U);
    entry.image.data_size = sprite.imageDataSize;
    entry.image.data = sprite.imageData;
    return &entry.image;
}

void PixelSpriteRenderer::draw(lv_layer_t* layer, const PixelSprite& sprite,
                               int32_t x, int32_t y, uint8_t scale,
                               bool flipX) {
    if (layer == nullptr || scale == 0 || !sprite.valid()) {
        return;
    }

    if (scale == 1 && !flipX) {
        lv_draw_image_dsc_t descriptor;
        lv_draw_image_dsc_init(&descriptor);
        descriptor.src = imageDescriptor(sprite);
        const lv_area_t area = {
            static_cast<lv_coord_t>(x),
            static_cast<lv_coord_t>(y),
            static_cast<lv_coord_t>(x + sprite.width - 1),
            static_cast<lv_coord_t>(y + sprite.height - 1),
        };
        lv_draw_image(layer, &descriptor, &area);
        return;
    }

    for (uint16_t row = 0; row < sprite.height; ++row) {
        uint16_t column = 0;
        while (column < sprite.width) {
            const uint16_t sourceColumn = flipX
                                              ? static_cast<uint16_t>(
                                                    sprite.width - 1U - column)
                                              : column;
            const uint32_t sourceIndex =
                static_cast<uint32_t>(row) * sprite.width + sourceColumn;
            const uint8_t opacity = sprite.opacityAt(sourceIndex);
            if (opacity <= LV_OPA_MIN) {
                ++column;
                continue;
            }

            const uint16_t pixel = sprite.pixelAt(sourceIndex);
            uint16_t run = 1;
            while (column + run < sprite.width) {
                const uint16_t nextColumn = flipX
                                                ? static_cast<uint16_t>(
                                                      sprite.width - 1U -
                                                      (column + run))
                                                : static_cast<uint16_t>(column + run);
                const uint32_t nextIndex =
                    static_cast<uint32_t>(row) * sprite.width + nextColumn;
                if (sprite.opacityAt(nextIndex) != opacity ||
                    sprite.pixelAt(nextIndex) != pixel) {
                    break;
                }
                ++run;
            }

            lv_draw_rect_dsc_t descriptor;
            lv_draw_rect_dsc_init(&descriptor);
            descriptor.bg_color = colorFromRgb565(pixel);
            descriptor.bg_opa = opacity;
            descriptor.radius = 0;
            lv_area_t area = {
                static_cast<lv_coord_t>(x + column * scale),
                static_cast<lv_coord_t>(y + row * scale),
                static_cast<lv_coord_t>(x + (column + run) * scale - 1),
                static_cast<lv_coord_t>(y + (row + 1U) * scale - 1),
            };
            lv_draw_rect(layer, &descriptor, &area);
            column = static_cast<uint16_t>(column + run);
        }
    }
}

void PixelSpriteRenderer::drawRegion(lv_layer_t* layer,
                                     const PixelSprite& sprite, int32_t x,
                                     int32_t y, uint16_t sourceY,
                                     uint16_t sourceHeight, uint8_t scale,
                                     bool flipX) {
    if (layer == nullptr || scale == 0 || !sprite.valid() ||
        sourceY >= sprite.height || sourceHeight == 0) {
        return;
    }

    const uint16_t endRow = static_cast<uint16_t>(
        std::min<uint32_t>(sprite.height,
                           static_cast<uint32_t>(sourceY) + sourceHeight));
    for (uint16_t row = sourceY; row < endRow; ++row) {
        uint16_t column = 0;
        while (column < sprite.width) {
            const uint16_t sourceColumn = flipX
                                              ? static_cast<uint16_t>(
                                                    sprite.width - 1U - column)
                                              : column;
            const uint32_t sourceIndex =
                static_cast<uint32_t>(row) * sprite.width + sourceColumn;
            const uint8_t opacity = sprite.opacityAt(sourceIndex);
            if (opacity <= LV_OPA_MIN) {
                ++column;
                continue;
            }

            const uint16_t pixel = sprite.pixelAt(sourceIndex);
            uint16_t run = 1;
            while (column + run < sprite.width) {
                const uint16_t nextColumn = flipX
                                                ? static_cast<uint16_t>(
                                                      sprite.width - 1U -
                                                      (column + run))
                                                : static_cast<uint16_t>(column + run);
                const uint32_t nextIndex =
                    static_cast<uint32_t>(row) * sprite.width + nextColumn;
                if (sprite.opacityAt(nextIndex) != opacity ||
                    sprite.pixelAt(nextIndex) != pixel) {
                    break;
                }
                ++run;
            }

            lv_draw_rect_dsc_t descriptor;
            lv_draw_rect_dsc_init(&descriptor);
            descriptor.bg_color = colorFromRgb565(pixel);
            descriptor.bg_opa = opacity;
            descriptor.radius = 0;
            const uint16_t targetRow = static_cast<uint16_t>(row - sourceY);
            lv_area_t area = {
                static_cast<lv_coord_t>(x + column * scale),
                static_cast<lv_coord_t>(y + targetRow * scale),
                static_cast<lv_coord_t>(x + (column + run) * scale - 1),
                static_cast<lv_coord_t>(y + (targetRow + 1U) * scale - 1),
            };
            lv_draw_rect(layer, &descriptor, &area);
            column = static_cast<uint16_t>(column + run);
        }
    }
}

}  // namespace pgos
