// GitHub Release OTA: manifest -> SD cache -> SHA-256 -> inactive app slot.
enum class OtaStage : uint8_t {
  Idle, Checking, Available, Downloading, Verifying, Installing,
  Rebooting, UpToDate, Error
};
enum class OtaError : uint8_t {
  None, Wifi, Radio, Manifest, Sd, Space, Download, Hash, Partition, Write, Task
};
struct OtaManifest {
  char version[24] = {};
  char sha256[65] = {};
  size_t size = 0;
};
static OtaManifest otaManifest;
static std::atomic<OtaStage> otaStage{OtaStage::Idle};
static std::atomic<OtaError> otaError{OtaError::None};
static std::atomic<int> otaProgress{0};
static std::atomic<bool> otaBusy{false};
static bool otaSdMounted = false;
static bool otaPendingVerify = false;
static uint32_t otaBootReadyMs = 0;

static void otaSetError(OtaError code, const char *detail) {
  otaError.store(code);
  otaStage.store(OtaStage::Error);
  Serial.printf("[OTA] %s\n", detail);
}

static bool otaVersionParts(const char *version, unsigned parts[3]) {
  char tail = 0;
  return version && sscanf(version, "%u.%u.%u%c",
                           &parts[0], &parts[1], &parts[2], &tail) == 3;
}

static bool otaIsNewer(const char *candidate) {
  unsigned next[3], current[3];
  if (!otaVersionParts(candidate, next) ||
      !otaVersionParts(DESKDOCK_VERSION, current)) return false;
  for (int i = 0; i < 3; ++i) {
    if (next[i] != current[i]) return next[i] > current[i];
  }
  return false;
}

struct OtaManifestResponse {
  String body;
  String location;
  bool overflow = false;
};

static void otaCaptureRedirect(esp_http_client_event_t *event, String *location) {
  if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key &&
      String(event->header_key).equalsIgnoreCase("Location") && event->header_value)
    *location = event->header_value;
}

static esp_err_t otaManifestHttpEvent(esp_http_client_event_t *event) {
  auto *response = static_cast<OtaManifestResponse *>(event->user_data);
  otaCaptureRedirect(event, &response->location);
  if (event->event_id != HTTP_EVENT_ON_DATA ||
      esp_http_client_get_status_code(event->client) != 200) return ESP_OK;
  if (response->body.length() + event->data_len > 2048) {
    response->overflow = true;
    return ESP_FAIL;
  }
  response->body.concat((const char *)event->data, event->data_len);
  return ESP_OK;
}

// GitHub Release assets redirect to a signed CDN URL. Recreate the client at
// each hop so a TLS connection to the previous host does not stay allocated.
static bool otaHttpFollow(const String &initialUrl, http_event_handle_cb callback,
                          void *context, String *location, int timeoutMs,
                          int *finalStatus) {
  if (xSemaphoreTake(networkHttpMutex, pdMS_TO_TICKS(15000)) != pdTRUE) return false;
  String url = initialUrl;
  bool success = false;
  *finalStatus = 0;
  for (int hop = 0; hop < 5; ++hop) {
    location->remove(0);
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.event_handler = callback;
    config.user_data = context;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = timeoutMs;
    config.buffer_size = 4096;
    config.disable_auto_redirect = true;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) break;
    const esp_err_t result = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    *finalStatus = status;
    if (result == ESP_OK && status == 200) {
      success = true;
      break;
    }
    if ((status == 301 || status == 302 || status == 303 ||
         status == 307 || status == 308) &&
        location->startsWith("https://") && location->length() <= 4096) {
      url = *location;
      Serial.printf("[OTA] HTTP redirect %d, hop %d\n", status, hop + 1);
      continue;
    }
    Serial.printf("[OTA] HTTP result=%d status=%d redirect=%u\n",
                  (int)result, status, (unsigned)location->length());
    break;
  }
  xSemaphoreGive(networkHttpMutex);
  return success;
}

