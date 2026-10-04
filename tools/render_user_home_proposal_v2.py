"""Render the refined DeskDock home mockup and its weather/stock scroll preview."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_PNG = ROOT / "mockups" / "home-user-proposal-v2-20261002.png"
OUTPUT_APNG = ROOT / "mockups" / "home-user-proposal-v2-scroll-20261002.png"
SCALE = 4
WIDTH, HEIGHT = 640, 172

COLORS = {
    "background": "#070b0a",
    "card": "#121916",
    "button": "#202824",
    "selected": "#253a30",
    "line": "#34433b",
    "text": "#f3f5f2",
    "muted": "#aab5b0",
    "accent": "#83d7bb",
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


def face(name, size):
    return ImageFont.truetype(FONTS[name], px(size))


def label(draw, value, x, y, font_name, size, color, anchor="left"):
    selected_font = face(font_name, size)
    left, top, right, _ = draw.textbbox((0, 0), value, font=selected_font)
    width = right - left
    if anchor == "center":
        x -= width / (2 * SCALE)
    elif anchor == "right":
        x -= width / SCALE
    draw.text((px(x) - left, px(y) - top), value, font=selected_font, fill=color)


def box(draw, x1, y1, x2, y2, *, fill=None, outline=None, radius=6):
    draw.rounded_rectangle((px(x1), px(y1), px(x2), px(y2)),
                           radius=px(radius), fill=fill, outline=outline,
                           width=px(1))


def draw_scrolling_row(draw, x, y, width, height, items, current, progress, painter):
    layer = Image.new("RGBA", (px(width), px(height)), (0, 0, 0, 0))
    layer_draw = ImageDraw.Draw(layer)
    painter(layer_draw, items[current], -height * progress)
    painter(layer_draw, items[(current + 1) % len(items)], height * (1 - progress))
    return layer


def render(weather_index=0, stock_index=0, progress=0.0):
    canvas = Image.new("RGBA", (px(WIDTH), px(HEIGHT)), COLORS["background"])
    draw = ImageDraw.Draw(canvas)

    # Keep the reference image's large left clock and fine outer frame.
    box(draw, 3, 2, 637, 169, outline=COLORS["line"], radius=8)
    draw.line((px(305), px(4), px(305), px(164)), fill="#263a31", width=px(1))
    label(draw, "09:41", 153, 54, "clock", 78, COLORS["text"], "center")
    label(draw, "2026   /   10   /   02   週五", 153, 145, "zh", 12,
          COLORS["muted"], "center")

    # Three aligned cards share fill, border, and corner treatment.
    box(draw, 315, 7, 576, 54, fill=COLORS["card"], outline=COLORS["line"])
    box(draw, 315, 62, 634, 114, fill=COLORS["card"], outline=COLORS["line"])
    box(draw, 315, 122, 634, 166, fill=COLORS["card"], outline=COLORS["line"])

    # Settings is now a full touch-sized control aligned to the weather card.
    box(draw, 583, 7, 634, 54, fill=COLORS["button"], outline=COLORS["line"])
    label(draw, "設定", 608.5, 22, "zh_bold", 16, COLORS["text"], "center")

    # Weather's detail line and the stock card each rotate independently.
    label(draw, "臺北 · 天氣", 325, 12, "zh", 12, COLORS["muted"])
    label(draw, "Wi-Fi 已連線 · 88%", 568, 12, "zh", 10, COLORS["muted"], "right")
    weather_items = [
        ("28–32°C", "降雨 40%", "digits_bold", 22),
        ("降雨 40%", "更新 09:40", "zh_bold", 18),
    ]

    def paint_weather(layer_draw, item, offset):
        label(layer_draw, item[0], 0, offset, item[2], item[3], COLORS["text"])
        label(layer_draw, item[1], 145, offset + 7, "zh", 12, COLORS["text"])

    weather_layer = draw_scrolling_row(draw, 325, 28, 242, 25, weather_items,
                                       weather_index, progress, paint_weather)
    canvas.alpha_composite(weather_layer, (px(325), px(28)))

    label(draw, "電台", 326, 80, "zh", 14, COLORS["muted"])
    # Station and play controls use the same dark button format as Settings.
    box(draw, 372, 70, 511, 106, fill=COLORS["selected"], outline=COLORS["line"])
    label(draw, "好事903", 441.5, 79, "zh_bold", 16, COLORS["text"], "center")
    draw.rectangle((px(384), px(104), px(499), px(106)), fill=COLORS["accent"])
    box(draw, 520, 70, 624, 106, fill=COLORS["button"], outline=COLORS["line"])
    draw.polygon([(px(537), px(79)), (px(537), px(97)), (px(550), px(88))],
                 fill=COLORS["text"])
    label(draw, "播放", 562, 79, "zh_bold", 16, COLORS["text"])

    stock_items = [
        ("台積電 2330", "2,510.00", "▲ 30.00"),
        ("0050 元大台灣50", "112.90", "▲ 0.85"),
    ]

    def paint_stock(layer_draw, item, offset):
        label(layer_draw, item[0], 3, offset + 11, "zh", 11, COLORS["accent"])
        label(layer_draw, item[1], 127, offset + 6, "digits_bold", 19, COLORS["text"])
        label(layer_draw, item[2], 241, offset + 14, "digits", 11, COLORS["gain"])

    stock_layer = draw_scrolling_row(draw, 320, 125, 310, 38, stock_items,
                                     stock_index, progress, paint_stock)
    canvas.alpha_composite(stock_layer, (px(320), px(125)))

    # Low profile dots show that both information cards have more than one page.
    active = (weather_index + (progress >= .5)) % len(weather_items)
    for index in range(2):
        draw.ellipse((px(556 + index * 8), px(47), px(560 + index * 8), px(51)),
                     fill=COLORS["accent"] if index == active else "#53665c")
    active = (stock_index + (progress >= .5)) % len(stock_items)
    for index in range(2):
        draw.ellipse((px(615 + index * 8), px(157), px(619 + index * 8), px(161)),
                     fill=COLORS["accent"] if index == active else "#53665c")

    return canvas.convert("RGB").resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)


OUTPUT_PNG.parent.mkdir(parents=True, exist_ok=True)
render().save(OUTPUT_PNG)
frames = [render(0, 0, 0)]
durations = [1800]
for step in (0.2, 0.4, 0.6, 0.8, 1.0):
    frames.append(render(0, 0, step))
    durations.append(90)
frames.append(render(1, 1, 0))
durations.append(1800)
for step in (0.2, 0.4, 0.6, 0.8, 1.0):
    frames.append(render(1, 1, step))
    durations.append(90)
for index, frame in enumerate(frames):
    # Force full-frame animation updates; cropped delta frames leave text trails.
    marker = (7, 11, 9 + index % 2)
    frame.putpixel((0, 0), marker)
    frame.putpixel((WIDTH - 1, HEIGHT - 1), marker)
frames[0].save(OUTPUT_APNG, save_all=True, append_images=frames[1:],
               duration=durations, loop=0, disposal=0, blend=0)
print(OUTPUT_PNG)
print(OUTPUT_APNG)
