#include "BestRadioHlsSource.h"

#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <freertos/task.h>
#include <time.h>
#include <algorithm>
#include <cstring>

namespace {
constexpr const char *kChannelIds[] = {"RA000013", "RA000010", "RA000012", "RA000011"};
struct TextSink { char *data; size_t size; size_t limit; bool overflow; };

esp_err_t receiveText(esp_http_client_event_t *event) {
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  auto *sink = static_cast<TextSink *>(event->user_data);
  if (sink->size + event->data_len >= sink->limit) {
    sink->overflow = true;
    return ESP_FAIL;
  }
  memcpy(sink->data + sink->size, event->data, event->data_len);
  sink->size += event->data_len;
  sink->data[sink->size] = 0;
  return ESP_OK;
}
}

BestRadioHlsSource::~BestRadioHlsSource() { close(); }

bool BestRadioHlsSource::begin(uint8_t channel, SemaphoreHandle_t httpMutex) {
  if (channel >= 4 || ring_ || !httpMutex) return false;
  channel_ = channel;
  httpMutex_ = httpMutex;
  ring_ = static_cast<uint8_t *>(heap_caps_malloc(kRingBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  text_ = static_cast<char *>(heap_caps_malloc(kTextBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  done_ = xSemaphoreCreateBinary();
  if (!ring_ || !text_ || !done_) { close(); return false; }
  if (xTaskCreatePinnedToCore(taskEntry, "best_hls", 8192, this, 2, nullptr, 0) != pdPASS) {
    close();
    return false;
  }
  workerStarted_ = true;
  return true;
}

bool BestRadioHlsSource::close() {
  stop_.store(true);
  if (done_) {
    // All HTTP calls have finite timeouts; wait until the worker no longer owns the buffers.
    if (workerStarted_) xSemaphoreTake(done_, portMAX_DELAY);
    vSemaphoreDelete(done_);
    done_ = nullptr;
    workerStarted_ = false;
  }
  if (ring_) { heap_caps_free(ring_); ring_ = nullptr; }
  if (text_) { heap_caps_free(text_); text_ = nullptr; }
  return true;
}

bool BestRadioHlsSource::isOpen() { return ring_ && !failed_.load() && !stop_.load(); }

size_t BestRadioHlsSource::buffered() const {
  return writePos_.load() - readPos_.load();
}

uint32_t BestRadioHlsSource::readNonBlock(void *data, uint32_t len) {
  if (!data || !len || !ring_) return 0;
  const size_t read = readPos_.load();
  const size_t count = std::min<size_t>(len, writePos_.load() - read);
  const size_t first = std::min(count, kRingBytes - read % kRingBytes);
  memcpy(data, ring_ + read % kRingBytes, first);
  if (count > first) memcpy(static_cast<uint8_t *>(data) + first, ring_, count - first);
  readPos_.store(read + count);
  bytesRead_ += count;
  return count;
}

uint32_t BestRadioHlsSource::read(void *data, uint32_t len) {
  const uint32_t start = millis();
  while (buffered() == 0 && !failed_.load() && !stop_.load() &&
         static_cast<uint32_t>(millis() - start) < 6000)
    vTaskDelay(pdMS_TO_TICKS(2));
  return readNonBlock(data, len);
}

bool BestRadioHlsSource::push(const uint8_t *data, size_t len) {
  while (len && !stop_.load()) {
    const size_t write = writePos_.load();
    const size_t available = kRingBytes - (write - readPos_.load());
    if (!available) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }
    const size_t n = std::min({len, available, kRingBytes - write % kRingBytes});
    memcpy(ring_ + write % kRingBytes, data, n);
    writePos_.store(write + n);
    data += n;
    len -= n;
  }
  return !stop_.load();
}

bool BestRadioHlsSource::fetchText(const String &url, size_t limit) {
  if (!text_ || limit > kTextBytes) return false;
  xSemaphoreTake(httpMutex_, portMAX_DELAY);
  if (stop_.load()) { xSemaphoreGive(httpMutex_); return false; }
  text_[0] = 0;
  TextSink sink{text_, 0, limit, false};
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.event_handler = receiveText;
  cfg.user_data = &sink;
  cfg.timeout_ms = 6000;
  cfg.buffer_size = 2048;
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) { xSemaphoreGive(httpMutex_); return false; }
  const esp_err_t result = esp_http_client_perform(client);
  lastHttpStatus_ = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  xSemaphoreGive(httpMutex_);
  return result == ESP_OK && lastHttpStatus_ == 200 && !sink.overflow && sink.size;
}

String BestRadioHlsSource::resolveUrl(const String &base, const String &relative) {
  if (relative.startsWith("http://") || relative.startsWith("https://")) return relative;
  const int scheme = base.indexOf("://");
  if (scheme < 0) return "";
  const int hostEnd = base.indexOf('/', scheme + 3);
  if (hostEnd < 0) return "";
  if (relative.startsWith("/")) return base.substring(0, hostEnd) + relative;
  const int pathEnd = base.indexOf('?');
  const String path = pathEnd < 0 ? base : base.substring(0, pathEnd);
  const int slash = path.lastIndexOf('/');
  return path.substring(0, slash + 1) + relative;
}

bool BestRadioHlsSource::resolveStation(String &master) {
  if (!fetchText("http://www.bestradio.com.tw/link.aspx", kTextBytes)) return false;
  const String needle = String("https://bestradiow-hichannel.cdn.hinet.net/live/") +
                        kChannelIds[channel_] + "/playlist.m3u8";
  const char *begin = strstr(text_, needle.c_str());
  if (!begin) return false;
  const char *end = strchr(begin, '"');
  if (!end || end - begin > 400 || end - begin < needle.length()) return false;
  master = "";
  if (!master.reserve(end - begin + 1)) return false;
  for (const char *p = begin; p < end; ++p) master += *p;
  const int pos = master.indexOf("expires=");
  expires_ = pos >= 0 ? static_cast<uint32_t>(master.substring(pos + 8).toInt()) : 0;
  Serial.printf("[HLS] official channel=%u URL expires=%u\n", channel_, expires_);
  return true;
}

bool BestRadioHlsSource::loadMediaPlaylist(String &media) {
  if (!fetchText(media, 4096)) return false;
  if (!strstr(text_, "#EXT-X-STREAM-INF")) return true;
  for (char *p = text_; p && *p;) {
    char *end = strchr(p, '\n');
    if (end) *end = 0;
    const String line = String(p); 
    if (line.length() && line[0] != '#' && line[0] != '\r') {
      String relative = line;
      relative.trim();
      media = resolveUrl(media, relative);
      return media.length() > 0;
    }
    p = end ? end + 1 : nullptr;
  }
  return false;
}

bool BestRadioHlsSource::nextSegment(const String &media, String &segment, uint32_t &sequence) {
  if (!fetchText(media, 4096)) return false;
  uint32_t base = 0;
  uint32_t index = 0;
  for (char *p = text_; p && *p;) {
    char *end = strchr(p, '\n');
    if (end) *end = 0;
    if (strncmp(p, "#EXT-X-MEDIA-SEQUENCE:", 22) == 0)
      base = strtoul(p + 22, nullptr, 10);
    else if (*p && *p != '#' && *p != '\r') {
      const uint32_t current = base + index++;
      if (lastSequence_ == UINT32_MAX || current > lastSequence_) {
        String relative(p); relative.trim();
        segment = resolveUrl(media, relative);
        sequence = current;
        return segment.length() > 0;
      }
    }
    p = end ? end + 1 : nullptr;
  }
  return false;
}

bool BestRadioHlsSource::processPacket(const uint8_t *packet) {
  if (packet[0] != 0x47 || (packet[1] & 0x80)) return false;
  const uint16_t pid = ((packet[1] & 0x1f) << 8) | packet[2];
  const bool start = packet[1] & 0x40;
  const uint8_t control = (packet[3] >> 4) & 3;
  if (!(control & 1)) return true;
  size_t offset = 4;
  if (control & 2) offset += 1 + packet[4];
  if (offset >= 188) return offset == 188;
  const uint8_t *p = packet + offset;
  size_t len = 188 - offset;
  if (pid == 0 && start) {
    const size_t pointer = p[0];
    if (pointer + 13 > len) return false;
    p += 1 + pointer; len -= 1 + pointer;
    if (p[0] != 0x00) return false;
    const size_t section = 3 + (((p[1] & 0x0f) << 8) | p[2]);
    if (section > len || section < 16) return false;
    for (size_t i = 8; i + 4 <= section - 4; i += 4)
      if (p[i] || p[i + 1]) { pmtPid_ = ((p[i + 2] & 0x1f) << 8) | p[i + 3]; break; }
  } else if (pid == pmtPid_ && start) {
    const size_t pointer = p[0];
    if (pointer + 13 > len) return false;
    p += 1 + pointer; len -= 1 + pointer;
    if (p[0] != 0x02) return false;
    const size_t section = 3 + (((p[1] & 0x0f) << 8) | p[2]);
    if (section > len || section < 16) return false;
    size_t i = 12 + (((p[10] & 0x0f) << 8) | p[11]);
    while (i + 5 <= section - 4) {
      if (p[i] == 0x0f) { audioPid_ = ((p[i + 1] & 0x1f) << 8) | p[i + 2]; break; }
      i += 5 + (((p[i + 3] & 0x0f) << 8) | p[i + 4]);
    }
  } else if (pid == audioPid_) {
    if (start) {
      if (len < 9 || p[0] || p[1] || p[2] != 1) return false;
      const size_t header = 9 + p[8];
      if (header > len) return false;
      p += header; len -= header;
    }
    return push(p, len);
  }
  return true;
}

bool BestRadioHlsSource::downloadSegment(const String &segment) {
  xSemaphoreTake(httpMutex_, portMAX_DELAY);
  if (stop_.load()) { xSemaphoreGive(httpMutex_); return false; }
  esp_http_client_config_t cfg = {};
  cfg.url = segment.c_str();
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = 6000;
  cfg.buffer_size = 2048;
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) { xSemaphoreGive(httpMutex_); return false; }
  bool okay = esp_http_client_open(client, 0) == ESP_OK;
  if (okay) {
    esp_http_client_fetch_headers(client);
    lastHttpStatus_ = esp_http_client_get_status_code(client);
    okay = lastHttpStatus_ == 200;
  }
  uint8_t packet[188];
  size_t packetCount = 0;
  while (okay && !stop_.load()) {
    size_t got = 0;
    while (got < sizeof(packet) && !stop_.load()) {
      const int n = esp_http_client_read(client, reinterpret_cast<char *>(packet) + got,
                                         sizeof(packet) - got);
      if (n <= 0) break;
      got += n;
    }
    if (!got) break;
    if (got != sizeof(packet) || !processPacket(packet)) { okay = false; break; }
    ++packetCount;
  }
  const bool complete = esp_http_client_is_complete_data_received(client);
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  xSemaphoreGive(httpMutex_);
  return okay && complete && packetCount && audioPid_ != 0xffff;
}

void BestRadioHlsSource::taskEntry(void *arg) {
  static_cast<BestRadioHlsSource *>(arg)->run();
  vTaskDelete(nullptr);
}

void BestRadioHlsSource::run() {
  String master, media;
  int errors = 0;
  if (!resolveStation(master)) errors = 5;
  else { media = master; if (!loadMediaPlaylist(media)) errors = 5; }
  while (!stop_.load() && errors < 5) {
    const time_t now = time(nullptr);
    if (expires_ && now > 1700000000 && static_cast<uint32_t>(now + 120) >= expires_) {
      if (!resolveStation(master)) { ++errors; vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
      media = master;
      if (!loadMediaPlaylist(media)) { ++errors; continue; }
    }
    String segment;
    uint32_t sequence = 0;
    if (!nextSegment(media, segment, sequence)) {
      if (lastHttpStatus_ != 200 && ++errors >= 2) {
        if (resolveStation(master)) { media = master; loadMediaPlaylist(media); errors = 0; }
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (!downloadSegment(segment)) {
      Serial.printf("[HLS] segment=%u failed http=%d\n", sequence, lastHttpStatus_);
      ++errors;
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    lastSequence_ = sequence;
    if (sequence % 6 == 0)
      Serial.printf("[HLS] segment=%u fill=%u heap=%u largest=%u psram=%u stack=%u\n",
                    sequence, (unsigned)buffered(),
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                    (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    errors = 0;
  }
  if (!stop_.load()) {
    failed_.store(true);
    Serial.printf("[HLS] stopped after errors=%d http=%d\n", errors, lastHttpStatus_);
  }
  xSemaphoreGive(done_);
}
