"""Generate matching PNG and LVGL weather icons from one drawing and WMO map."""

import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "weather"
WIDTH, HEIGHT, SCALE = 48, 36, 4
CLOUD = "#D7E2DC"
SUN = "#F5C978"
MOON = "#F5D899"
RAIN = "#83C8EB"

# Open-Meteo WMO codes. The generated firmware and preview manifest share this map.
GROUPS = {
    "clear": [0],
    "partial": [1, 2],
    "cloudy": [3],
    "fog": [45, 48],
    "drizzle": [51, 53, 55],
    "freezing": [56, 57, 66, 67],
    "rain": [61, 63, 65, 80, 81, 82],
    "snow": [71, 73, 75, 77, 85, 86],
    "thunder": [95, 97],
    "hail": [96, 99],
}
SINGLE = {"cloudy", "fog"}


def draw_icon(condition, day):
    image = Image.new("RGBA", (WIDTH * SCALE, HEIGHT * SCALE))
    draw = ImageDraw.Draw(image)

    def box(coords):
        return tuple(round(value * SCALE) for value in coords)

    def ellipse(coords, color):
        draw.ellipse(box(coords), fill=color)

    def line(coords, color, width=1.5):
        draw.line(box(coords), fill=color, width=round(width * SCALE))

    def cloud():
        ellipse((10, 15, 25, 27), CLOUD)
        ellipse((17, 10, 33, 27), CLOUD)
        ellipse((28, 16, 40, 27), CLOUD)
        draw.rectangle(box((17, 21, 34, 27)), fill=CLOUD)

    def moon(cx, cy, radius):
        ellipse((cx - radius, cy - radius, cx + radius, cy + radius), MOON)
        ellipse((cx - radius + 5, cy - radius - 3, cx + radius + 5, cy + radius - 3), (0, 0, 0, 0))

    def sun(cx, cy, radius):
        ellipse((cx - radius, cy - radius, cx + radius, cy + radius), SUN)
        for angle in range(0, 360, 45):
            radians = math.radians(angle)
            line(
                (
                    cx + (radius + 2) * math.cos(radians),
                    cy + (radius + 2) * math.sin(radians),
                    cx + (radius + 4) * math.cos(radians),
                    cy + (radius + 4) * math.sin(radians),
                ),
                SUN,
            )

    def snowflake(cx, cy, radius=2):
        for angle in (0, 60, 120):
            radians = math.radians(angle)
            dx, dy = radius * math.cos(radians), radius * math.sin(radians)
            line((cx - dx, cy - dy, cx + dx, cy + dy), "#E2EEF5", 1)

    if condition == "clear":
        if day:
            sun(24, 18, 8)
        else:
            moon(23, 18, 10)
    else:
        if condition == "partial":
            if day:
                sun(16, 11, 6)
            else:
                moon(16, 11, 8)
        elif not day and condition not in SINGLE and condition != "unknown":
            moon(15, 10, 7)
        cloud()
        if condition == "fog":
            for y, left, right in ((28, 8, 37), (32, 12, 40)):
                line((left, y, right, y), "#9EB7B2", 1.5)
        elif condition == "drizzle":
            for x in (17, 25, 33):
                ellipse((x, 30, x + 1.5, 31.5), RAIN)
        elif condition == "rain":
            for x in (17, 25, 33):
                line((x + 1, 29, x - 1, 33), RAIN, 1.6)
        elif condition == "freezing":
            line((17, 29, 15, 33), RAIN)
            snowflake(25, 31)
            line((34, 29, 32, 33), RAIN)
        elif condition == "snow":
            for x in (17, 25, 33):
                snowflake(x, 31)
        elif condition in {"thunder", "hail"}:
            draw.polygon(box((25, 22, 20, 29, 24, 29, 21, 35, 30, 26, 25, 26)), fill="#F7CF62")
            if condition == "hail":
                for x in (14, 34):
                    ellipse((x, 30, x + 3, 33), "#CFE8F4")
        elif condition == "unknown":
            font = ImageFont.truetype(str(ROOT / "assets/fonts/HankenGrotesk.ttf"), 16 * SCALE)
            draw.text((23 * SCALE, 17 * SCALE), "?", font=font, fill="#526C61", anchor="mm")

    # Use the same quantized pixels for the PNG and the firmware's indexed image.
    image = image.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    return image.quantize(colors=16, method=Image.Quantize.FASTOCTREE)


def lvgl_image(name, image):
    rgba_palette = image.getpalette("RGBA")
    palette = bytearray()
    for index in range(16):
        red, green, blue, alpha = rgba_palette[index * 4:index * 4 + 4]
        palette.extend((blue, green, red, alpha))
    indices = list(image.tobytes())
    assert len(indices) == WIDTH * HEIGHT and max(indices) < 16
    payload = palette + bytearray(
        (indices[i] << 4) | indices[i + 1] for i in range(0, len(indices), 2)
    )
    symbol = "kWeather" + "".join(part.title() for part in name.split("-"))
    result = [f"alignas(4) static const uint8_t {symbol}Pixels[] = {{"]
    for offset in range(0, len(payload), 16):
        result.append("  " + ", ".join(f"0x{value:02X}" for value in payload[offset:offset + 16]) + ",")
    result += [
        "};",
        f"static const lv_image_dsc_t {symbol} = {{",
        f"  .header = {{.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_I4, .w = {WIDTH}, .h = {HEIGHT}, .stride = {WIDTH // 2}}},",
        f"  .data_size = sizeof({symbol}Pixels), .data = {symbol}Pixels",
        "};",
    ]
    return "\n".join(result), symbol


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    names = {}
    for condition in GROUPS:
        names[condition] = (
            (condition, condition)
            if condition in SINGLE else (condition + "-day", condition + "-night")
        )
    names["unknown"] = ("unknown", "unknown")
    definitions, symbols = [], {}
    for day_name, night_name in names.values():
        for name in dict.fromkeys((day_name, night_name)):
            condition = name.removesuffix("-day").removesuffix("-night")
            image = draw_icon(condition, not name.endswith("-night"))
            image.convert("RGBA").save(OUT / f"{name}.png")
            definition, symbols[name] = lvgl_image(name, image)
            definitions.append(definition)

    cases = []
    mapping = {}
    for condition, codes in GROUPS.items():
        day, night = names[condition]
        for code in codes:
            mapping[str(code)] = {"day": day + ".png", "night": night + ".png"}
            cases.append(f"    case {code}:")
        result = f"&{symbols[day]}" if day == night else f"day ? &{symbols[day]} : &{symbols[night]}"
        cases.append(f"      return {result};")
    definitions.append(
        "static const lv_image_dsc_t *weatherIconImage(int code, bool day) {\n"
        "  switch (code) {\n" + "\n".join(cases) + "\n"
        f"    default: return &{symbols['unknown']};\n"
        "  }\n}"
    )
    (ROOT / "weather_icons.inc").write_text(
        "// Generated by tools/generate_weather_icons.py. Shared with assets/weather PNGs.\n"
        + "\n\n".join(definitions) + "\n",
        encoding="ascii",
    )
    (OUT / "manifest.json").write_text(
        json.dumps({"width": WIDTH, "height": HEIGHT, "default": "unknown.png", "codes": mapping}, indent=2)
        + "\n",
        encoding="ascii",
    )
    print(f"Generated {len(symbols)} icons and {len(mapping)} WMO mappings")


if __name__ == "__main__":
    main()