static bool otaReadManifest() {
  OtaManifestResponse response;
  response.body.reserve(2048);
  const String url = String("https://github.com/") +
      DESKDOCK_GITHUB_REPOSITORY + "/releases/latest/download/version.json";
  int status = 0;
  if (!otaHttpFollow(url, otaManifestHttpEvent, &response, &response.location,
                     10000, &status) || response.overflow) return false;
  cJSON *root = cJSON_Parse(response.body.c_str());
  if (!root) return false;
  const char *board = jsonString(root, "board");
  const char *version = jsonString(root, "version");
  const char *sha = jsonString(root, "sha256");
  const char *file = jsonString(root, "file");
  const cJSON *size = cJSON_GetObjectItemCaseSensitive(root, "size");
  bool valid = strcmp(board, DESKDOCK_BOARD_ID) == 0 &&
               strcmp(file, "firmware.bin") == 0 &&
               strlen(version) < sizeof(otaManifest.version) &&
               strlen(sha) == 64 && cJSON_IsNumber(size) &&
               size->valuedouble >= 0x10000 &&
               size->valuedouble <= 0x400000 &&
               floor(size->valuedouble) == size->valuedouble;
  unsigned parts[3];
  valid = valid && otaVersionParts(version, parts);
  for (size_t i = 0; valid && i < 64; ++i)
    valid = (sha[i] >= '0' && sha[i] <= '9') ||
            (sha[i] >= 'a' && sha[i] <= 'f');
  if (valid) {
    strlcpy(otaManifest.version, version, sizeof(otaManifest.version));
    strlcpy(otaManifest.sha256, sha, sizeof(otaManifest.sha256));
    otaManifest.size = (size_t)size->valuedouble;
  }
  cJSON_Delete(root);
  return valid;
}

