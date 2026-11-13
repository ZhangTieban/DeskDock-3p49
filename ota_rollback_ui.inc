static lv_obj_t *otaPanel = nullptr, *otaInstallButton = nullptr;
static lv_obj_t *otaPanelStatusLabel = nullptr, *rollbackStatusLabel = nullptr;
static lv_obj_t *rollbackButton = nullptr, *rollbackScanButton = nullptr, *otaBackButton = nullptr;
static lv_obj_t *rollbackBootLabel = nullptr, *rollbackDialog = nullptr;
static lv_obj_t *factoryDialog=nullptr, *factoryResetButton=nullptr, *factoryResetStatusLabel=nullptr;
static void factoryAsk(lv_event_t *);
static void factoryCloseDialog(lv_event_t *);

static bool otaRollbackDialogOpen() { return rollbackDialog != nullptr || factoryDialog != nullptr; }

static void rollbackCloseDialog(lv_event_t *) {
  if (!rollbackDialog) return;
  lv_obj_delete_async(rollbackDialog);
  rollbackDialog = nullptr;
}

static bool otaNavigateAway() {
  if (otaBusy.load()) return false;
  rollbackCloseDialog(nullptr);
  factoryCloseDialog(nullptr);
  return true;
}

static void rollbackStart(bool execute) {
  if (otaBusy.exchange(true)) return;
  rollbackStage.store(RollbackStage::Checking);
  if (xTaskCreatePinnedToCore(rollbackWorker,"rollback",12288,
                            execute ? (void *)1 : nullptr,1,nullptr,0) != pdPASS) {
    rollbackStage.store(RollbackStage::Task);
    otaBusy.store(false);
  }
}
static void rollbackScanClicked(lv_event_t *) {
  if (otaRollbackDialogOpen()) return;
  rollbackStart(false);
}

static void rollbackConfirmClicked(lv_event_t *) {
  if (!rollbackDialog || rollbackStage.load() != RollbackStage::Ready || otaBusy.load()) return;
  rollbackCloseDialog(nullptr);
  rollbackStart(true);
}

