#include <esp_image_format.h>
#include "ota_rollback_policy.h"
#include "ota_identity_scan.h"
#include "ota_legacy_images.h"

// Referenced in boot logging so the linker retains this identity in the image.
static const char kDeskDockRollbackIdentity[] = DESKDOCK_ROLLBACK_ID_PREFIX DESKDOCK_VERSION "|";
enum class RollbackStage : uint8_t {
  Unchecked, Checking, Ready, NoSlot, CurrentPending, Unconfirmed, Invalid,
  Unknown, NotOlder, Changed, Audio, Storage, Select, Task, Rebooting
};
struct RollbackTarget {
  uint32_t address = 0;
  uint8_t digest[32] = {};
  char version[24] = {};
  uint8_t bootAck = 0;
  uint8_t reserved[3] = {};
};
static RollbackTarget rollbackTarget, rollbackRequested;
static std::atomic<RollbackStage> rollbackStage{RollbackStage::Unchecked};
enum class RollbackBoot : uint8_t { None, Waiting, Confirmed, Failed, Legacy };
static std::atomic<RollbackBoot> rollbackBoot{RollbackBoot::None};
struct RollbackBootRecord {
  uint32_t magic = 0x52423101;
  RollbackTarget target;
};
static RollbackBootRecord rollbackBootRecord;
static_assert(sizeof(RollbackTarget) == 64 && sizeof(RollbackBootRecord) == 68,
              "Rollback NVS record layout changed");

static bool rollbackStoreRequest(const RollbackTarget &target) {
  Preferences store;
  if (!store.begin("desk_rb", false)) return false;
  RollbackBootRecord record = {};
  record.target = target;
  const bool saved = store.putBytes("pending", &record, sizeof(record)) == sizeof(record);
  store.end();
  return saved;
}
static void rollbackClearRequest() {
  Preferences store;
  if (store.begin("desk_rb", false)) { store.remove("pending"); store.end(); }
}

static bool rollbackReadIdentity(const esp_partition_t *partition, uint32_t imageSize,
                                 char (&version)[24], bool &found) {
  return deskReadImageIdentity(imageSize,version,found,
    [partition](uint32_t offset, uint8_t *block, size_t count) {
      return esp_partition_read(partition,offset,block,count) == ESP_OK;
    }, []() { vTaskDelay(1); });
}

static RollbackStage rollbackInspect(RollbackTarget &target) {
  if (otaPendingVerify.load() || rollbackBoot.load() == RollbackBoot::Waiting)
    return RollbackStage::CurrentPending;
  const auto *running = esp_ota_get_running_partition();
  const auto *boot = esp_ota_get_boot_partition();
  const auto *candidate = esp_ota_get_next_update_partition(nullptr);
  if (!running || !boot || !candidate || esp_ota_get_app_partition_count() != 2 ||
      running->address != boot->address || running->address == candidate->address ||
      (running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 && running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_1) ||
      candidate->subtype != (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ?
                            ESP_PARTITION_SUBTYPE_APP_OTA_1 : ESP_PARTITION_SUBTYPE_APP_OTA_0))
    return RollbackStage::NoSlot;
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(candidate,&state) != ESP_OK || state != ESP_OTA_IMG_VALID)
    return RollbackStage::Unconfirmed;
  if (esp_partition_get_sha256(candidate,target.digest) != ESP_OK) return RollbackStage::Invalid;
  esp_image_metadata_t metadata = {};
  const esp_partition_pos_t position = {candidate->address,candidate->size};
  if (esp_image_get_metadata(&position,&metadata) != ESP_OK || !metadata.image_len ||
      metadata.image_len > candidate->size) return RollbackStage::Invalid;
  bool identityFound = false;
  if (!rollbackReadIdentity(candidate,metadata.image_len,target.version,identityFound))
    return RollbackStage::Invalid;
  target.bootAck = identityFound;
  if (!identityFound) {
    for (const auto &image : kLegacyOtaImages) {
      if (!memcmp(image.digest,target.digest,32)) {
        strlcpy(target.version,image.version,sizeof(target.version)); break;
      }
    }
  }
  if (!target.version[0]) return RollbackStage::Unknown;
  if (!rollbackOlder(target.version,DESKDOCK_VERSION)) return RollbackStage::NotOlder;
  if (!esp_ota_check_rollback_is_possible()) return RollbackStage::Unconfirmed;
  target.address = candidate->address;
  return RollbackStage::Ready;
}

static bool rollbackStopAudio(bool &attempted) {
  attempted = true;
  return otaStopAudio();
}

