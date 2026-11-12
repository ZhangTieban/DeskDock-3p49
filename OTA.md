# DeskDock OTA 發布與復原

## 裝置端

- 目標板：Waveshare ESP32-S3-Touch-LCD-3.49，16 MB Flash、OPI PSRAM。
- v0.2.04 起更新入口：「設定 → 一般 → 更新/退版 → 檢查更新」。找到較新版後，按「安裝更新」。v0.2.03 及以前維持一般頁的「檢查更新」。
- 更新會暫停網路電台，透過 HTTPS 取得公開 GitHub Release 的
  `version.json`，把 `firmware.bin` 寫入 FAT32 SD 卡
  `/update/firmware.tmp`。下載大小與 SHA-256 均符合後才重新命名，
  並逐塊寫入未使用的 OTA 分區。失敗時不改開機分區。
- 更新成功會自動重啟。新韌體執行滿 10 秒且 UI、電台及網路工作已建立後，
  才標記為有效；重啟或當機發生在此之前時，啟用 bootloader rollback。
- 沒有 SD 卡或 Wi-Fi 時，更新按鈕會顯示錯誤，現有功能繼續運作。
- SD 使用 1-bit SDMMC：CLK GPIO41、CMD GPIO39、D0 GPIO40。無自動格式化。
- GPIO16 實體鍵目前用於短按切換設定、長按關機；更新改用觸控按鈕。

## 手動退回前一版（v0.2.04）

「更新/退版」頁同時顯示目前版本、前版是否可用與停用原因。退版不需要 Wi-Fi 或 SD 卡；只切換到裝置另一個已驗證、已完成啟動確認的 OTA 分區，不下載或覆寫 app，也不清除設定與 SD 資料。

按「退回前一版」先顯示具體目標與確認畫面。確認後再次核對版本、分區地址與映像 SHA-256，停止音訊並將待存設定寫入 NVS，保存本次退版請求後才呼叫 ESP-IDF 的開機分區切換。忙碌期間停用重複操作與離開畫面；驗證或儲存失敗時不主動重啟。失敗後 MP3 可手動恢復播放。

僅接受兩個標準 OTA app 分區，且目標必須為 `ESP_OTA_IMG_VALID`、版本確實較舊並通過 SDK 退版檢查。缺少分區、前版未確認、映像損壞、未知身分或另一分區版本較新時停用退版。剛開機尚未完成 10 秒首次確認時，稍候按「重查前版」。第一次 USB 安裝後沒有有效前版狀態時，不能保證可退版。

舊 ESP-IDF app descriptor 的 `version` 是共用編譯資訊，不能當作 DeskDock 版號。已驗證的官方 0.1.27、0.2.02、0.2.03 透過完整映像的驗證 digest 對照；後續支援此功能的版本在映像內帶有板型、退版協定與實際版號標記。未知的本機自編舊映像不猜版號，按鈕會停用。舊版對照由 `tools/generate_ota_legacy.py` 依 Release manifest 與下載映像產生。

支援此功能的目標韌體會在重啟後核對退版請求中的地址、digest 與實際運行版號，通過首次啟動確認後才顯示完成；首次啟動失敗回到原版時顯示未完成。0.2.03 及以前的舊程式沒有這些新邏輯，退回後需由一般頁的版本資訊人工確認，也不會出現新的退版按鈕。公開新版可透過舊版的「檢查更新」再升級；尚未發布的開發版則需 USB 重新安裝。

實際切換、斷電恢復與硬體驗收需另做裝置測試，介面模擬與編譯不代表實機已通過。每次發布後保留既有 Release；可否退版仍以裝置另一個分區的有效映像為準。

## v0.2.05 更新與音訊修正

- 純粹檢查新版不再中止 MP3；實際安裝與退版仍先停止音訊。
- SD 字型、MP3 與 OTA 共用同一個掛載入口，檔案額度統一為 40。
- 打包時核對 ESP32-S3、映像結構、內建 SHA 與 DeskDock 實際版號；裝置端寫入前先核對板型／版號身分，寫入後再次核對才切換開機分區。0.2.04 起的身分標記繼續沿用。
- Wi-Fi 斷線鎖定當次電台串流，主迴圈獨立靜音，音訊工作先關閉 I2S 再等待網路清理；重新連線後使用既有最多三次的重試流程建立新串流，MP3 不受 Wi-Fi 斷線影響。
- ESP8266Audio 2.4.1 必須套用 `tools/patch_audio_library.py`；CI 與本機共用修補，處理 MP3 同步碼越界、跨區塊同步碼與檔尾。正常 MP3 結束做最長 750 ms 的 DMA 排空，手動停止可中斷等待。

尚未完成實機斷線錄音、異常 SD 卡與分區切換驗收。SD driver 若永久阻塞，producer 的安全清理仍可能等待；本次先確保關閉輸出，不強制釋放工作仍在使用的記憶體。LCD QSPI 傳輸協定保持不變，未採用尚未實測的局部視窗傳輸。

## 首次安裝

現有單一 app 分區無法單靠 app OTA 改成 A/B。**首次須用 USB 同時刷入
bootloader、`partitions.csv` 與 app。**本 sketch 的自訂分區保留 NVS
原位，app0/app1 各 4 MiB。刷入前請備份裝置資料並確認 16 MB Flash；
不要勾選抹除整片 Flash。日後一般更新只發布 app `.bin`，絕不能把
merged/full-flash `.bin` 當作 `firmware.bin`。

首次換分區後應以序列埠確認運行分區與 rollback 狀態，並做一次實際 OTA
及中斷測試。編譯成功本身不能證明 SD、GitHub 跳轉、斷電恢復可用。

## 發布

1. 修改 `ota_config.h` 的 `DESKDOCK_VERSION`，使用遞增的
   `MAJOR.MINOR.PATCH`。Git tag 必須是對應的 `vMAJOR.MINOR.PATCH`。
2. 編譯 sketch。CI 或本機輸出是 `DeskDock_3p49.ino.bin`。
3. 執行：
   `python tools/package_release.py --bin <app.bin> --out release_assets`
4. 在 `ZhangTieban/DeskDock-3p49` 建立非 prerelease 的 Release，tag
   為 `vMAJOR.MINOR.PATCH`，附上 `release_assets/firmware.bin` 與
   `release_assets/version.json`。兩個檔案必須來自同一次編譯。

manifest 格式：

```json
{
  "board": "DeskDock-3p49",
  "version": "0.1.0",
  "file": "firmware.bin",
  "size": 2063776,
  "sha256": "64 個小寫十六進位字元"
}
```

裝置從 `releases/latest/download/version.json` 取得 manifest，再用版本
組出固定 tag 的下載 URL，避免檢查與下載之間 latest 指向另一版。
SHA-256 用於傳輸完整性；若要防止 Repo 帳號遭入侵後發布惡意韌體，
仍須加入離線簽章與裝置端公鑰驗證。