static void rollbackAsk(lv_event_t *) {
  if (otaRollbackDialogOpen() || otaBusy.load() || rollbackStage.load() != RollbackStage::Ready) return;
  rollbackRequested = rollbackTarget;
  rollbackDialog = lv_obj_create(otaPanel);
  lv_obj_set_pos(rollbackDialog,0,0); lv_obj_set_size(rollbackDialog,640,172);
  lv_obj_set_style_pad_all(rollbackDialog,0,0);
  lv_obj_set_style_border_width(rollbackDialog,0,0);
  lv_obj_set_style_bg_color(rollbackDialog,lv_color_hex(0x080B0B),0);
  lv_obj_remove_flag(rollbackDialog,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *box = lv_obj_create(rollbackDialog);
  lv_obj_set_pos(box,48,10); lv_obj_set_size(box,544,152);
  lv_obj_set_style_pad_all(box,0,0);
  lv_obj_set_style_radius(box,7,0);
  lv_obj_set_style_border_width(box,1,0);
  lv_obj_set_style_border_color(box,lv_color_hex(0x83D7BB),0);
  lv_obj_set_style_bg_color(box,lv_color_hex(0x121916),0);
  lv_obj_remove_flag(box,LV_OBJ_FLAG_SCROLLABLE);
  generalLabel(box,"確認退回前一版",12,7,520);
  char text[100];
  snprintf(text,sizeof(text),"目前 %s  →  前版 %s",DESKDOCK_VERSION,rollbackRequested.version);
  generalLabel(box,text,12,34,520);
  lv_obj_t *detail = generalLabel(box,rollbackRequested.bootAck ?
      "保留設定與 SD 資料。退版後重新啟動並核對版本。" :
      "保留設定與 SD。舊版無退版按鈕，可檢查更新升級。",12,61,520);
  lv_label_set_long_mode(detail,LV_LABEL_LONG_WRAP);
  generalButton(box,"取消",12,109,247,32,rollbackCloseDialog);
  generalButton(box,"確認退版並重啟",275,109,257,32,rollbackConfirmClicked);
}

static void otaPanelBack(lv_event_t *) {
  if (!otaNavigateAway()) return;
  lv_screen_load(settingsScreen);
  lastInputMs = millis();
}

static lv_obj_t *otaPanelCard(int x,int width) {
  lv_obj_t *card = lv_obj_create(otaPanel);
  lv_obj_set_pos(card,x,44); lv_obj_set_size(card,width,120);
  lv_obj_set_style_pad_all(card,0,0);
  lv_obj_set_style_radius(card,7,0);
  lv_obj_set_style_bg_color(card,lv_color_hex(0x121916),0);
  lv_obj_set_style_border_width(card,1,0);
  lv_obj_set_style_border_color(card,lv_color_hex(0x28372F),0);
  lv_obj_remove_flag(card,LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

static void otaOpenPanel(lv_event_t *) {
  if (!otaPanel) {
    otaPanel = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(otaPanel,lv_color_hex(0x080B0B),0);
    lv_obj_set_style_pad_all(otaPanel,0,0);
    lv_obj_set_style_border_width(otaPanel,0,0);
    lv_obj_remove_flag(otaPanel,LV_OBJ_FLAG_SCROLLABLE);
    generalLabel(otaPanel,"系統設定",12,7,150);
    char text[64]; snprintf(text,sizeof(text),"目前 %s",DESKDOCK_VERSION);
    generalLabel(otaPanel,text,168,7,160);
    rollbackBootLabel = label(otaPanel,"",328,11,208);
    lv_obj_set_style_text_font(rollbackBootLabel,&desk_font_10_bold,0);
    otaBackButton = generalButton(otaPanel,"返回一般",540,4,88,32,otaPanelBack);
    lv_obj_t *update = otaPanelCard(12,196);
    generalLabel(update,"韌體更新",10,5,176);
    otaPanelStatusLabel = generalLabel(update,"",10,33,176);
    lv_label_set_long_mode(otaPanelStatusLabel,LV_LABEL_LONG_WRAP);
    otaInstallButton = generalButton(update,"檢查更新",10,77,176,33,otaButtonClicked);
    lv_obj_t *previous = otaPanelCard(220,196);
    generalLabel(previous,"退回前一版",10,5,100);
    rollbackScanButton = generalButton(previous,"重查",120,2,66,27,rollbackScanClicked);
    rollbackStatusLabel = generalLabel(previous,"尚未檢查",10,33,176);
    lv_label_set_long_mode(rollbackStatusLabel,LV_LABEL_LONG_WRAP);
    rollbackButton = generalButton(previous,"退回前一版",10,77,176,33,rollbackAsk);
    lv_obj_t *initial = otaPanelCard(428,200);
    generalLabel(initial,"回到初始設定",10,5,180);
    factoryResetStatusLabel=generalLabel(initial,"清除設定與 Wi-Fi",10,33,180);
    lv_label_set_long_mode(factoryResetStatusLabel,LV_LABEL_LONG_WRAP);
    factoryResetButton=generalButton(initial,"回到初始設定",10,77,180,33,factoryAsk);
  }
  lv_screen_load(otaPanel);
  if (!otaBusy.load()) rollbackStart(false);
  refreshOtaUi();
  lastInputMs = millis();
}

static void otaControlEnabled(lv_obj_t *control,bool enabled) {
  if (!control) return;
  if (enabled) lv_obj_remove_state(control,LV_STATE_DISABLED);
  else lv_obj_add_state(control,LV_STATE_DISABLED);
}

#include "factory_reset_ui.inc"

static void refreshRollbackUi() {
  refreshFactoryResetUi();
  if (!rollbackStatusLabel) return;
  const RollbackStage stage = rollbackStage.load();
  const bool busy = otaBusy.load();
  char text[96] = {};
  switch (stage) {
    case RollbackStage::Unchecked: strcpy(text,"尚未檢查"); break;
    case RollbackStage::Checking: strcpy(text,"正在驗證前版..."); break;
    case RollbackStage::Ready:
      snprintf(text,sizeof(text),"可退回 %s%s",rollbackTarget.version,
               rollbackTarget.bootAck ? "" : "（舊版介面）"); break;
    case RollbackStage::NoSlot: strcpy(text,"沒有可用的 A/B 前版分區"); break;
    case RollbackStage::CurrentPending: strcpy(text,"目前版本尚在啟動確認，請稍後重查"); break;
    case RollbackStage::Unconfirmed: strcpy(text,"前版未完成啟動驗證或無有效前版"); break;
    case RollbackStage::Invalid: strcpy(text,"前版映像驗證失敗"); break;
    case RollbackStage::Unknown: strcpy(text,"無法辨識前版版本，已停用退版"); break;
    case RollbackStage::NotOlder: strcpy(text,"另一分區不是較舊版本"); break;
    case RollbackStage::Changed: strcpy(text,"前版已變更，請重新檢查"); break;
    case RollbackStage::Audio: strcpy(text,"無法停止音訊，未執行退版"); break;
    case RollbackStage::Storage: strcpy(text,"無法儲存退版紀錄，未執行退版"); break;
    case RollbackStage::Select: strcpy(text,"開機分區切換失敗，未重新啟動"); break;
    case RollbackStage::Task: strcpy(text,"無法建立退版工作，請重查"); break;
    case RollbackStage::Rebooting: strcpy(text,"前版已選定，正在重新啟動..."); break;
  }
  if (strcmp(lv_label_get_text(rollbackStatusLabel),text)) lv_label_set_text(rollbackStatusLabel,text);
  otaControlEnabled(rollbackButton,!busy && !otaRollbackDialogOpen() && stage == RollbackStage::Ready);
  otaControlEnabled(rollbackScanButton,!busy && !otaRollbackDialogOpen());
  otaControlEnabled(otaInstallButton,!busy && !otaRollbackDialogOpen());
  otaControlEnabled(otaBackButton,!busy);
  switch (rollbackBoot.load()) {
    case RollbackBoot::Waiting: strcpy(text,"退版啟動確認中"); break;
    case RollbackBoot::Confirmed: strcpy(text,"退版完成，版本已確認"); break;
    case RollbackBoot::Failed: strcpy(text,"退版未完成，已回到目前版本"); break;
    case RollbackBoot::Legacy:
      snprintf(text,sizeof(text),"上次退版目標 %s",rollbackBootRecord.target.version); break;
    default: text[0] = '\0'; break;
  }
  if (strcmp(lv_label_get_text(rollbackBootLabel),text)) lv_label_set_text(rollbackBootLabel,text);
}
