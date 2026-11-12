"""Audit Unicode 17 CJK coverage and package the SD-card font directory."""
from pathlib import Path
import hashlib
import json
from zipfile import ZipFile, ZIP_DEFLATED
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT/'sdcard/fonts'
RANGES = {'Basic': (0x4E00,0x9FFF), 'A':(0x3400,0x4DBF),
          'B':(0x20000,0x2A6DF), 'C':(0x2A700,0x2B73F), 'D':(0x2B740,0x2B81D),
          'E':(0x2B820,0x2CEAD), 'F':(0x2CEB0,0x2EBE0), 'G':(0x30000,0x3134A),
          'H':(0x31350,0x323AF), 'I':(0x2EBF0,0x2EE5D), 'J':(0x323B0,0x33479)}

def main():
    covered=set(); files={}
    for path in sorted(DEST.glob('*.ttf')):
        font=TTFont(path, lazy=True)
        cmap=font.getBestCmap()
        covered.update(cmap)
        files[path.name]={'bytes':path.stat().st_size,'codepoints':len(cmap),
                         'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
        font.close()
    ranges={}
    for name,(lo,hi) in RANGES.items():
        missing=sorted(set(range(lo,hi+1))-covered)
        ranges[name]={'expected':hi-lo+1,'mapped':hi-lo+1-len(missing),
                      'missing':[f'U+{cp:X}' for cp in missing]}
        assert not missing, (name,missing[:10])
    assert all(cp in covered for cp in (0x20BB7,0x20BB6,0x9F98,0x33479))
    compatibility = set(range(0xF900,0xFA6E)) | set(range(0xFA70,0xFADA)) | set(range(0x2F800,0x2FA1E))
    assert not compatibility-covered
    report={'unicode':'17.0','cjk_unified_total':sum(v['mapped'] for v in ranges.values()),
            'cjk_compatibility_total':len(compatibility),
            'source':'https://kamichikoichi.github.io/jigmo/', 'files':files,'ranges':ranges,
            'note':'Unicode mapped glyphs only; IVS shaping is not implemented by LVGL tiny_ttf.'}
    (DEST/'coverage.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    with ZipFile(ROOT/'sdcard-fonts.zip','w',ZIP_DEFLATED,compresslevel=9) as archive:
        for path in sorted((ROOT/'sdcard').rglob('*')):
            if path.is_file(): archive.write(path,path.relative_to(ROOT/'sdcard'))
    print(f"CJK Basic + A-J: {report['cjk_unified_total']} glyphs, no missing codepoints")
    print(f"SD package: {(ROOT/'sdcard-fonts.zip').stat().st_size} bytes")

if __name__=='__main__': main()
