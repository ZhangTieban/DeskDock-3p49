"""Second round of 640x172 DeskDock UI concepts; battery ring is unchanged."""

from pathlib import Path

from PIL import Image, ImageDraw

import render_ui_concepts as ui


OUT = ui.OUT


def save(image, name):
    image.resize((640, 172), Image.Resampling.LANCZOS).save(OUT / name)


def split_digits():
    image, draw = ui.home_shell()
    face = ui.font(ui.FONTS / "Manrope.ttf", 95, 750)
    date = ui.font(ui.FONTS / "Manrope.ttf", 18, 500)
    ui.text(draw, (170, 32), "15", face)
    ui.text(draw, (335, 32), "09", face)
    ui.text(draw, (294, 43), ":", ui.font(ui.FONTS / "Manrope.ttf", 72, 400), ui.MINT)
    date_text = "2026 / 09 / 24"
    x = (640 - ui.width(draw, date_text, date) - 42) / 2
    ui.text(draw, (x, 106), date_text, date, ui.MUTED)
    ui.text(draw, (x + ui.width(draw, date_text, date) + 13, 108), "週四", ui.CN_SMALL, ui.MUTED)
    save(image, "ui-d-split.png")


def calendar_rail():
    image, draw = ui.home_shell()
    time = ui.font(ui.FONTS / "HankenGrotesk.ttf", 93, 700)
    day = ui.font(ui.FONTS / "HankenGrotesk.ttf", 58, 700)
    year = ui.font(ui.FONTS / "HankenGrotesk.ttf", 16, 500)
    ui.text(draw, (35, 40), "15:09", time)
    ui.line(draw, (338, 48, 338, 116), "#32403b", 2)
    ui.text(draw, (361, 49), "09月", ui.CN_BOLD, ui.MUTED)
    ui.text(draw, (437, 44), "24", day)
    ui.text(draw, (362, 101), "2026", year, ui.MUTED)
    ui.text(draw, (425, 102), "週四", ui.CN, ui.MUTED)
    save(image, "ui-e-calendar.png")


def selected_home(hour12):
    image, draw = ui.home_shell()
    time = ui.font(ui.FONTS / "HankenGrotesk.ttf", 93, 700)
    day = ui.font(ui.FONTS / "HankenGrotesk.ttf", 58, 700)
    year = ui.font(ui.FONTS / "D-DIN.ttf", 18)
    ui.text(draw, (60, 33), "03:09" if hour12 else "15:09", time)
    if hour12:
        ui.text(draw, (304, 74), "PM", ui.font(ui.FONTS / "HankenGrotesk.ttf", 16, 600), ui.MUTED)
    ui.line(draw, (348, 48, 348, 116), "#32403b", 2)
    ui.text(draw, (368, 52), "09月", ui.font(ui.NOTO, 20, 600))
    ui.text(draw, (441, 43), "24", day)
    ui.text(draw, (368, 101), "2026", year)
    ui.text(draw, (444, 102), "週四", ui.CN, ui.MUTED)
    save(image, "ui-e-selected-12h.png" if hour12 else "ui-e-selected-24h.png")


def quiet_center():
    image, draw = ui.home_shell()
    clock = ui.font(ui.FONTS / "WorkSans.ttf", 98, 340)
    date = ui.font(ui.FONTS / "WorkSans.ttf", 18, 500)
    time = "15:09"
    ui.text(draw, ((640 - ui.width(draw, time, clock)) / 2, 30), time, clock)
    date_text = "2026 / 09 / 24"
    x = (640 - ui.width(draw, date_text, date)) / 2
    ui.text(draw, (x, 106), date_text, date, ui.MUTED)
    ui.text(draw, (28, 17), "週四", ui.CN, ui.MUTED)
    save(image, "ui-f-quiet.png")


def settings_canvas():
    image = Image.new("RGB", (640 * ui.S, 172 * ui.S), ui.BG)
    return image, ImageDraw.Draw(image)


def slider(draw, x, y, width, fraction, accent=ui.MINT):
    ui.rect(draw, (x, y, x + width, y + 7), "#29342d", radius=3)
    ui.rect(draw, (x, y, x + width * fraction, y + 7), accent, radius=3)
    cx = (x + width * fraction) * ui.S
    draw.ellipse((cx - 6 * ui.S, (y - 4) * ui.S,
                  cx + 6 * ui.S, (y + 11) * ui.S), fill=ui.WHITE)


