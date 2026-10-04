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

## OTA 更新

「設定 → 一般 → 檢查更新」會查詢公開
[GitHub Releases](https://github.com/ZhangTieban/DeskDock-3p49/releases)，
發現較新版後可按「安裝更新」。更新會先在 SD 卡暫存、驗證大小與
SHA-256，再寫入未使用的 OTA 分區。首次由舊單分區韌體遷移須 USB
刷入 bootloader、分區表與 app；後續才可用 OTA。完整發布與復原步驟
見 [OTA.md](OTA.md)。

GPIO16 實體鍵短按切換設定、長按關機。韌體更新使用設定頁觸控鍵，
不更動實體鍵功能。

## 裝置資料

Wi-Fi 密碼與設定存在裝置 NVS，原始碼與 Release 不包含使用者憑證。
天氣與股票資料依各來源的可用性更新；網路電台串流網址可能隨電台變動。
