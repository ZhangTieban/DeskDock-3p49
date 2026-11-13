# DeskDock 3.49

Waveshare ESP32-S3-Touch-LCD-3.49 桌面資訊與網路電台韌體。首頁顯示時鐘、
Open-Meteo 天氣、臺北股市與網路電台，設定頁提供 Wi-Fi、天氣地點、
自選股、音量及顯示調整。

## 編譯

- Arduino ESP32 core 3.3.7
- LVGL 9.5.0、ESP8266Audio 2.4.1
- ESP32-S3、16 MB Flash、OPI PSRAM、USB CDC On Boot
- sketch 目錄中的 `partitions.csv` 會建立 4 MiB × 2 個 OTA app 分區。

Arduino 要求 sketch 目錄與主 `.ino` 同名，因此 clone 時指定目錄名：

```sh
git clone https://github.com/ZhangTieban/DeskDock-3p49.git DeskDock_3p49
cd DeskDock_3p49
# 安裝上述固定版本的 ESP8266Audio 後，先套用與 CI 相同的邊界修正：
python3 tools/patch_audio_library.py --library-dir "$(arduino-cli config get directories.user)/libraries/ESP8266Audio"
python3 tools/patch_audio_stability.py --library-dir "$(arduino-cli config get directories.user)/libraries/ESP8266Audio"
python3 tools/patch_esp32_core.py --core-dir "$(arduino-cli config get directories.data)/packages/esp32/hardware/esp32/3.3.7"
arduino-cli compile \
  --fqbn 'esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app' \
  --build-path build-ci .
```

Windows 若編譯器無法處理含中文的專案路徑，可先用 `subst` 指向短 ASCII 路徑。

## SD 字型

將 [sdcard](sdcard/README.md) 中的 `fonts` 資料夾放到 FAT32 SD 卡根目錄，
或解壓 `sdcard-fonts.zip` 到卡片根目錄，然後重新啟動。韌體內建字形
仍可在缺卡時顯示；常用中文字形由韌體快速繪製，缺字時從 SD 上的完整
Noto Sans TC Regular／Bold 字型讀取，新個股名稱無須重新產生韌體字集。
從舊版升級時，請同時更新 SD 卡的字型資料夾；單獨 OTA 韌體不會寫入 SD 字型。

## 股票報價

臺灣上市股票於週一至週五 09:00–13:40 每 10 分鐘批次讀取證交所即時行情，
顯示最後成交價、相對昨收漲跌、當日高低價、成交量（張）與報價時間。
只接受當日日期的報價，休市或來源未提供當日成交價時保留既有資料；
盤後資料仍由原本的收盤行情來源更新。

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

## 地區與初始設定（0.2.06）

天氣頁以「地區數量」設定 1～6 個地點，預設沿用三個地點；啟用地點才查詢並參與首頁輪播。縮減數量保留未啟用地點的選擇，方便再增加。原本未連動首頁的主／副／三選單已移除，首頁天氣資訊維持原排版。

「一般 → 更新/退版 → 系統設定」新增「回到初始設定」。確認後停止音訊，清除 Wi-Fi、天氣、股票、亮度、音量、時間格式與輪播設定，再重新啟動。保留目前 app、前版分區、SD 字庫、MP3 與硬體 RTC；不格式化 SD、不抹除整片 Flash。OTA 或退版忙碌、首次啟動驗證中不允許重設。

Arduino ESP32 3.3.7 的 DNS 快取清除會同步呼叫待完成的 DNS/SNTP 回呼；電台首次解析網址可能在錯誤執行緒觸發 TCP/IP 核心鎖斷言。`tools/patch_esp32_core.py` 只為快取清除加上核心鎖，保留原有鎖所有權，阻塞式連線仍在鎖外。本機與發布 CI 使用相同固定版本修補；不關閉 SDK 執行緒檢查。

## 觸控與音訊穩定性（0.2.07）

觸控改由獨立工作每 10 ms 取樣，畫面工作只讀取已過濾的事件。按下與放開皆須連續有效樣本；短暫讀取失敗保留拖曳，逾時、過舊或佇列溢位則取消操作。換頁及重設／退版確認視窗會清除待處理觸控，避免舊點擊落到新控制項。觸控 I2C 逾時有界限，連續逾時會嘗試復原專用匯流排；序列埠每五秒回報讀取、錯誤、復原及溢位累計。

AAC 解碼器大型狀態移至 PSRAM；外部記憶體配置失敗時停止啟動，不回退擠占內部 RAM。I2S 配置與取樣率切換失敗改為正常回報及清理，避免配置失敗直接斷言重啟。好事電台的播放清單暫時性失敗最多重試一次，初始預載等待延長至 45 秒；音訊緩衝足夠時，在下一次下載前釋放 HTTP 閘門與 TLS 記憶體。既有 Wi-Fi 斷線停止及 DNS 修正保留。

固定版本修補、觸控過濾與 AAC 配置失敗路徑均有測試；操作手感、長時間串流與弱訊號復原仍需實機驗收。介面位置、音量規則及 SD 字庫包不變。

## 好事電台與時間設定（0.2.08）

好事 989、903、935 與港都 983 改用官方 CDN 支援的 HTTP HLS 串流，保留官方動態簽章與到期更新，避免建立 TLS 連線時因內部記憶體配置失敗而無法預載。四台官方播放清單與音訊分段下載均已驗證；989 經使用者確認可播放，903 與 983 的裝置紀錄已進入播放狀態，935 尚待實機確認。

一般設定移除手動日期／時間輸入，改為「時間格式」卡片，保留 12／24 小時切換；既有 Wi-Fi 自動校時與 RTC 行為維持不變。

## OTA TLS 記憶體修正（0.2.09）

修正更新檢查／下載從 GitHub 跳轉至 Release CDN 時，因 TLS 內部記憶體配置失敗而中止。只將 OTA 工作中 1 KiB 以上的 mbedTLS 配置移到 PSRAM；其他工作維持固定 SDK 的原有配置方式，PSRAM 配置失敗時正常回報，不回退占用內部 RAM。HTTPS 憑證、檔案大小、SHA-256、板型／版號與寫入後驗證均保留。

CI 包含其他工作配置隔離、PSRAM 不足及配置大小溢位測試。若舊版已因 TLS 記憶體不足而無法取得更新資訊，需先透過 USB 補入修正版，再使用一般 OTA 更新；不清除 NVS 或 SD 資料。

## 城市按鈕與操作音效（0.2.10）

天氣地區改為點選地區後開啟 22 縣市按鈕面板，目前城市以顏色標示；選取後立即套用並返回，返回鍵不更動城市。保留 1～6 個地區、順序、設定儲存及更新規則。首頁時間下移 5 像素，一般設定的版本號與「系統與網路」標題對齊。

MP3 暫停會釋放 I2S 輸出給操作音效，續播時重建原取樣率的輸出，保留解碼進度。操作音效僅配置 TX，停止／結束後確認 I2S 與 Codec 均恢復才啟用；恢復失敗會保留重試狀態。播放或連線中仍抑制操作音效，暫停、停止與播放結束後可恢復。
