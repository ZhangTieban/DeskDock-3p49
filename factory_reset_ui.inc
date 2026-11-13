enum class FactoryResetStage : uint8_t { Idle, Working, AudioError, StorageError, TaskError };
static std::atomic<FactoryResetStage> factoryResetStage{FactoryResetStage::Idle};

static void factoryCloseDialog(lv_event_t *) {
  if (!factoryDialog) return;
  lv_obj_delete_async(factoryDialog);
  factoryDialog=nullptr;
}

static void factoryResetWorker(void *) {
  FactoryResetStage error=FactoryResetStage::AudioError;
  if (otaStopAudio()) {
    // All application NVS writers use configMutex. Keep it through restart so
    // an in-flight weather fetch or settings writer cannot repopulate defaults.
    xSemaphoreTake(configMutex,portMAX_DELAY);
    pendingSettingsMask.store(0);
    if (prefs.clear()) {
      WiFi.disconnect(true,true);
      Serial.println("[RESET] desk349 settings cleared; firmware and SD preserved");
      vTaskDelay(pdMS_TO_TICKS(200));
      esp_restart();
    }
    xSemaphoreGive(configMutex);
    error=FactoryResetStage::StorageError;
  }
  factoryResetStage.store(error);
  factoryResetActive.store(false);
  otaBusy.store(false);
  vTaskDelete(nullptr);
}

static void factoryConfirm(lv_event_t *) {
  if (!factoryDialog || otaPendingVerify.load() ||
      rollbackBoot.load()==RollbackBoot::Waiting || otaBusy.exchange(true)) return;
  factoryCloseDialog(nullptr);
  factoryResetStage.store(FactoryResetStage::Working);
  factoryResetActive.store(true);
  lv_label_set_text(factoryResetStatusLabel,"正在恢復並重啟...");
  otaControlEnabled(factoryResetButton,false);
  otaControlEnabled(otaInstallButton,false);
  otaControlEnabled(rollbackButton,false);
  otaControlEnabled(rollbackScanButton,false);
  otaControlEnabled(otaBackButton,false);
  if (xTaskCreatePinnedToCore(factoryResetWorker,"factory reset",6144,nullptr,1,nullptr,0)!=pdPASS) {
    factoryResetStage.store(FactoryResetStage::TaskError);
    factoryResetActive.store(false);
    otaBusy.store(false);
  }
}

static void factoryAsk(lv_event_t *) {
  if (otaRollbackDialogOpen() || otaBusy.load() || otaPendingVerify.load() ||
      rollbackBoot.load()==RollbackBoot::Waiting) return;
  factoryDialog=lv_obj_create(otaPanel);
  lv_obj_set_pos(factoryDialog,0,0);lv_obj_set_size(factoryDialog,640,172);
  lv_obj_set_style_pad_all(factoryDialog,0,0);
  lv_obj_set_style_border_width(factoryDialog,0,0);
  lv_obj_set_style_bg_color(factoryDialog,lv_color_hex(0x080B0B),0);
  lv_obj_remove_flag(factoryDialog,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *box=lv_obj_create(factoryDialog);
  lv_obj_set_pos(box,38,6);lv_obj_set_size(box,564,160);
  lv_obj_set_style_pad_all(box,0,0);
  lv_obj_set_style_radius(box,7,0);
  lv_obj_set_style_border_width(box,1,0);
  lv_obj_set_style_bg_color(box,lv_color_hex(0x121916),0);
  lv_obj_remove_flag(box,LV_OBJ_FLAG_SCROLLABLE);
  generalLabel(box,"確認回到初始設定",12,6,540);
  lv_obj_t *detail=generalLabel(box,
      "清除 Wi-Fi、天氣、股票、亮度、音量與輪播設定。",12,34,540);
  lv_label_set_long_mode(detail,LV_LABEL_LONG_WRAP);
  generalLabel(box,"保留目前韌體、前版、SD 字庫與 MP3。",12,81,540);
  generalButton(box,"取消",12,118,260,32,factoryCloseDialog);
  generalButton(box,"確認恢復並重啟",284,118,268,32,factoryConfirm);
}

static void refreshFactoryResetUi() {
  if (!factoryResetStatusLabel) return;
  const char *text="清除設定與 Wi-Fi";
  switch(factoryResetStage.load()) {
    case FactoryResetStage::Working: text="正在恢復並重啟...";break;
    case FactoryResetStage::AudioError: text="無法停止音訊";break;
    case FactoryResetStage::StorageError: text="設定清除失敗";break;
    case FactoryResetStage::TaskError: text="工作建立失敗";break;
    default: break;
  }
  if (strcmp(lv_label_get_text(factoryResetStatusLabel),text))
    lv_label_set_text(factoryResetStatusLabel,text);
  otaControlEnabled(factoryResetButton,!otaBusy.load() && !otaRollbackDialogOpen() &&
      !otaPendingVerify.load() && rollbackBoot.load()!=RollbackBoot::Waiting);
}
