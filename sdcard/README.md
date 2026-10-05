# DeskDock SD 字型庫

將此目錄的 `fonts` 資料夾複製到 FAT32 SD 卡根目錄，形成
`/fonts/NotoSansTC-Regular.ttf` 與 `/fonts/NotoSansTC-Bold.ttf`。
插入 SD 卡後重新啟動 DeskDock。舊版的 `NotoSansTC-VF.ttf` 可刪除。

開機序列埠會顯示 `[FONT] SD mounted, 14/14 font fallbacks ready`。
所有 14 個現有字型的中文字形會優先從 SD 上對應的完整 Noto Sans TC
Regular (400) 或 Bold (700) TTF 即時讀取並快取。原有 D-DIN、Hanken、符號
等內建字形仍維持原樣。因此新增個股的中文名稱無須重建韌體字集，
粗體中文也會維持相同字重。SD 卡或檔案缺失時，仍會顯示內建字形；若 SD 字型未涵蓋某個
Unicode 字元，該字元仍可能缺字。開機後請勿拔出正在使用的 SD 卡。

SD 卡同時供 OTA 更新使用，請保留 `/update` 資料夾與足夠空間。
首次啟用此功能需要燒入包含 SD 字型支援的新韌體；只複製字型檔不會
使舊版韌體自動使用它們。

兩個字型由本機安裝的 `NotoSansTC-VF.ttf` 固定為 400 與 700 字重；
其授權為 SIL OFL 1.1，
授權文字位於 `fonts/NotoSansTC-OFL.txt`。
