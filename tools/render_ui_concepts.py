"""Render LVGL-sized home and settings concepts without changing firmware."""

from math import cos, pi, sin
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "mockups"
FONTS = ROOT / "assets/fonts"
NOTO = Path(r"C:\Windows\Fonts\NotoSansTC-VF.ttf")
S = 2
BG, WHITE, MUTED = "#080b0b", "#f3f5f2", "#aab5b0"
MINT, RED, GREEN = "#83d7bb", "#f27873", "#7ad44a"


def font(path, size, weight=None):
    face = ImageFont.truetype(str(path), size * S)
    if weight is not None:
        face.set_variation_by_axes([weight])
    return face


CN = font(NOTO, 16, 400)
CN_BOLD = font(NOTO, 16, 700)
CN_SMALL = font(NOTO, 13, 400)


def text(draw, xy, value, face=CN, fill=WHITE, anchor="lt"):
    draw.text((xy[0] * S, xy[1] * S), value, font=face, fill=fill, anchor=anchor)


def width(draw, value, face):
    return draw.textlength(value, font=face) / S


def line(draw, points, fill, line_width=1):
    draw.line(tuple(v * S for v in points), fill=fill, width=line_width * S)


def rect(draw, xy, fill, outline=None, radius=0):
    box = tuple(v * S for v in xy)
    draw.rounded_rectangle(box, radius * S, fill=fill, outline=outline, width=S)


def status(draw):
    cx, cy = 557 * S, 36 * S
    for radius in (14 * S, 9 * S):
        draw.arc((cx - radius, cy - radius, cx + radius, cy + radius), 206, 334,
                 fill="#dce9e1", width=2 * S)
    draw.ellipse((cx - 2 * S, cy + S, cx + 2 * S, cy + 5 * S), fill="#dce9e1")
    gx, gy = 591 * S, 28 * S
    draw.ellipse((gx - 10 * S, gy - 10 * S, gx + 10 * S, gy + 10 * S),
                 outline=WHITE, width=2 * S)
    draw.ellipse((gx - 3 * S, gy - 3 * S, gx + 3 * S, gy + 3 * S),
                 outline=WHITE, width=2 * S)
    for i in range(8):
        angle = i * pi / 4
        line(draw, (591 + 10 * cos(angle), 28 + 10 * sin(angle),
                    591 + 15 * cos(angle), 28 + 15 * sin(angle)), WHITE, 2)


def home_shell():
    image = Image.new("RGB", (640 * S, 172 * S), BG)
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((8 * S, 4 * S, 632 * S, 168 * S), 24 * S,
                           outline="#ff5a4f", width=6 * S)
    status(draw)
    line(draw, (27, 129, 613, 129), "#32403b")
    text(draw, (28, 136), "大盤", CN_BOLD)
    text(draw, (85, 136), "48,024.60", CN_BOLD)
    text(draw, (176, 136), "-132.69", CN_BOLD, GREEN)
    text(draw, (575, 137), "2/4", CN_SMALL, MUTED)
    return image, draw


def home_concept(name, family, clock_size, clock_weight, clock_xy, date_mode):
    image, draw = home_shell()
    clock_face = font(FONTS / family, clock_size, clock_weight)
    date_path = FONTS / ("D-DIN.ttf" if family == "D-DIN-Bold.ttf" else family)
    date_face = font(date_path, 19, 500 if family != "D-DIN-Bold.ttf" else None)
    if date_mode == "top":
        text(draw, (27, 17), "2026 / 09 / 24", date_face, "#d8dfdc")
        x = 27 + width(draw, "2026 / 09 / 24", date_face) + 14
        text(draw, (x, 19), "週四", CN, MUTED)
    elif date_mode == "right":
        text(draw, (384, 76), "2026 / 09 / 24", date_face, WHITE)
        text(draw, (384, 102), "星期四", CN, MUTED)
        line(draw, (364, 68, 364, 118), "#32403b", 2)
    elif date_mode == "large":
        date_large = font(FONTS / family, 32, 600)
        text(draw, (380, 67), "09 / 24", date_large, WHITE)
        text(draw, (382, 108), "2026  週四", CN, MUTED)
        line(draw, (363, 67, 363, 118), MINT, 2)
    elif date_mode == "baseline":
        text(draw, (27, 17), "2026 / 09 / 24", date_face, "#d8dfdc")
        text(draw, (170, 19), "週四", CN, MUTED)
    time = "15:09"
    x, y = clock_xy
    if x == "center":
        x = (640 - width(draw, time, clock_face)) / 2
    text(draw, (x, y), time, clock_face)
    image.resize((640, 172), Image.Resampling.LANCZOS).save(OUT / name)


