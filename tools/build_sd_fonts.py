"""Create complete fixed-weight Noto Sans TC fonts for the DeskDock SD card."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path(r"C:\Windows\Fonts\NotoSansTC-VF.ttf")
DEST = ROOT / "sdcard" / "fonts"


def main() -> None:
    if not SOURCE.is_file():
        raise SystemExit(f"Missing source font: {SOURCE}")
    DEST.mkdir(parents=True, exist_ok=True)
    for weight, name in ((400, "Regular"), (700, "Bold")):
        font = TTFont(SOURCE)
        fixed = instantiateVariableFont(font, {"wght": weight}, inplace=True)
        fixed["head"].macStyle = (fixed["head"].macStyle & ~1) | (weight == 700)
        fixed["OS/2"].fsSelection = (fixed["OS/2"].fsSelection & ~(0x20 | 0x40)) | (0x20 if weight == 700 else 0x40)
        names = fixed["name"]
        values = {
            1: "Noto Sans TC", 2: name, 4: f"Noto Sans TC {name}",
            6: f"NotoSansTC-{name}", 16: "Noto Sans TC", 17: name,
        }
        for record in list(names.names):
            if record.nameID in values:
                names.setName(values[record.nameID], record.nameID,
                              record.platformID, record.platEncID, record.langID)
        output = DEST / f"NotoSansTC-{name}.ttf"
        fixed.save(output)
        print(f"{name}: {output} ({output.stat().st_size} bytes)")
    old = DEST / "NotoSansTC-VF.ttf"
    if old.exists():
        old.unlink()
    package = ROOT / "sdcard-fonts.zip"
    with ZipFile(package, "w", ZIP_DEFLATED, compresslevel=9) as archive:
        for item in sorted((ROOT / "sdcard").rglob("*")):
            if item.is_file():
                archive.write(item, item.relative_to(ROOT / "sdcard"))
    print(f"Package: {package} ({package.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
