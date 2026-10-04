"""Render the user-supplied DeskDock home composition for visual approval."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "mockups" / "home-user-proposal-20261002.png"
SCALE = 4
WIDTH, HEIGHT = 640, 172

COLORS = {
    "background": "#070b0a",
    "surface": "#101713",
    "line": "#34483e",
    "text": "#f3f6f1",
    "muted": "#a2b9b2",
    "accent": "#9bd8bc",
    "gain": "#ee8982",
}

FONTS = {
    "clock": r"C:\Windows\Fonts\segoeuil.ttf",
    "digits": r"C:\Windows\Fonts\segoeui.ttf",
    "digits_bold": r"C:\Windows\Fonts\segoeuib.ttf",
    "zh": r"C:\Windows\Fonts\msjh.ttc",
    "zh_bold": r"C:\Windows\Fonts\msjhbd.ttc",
}


def px(value):
    return round(value * SCALE)


def font(name, size):
    return ImageFont.truetype(FONTS[name], px(size))


canvas = Image.new("RGB", (px(WIDTH), px(HEIGHT)), COLORS["background"])
draw = ImageDraw.Draw(canvas)


def rectangle(box, fill=None, outline=None, width=1, radius=0):
    scaled = tuple(px(v) for v in box)
    if radius:
        draw.rounded_rectangle(scaled, radius=px(radius), fill=fill,
                               outline=outline, width=px(width))
    else:
        draw.rectangle(scaled, fill=fill, outline=outline, width=px(width))


def line(points, fill, width=1):
    draw.line([(px(x), px(y)) for x, y in points], fill=fill, width=px(width))


def label(value, x, y, font_name, size, color, anchor="left"):
    face = font(font_name, size)
    bbox = draw.textbbox((0, 0), value, font=face)
    glyph_width = bbox[2] - bbox[0]
    if anchor == "center":
        x = x - glyph_width / (2 * SCALE)
    elif anchor == "right":
        x = x - glyph_width / SCALE
    draw.text((px(x) - bbox[0], px(y) - bbox[1]), value, font=face, fill=color)


# Thin frame and the main division reproduce the supplied composition.
rectangle((3, 2, 637, 169), outline=COLORS["line"], radius=8)
line([(305, 4), (305, 163)], "#263a31")

# Left: one dominant clock, with the date tucked below it.
label("09:41", 153, 54, "clock", 78, COLORS["text"], "center")
label("2026   /   10   /   02   週五", 153, 145, "zh", 12,
      COLORS["muted"], "center")

# Upper right: weather and connection/battery in one shallow band.
label("臺北 · 天氣", 326, 30, "zh", 13, COLORS["muted"])
label("Wi-Fi 已連線   ·   電量 88%", 624, 10, "zh", 11,
      COLORS["muted"], "right")
label("28–32°C", 462, 27, "digits_bold", 24, COLORS["text"])
label("降雨 40%", 625, 38, "zh", 12, COLORS["text"], "right")

# Middle right: prominent radio control, matching the attached hierarchy.
rectangle((315, 65, 634, 129), fill=COLORS["surface"], radius=12)
label("電台", 325, 84, "zh", 16, COLORS["text"])
label("好事903", 372, 84, "zh_bold", 17, COLORS["text"])
rectangle((535, 78, 622, 115), fill="#203329", radius=8)
draw.polygon([(px(552), px(88)), (px(552), px(105)), (px(564), px(96.5))],
             fill=COLORS["text"])
label("播放", 576, 87, "zh_bold", 15, COLORS["text"])

# Lower right: stock strip, positive change, and a compact settings entry.
rectangle((315, 136, 634, 166), fill="#0d1511", outline="#284036", radius=5)
label("台積電 2330", 322, 146, "zh", 11, COLORS["accent"])
label("2,510.00", 453, 140, "digits_bold", 20, COLORS["text"])
label("▲ 30.00", 562, 148, "digits", 11, COLORS["gain"])
rectangle((608, 142, 630, 161), fill="#193025", radius=3)
label("設", 619, 145, "zh", 11, COLORS["text"], "center")

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
canvas.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS).save(OUTPUT)
print(OUTPUT)
