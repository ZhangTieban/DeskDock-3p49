# DeskDock 3.49

Waveshare ESP32-S3-Touch-LCD-3.49 桌面資訊與網路電台韌體。首頁顯示時鐘、
Open-Meteo 天氣、臺北股市與網路電台，設定頁提供 Wi-Fi、天氣地點、
自選股、音效及顯示調整。

## 編譯

- Arduino ESP32 core 3.3.7
- LVGL 9.5.0、ESP8266Audio 2.4.1
- ESP32-S3、16 MB Flash、OPI PSRAM、USB CDC On Boot
- sketch 目錄中的 `partitions.csv` 會建立 4 MiB × 2 個 OTA app 分區。

Arduino 要求 sketch 目錄與主 `.ino` 同名，因此 clone 時指定目錄名：

```sh
git clone https://github.com/ZhangTieban/DeskDock-3p49.git DeskDock_3p49
cd DeskDock_3p49
arduino-cli compile \
  --fqbn 'esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app' \
  --build-path build-ci .
```

Windows 若編譯器無法處理含中文的專案路徑，可先用 `subst` 指向短 ASCII 路徑。

## SD 字型

將 [sdcard](sdcard/README.md) 中的 `fonts` 資料夾放到 FAT32 SD 卡根目錄，
或解壓 `sdcard-fonts.zip` 到卡片根目錄，然後重新啟動。韌體內建字形
仍可在缺卡時顯示；所有現有字型缺少的字形會從 SD 上的完整
Noto Sans TC 字型讀取，新個股名稱無須重新產生韌體字集。

## 室內溫溼度（AHT30）

外接 AHT30 模組接至 J5：VCC → 2 腳 3V3、GND → 3 腳、SCL → 4 腳
（GPIO48）、SDA → 5 腳（GPIO47）。模組的 I²C 上拉必須接 3.3V，
不可把 5V 訊號直接接到 ESP32-S3。韌體每 10 秒讀取一次，顯示於首頁
天氣區「室內」欄位；未接或讀取失敗時顯示 `--°C`、`--%`。

## OTA 更新

「設定 → 一般 → 檢查更新」會查詢公開
[GitHub Releases](https://github.com/ZhangTieban/DeskDock-3p49/releases)，
發現較新版後可按「安裝更新」。更新會先在 SD 卡暫存、驗證大小與
SHA-256，再寫入未使用的 OTA 分區。首次由舊單分區韌體遷移須 USB
刷入 bootloader、分區表與 app；後續才可用 OTA。完整發布與復原步驟
見 [OTA.md](OTA.md)。

BOOT（GPIO0）短按切換首頁與設定頁；GPIO16 電源鍵按住約 5 秒關機。
若按住 BOOT 時重置或上電，ESP32-S3 會進入下載模式。韌體更新使用
設定頁觸控鍵。

## 裝置資料

Wi-Fi 密碼與設定存在裝置 NVS，原始碼與 Release 不包含使用者憑證。
天氣與股票資料依各來源的可用性更新；網路電台串流網址可能隨電台變動。
