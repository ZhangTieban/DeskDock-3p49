#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Serialize short SD operations across text rendering and the MP3 producer.
inline SemaphoreHandle_t deskSdMutex() {
  static StaticSemaphore_t storage;
  static SemaphoreHandle_t mutex = xSemaphoreCreateMutexStatic(&storage);
  return mutex;
}
class DeskSdLock {
 public:
  DeskSdLock() { xSemaphoreTake(deskSdMutex(), portMAX_DELAY); }
  ~DeskSdLock() { xSemaphoreGive(deskSdMutex()); }
  DeskSdLock(const DeskSdLock &) = delete;
  DeskSdLock &operator=(const DeskSdLock &) = delete;
};
