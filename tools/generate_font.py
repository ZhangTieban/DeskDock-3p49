"""Generate LVGL A4 glyph subsets from OFL fonts for DeskDock."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = Path(r"C:\Windows\Fonts\NotoSansTC-VF.ttf")
DIN = ROOT / "assets/fonts/D-DIN-Bold.ttf"
DIN_REGULAR = ROOT / "assets/fonts/D-DIN.ttf"
SYMBOL_FONT = ROOT / "assets/fonts/NotoSansSymbols.ttf"
HANKEN = ROOT / "assets/fonts/HankenGrotesk.ttf"
TEXT = (
    "設定桌面時間日期電量天氣臺股股市股票代號收盤資料尚未連線已連線"
    "網路名稱密碼連接授權碼縣市預報更新失敗格式錯誤無此請先輸入"
    "亮度省電分鐘音量測試音效取消儲存手動自動校時電池低正常"
    "降雨機率上次開啟待取得高溫低溫臺北新桃園中南雄基隆竹苗栗"
    "彰化投雲嘉義屏東宜蘭花蓮東澎湖金門江連馬祖轉圈"
    "測版號今明小時週日一二三四五六"
    "價或林灣示與象顯／：搜尋優刪除記憶需開文字放英數空白有符重大夜盤充個不足月"
)
ASCII = "".join(chr(i) for i in range(32, 127))
SOURCE_UNICODE = "".join(
    char for path in (ROOT / "DeskDock_3p49.ino", ROOT / "home_ui.inc",
                      ROOT / "ota_update.inc")
    for char in path.read_text(encoding="utf-8")
    if ord(char) >= 0xA0
)


def glyph_data(font, char):
    box = font.getbbox(char, anchor="ls")
    left, top, right, bottom = box
    width, height = max(0, right - left), max(0, bottom - top)
    advance = max(1, round(font.getlength(char)))
    if width == 0 or height == 0:
        return advance, 0, 0, 0, 0, b""
    image = Image.new("L", (width, height), 0)
    ImageDraw.Draw(image).text((-left, -top), char, fill=255, font=font, anchor="ls")
    pixels = image.load()
    bits = bytearray()
    for y in range(height):
        for x in range(0, width, 2):
            high = (pixels[x, y] + 8) // 17
            low = (pixels[x + 1, y] + 8) // 17 if x + 1 < width else 0
            bits.append((min(15, high) << 4) | min(15, low))
    return advance, width, height, left, -bottom, bytes(bits)


def emit_font(size, chars, font_path=FONT, suffix="", weight=None):
    font = ImageFont.truetype(str(font_path), size)
    if weight is not None:
        font.set_variation_by_axes([weight])
    name = f"{size}{suffix}"
    glyphs = []
    data = bytearray()
    for char in sorted(set(chars)):
        adv, width, height, x, y, bits = glyph_data(font, char)
        glyphs.append((ord(char), len(data), adv, width, height, x, y))
        data.extend(bits)
    out = [f"static const uint8_t desk_bits_{name}[] = {{"]
    for i in range(0, len(data), 16):
        out.append("  " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
    out.append("};")
    out.append(f"static const DeskGlyph desk_glyphs_{name}[] = {{")
    for glyph in glyphs:
        out.append("  {" + ", ".join(str(value) for value in glyph) + "},")
    out.append("};")
    out.append(
        f"static const DeskFontBlob desk_blob_{name} = "
        f"{{desk_glyphs_{name}, {len(glyphs)}, desk_bits_{name}}};"
    )
    return "\n".join(out)


content = "// Generated from Noto Sans TC, D-DIN, Noto Sans Symbols (SIL OFL). Do not edit by hand.\n"
content += emit_font(16, ASCII + TEXT + SOURCE_UNICODE) + "\n"
content += emit_font(16, ASCII + TEXT + SOURCE_UNICODE, suffix="_bold", weight=700) + "\n"
content += emit_font(14, "最高最低成交量股降雨濕度體感風速日落UV 0123456789.,:/%°Ckmh-", suffix="_bold", weight=700) + "\n"
content += emit_font(10, "上一台播放暫停下降雨更新預報資料 Wi-Fi·0123456789:/%", suffix="_bold", weight=700) + "\n"
content += emit_font(18, "0123456789 /-", DIN_REGULAR) + "\n"
content += emit_font(20, ASCII + TEXT + SOURCE_UNICODE) + "\n"
content += emit_font(20, "0123456789.,+-°C%", HANKEN, suffix="_bold", weight=700) + "\n"
content += emit_font(24, "☀☁☂", suffix="_weather") + "\n"
content += emit_font(24, "☾", SYMBOL_FONT, suffix="_moon") + "\n"
content += emit_font(34, "⚙", SYMBOL_FONT) + "\n"
content += emit_font(58, "0123456789/", HANKEN, weight=700) + "\n"
content += emit_font(70, "0123456789:-") + "\n"
content += emit_font(86, "0123456789:-", DIN) + "\n"
content += emit_font(93, "0123456789:-", HANKEN, weight=700) + "\n"
(ROOT / "desk_font_data.inc").write_text(content, encoding="ascii")
