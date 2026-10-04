#pragma once

#include <Arduino.h>
#include <AudioFileSource.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>

// Audio-only HLS source for Best Radio's signed AAC-in-MPEG-TS streams.
// One producer owns the network; the decoder reads from a PSRAM SPSC ring.
class BestRadioHlsSource final : public AudioFileSource {
 public:
  BestRadioHlsSource() = default;
  ~BestRadioHlsSource() override;
  bool begin(uint8_t channel, SemaphoreHandle_t httpMutex);
  bool close() override;
  bool isOpen() override;
  uint32_t read(void *data, uint32_t len) override;
  uint32_t readNonBlock(void *data, uint32_t len) override;
  uint32_t getSize() override { return 0; }
  uint32_t getPos() override { return bytesRead_; }
  bool loop() override { return !failed_.load(); }
  size_t buffered() const;
  bool failed() const { return failed_.load(); }

 private:
  static constexpr size_t kRingBytes = 256 * 1024;
  static constexpr size_t kTextBytes = 32 * 1024;
  static void taskEntry(void *arg);
  void run();
  bool resolveStation(String &master);
  bool loadMediaPlaylist(String &media);
  bool nextSegment(const String &media, String &segment, uint32_t &sequence);
  bool downloadSegment(const String &segment);
  bool fetchText(const String &url, size_t limit);
  bool push(const uint8_t *data, size_t len);
  bool processPacket(const uint8_t *packet);
  static String resolveUrl(const String &base, const String &relative);

  uint8_t channel_ = 0;
  uint8_t *ring_ = nullptr;
  char *text_ = nullptr;
  SemaphoreHandle_t done_ = nullptr;
  SemaphoreHandle_t httpMutex_ = nullptr;
  bool workerStarted_ = false;
  std::atomic<size_t> readPos_{0}, writePos_{0};
  std::atomic<bool> stop_{false}, failed_{false};
  uint32_t bytesRead_ = 0;
  uint32_t lastSequence_ = UINT32_MAX;
  uint16_t pmtPid_ = 0xffff, audioPid_ = 0xffff;
  uint32_t expires_ = 0;
  int lastHttpStatus_ = 0;
};
