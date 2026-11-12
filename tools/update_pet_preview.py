"""Refresh the preview runtime and shared animation data without changing its layout."""
from pathlib import Path
import sys

root=Path(__file__).resolve().parents[1]
preview=Path(sys.argv[1])
text=preview.read_text(encoding='utf-8')
start=text.index('    const petData =')
end=text.index('    function render() {',start)
runtime=(root/'tools/pet_preview.js').read_text(encoding='utf-8')
runtime=runtime.replace('__PET_DATA__',(root/'tools/pet_scene.json').read_text(encoding='utf-8'))
preview.write_text(text[:start]+runtime+'\n'+text[end:],encoding='utf-8')
