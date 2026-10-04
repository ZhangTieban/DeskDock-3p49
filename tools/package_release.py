"""Create the two GitHub Release assets consumed by DeskDock OTA."""

import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path

BOARD = "DeskDock-3p49"
VERSION_SOURCE = Path(__file__).resolve().parents[1] / "ota_config.h"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin", type=Path, required=True, help="Arduino app .bin, not merged flash image")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    text = VERSION_SOURCE.read_text(encoding="utf-8")
    match = re.search(r'^#define DESKDOCK_VERSION "(\d+\.\d+\.\d+)"$', text, re.M)
    if not match:
        parser.error("DESKDOCK_VERSION must be a numeric MAJOR.MINOR.PATCH")
    version = match.group(1)
    payload = args.bin.read_bytes()
    if not 0x10000 <= len(payload) <= 0x400000 or payload[0] != 0xE9:
        parser.error("Expected an ESP32 app image between 64 KiB and 4 MiB")
    args.out.mkdir(parents=True, exist_ok=True)
    target = args.out / "firmware.bin"
    shutil.copyfile(args.bin, target)
    manifest = {
        "board": BOARD,
        "version": version,
        "file": "firmware.bin",
        "size": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(),
    }
    (args.out / "version.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"v{version}: {target} ({len(payload)} bytes)")
    print(f"SHA-256: {manifest['sha256']}")


if __name__ == "__main__":
    main()
