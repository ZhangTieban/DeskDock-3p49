"""Render 640x172 clock-font comparisons for the DeskDock home screen."""

from math import cos, pi, sin
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "mockups"
FONTS = Path(r"C:\Windows\Fonts")
NOTO = FONTS / "NotoSansTC-VF.ttf"
WHITE = "#f7faf8"
MUTED = "#aab5b0"

OPTIONS = (
    ("font-a-noto.png", NOTO, NOTO, 86),
    ("font-b-segoe-light.png", FONTS / "segoeuil.ttf", FONTS / "segoeui.ttf", 94),
    ("font-c-d-din.png", ROOT / "assets/fonts/D-DIN-Bold.ttf", ROOT / "assets/fonts/D-DIN.ttf", 86),
)


def render(filename, clock_path, date_path, clock_size):
    image = Image.new("RGB", (640, 172), "#080b0b")
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((5, 5, 634, 166), radius=22, outline="#e8504e", width=5)

    date_font = ImageFont.truetype(str(date_path), 17)
    chinese_font = ImageFont.truetype(str(NOTO), 16)
    date_text = "2026 / 09 / 24"
    draw.text((26, 18), date_text, font=date_font, fill="#d8dfdc", anchor="lt")
    weekday_x = 26 + round(draw.textlength(date_text, font=date_font)) + 15
    draw.text((weekday_x, 19), "週四", font=chinese_font, fill="#8e9b95", anchor="lt")

    for box in ((448, 19, 474, 43), (453, 25, 469, 42)):
        draw.arc(box, 205, 335, fill="#dce9e1", width=2)
    draw.ellipse((460, 35, 463, 38), fill="#dce9e1")
    draw.text((491, 19), "無電池", font=chinese_font, fill="#f27873", anchor="lt")

    cx, cy = 606, 29
    draw.ellipse((cx - 12, cy - 12, cx + 12, cy + 12), outline="#dce9e1", width=3)
    draw.ellipse((cx - 4, cy - 4, cx + 4, cy + 4), outline="#dce9e1", width=2)
    for i in range(8):
        angle = i * pi / 4
        draw.line((cx + 12 * cos(angle), cy + 12 * sin(angle),
                   cx + 17 * cos(angle), cy + 17 * sin(angle)), fill="#dce9e1", width=3)

    clock_font = ImageFont.truetype(str(clock_path), clock_size)
    clock = "15:09"
    clock_width = draw.textlength(clock, font=clock_font)
    draw.text(((640 - clock_width) / 2, 46), clock, font=clock_font, fill=WHITE, anchor="lt")

    draw.line((27, 129, 613, 129), fill="#32403b", width=1)
    draw.text((28, 136), "天氣", font=chinese_font, fill="#83d7bb", anchor="lt")
    draw.text((83, 136), "臺北市  27–31°C  降雨 20%", font=chinese_font, fill=WHITE, anchor="lt")
    pager_font = ImageFont.truetype(str(date_path), 14)
    draw.text((572, 138), "1/4", font=pager_font, fill=MUTED, anchor="lt")
    image.save(OUT / filename)


if __name__ == "__main__":
    for option in OPTIONS:
        render(*option)