def sidebar_settings():
    image, draw = settings_canvas()
    ui.rect(draw, (0, 0, 170, 171), "#121916")
    ui.text(draw, (18, 12), "設定", ui.font(ui.NOTO, 20, 700))
    for y, caption, active in ((50, "顯示與時間", True),
                               (90, "Wi-Fi", False),
                               (130, "天氣與股票", False)):
        if active:
            ui.rect(draw, (8, y - 4, 163, y + 30), "#253a30", radius=4)
            ui.rect(draw, (8, y - 4, 11, y + 30), ui.MINT)
        ui.text(draw, (24, y), caption, ui.CN, ui.WHITE if active else ui.MUTED)
    ui.button(draw, 575, 7, 54, 29, "‹")
    for y, label, val, pct in ((22, "亮度", "80%", .8),
                               (63, "省電", "10 分", .17),
                               (104, "音量", "60%", .6)):
        ui.text(draw, (190, y), label, ui.CN_BOLD)
        slider(draw, 275, y + 8, 237, pct)
        ui.text(draw, (535, y), val, ui.CN_SMALL, ui.MUTED)
    for x, caption in ((214, "日期時間"), (318, "12 小時"),
                       (422, "電池：自動"), (526, "測試音效")):
        ui.button(draw, x, 139, 96, 28, caption, accent=caption == "日期時間")
    save(image, "ui-d-settings.png")


def bottom_tabs_settings():
    image, draw = settings_canvas()
    ui.text(draw, (18, 8), "設定", ui.font(ui.NOTO, 20, 700))
    ui.button(draw, 575, 4, 54, 29, "‹")
    for x, caption, val, pct in ((18, "亮度", "80%", .8),
                                 (230, "省電", "10 分", .17),
                                 (442, "音量", "60%", .6)):
        ui.text(draw, (x, 44), caption, ui.CN_BOLD)
        ui.text(draw, (x + 122, 46), val, ui.CN_SMALL, ui.MUTED)
        slider(draw, x, 77, 180, pct)
    for x, caption in ((18, "日期時間"), (230, "電池：自動"), (442, "測試音效")):
        ui.button(draw, x, 100, 180, 29, caption, accent=caption == "日期時間")
    ui.rect(draw, (0, 137, 639, 171), "#121916")
    for i, caption in enumerate(("顯示與時間", "Wi-Fi", "天氣與股票")):
        x = i * 213
        if i == 0:
            ui.rect(draw, (x, 137, x + 212, 170), "#253a30")
            ui.line(draw, (x + 18, 138, x + 193, 138), ui.MINT, 2)
        ui.text(draw, (x + (213 - ui.width(draw, caption, ui.CN)) / 2, 146),
                caption, ui.CN, ui.WHITE if i == 0 else ui.MUTED)
    save(image, "ui-e-settings.png")


def list_settings():
    image, draw = settings_canvas()
    ui.text(draw, (18, 8), "設定", ui.font(ui.NOTO, 20, 700))
    ui.button(draw, 575, 4, 54, 29, "‹")
    ui.rect(draw, (0, 37, 639, 69), "#121916")
    for i, caption in enumerate(("顯示與時間", "Wi-Fi", "天氣與股票")):
        x = i * 213
        if i == 0:
            ui.rect(draw, (x, 37, x + 212, 69), "#253a30")
        ui.text(draw, (x + (213 - ui.width(draw, caption, ui.CN)) / 2, 44),
                caption, ui.CN, ui.WHITE if i == 0 else ui.MUTED)
    for x, caption, val, pct in ((18, "亮度", "80%", .8),
                                 (230, "省電", "10 分", .17),
                                 (442, "音量", "60%", .6)):
        ui.text(draw, (x, 78), caption, ui.CN_BOLD)
        ui.text(draw, (x + 130, 79), val, ui.CN_SMALL, ui.MUTED)
        slider(draw, x, 111, 180, pct, "#b4d8c2")
    for x, caption in ((18, "日期與時間"), (230, "電池：自動"), (442, "測試音效")):
        ui.button(draw, x, 137, 180, 30, caption)
    save(image, "ui-f-settings.png")


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    split_digits()
    calendar_rail()
    selected_home(False)
    selected_home(True)
    quiet_center()
    sidebar_settings()
    bottom_tabs_settings()
    list_settings()
