#pragma once

#include <atomic>
#include <cstdint>
#include <esp_heap_caps.h>
extern "C" {
#include <esp_mem.h>
}
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/platform.h>

static std::atomic<TaskHandle_t> otaTlsOwner{nullptr};
static std::atomic<size_t> otaTlsPsramAllocations{0};

static void *otaTlsCalloc(size_t count, size_t size) {
  if (size && count > SIZE_MAX / size) return nullptr;
  if (otaTlsOwner.load() == xTaskGetCurrentTaskHandle() && count * size >= 1024) {
    void *memory = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (memory) ++otaTlsPsramAllocations;
    return memory;
  }
  return esp_mbedtls_mem_calloc(count, size);
}

static void otaInitTlsMemory() {
  // Install once before networking starts. Route only the OTA worker's large
  // allocations to PSRAM; other tasks retain the pinned SDK allocator.
  // Both allocators use heap_caps_free, including allocations made before init.
  mbedtls_platform_set_calloc_free(otaTlsCalloc, esp_mbedtls_mem_free);
}
