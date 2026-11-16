# SD 異常事件紀錄（0.2.14，始於 0.2.11 本機測試）

OTA 安裝此版後，插著 FAT32 SD 卡開機，程式會自行建立 `/logs`，不需先建資料夾或開啟選項。

電池顯示設定記錄 `BATTERY_MODE`（a=0依電壓、1已安裝、2手動隱藏；b=量測毫伏），開機及切換時記錄。舊 `batt_mode2=2` 的隱藏預設被改為依電壓判斷時，記錄 `BATTERY_MODE_MIGRATED`（a=舊模式、b=新模式）。新選擇使用 `batt_mode3` 保存，不再把舊隱藏預設視為手動選擇。
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

事件先放入 1024 筆 PSRAM 佇列（容納 256 首長檔名清單），背景工作逐筆寫入並檢查 write/fsync；不在觸控、音訊或 Wi-Fi 回呼裡等待 SD。
正常關機與 OTA 重新啟動前會限時等待寫入完成。SD 故障時不覆寫舊檔，待寫入事件先留在佇列，
恢復後重試，佇列溢位會計數。卡滿／卡故障／拔卡／突然斷電仍可能使尚未落盤的事件遺失；
不能用軟體保證故障媒體或瞬間斷電下的零遺失。Panic 當下不冒險操作 SD，改在下次開機匯出 SDK 已留下的 Flash coredump。

完整網址、簽章 token、密碼與 Authorization 不寫入事件說明；第三方 SDK 的相應訊息會標記遮蔽。
非阻塞 HTTPS 輪詢時，SDK transport 的正常 EAGAIN（socket 尚未就緒）不列為錯誤；只針對該請求工作過濾，真正的最終 HTTP 失敗／逾時仍保留 errno、狀態、耗時與大小。
電台播放時，天氣／小型行情可在緩衝與記憶體足夠時更新；遠端資料的 NVS 快取寫入延到電台停止後，避免 Flash erase/GC 卡住音訊與 UI。城市／Wi-Fi 等使用者設定仍沿用原本儲存流程。
單筆文字說明採有界 UTF-8（最多 111 bytes），超長 SDK 訊息截斷；事件碼、數值狀態與記憶體快照獨立保留，panic 的完整資料另由有效 Flash coredump 保存。
配置失敗回呼只做計數，避免在記憶體已不足時再次配置；`ALLOC_FAILED` 約每秒彙總一次，a=最後一筆要求的位元組數、b=這段期間的失敗次數。

## MP3 清單與播放紀錄（0.2.14）

開機自動掃描一次 `/music` 第一層，之後沿用原本 MP3 畫面的重新掃描。最多收錄 256 首、檢查 4096 個目錄項目，不遞迴子資料夾；超過 191 bytes 的完整路徑仍略過。新舊兩份掃描清單均放 PSRAM，配置失敗保留原清單。LOG 的曲數是這次收錄的可播放清單，不能把達到上限時的數字當成整張 SD 的歌曲總數。

| 事件 | 參數／用途 |
|---|---|
| MP3_SCAN_BEGIN / MP3_SCAN_END | a=掃描識別碼；END 的 b=收錄曲數，detail 含耗時、已檢查項目及長路徑略過數 |
| MP3_SCAN_FOUND / MP3_SCAN_EMPTY / MP3_SCAN_FAILED | 找到歌曲、清單為空、SD 或 /music 不可用；a=掃描識別碼、b=曲數 |
| MP3_SCAN_LIMIT / MP3_PATH_SKIPPED | 達到既有掃描限制、路徑過長略過；不宣稱後續檔案已掃完 |
| MP3_LIST_BEGIN / MP3_LIST_END | a=清單識別碼、b=收錄曲數；兩者之間為依播放器順序列出的曲目 |
| MP3_TRACK_PATH | a=曲目編號（從1開始）、b=文字片段編號（從1開始），detail=完整路徑的片段 |
| MP3_OPEN_BEGIN / MP3_PLAY_PATH / MP3_PLAYING | 開啟前、播放路徑及啟播成功；路徑分段方式同 TRACK_PATH，PLAYING 含檔案大小與循環模式 |
| MP3_PAUSED / MP3_RESUMED / MP3_STOP / MP3_CLOSED | 暫停、續播、停止／換來源及資源釋放；附上各階段記憶體快照 |
| MP3_EOF / MP3_CANCELLED / MP3_DECODE_FAILED | 自然結束、新命令取消、非預期中斷；detail 含位元組位置、大小、SD/codec/decoder 狀態 |
| MP3_OPEN_FAILED / MP3_PAUSE_FAILED / MP3_RESUME_FAILED | 開檔、解碼、I2S 或暫停／續播轉換失敗；新命令取消開啟另記 INFO 的 MP3_OPEN_CANCELLED |
| MP3_BUFFER_LOW / MP3_BUFFER_RECOVERED | 檔案尚有資料時緩衝不足／恢復；排除正常檔尾與暫停狀態 |
| MP3_STATUS | 播放或暫停期間每30秒一筆；a=曲目編號、b=音訊工作最低剩餘堆疊 bytes，detail 含位置、大小、循環模式 |
| MP3_REPEAT_MODE | a=0不循環、1單曲、2清單 |

