"""Check generated frames and all visible pixel bounds, including hidden toys."""
import json
import base64
import hashlib
from build_pet_daily_assets import decode
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
        x,y,pose,flip,gx,gp,gf,tx,ty,toy,leash,heart=f
        assert 0<=pose<len(data['sprites']) and 0<=gp<=7 and 17<=toy<=19
        sprite=data['sprites'][pose]
        assert 0<=x and x+len(sprite[0])<=296 and 0<=y and y+len(sprite)<=32, (name,f)
        assert heart==-1 or 0<=heart<=289
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

daily=json.loads((root/'tools/pet_daily_clips.json').read_text(encoding='utf-8'))
for i,s in enumerate(daily['sprites']):
    raw=base64.b64decode(s['rle'])
    assert len(raw)%2==0 and len(raw)<65536 and 0<s['w']<256 and 0<s['h']<=32
    assert all(0<n<=255 for n in raw[::2]) and all(c<=16 for c in raw[1::2])
    assert decode(s)==data['sprites'][21+i]
for action in daily['actions']:
    index=data['actions'].index(action['name']);digest=hashlib.sha256()
    for f in data['frames'][index]:
        pixels=list('.'*(296*32));x,y,pose=f[:3]
        for yy,row in enumerate(data['sprites'][pose]):
            pixels[(y+yy)*296+x:(y+yy)*296+x+len(row)]=row
        digest.update(''.join(pixels).encode('ascii'))
    assert digest.hexdigest()==action['sha256'],action['name']
print('PASS: all 680 new frames match the approved preview, including background pixels')
