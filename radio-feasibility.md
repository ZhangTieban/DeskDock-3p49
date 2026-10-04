# 網路廣播電台可行性（2026-10-01）

V1 板沒有 FM 接收晶片；目前的「收音機」透過 Wi-Fi 播放網路串流。

| 電台 | 網路音源觀察 | 本板狀態 |
| --- | --- | --- |
| SomaFM Groove / Space | HTTP MP3 | 已上板播放 |
| 飛碟 UFO 92.1 | HTTP AAC | 已上板播放，使用者確認順暢 |
| 中廣 BCC 103.3 | HTTP AAC | 已上板播放，使用者確認順暢 |
| [Hit FM](https://www.hitfm.com.tw/newweb/onair.php) | 官網 API 回傳短效簽章 HTTPS HLS；播放清單指向 AAC 的 MPEG-TS 片段 | 實驗版上板連線失敗，已移除 |
| [KISS 99.9](https://www.kiss.com.tw/radio_hq.php) | 官網播放器使用短效簽章 HTTPS HLS | 尚未上板 |
| [Best Radio](http://www.bestradio.com.tw/) | 官網播放器使用短效簽章 HTTPS HLS | 尚未上板 |
| [POP 91.7](https://www.pop917.com/radio.aspx) | 官網 API 在指定 Referer 下回傳短效簽章 HTTPS HLS | 尚未上板 |

目前韌體的播放管線可直接讀 `http://` MP3 / AAC。HLS 不能只把 `.m3u8` 當成音訊 URL：需取得並更新簽章網址、解析主與媒體播放清單、處理片段與序號、從 MPEG-TS 擷取 AAC，還要在網路或解碼中斷後恢復。自訂電台欄位仍僅接受 HTTP MP3。

曾加入 ESP32-audioI2S HLS 原型，編譯大小約 2.89 MB（`huge_app` 分割區 91%）。實板點選 Hit 107.7 得到 `Hit API status=-1`、`HTTPClient error=connection refused`；同版的股票 HTTPS 也出現 `mbedtls_ssl_setup returned -0x7F00`。[Espressif 錯誤碼文件](https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/advanced-development/protocol/mbedtls-troubleshooting.html)將 `-0x7F00` 定義為 TLS 記憶體配置失敗。這次原型沒有通過播放測試，不能把編譯成功視為支援 HLS。

後續實作方向：

1. **板端原生 HLS**：在現有音訊工作中分階段加入 HTTPS、清單解析與 TS/AAC 擷取，限制同時連線數和記憶體峰值，並以股票 HTTPS 正常、持續播放、切台與恢復測試作為驗收。可不依賴其他裝置，但開發與驗證量較大。
2. **區網轉流服務**：由常開電腦或 Raspberry Pi 取得官網 HLS，轉成板子可讀的 HTTP MP3/AAC；板端改動較少，但收聽依賴該服務在線。

以上網址來自電台官網與當次播放器行為；簽章連結會過期，不應寫死在韌體中。
