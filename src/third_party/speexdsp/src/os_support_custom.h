#pragma once

#include <esp_heap_caps.h>

static inline void* speex_alloc(int size) {
    return heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static inline void* speex_alloc_scratch(int size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static inline void* speex_realloc(void* pointer, int size) {
    return heap_caps_realloc(pointer, size,
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static inline void speex_free(void* pointer) {
    heap_caps_free(pointer);
}

static inline void speex_free_scratch(void* pointer) {
    heap_caps_free(pointer);
}
