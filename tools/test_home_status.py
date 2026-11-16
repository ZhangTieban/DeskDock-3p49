"""Check the shipped 12px subset and every supported Easter egg at 640px."""
import re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/'desk_status_font_data.inc').read_text(encoding='ascii')
packed=source.split('desk_bits_12_bold[] = {',1)[1].split('};',1)[0]
bits=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',packed))
table=source.split('desk_glyphs_12_bold[] = {',1)[1].split('};',1)[0]
glyphs={}
for row in re.findall(r'\{([^{}]+)\}',table):
    cp,offset,advance,w,h,x,y=map(int,row.split(','))
    assert offset+((w+1)//2)*h<=len(bits)
    glyphs[chr(cp)]=(advance,w,h,x,y)
def width(text):return sum(glyphs[c][0] for c in text)
text=['阿喵喵生日快樂~']+[f'認識阿喵喵{n}周年~' for n in range(1,101)]
right=max(12+width('Wi-Fi')+2+width(p)+2+12 for p in ['--%']+[f'{n}%' for n in range(101)])
gap=min((307-width(t))//2-right for t in text)
assert gap>=4,(right,gap)
assert all((307-width(t))//2+width(t)+4+12<=307 for t in text)
assert max(g[2] for g in glyphs.values())<=14
print(f'PASS: 12px status glyphs, 101 Easter eggs centered, minimum {gap}px power gap, icon gap 4px; power right={right}')
