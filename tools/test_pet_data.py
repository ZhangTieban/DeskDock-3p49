"""Check generated frames and all visible pixel bounds, including hidden toys."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
data=json.loads((root/'tools/pet_scene.json').read_text(encoding='utf-8'))
assert len(data['sprites'][0])==21 and len(data['sprites'][0][0])==32
assert len(data['sprites'][9])==32 and len(data['sprites'][9][0])==18
assert data['sprites'][9][4][5:14]=='EBBEEEBBE'
assert data['sprites'][15][12][5:14]=='EBBEEEBBE'
assert data['sprites'][9][5][6]=='3' and data['sprites'][9][5][11]=='3'
assert len({''.join(s) for s in data['sprites'][10:14]})==4
for sprite in data['sprites']:
    assert sprite and len({len(row) for row in sprite})==1
    assert all(c in '.123456789ABCDEF' for row in sprite for c in row)
for name,frames in zip(data['actions'],data['frames']):
    for f in frames:
        x,y,pose,flip,gx,gp,gf,tx,ty,toy,leash=f
        assert 0<=x<=264 and 0<=y<=11, (name,f)
        assert 0<=pose<=8 and 0<=gp<=7 and 17<=toy<=19
        assert gx==-32 or 0<=gx<=278, (name,f)
        assert tx==-32 or 0<=tx<=288, (name,f)
        assert 0<=ty<=27, (name,f)
        if name=='sleep': assert pose==4 and flip==0
        if name=='eat': assert pose in (5,6) and flip==0
        if name=='leash':
            assert leash==1
            if pose!=7: assert (gx>x+32 if flip else gx+18<x), f
    if name in ('walk','run','leash'):
        assert max(abs(frames[(i+1)%len(frames)][0]-f[0]) for i,f in enumerate(frames))<=5
    if name=='leash':
        assert max(abs(frames[(i+1)%len(frames)][4]-f[4]) for i,f in enumerate(frames))<=4
print(f"PASS: {sum(map(len,data['frames']))} shared frames, sprite indices/bounds, continuous walking/running/leash loops")
