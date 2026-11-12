"""Bind legacy DeskDock versions to verified ESP image digests, not generic IDF app_desc versions."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('folders',nargs='+',type=Path)
args=parser.parse_args()
entries=[]
for folder in args.folders:
    manifest=json.loads((folder/'version.json').read_text(encoding='utf-8'))
    image=(folder/'firmware.bin').read_bytes()
    assert manifest['board']=='DeskDock-3p49'
    assert hashlib.sha256(image).hexdigest()==manifest['sha256']
    assert len(image)==manifest['size'] and image[0]==0xe9 and image[23]==1
    assert hashlib.sha256(image[:-32]).digest()==image[-32:]
    version=manifest['version']
    assert version.encode()+b'\0' in image
    entries.append((version,image[-32:].hex()))
lines=['// Generated from verified release images by tools/generate_ota_legacy.py.',
       '#pragma once','#include <stdint.h>',
       'struct LegacyOtaImage { const char *version; uint8_t digest[32]; };',
       'static constexpr LegacyOtaImage kLegacyOtaImages[] = {']
for version,digest in entries:
    lines.append('  {"'+version+'", {'+', '.join('0x'+digest[i:i+2] for i in range(0,64,2))+'}},')
lines.append('};')
(ROOT/'ota_legacy_images.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Verified legacy versions: '+', '.join(v for v,_ in entries))
