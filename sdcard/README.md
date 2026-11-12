# DeskDock SD 字型庫

將此目錄的 `fonts` 資料夾複製到 FAT32 SD 卡根目錄，形成
`/fonts/NotoSansTC-Regular.ttf`、`/fonts/NotoSansTC-Bold.ttf`，以及
`/fonts/Jigmo.ttf`、`/fonts/Jigmo2.ttf`、`/fonts/Jigmo3.ttf`。
插入 SD 卡後重新啟動 DeskDock。舊版的 `NotoSansTC-VF.ttf` 可刪除。

完整安裝時，開機序列埠會顯示 `[FONT] SD mounted, 20/20 font fallbacks ready`。
五種會顯示中文的文字大小先使用韌體內建的常用字形；缺字時才從 SD 上
對應的 Noto Sans TC Regular (400) 或 Bold (700) TTF 讀取並快取，
再依序使用 Jigmo、Jigmo2、Jigmo3 補足罕用漢字。
數字、圖示字型沿用原本的內建字形與文字字型備援。因此新增個股的
中文名稱無須重建韌體字集；Noto 未收錄的罕字使用 Jigmo 明體單一字重。
已以實際 cmap 驗證 Unicode 17 中文基本區與擴充 A–J 共 101,984 字無缺字，
另包含 CJK 相容漢字。逐檔 SHA256 與覆蓋結果見 `fonts/coverage.json`。
這裡指 Unicode 已編碼的字元；自造字與 IVS 字形選擇序列不在 LVGL tiny_ttf 的支援範圍。
字型資料約 87 MB，ZIP 約 44 MB。SD 卡或檔案缺失時，
仍會顯示內建字形；若 SD 字型未涵蓋某個
Unicode 字元，該字元仍可能缺字。開機後請勿拔出正在使用的 SD 卡。

SD 卡同時供 OTA 更新使用，請保留 `/update` 資料夾與足夠空間。

## 本機 MP3

在 FAT32 SD 卡根目錄建立 `/music`，將 `.mp3` 檔案直接放入該資料夾。
進入設定頁的「MP3」會自動掃描資料夾的前 256 個項目，依檔名排序並顯示最多 32 首。播放或暫停時，返回此頁會保留目前曲目。
可使用上一首、播放／暫停及下一首，音量沿用「一般」設定。循環按鈕依序切換「不循環 → 單曲循環 → 清單循環」。
MP3 採用 64 KiB PSRAM 背景預讀，啟播前先緩衝最多 32 KiB，減少字型讀取造成的 SD 存取抖動。
電台與 MP3 共用具峰值限制的動態音量補償，增益範圍為 0.25–2 倍；
不同錄音內容仍可能有主觀響度差異，效果須以上板試聽確認。
本機 MP3 與網路電台共用音訊輸出，開始播放其中一種時會停止另一種。
播放中請勿拔出 SD 卡；OTA 檢查／安裝會先停止音訊再使用 SD 卡。
首次啟用此功能需要燒入包含 SD 字型支援的新韌體；只複製字型檔不會
使舊版韌體自動使用它們。

兩個字型由本機安裝的 `NotoSansTC-VF.ttf` 固定為 400 與 700 字重；
其授權為 SIL OFL 1.1，
授權文字位於 `fonts/NotoSansTC-OFL.txt`。

Jigmo 2025-09-12 取自官方 https://kamichikoichi.github.io/jigmo/，授權為 CC0 1.0，
見 `fonts/LICENSE-Jigmo.txt`。原始 ZIP 的 SHA256 為
`5744c7386d129475d87607ca66d043c8793c65448adeaedc921b6931890e5d0b`；
官方頁面列出的 40 位雜湊實為 SHA1，已比對相符。

## 首頁角色時段

睡覺：22:00–翌日 07:00、13:00–14:00。
吃飯：08:00–08:10、18:00–18:30。
女孩遛狗：17:30–18:00。所有結束時間均不包含在事件時段內。
其他時間只輪流走路、奔跑、坐著、玩球、接飛盤、拔河、丟球取回及摸摸陪坐，沒有隨機睡覺或吃飯。
操作首頁會短暫顯示愛心，不會中斷既定時段事件。