長中文／日文檔名不直接截斷，使用多筆 PATH 事件保存。將同一清單與同一曲目依 b 排序後串接 detail，即為完整檔名／路徑。一般事件與 SDK 的文字長度限制維持不變。

每筆 JSON 額外包含 `mp3_state`（0停止、1開啟中、2播放、3暫停、4失敗、5結束）、`mp3_track`（1起算，0無曲目）、`mp3_tracks`（清單總數）、`mp3_buffer`（位元組）與 `mp3_progress`（0–1000）。本修訂版 progress 改以播放毫秒／曲長計算，早期 0.2.14 的 progress 為位元組比例；以有無 `mp3_elapsed_ms` 區分。原有記憶體欄位繼續保留，CSV 也帶出新欄位；舊檔缺少的新欄位留白。

新增欄位：

| 欄位 | 意義 |
|---|---|
| mp3_elapsed_ms | 已成功提交 I2S 的 PCM 音訊毫秒，非開檔後經過時間；不含暫停、等資料或收尾靜音，與實際喇叭間可能差 DMA 佇列延遲 |
| mp3_duration_ms / mp3_remaining_ms | 曲長／剩餘毫秒；無可靠曲長時為 null，畫面顯示 --:-- |
| mp3_duration_estimated | true=CBR 依檔案大小／位元率估算（畫面「約」）；false=Xing/Info/VBRI 宣告取樣長度或自然結束後的實際長度；不保證宣告標頭一定正確 |
| cpu0_permille / cpu1_permille | 0–1000，除以10即各核心百分比；根據 ESP-IDF 啟用的 idle runtime counter 差值，每約1秒取樣；-1=尚無樣本，為工作排程負載近似值，無法精確拆分中斷時間 |
| battery_mv | 沿用板上 ADC 分壓量測的電池毫伏，每15秒更新；0=尚未量測，非獨立校準電表 |
| net_rx_Bps | 最近取樣窗內的應用下載 bytes/s，含 ICY 音訊（不含 ICY metadata）、HLS HTTP payload、天氣／金融／OTA 回應；不是 Wi-Fi 連線速率或網路最大頻寬，沒有下載可為0 |
| net_rx_bytes | 開機後已計量下載 bytes 累計，uint32 約4GiB繞回；不含 TCP/TLS/MAC 標頭、DNS、NTP 或上傳 |
| metrics_window_ms | CPU／流量的實際取樣窗長度；SD 忙碌時可能超過1秒 |

曲長只在選曲開檔時檢查 ID3v2 後最多64KiB，排除 ID3v1。避免為求倒數而掃完整首或全部256首、干擾音訊。無索引 VBR 無法預知剩餘時間；不把位元組比值冒充播放秒數。

USB `LOG MP3 LIST` 會把目前快取清單寫入 SD LOG，不重新掃描或中斷播放。明確測試指令：`LOG TEST MP3 SCAN`、`LOG TEST MP3 PLAY 1`、`LOG TEST MP3 PAUSE`、`LOG TEST MP3 RESUME`、`LOG TEST MP3 STOP`；測試會留下 SELF_TEST_MP3 標記。播放中不接受重新掃描，保留既有保護。