static bool otaStopRadio() {
  if (!radioTaskHandle) return true;
  radioCommand.store(0);
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 25000) {
    if (radioCommand.load() == -1 && radioState.load() == 0) {
      vTaskDelay(pdMS_TO_TICKS(300));
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  return false;
}

struct OtaDownload {
  File *file;
  mbedtls_sha256_context *sha;
  size_t received = 0;
  size_t expected = 0;
  bool failed = false;
  String location;
};

static esp_err_t otaHttpEvent(esp_http_client_event_t *event) {
  auto *transfer = static_cast<OtaDownload *>(event->user_data);
  otaCaptureRedirect(event, &transfer->location);
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  if (esp_http_client_get_status_code(event->client) != 200) return ESP_OK;
  const size_t count = (size_t)event->data_len;
  if (!count || transfer->received > transfer->expected ||
      count > transfer->expected - transfer->received ||
      transfer->file->write((const uint8_t *)event->data, count) != count ||
      mbedtls_sha256_update(transfer->sha, (const unsigned char *)event->data, count)) {
    transfer->failed = true;
    return ESP_FAIL;
  }
  transfer->received += count;
  otaProgress.store((int)(transfer->received * 100 / transfer->expected));
  return ESP_OK;
}

static bool otaDownloadToSd() {
  if (!otaSdMounted) {
    otaSdMounted = SD_MMC.setPins(41, 39, 40) &&
                   SD_MMC.begin("/sdcard", true, false);
  }
  if (!otaSdMounted || SD_MMC.cardType() == CARD_NONE) {
    if (otaSdMounted) SD_MMC.end();
    otaSdMounted = false;
    otaSetError(OtaError::Sd, "SD mount failed; check FAT32 card and GPIO39/40/41");
    return false;
  }
  const uint64_t total = SD_MMC.totalBytes();
  const uint64_t used = SD_MMC.usedBytes();
  if (total <= used || total - used < otaManifest.size + 1024 * 1024) {
    otaSetError(OtaError::Space, "SD free space too small");
    return false;
  }
  if (!SD_MMC.mkdir("/update") && !SD_MMC.exists("/update")) {
    otaSetError(OtaError::Sd, "SD /update directory failed");
    return false;
  }
  SD_MMC.remove("/update/firmware.tmp");
  File output = SD_MMC.open("/update/firmware.tmp", FILE_WRITE);
  if (!output) {
    otaSetError(OtaError::Sd, "SD firmware.tmp open failed");
    return false;
  }
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  if (mbedtls_sha256_starts(&sha, 0)) {
    output.close();
    mbedtls_sha256_free(&sha);
    otaSetError(OtaError::Hash, "SHA-256 init failed");
    return false;
  }
  OtaDownload transfer{&output, &sha, 0, otaManifest.size, false};
  const String url = String("https://github.com/") + DESKDOCK_GITHUB_REPOSITORY +
      "/releases/download/v" + otaManifest.version + "/firmware.bin";
  otaStage.store(OtaStage::Downloading);
  int status = 0;
  const bool downloaded = otaHttpFollow(url, otaHttpEvent, &transfer,
                                        &transfer.location, 20000, &status);
  output.flush();
  output.close();
  unsigned char digest[32];
  const int hashResult = mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  if (!downloaded || status != 200 || transfer.failed ||
      transfer.received != transfer.expected) {
    SD_MMC.remove("/update/firmware.tmp");
    Serial.printf("[OTA] download http=%d bytes=%u expected=%u\n",
                  status, (unsigned)transfer.received,
                  (unsigned)transfer.expected);
    otaSetError(OtaError::Download, "firmware download incomplete");
    return false;
  }
  otaStage.store(OtaStage::Verifying);
  char actual[65];
  for (size_t i = 0; i < 32; ++i)
    snprintf(actual + i * 2, 3, "%02x", digest[i]);
  if (hashResult || strcmp(actual, otaManifest.sha256) != 0) {
    SD_MMC.remove("/update/firmware.tmp");
    otaSetError(OtaError::Hash, "firmware SHA-256 mismatch");
    return false;
  }
  SD_MMC.remove("/update/firmware.bin");
  if (!SD_MMC.rename("/update/firmware.tmp", "/update/firmware.bin")) {
    otaSetError(OtaError::Sd, "SD firmware rename failed");
    return false;
  }
  Serial.printf("[OTA] verified %s %u bytes\n", otaManifest.version,
                (unsigned)otaManifest.size);
  return true;
}

static bool otaInstallFromSd() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
  if (!running || !next || running->address == next->address ||
      next->size < otaManifest.size) {
    otaSetError(OtaError::Partition, "A/B OTA partition unavailable");
    return false;
  }
  File input = SD_MMC.open("/update/firmware.bin", FILE_READ);
  if (!input || input.size() != otaManifest.size || input.read() != 0xE9) {
    if (input) input.close();
    otaSetError(OtaError::Sd, "verified firmware file unavailable or invalid");
    return false;
  }
  input.seek(0);
  otaStage.store(OtaStage::Installing);
  esp_ota_handle_t handle = 0;
  esp_err_t err = esp_ota_begin(next, otaManifest.size, &handle);
  uint8_t buffer[4096];
  size_t written = 0;
  while (err == ESP_OK && written < otaManifest.size) {
    const size_t count = input.read(buffer,
        std::min(sizeof(buffer), otaManifest.size - written));
    if (!count) { err = ESP_FAIL; break; }
    err = esp_ota_write(handle, buffer, count);
    if (err == ESP_OK) {
      written += count;
      otaProgress.store((int)(written * 100 / otaManifest.size));
      vTaskDelay(1);
    }
  }
  input.close();
  if (err == ESP_OK) err = esp_ota_end(handle);
  else if (handle) esp_ota_abort(handle);
  if (err == ESP_OK) err = esp_ota_set_boot_partition(next);
  if (err != ESP_OK) {
    Serial.printf("[OTA] write error=%s\n", esp_err_to_name(err));
    otaSetError(OtaError::Write, "OTA partition write/verify failed");
    return false;
  }
  Serial.printf("[OTA] boot slot=%s version=%s\n", next->label, otaManifest.version);
  otaStage.store(OtaStage::Rebooting);
  vTaskDelay(pdMS_TO_TICKS(1500));
  esp_restart();
  return true;
}

