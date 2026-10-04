#pragma once

#include <stddef.h>
#include "esp_heap_caps.h"

static inline void *lvgl_psram_alloc(size_t bytes) {
  return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
