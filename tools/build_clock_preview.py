"""Export firmware A4 glyphs so the clock preview uses actual LCD pixel bounds."""
import json
import re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/'desk_font_data.inc').read_text()
result={}
for name,height,base,chars in [('88',99,9,'0123456789:-'),('14_bold',20,4,'AMP'),
                              ('16_bold',23,5,'0123456789 /週日一二三四五六-')]:
    packed=re.search(r'desk_bits_'+name+r'\[\] = \{(.*?)\};',source,re.S).group(1)
    bits=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})',packed))
    table=re.search(r'desk_glyphs_'+name+r'\[\] = \{(.*?)\};',source,re.S).group(1)
    glyphs={}
    for row in re.findall(r'\{([^{}]+)\}',table):
        cp,offset,advance,w,h,x,y=map(int,row.split(','))
        if chr(cp) not in chars: continue
        glyphs[chr(cp)]={'advance':advance,'w':w,'h':h,'x':x,'y':y,
                        'bits':bits[offset:offset+((w+1)//2)*h].hex()}
    assert set(chars)==glyphs.keys()
    result[name]={'height':height,'base':base,'glyphs':glyphs}
(root/'tools/clock_preview.json').write_text(json.dumps(result,separators=(',',':')))