static void otaWorker(void *parameter) {
  const bool install = parameter != nullptr;
  const int resumeStation = radioNeedsNetwork() ? radioStation.load() : 0;
  bool successful = false;
  if (WiFi.status() != WL_CONNECTED)
    otaSetError(OtaError::Wifi, "Wi-Fi disconnected");
  else if (!otaStopRadio())
    otaSetError(OtaError::Radio, "radio could not stop");
  else if (!install) {
    otaStage.store(OtaStage::Checking);
    if (!otaReadManifest())
      otaSetError(OtaError::Manifest, "Release manifest unavailable/invalid");
    else if (!otaIsNewer(otaManifest.version)) {
      otaStage.store(OtaStage::UpToDate);
      Serial.printf("[OTA] current %s latest %s\n",
                    DESKDOCK_VERSION, otaManifest.version);
    } else {
      otaStage.store(OtaStage::Available);
      Serial.printf("[OTA] update available %s -> %s\n",
                    DESKDOCK_VERSION, otaManifest.version);
    }
    successful = true;
  } else if (otaDownloadToSd()) {
    otaProgress.store(0);
    successful = otaInstallFromSd();
  }
  if (resumeStation && radioTaskHandle &&
      (!install || !successful) && WiFi.status() == WL_CONNECTED)
    radioCommand.store(resumeStation);
  otaBusy.store(false);
  vTaskDelete(nullptr);
}

static void otaButtonClicked(lv_event_t *) {
  if (otaBusy.exchange(true)) return;
  const bool install = otaStage.load() == OtaStage::Available;
  otaError.store(OtaError::None);
  otaProgress.store(0);
  otaStage.store(install ? OtaStage::Downloading : OtaStage::Checking);
  if (xTaskCreatePinnedToCore(otaWorker, "ota", 12288,
                              install ? (void *)1 : nullptr, 1, nullptr, 0) != pdPASS) {
    otaBusy.store(false);
    otaSetError(OtaError::Task, "OTA task creation failed");
  }
}

static void refreshOtaUi() {
  if (!otaButton || !otaStatusLabel) return;
  const OtaStage stage = otaStage.load();
  const char *action = stage == OtaStage::Available ? "安裝更新" : "檢查更新";
  lv_obj_t *buttonLabel = lv_obj_get_child(otaButton, 0);
  if (buttonLabel && strcmp(lv_label_get_text(buttonLabel), action))
    lv_label_set_text(buttonLabel, action);
  char text[40];
  switch (stage) {
    case OtaStage::Idle: snprintf(text, sizeof(text), "版本 %s", DESKDOCK_VERSION); break;
    case OtaStage::Checking: strcpy(text, "檢查中..."); break;
    case OtaStage::Available: snprintf(text, sizeof(text), "新版 %s", otaManifest.version); break;
    case OtaStage::Downloading:
      snprintf(text, sizeof(text), "下載 %d%%", otaProgress.load()); break;
    case OtaStage::Verifying: strcpy(text, "驗證中..."); break;
    case OtaStage::Installing:
      snprintf(text, sizeof(text), "寫入 %d%%", otaProgress.load()); break;
    case OtaStage::Rebooting: strcpy(text, "重新啟動..."); break;
    case OtaStage::UpToDate: strcpy(text, "已是最新版"); break;
    default:
      switch (otaError.load()) {
        case OtaError::Wifi: strcpy(text, "Wi-Fi 未連線"); break;
        case OtaError::Sd: case OtaError::Space: strcpy(text, "SD 卡錯誤"); break;
        case OtaError::Hash: strcpy(text, "驗證失敗"); break;
        case OtaError::Partition: strcpy(text, "分區錯誤"); break;
        default: strcpy(text, "更新失敗"); break;
      }
      break;
  }
  if (strcmp(lv_label_get_text(otaStatusLabel), text))
    lv_label_set_text(otaStatusLabel, text);
}

static void otaCheckFirstBoot() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  Serial.printf("[OTA] running=%s version=%s\n",
                running ? running->label : "unknown", DESKDOCK_VERSION);
  esp_ota_img_states_t state;
  otaPendingVerify = running &&
      esp_ota_get_state_partition(running, &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY;
  if (otaPendingVerify)
    Serial.printf("[OTA] pending first-boot validation on %s\n", running->label);
}

static void otaConfirmFirstBoot(uint32_t now) {
  if (!otaPendingVerify) return;
  if (!otaBootReadyMs) otaBootReadyMs = now;
  if ((uint32_t)(now - otaBootReadyMs) < 10000 || !homeScreen ||
      !settingsScreen || !radioTaskHandle || !networkTaskHandle) return;
  const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err == ESP_OK) {
    otaPendingVerify = false;
    Serial.println("[OTA] first boot valid");
  } else {
    Serial.printf("[OTA] first boot confirmation failed: %s\n", esp_err_to_name(err));
  }
}
