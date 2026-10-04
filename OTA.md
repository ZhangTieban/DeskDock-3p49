# DeskDock OTA 發布與復原

## 裝置端

- 目標板：Waveshare ESP32-S3-Touch-LCD-3.49，16 MB Flash、OPI PSRAM。
- 更新入口：「設定 → 一般 → 檢查更新」。找到較新版後，按「安裝更新」。
- 更新會暫停網路電台，透過 HTTPS 取得公開 GitHub Release 的
  `version.json`，把 `firmware.bin` 寫入 FAT32 SD 卡
  `/update/firmware.tmp`。下載大小與 SHA-256 均符合後才重新命名，
  並逐塊寫入未使用的 OTA 分區。失敗時不改開機分區。
- 更新成功會自動重啟。新韌體執行滿 10 秒且 UI、電台及網路工作已建立後，
  才標記為有效；重啟或當機發生在此之前時，啟用 bootloader rollback。
- 沒有 SD 卡或 Wi-Fi 時，更新按鈕會顯示錯誤，現有功能繼續運作。
- SD 使用 1-bit SDMMC：CLK GPIO41、CMD GPIO39、D0 GPIO40。無自動格式化。
- GPIO16 實體鍵目前用於短按切換設定、長按關機；更新改用觸控按鈕。

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
