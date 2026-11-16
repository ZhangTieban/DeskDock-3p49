# SD 異常事件紀錄（0.2.13，始於 0.2.11 本機測試）

OTA 安裝此版後，插著 FAT32 SD 卡開機，程式會自行建立 `/logs`，不需先建資料夾或開啟選項。
開機序列埠會顯示 `[EVENTLOG] ready /logs/events-....jsonl`。原有 fonts、music、update 與設定保留。

每次開機建立獨立 JSONL 檔，一行一筆事件；單檔達 8 MiB 另開新檔，**不自動刪除或覆寫舊紀錄**。
可關機後取出 SD 卡，把整個 logs 資料夾複製到電腦。不要在運行中直接拔卡。
檔案為 UTF-8，可用文字編輯器開啟；也可執行：

```powershell
powershell -ExecutionPolicy Bypass -File tools/export_event_log.ps1 -LogDirectory E:\logs -OutCsv .\events.csv
```

每筆包含台灣時間（毫秒）、本次開機 uptime、開機識別碼、事件識別碼、嚴重程度、事件參數與說明，
以及內部可用 heap、最大連續區塊、歷史最低 heap、PSRAM、Wi-Fi RSSI、電台編號、播放狀態與緩衝位元組數。
時鐘尚未有效時 `time` 為 null，以 uptime 與之後的 TIME_SYNC 紀錄對照，不捏造日期。

| 事件 | 說明／a、b 參數 |
|---|---|
| BOOT / SHUTDOWN / TIME_SYNC | 重啟原因、正常關機、NTP 校時錨點；BOOT 的 a 為 ESP reset reason |
| HTTP_ERROR / HTTP_SLOW | HTTP/逾時錯誤或慢回應；含端點類型、耗時、大小與 errno，不寫完整網址 |
| HTTP_DEFERRED / HTTP_RESUMED / HTTP_CANCELLED | 等待音訊／記憶體預算、恢復下載、為新播放命令取消並排隊重試 |
| NIGHT_DEFERRED | 大型夜盤資料保留原本待電台停止後更新的行為，避免長下載壓低音訊緩衝 |
| HLS_PLAYLIST_ERROR / HLS_SEGMENT_ERROR | 播放清單或音訊分段失敗 |
| NETWORK_SLOW / HLS_PLAYLIST_SLOW | 分段下載慢；NETWORK_SLOW 的 a=毫秒、b=位元組 |
| RADIO_PLAYING / RADIO_CONNECT_FAILED / RADIO_STREAM_ENDED | 啟播、連線失敗、非主動切台的串流中斷 |
| AUDIO_BUFFER_LOW / AUDIO_STARVATION / AUDIO_BUFFER_RECOVERED | 緩衝不足、解碼器等待缺料、緩衝恢復 |
| WIFI_DISCONNECT_REASON / WIFI_WEAK | Wi-Fi 斷線原因碼／訊號偏弱 |
| ALLOC_FAILED / MEMORY_LOW / MEMORY_RECOVERED | 實際配置失敗／記憶體或連續區塊不足／恢復 |
| UI_SLOW | LVGL handler ≥100ms；a=handler 微秒、b=render 微秒 |
| SDK_LOG / OTA_ERROR | SDK 警告／錯誤、OTA 錯誤 |
| SD_RECOVERED / LOG_DROPPED | SD 曾寫入失敗後恢復／未入列事件數，並非假裝零遺失 |
| COREDUMP_SAVED | 開機後把有效 Flash coredump 複製到同目錄；可能來自較早的故障，不清除 Flash |
| HEALTH | 每 60 秒的狀態快照；a=記錄工作剩餘 stack、b=累計 SD 寫入失敗 |
| SELF_TEST | 人工觸發的紀錄自測，**不是真實異常** |

USB 串口 115200 可輸入 `LOG STATUS` 查看目前檔名、待寫入筆數與失敗計數；
`LOG LAST` 讀出目前檔案最後約 1.5 KiB（開頭可能是半行）；`LOG TEST` 寫入一筆明確標記的自測。
也可輸入 `LOG LAST events-完整檔名.jsonl` 讀取較早一個檔案的末段，用於重啟後核對紀錄仍存在；完整檔案請從 SD 卡複製。

實機回歸測試另可明確輸入 `LOG TEST PLAY 6`（播放好事989）、`LOG TEST WEATHER`（更新目前城市），
與 `LOG TEST UI`（首頁電台／天氣切換 10 步）。這些測試不改 NVS 設定，會留下 SELF_TEST_* 標記，
平時不會自行執行；UI 測試只切換顯示，不停止音訊。
`LOG TEST WEATHER 20` 與 `LOG TEST WEATHER 21` 分別只探測金門、連江，不更動畫面、城市設定或快取。
`LOG TEST STOP` 停止串流，供驗證停止後的快取與夜盤更新。

事件先放入 128 筆 PSRAM 佇列，背景工作逐筆寫入並檢查 write/fsync；不在觸控、音訊或 Wi-Fi 回呼裡等待 SD。
正常關機與 OTA 重新啟動前會限時等待寫入完成。SD 故障時不覆寫舊檔，待寫入事件先留在佇列，
恢復後重試，佇列溢位會計數。卡滿／卡故障／拔卡／突然斷電仍可能使尚未落盤的事件遺失；
不能用軟體保證故障媒體或瞬間斷電下的零遺失。Panic 當下不冒險操作 SD，改在下次開機匯出 SDK 已留下的 Flash coredump。

完整網址、簽章 token、密碼與 Authorization 不寫入事件說明；第三方 SDK 的相應訊息會標記遮蔽。
非阻塞 HTTPS 輪詢時，SDK transport 的正常 EAGAIN（socket 尚未就緒）不列為錯誤；只針對該請求工作過濾，真正的最終 HTTP 失敗／逾時仍保留 errno、狀態、耗時與大小。
電台播放時，天氣／小型行情可在緩衝與記憶體足夠時更新；遠端資料的 NVS 快取寫入延到電台停止後，避免 Flash erase/GC 卡住音訊與 UI。城市／Wi-Fi 等使用者設定仍沿用原本儲存流程。
單筆文字說明採有界 UTF-8（最多 111 bytes），超長 SDK 訊息截斷；事件碼、數值狀態與記憶體快照獨立保留，panic 的完整資料另由有效 Flash coredump 保存。
配置失敗回呼只做計數，避免在記憶體已不足時再次配置；`ALLOC_FAILED` 約每秒彙總一次，a=最後一筆要求的位元組數、b=這段期間的失敗次數。