static void rollbackWorker(void *parameter) {
  const bool execute = parameter != nullptr;
  bool audioStopAttempted = false;
  RollbackTarget candidate;
  RollbackStage result = rollbackInspect(candidate);
  const int resumeStation = execute && radioNeedsNetwork() ? radioStation.load() : 0;
  if (result == RollbackStage::Ready && execute) {
    if (candidate.address != rollbackRequested.address ||
        strcmp(candidate.version,rollbackRequested.version) ||
        memcmp(candidate.digest,rollbackRequested.digest,32)) result = RollbackStage::Changed;
    else if (!rollbackStopAudio(audioStopAttempted)) result = RollbackStage::Audio;
    else {
      const auto *partition = esp_ota_get_next_update_partition(nullptr);
      uint8_t digest[32] = {};
      if (!partition || partition->address != candidate.address ||
          esp_partition_get_sha256(partition,digest) != ESP_OK ||
          memcmp(digest,candidate.digest,32)) result = RollbackStage::Changed;
      else {
        flushPendingSettings(true);
        if (pendingSettingsMask.load() || !rollbackStoreRequest(candidate)) result = RollbackStage::Storage;
        else {
          const esp_err_t error = esp_ota_set_boot_partition(partition);
          if (error != ESP_OK) {
            rollbackClearRequest(); result = RollbackStage::Select;
            Serial.printf("[ROLLBACK] boot selection failed: %s\n",esp_err_to_name(error));
          } else {
            Serial.printf("[ROLLBACK] %s -> %s slot=%s address=0x%lx ack=%d\n",
                DESKDOCK_VERSION,candidate.version,partition->label,(unsigned long)partition->address,candidate.bootAck);
            rollbackStage.store(RollbackStage::Rebooting);
            vTaskDelay(pdMS_TO_TICKS(1500));
            esp_restart();
          }
        }
      }
    }
  }
  if (!execute && result == RollbackStage::Ready) rollbackTarget = candidate;
  if (audioStopAttempted && resumeStation && result != RollbackStage::Ready && WiFi.status() == WL_CONNECTED)
    radioCommand.store(resumeStation);
  rollbackStage.store(result);
  otaBusy.store(false);
  vTaskDelete(nullptr);
}

static void rollbackCheckBootRecord() {
  Serial.printf("[ROLLBACK] identity=%s\n",kDeskDockRollbackIdentity);
  Preferences store;
  if (!store.begin("desk_rb",true)) return;
  const bool loaded = store.getBytesLength("pending") == sizeof(rollbackBootRecord) &&
      store.getBytes("pending",&rollbackBootRecord,sizeof(rollbackBootRecord)) == sizeof(rollbackBootRecord);
  store.end();
  uint16_t parts[3] = {};
  if (!loaded || rollbackBootRecord.magic != 0x52423101 ||
      !memchr(rollbackBootRecord.target.version,'\0',sizeof(rollbackBootRecord.target.version)) ||
      rollbackBootRecord.target.bootAck > 1 ||
      !rollbackVersion(rollbackBootRecord.target.version,parts)) return;
  if (!rollbackBootRecord.target.bootAck) {
    const auto *invalid = esp_ota_get_last_invalid_partition();
    rollbackBoot.store(invalid && invalid->address == rollbackBootRecord.target.address ?
                       RollbackBoot::Failed : RollbackBoot::Legacy);
    rollbackClearRequest();
    return;
  }
  const auto *running = esp_ota_get_running_partition();
  uint8_t digest[32] = {};
  const bool matches = running && running->address == rollbackBootRecord.target.address &&
      !strcmp(DESKDOCK_VERSION,rollbackBootRecord.target.version) &&
      esp_partition_get_sha256(running,digest) == ESP_OK &&
      !memcmp(digest,rollbackBootRecord.target.digest,32);
  rollbackBoot.store(matches ? RollbackBoot::Waiting : RollbackBoot::Failed);
  if (!matches) rollbackClearRequest();
  Serial.printf("[ROLLBACK] requested=%s actual=%s identity_match=%d\n",
      rollbackBootRecord.target.version,DESKDOCK_VERSION,matches);
}

static void rollbackConfirmBoot() {
  if (rollbackBoot.load() != RollbackBoot::Waiting) return;
  rollbackClearRequest();
  rollbackBoot.store(RollbackBoot::Confirmed);
  Serial.printf("[ROLLBACK] boot confirmed %s\n",DESKDOCK_VERSION);
}