def button(draw, x, y, w, h, caption, accent=False):
    rect(draw, (x, y, x + w, y + h), "#253a30" if accent else "#202824",
         "#466251" if accent else "#34433b", 5)
    x_text = x + (w - width(draw, caption, CN)) / 2
    text(draw, (x_text, y + (h - 16) / 2 - 1), caption)


def settings_concept(name, accent, variant):
    image = Image.new("RGB", (640 * S, 172 * S), BG)
    draw = ImageDraw.Draw(image)
    text(draw, (18, 8), "設定", font(NOTO, 20, 700))
    button(draw, 575, 4, 54, 29, "‹")
    rect(draw, (0, 37, 639, 70), "#121916")
    for i, title in enumerate(("顯示與時間", "Wi-Fi", "天氣與股票")):
        x = i * 213
        if i == 0:
            rect(draw, (x, 37, x + 212, 70), "#253a30")
            line(draw, (x + 25, 69, x + 188, 69), accent, 2)
        text(draw, (x + (213 - width(draw, title, CN)) / 2, 45), title,
             CN, WHITE if i == 0 else MUTED)
    if variant == "meters":
        for x, label, val, pct in ((18, "亮度", "80%", .8),
                                   (230, "省電", "10 分", .17),
                                   (442, "音量", "60%", .6)):
            text(draw, (x, 79), label)
            text(draw, (x + 117, 79), val, CN_SMALL, MUTED)
            rect(draw, (x, 112, x + 180, 123), "#29342d", radius=6)
            rect(draw, (x, 112, x + 180 * pct, 123), accent, radius=6)
            draw.ellipse(((x + 180 * pct - 7) * S, 108 * S,
                          (x + 180 * pct + 7) * S, 127 * S), fill=WHITE)
    elif variant == "rows":
        for x, label, val, pct in ((18, "亮度", "80%", .8),
                                   (230, "省電", "10 分", .17),
                                   (442, "音量", "60%", .6)):
            text(draw, (x, 80), label, CN_BOLD)
            text(draw, (x + 132, 80), val, CN_SMALL, MUTED)
            rect(draw, (x, 112, x + 180, 116), "#29342d", radius=2)
            rect(draw, (x, 112, x + 180 * pct, 116), accent, radius=2)
            draw.ellipse(((x + 180 * pct - 5) * S, 108 * S,
                          (x + 180 * pct + 5) * S, 120 * S), fill=WHITE)
    else:
        for x, label, val, pct in ((18, "亮度", "80%", .8),
                                   (230, "省電", "10 分", .17),
                                   (442, "音量", "60%", .6)):
            text(draw, (x, 78), label, CN_BOLD)
            text(draw, (x + 115, 78), val, CN_SMALL, accent)
            rect(draw, (x, 109, x + 180, 126), "#29342d", radius=4)
            rect(draw, (x, 109, x + 180 * pct, 126), accent, radius=4)
    for x, caption in ((18, "日期與時間"), (230, "電池：自動"), (442, "測試音效")):
        button(draw, x, 137, 180, 30, caption, accent=caption == "日期與時間")
    image.resize((640, 172), Image.Resampling.LANCZOS).save(OUT / name)


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    home_concept("ui-ref-d-din.png", "D-DIN-Bold.ttf", 86, None, ("center", 46), "baseline")
    home_concept("ui-a-manrope.png", "Manrope.ttf", 90, 700, ("center", 42), "top")
    home_concept("ui-b-hanken.png", "HankenGrotesk.ttf", 94, 700, (35, 39), "right")
    home_concept("ui-c-worksans.png", "WorkSans.ttf", 89, 650, (43, 41), "large")
    settings_concept("ui-a-settings.png", MINT, "meters")
    settings_concept("ui-b-settings.png", "#8cbfa8", "rows")
    settings_concept("ui-c-settings.png", "#a5d9b4", "blocks")
