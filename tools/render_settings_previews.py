"""Render the three 640x172 DeskDock settings tabs for layout review."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "mockups"
NOTO = Path(r"C:\Windows\Fonts\NotoSansTC-VF.ttf")
FONT = ImageFont.truetype(str(NOTO), 16)
TITLE = ImageFont.truetype(str(NOTO), 20)
SMALL = ImageFont.truetype(str(NOTO), 14)
WHITE = "#f3f5f2"
MUTED = "#b3bdb7"
MINT = "#83d7bb"
CONTROL = "#202824"


def txt(draw, xy, value, font=FONT, color=WHITE):
    draw.text(xy, value, font=font, fill=color, anchor="lt")


def button(draw, xy, value):
    x, y, w, h = xy
    draw.rounded_rectangle((x, y, x + w, y + h), 5, fill=CONTROL, outline="#34433b")
    bbox = draw.textbbox((0, 0), value, font=FONT)
    width = bbox[2] - bbox[0]
    height = bbox[3] - bbox[1]
    txt(draw, (x + (w - width) / 2, y + (h - height) / 2 - 1), value)


def base(active):
    image = Image.new("RGB", (640, 172), "#080b0b")
    draw = ImageDraw.Draw(image)
    txt(draw, (18, 8), "設定", TITLE)
    button(draw, (575, 4, 54, 29), "‹")
    draw.rectangle((0, 37, 639, 70), fill="#121916")
    tabs = ("顯示與時間", "Wi-Fi", "天氣與股票")
    for i, name in enumerate(tabs):
        x = i * 213
        if i == active:
            draw.rectangle((x, 37, x + 212, 70), fill="#253a30")
            draw.rectangle((x + 24, 68, x + 188, 70), fill=MINT)
        width = draw.textlength(name, font=FONT)
        txt(draw, (x + (213 - width) / 2, 45), name, color=WHITE if i == active else MUTED)
    return image, draw


image, draw = base(0)
for x, title, value, fraction in ((18, "亮度", "80%", .8),
                                  (230, "省電", "10 分", .17),
                                  (442, "音量", "60%", .6)):
    txt(draw, (x, 78), title)
    txt(draw, (x + 112, 78), value, color=MUTED)
    draw.rounded_rectangle((x, 112, x + 180, 126), 7, fill="#29342d")
    draw.rounded_rectangle((x, 112, x + int(180 * fraction), 126), 7, fill=MINT)
    draw.ellipse((x + int(180 * fraction) - 9, 109, x + int(180 * fraction) + 9, 129), fill=WHITE)
for x, name in ((18, "日期與時間"), (230, "電池：自動"), (442, "測試音效")):
    button(draw, (x, 137, 180, 30), name)
image.save(OUT / "settings-general.png")

image, draw = base(1)
txt(draw, (18, 82), "未連線")
for x, w, name in ((197, 76, "搜尋"), (280, 82, "優先 ↑"),
                   (369, 76, "刪除"), (452, 76, "手動"), (535, 87, "連線")):
    button(draw, (x, 74, w, 30), name)
draw.rounded_rectangle((18, 110, 622, 170), 3, fill="#141b18")
txt(draw, (27, 116), "1. 家用網路  已記憶", color=WHITE)
draw.rectangle((18, 141, 622, 170), fill=CONTROL)
txt(draw, (27, 146), "   Guest  開放", color=MUTED)
image.save(OUT / "settings-wifi.png")

image, draw = base(2)
for x, title in ((18, "股票代號"), (230, "天氣地點"), (442, "氣象授權碼")):
    txt(draw, (x, 77), title)
for x, value in ((18, "2330"), (230, "臺北市  v"), (442, "輸入授權碼")):
    button(draw, (x, 103, 180, 51), value)
image.save(OUT / "settings-data.png")
