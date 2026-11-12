#pragma once
#include <AudioFileSource.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include "sd_access.h"

// One SD producer; the decoder consumes encoded bytes from a 64 KiB PSRAM ring.
class BufferedSdSource final : public AudioFileSource {
 public:
  explicit BufferedSdSource(const char *path) {
    ring_ = static_cast<uint8_t *>(heap_caps_malloc(kCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    done_ = xSemaphoreCreateBinary();
    if (!ring_ || !done_) return;
    { DeskSdLock lock; file_ = SD_MMC.open(path, FILE_READ); if (file_) size_ = file_.size(); }
    if (!file_ || !size_) return;
    worker_ = xTaskCreatePinnedToCore(workerEntry, "mp3-sd", 4096, this, 2, nullptr, 0) == pdPASS;
  }
  ~BufferedSdSource() override { close(); }
  bool isOpen() override { return worker_ && !stop_.load() && !failed_.load(); }
  uint32_t getSize() override { return size_; }
  uint32_t getPos() override { return consumed_; }
  size_t buffered() const { return write_.load() - read_.load(); }
  bool ready() const { return buffered() >= std::min(size_, uint32_t(32768)) || eof_.load(); }
  bool failed() const { return failed_.load(); }
  uint32_t read(void *data, uint32_t length) override {
    uint32_t copied = 0;
    const uint32_t started = millis();
    while (copied < length && !stop_.load()) {
      const size_t r = read_.load();
      const size_t available = write_.load() - r;
      if (!available) {
        if (eof_.load() || failed_.load() || millis() - started >= 2000) break;
        vTaskDelay(1);
        continue;
      }
      const size_t count = std::min({available, size_t(length - copied), kCapacity - r % kCapacity});
      memcpy(static_cast<uint8_t *>(data) + copied, ring_ + r % kCapacity, count);
      read_.store(r + count);
      copied += count;
    }
    consumed_ += copied;
    return copied;
  }
  bool seek(int32_t offset, int origin) override {
    const int64_t target = int64_t(origin == SEEK_CUR ? consumed_ : origin == SEEK_END ? size_ : 0) + offset;
    if (target < 0 || target > size_ || !worker_ || failed_.load()) return false;
    // Suspend producer at its explicit handshake, never during an SD call.
    seekTarget_.store(static_cast<uint32_t>(target));
    seekPending_.store(true);
    while (seekPending_.load() && !stop_.load() && !failed_.load()) vTaskDelay(1);
    consumed_ = static_cast<uint32_t>(target);
    return !failed_.load();
  }
  bool close() override {
    stop_.store(true);
    if (worker_) { xSemaphoreTake(done_, portMAX_DELAY); worker_ = false; }
    { DeskSdLock lock; if (file_) file_.close(); }
    if (done_) { vSemaphoreDelete(done_); done_ = nullptr; }
    free(ring_); ring_ = nullptr;
    return true;
  }
 private:
  static constexpr size_t kCapacity = 64 * 1024;
  static void workerEntry(void *self) {
    auto *source = static_cast<BufferedSdSource *>(self);
    source->run();
    xSemaphoreGive(source->done_);
    vTaskDelete(nullptr);
  }
  void run() {
    while (!stop_.load()) {
      if (seekPending_.load()) {
        bool ok;
        { DeskSdLock lock; ok = file_.seek(seekTarget_.load()); }
        read_.store(0); write_.store(0); eof_.store(false); failed_.store(!ok);
        seekPending_.store(false);
      }
      const size_t w = write_.load();
      const size_t room = kCapacity - (w - read_.load());
      if (eof_.load() || room < 4096) { vTaskDelay(1); continue; }
      const size_t count = std::min({room, size_t(4096), kCapacity - w % kCapacity});
      size_t got;
      { DeskSdLock lock;
        got = file_.read(ring_ + w % kCapacity, count);
        if (!got) { if (file_.position() >= size_) eof_.store(true); else failed_.store(true); }
      }
      if (got) write_.store(w + got);
      if (failed_.load()) break;
      taskYIELD();
    }
  }
  File file_;
  uint8_t *ring_ = nullptr;
  SemaphoreHandle_t done_ = nullptr;
  bool worker_ = false;
  uint32_t size_ = 0, consumed_ = 0;
  std::atomic<size_t> read_{0}, write_{0};
  std::atomic<bool> stop_{false}, eof_{false}, failed_{false}, seekPending_{false};
  std::atomic<uint32_t> seekTarget_{0};
};
