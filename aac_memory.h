#pragma once
#include <stdlib.h>
#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

// Keep DMA/TLS/internal RAM available. On boards without PSRAM, retain the
// library's original allocation policy. Do not spill a failed PSRAM allocation
// back into scarce internal RAM on a PSRAM-equipped board.
static inline void *deskAacAllocate(size_t bytes) {
#if defined(ESP32)
  if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM)>0)
    return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#endif
  return malloc(bytes);
}
