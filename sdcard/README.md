# DeskDock SD 字型庫

將此目錄的 `fonts` 資料夾複製到 FAT32 SD 卡根目錄，形成
`/fonts/NotoSansTC-VF.ttf`。插入 SD 卡後重新啟動 DeskDock。

開機序列埠會顯示 `[FONT] SD mounted, 14/14 font fallbacks ready`。
所有 14 個現有字型會先使用韌體內建的常用字形，缺少的字形則從 SD
上的完整 Noto Sans TC TTF 即時讀取並快取。原有 D-DIN、Hanken、符號
等內建字形仍維持原樣；它們缺字時也會透過相同 SD 字型補上。因此新增個股的中文名稱無須重建韌體
字集。SD 卡或檔案缺失時，仍會顯示內建字形；若 SD 字型未涵蓋某個
Unicode 字元，該字元仍可能缺字。開機後請勿拔出正在使用的 SD 卡。

SD 卡同時供 OTA 更新使用，請保留 `/update` 資料夾與足夠空間。
首次啟用此功能需要燒入包含 SD 字型支援的新韌體；只複製字型檔不會
使舊版韌體自動使用它們。

`NotoSansTC-VF.ttf` 來自本機安裝的 Noto Sans TC；其授權為 SIL OFL 1.1，
授權文字位於 `fonts/NotoSansTC-OFL.txt`。
