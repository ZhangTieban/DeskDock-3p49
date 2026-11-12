"""Generate the same pixel sprites and 80 ms motion frames for firmware and preview."""
import json
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def generate():
    source = (ROOT / 'home_dog.inc').read_text(encoding='utf-8')
    def rows(name, end):
        text = source.split(name, 1)[1].split(end, 1)[0]
        return re.findall(r'"([.1-8]+)"', text)
    body = rows('kShibaBody[', 'static constexpr char kShibaFeet')
    feet = rows('kShibaFeet[', 'static constexpr char kShibaAsleep')
    asleep = rows('kShibaAsleep[', 'static constexpr char kShibaEating')
    eating = rows('kShibaEating[', 'static constexpr char kShibaSitting')
    sitting = rows('kShibaSitting[', 'static constexpr char kShibaPlaying')
    playing = rows('kShibaPlaying[', 'static constexpr char kShibaBall')
    # Normalize old rows, some of which omitted trailing transparent pixels.
    body = [s.ljust(40, '.') for s in body]
    sprites = []
    for frame in range(4):
        pose = body.copy()
        for y in range(18, 26):
            pose[y] = ''.join(a if a != '.' else b for a, b in zip(pose[y], feet[frame*8+y-18].ljust(40, '.')))
        sprites.append(pose)
    sprites += [asleep, eating[:26], eating[26:], sitting, playing]
    sprites = [[s.ljust(40, '.') for s in pose] for pose in sprites]
    # Dog is 80% of its former size; the 18 x 32 girl keeps her original size.
    sprites = [[''.join(pose[min(25,int((y+.5)*26/21))][min(39,int((x+.5)*40/32))]
                        for x in range(32)) for y in range(21)] for pose in sprites]

    # 18 x 32 chibi girl. Hair A, skin B, dress C, shoes D, highlights E.
    stand = [
        '......AAAAAA......','.....AAAAAAAA.....','....AAAAAAAAAA....',
        '....AAABBBBBAA....','....AABBBBBBAAA...','....AABB3B3BAAA...',
        '....AABBBBBBAAA...','.....ABBBBBBAA....','.....AAABBBAA.....',
        '......CCBBCC......','.....CCCCCCCC.....','....BCCCCCCCCB....',
        '....BCCCCCCCCB....','....BCCCCCCCCB....','....BCCCCCCCCB....',
        '....BCCCCCCCCB....','....BBCCCCCCBB....','.....CCCCCCCC.....',
        '.....CCCCCCCC.....','....CCCCCCCCCC....','....CCCCCCCCCC....',
        '...CCCCCCCCCCCC...','...CCCCCCCCCCCC...','....CCCCCCCCCC....',
        '......BB..BB......','......BB..BB......','......BB..BB......',
        '......BB..BB......','......BB..BB......','.....DDD..DDD.....',
        '.....DDD..DDD.....','..................']
    # Two outlined lenses and a bridge, retained by every standing/kneeling pose.
    for y, pixels in ((3,'EEEEBEEEE'),(4,'EBBEEEBBE'),(5,'E3BEBE3BE'),(6,'EEEEBEEEE')):
        row=list(stand[y]); row[5:14]=pixels; stand[y]=''.join(row)
    girl = [stand]
    profile=stand.copy()
    profile[3:9]=['....AAAAABEEEEA...', '....AAAEEEBBBEA...',
                  '....AAAABBE3BEB...', '....AAAABBEEEEB...',
                  '.....AAABBBBBA....','.....AAAABBBA.....']
    def stroke(pixels,a,b,color,width=2):
        x1,y1=a; x2,y2=b; length=max(abs(x2-x1),abs(y2-y1),1)
        for i in range(length+1):
            x=round(x1+(x2-x1)*i/length); y=round(y1+(y2-y1)*i/length)
            for xx in range(x,x+width):
                if 0<=xx<18 and 0<=y<32: pixels[y][xx]=color
    # Contact, passing, opposite contact, opposite passing. Knees articulate;
    # the supporting foot stays grounded while the other foot swings forward.
    legs=[((5,27),(3,29),(12,27),(13,29)),
          ((6,26),(8,28),(10,27),(10,29)),
          ((10,27),(12,29),(7,27),(5,29)),
          ((8,27),(8,29),(12,26),(10,28))]
    for step in range(4):
        pixels=[list(row) for row in profile]
        for y in range(24,32): pixels[y]=list('.'*18)
        bk,ba,fk,fa=legs[step]
        for hip,knee,ankle,color in (((7,24),bk,ba,'9'),((10,24),fk,fa,'B')):
            stroke(pixels,hip,knee,color); stroke(pixels,knee,ankle,color)
            stroke(pixels,(ankle[0],ankle[1]+1),(ankle[0]+2,ankle[1]+1),'D')
        # One hand holds the lead; the free arm swings opposite the front leg.
        for y in range(11,18):
            for x in (3,4,13,14,15,16): pixels[y][x]='.'
        stroke(pixels,(5,12),((7,17) if step<2 else (3,17)),'9')
        stroke(pixels,(12,12),(14,15),'B'); stroke(pixels,(14,15),(15,16),'B')
        girl.append([''.join(row) for row in pixels])
    throwing = stand.copy()
    for y in range(7, 16):
        a = list(throwing[y]); a[14:18] = list('BB..' if y < 10 else 'B...'); throwing[y] = ''.join(a)
    throwing[6] = throwing[6][:14] + 'BBBB'
    girl.append(throwing)
    crouch = ['.'*18]*8 + stand[:24]
    crouch[-6:] = ['....CCCCCCCCCC....','....CCBBCCBBCC....','......BB..BB......','.....BBB..BBB.....','....DDDD..DDDD....','..................']
    girl.append(crouch)
    pat = crouch.copy()
    for y in range(17, 21):
        pat[y] = pat[y][:13] + 'BBBBB'
    girl.append(pat)
    sprites += girl
    # ball, flying disc, rope, heart; dog indices 0..8, girl 9..16.
    sprites += [['.777.','78777','77877','77777','.777.'],
                ['.888888.','77777777','.777777.'],
                ['666666666666'],
                ['.FF.FF.','FFFFFFF','FFFFFFF','.FFFFF.','..FFF..','...F...']]
    palette = ['070b0a','dc863b','f9d39a','62381f','ae5727','f5af66','81cbb6','52b6f2','c6ecff','cf9686','49313d','f9c7ac','e88fa8','493f60','b3b9c5','f78ba6']
    actions = ['walk','run','sit','ball','frisbee','tug','fetch','pet','leash','eat','sleep']
    labels = ['走路','奔跑','坐著','自己玩球','接飛盤','女孩拔河','女孩丟球／柴犬取回','女孩摸摸／陪坐','女孩遛狗','女孩餵食／吃飯','睡覺']
    frames = []
    # Each frame: dog x/y/pose/flip, girl x/pose/flip, toy x/y/sprite, leash.
    for action in actions:
        seq=[]
        count = 240 if action in ('walk','run','leash') else 120
        for t in range(count):
            p=t/count; tri=1-abs(2*p-1); flip=int(p>=.5)
            phase=(p+104/480)%1
            dx=round(8+(1-abs(2*phase-1))*240); flip=int(phase>=.5)
            dy=6; dp=(t//2)%4; gx=-32; gp=0; gf=0; tx=-32; ty=0; toy=17; leash=0
            if action=='run':
                phase=(p*2+104/480)%1; dx=round(8+(1-abs(2*phase-1))*240); flip=int(phase>=.5); dy=5-(t%4==1); dp=t%4
            elif action=='sit': dx=112; dp=7; flip=0
            elif action=='ball': dx=112; dp=8; flip=0; tx=146+round(8*math.sin(p*math.tau)); ty=22-round(10*abs(math.sin(p*math.tau)))
            elif action=='frisbee':
                gx=12; gp=5 if t<20 else 0; dx=round(32+tri*198); flip=int(p>=.5); dp=t%4; dy=3 if 42<t<60 else 6; tx=round(30+tri*228); ty=round(6+20*p-9*math.sin(p*math.tau)); toy=18
                if t>=60: tx=dx-4; ty=19
            elif action=='tug': gx=100; gp=6; dx=124+(t//8)%2*2; dp=8; flip=1; tx=116; ty=22; toy=19
            elif action=='fetch':
                gx=12; gp=5 if t<16 else 0; dx=round(32+tri*208); flip=int(p>=.5); dp=t%4; tx=round(30+min(t/48,1)*234); ty=round(5+22*(min(t/48,1)**2))
                if t>=60: tx=dx-4; ty=19
            elif action=='pet': gx=110; gp=7 if (t//16)%2==0 else 6; dx=118; dp=7; flip=1
            elif action=='leash':
                # Pause the dog at each end while the girl walks around behind it.
                # No instantaneous swap of the two characters at the turn.
                if t<96:
                    dx=round(50+t*160/96); gx=dx-34; flip=0; gf=0
                elif t<120:
                    dx=210; gx=round(176+(t-96)*74/24); flip=0; gf=0; dp=7
                elif t<216:
                    dx=round(210-(t-120)*160/96); gx=dx+40; flip=1; gf=1
                else:
                    dx=50; gx=round(90-(t-216)*74/24); flip=1; gf=1; dp=7
                gp=1+(t//3)%4; leash=1
            elif action=='eat': dx=224; dp=5+(t//5)%2; gx=202; gp=6; flip=0
            elif action=='sleep': dx=112; dp=4; dy=6; flip=0
            dy += 5  # keep all paws on the same baseline after scaling
            if action=='ball':
                tx=dx+round((tx-dx)*.8)
                ty=11+round((ty-6)*.8)
            elif action=='tug': ty=24
            seq.append([dx,dy,dp,flip,gx,gp,gf,tx,ty,toy,leash])
        if action in ('fetch','frisbee'):
            # Walk up to the girl, play, then return to the common daytime anchor.
            approach=[[112-t*2,11,(t//2)%4,1,12,0,0,-32,0,17,0] for t in range(40)]
            departure=[[32+t*2,11,(t//2)%4,0,12,0,0,-32,0,17,0] for t in range(40)]
            seq=approach+seq+departure
        frames.append(seq)
    data={'sprites':sprites,'palette':palette,'actions':actions,'labels':labels,'frames':frames}
    (ROOT/'tools/pet_scene.json').write_text(json.dumps(data,separators=(',',':'),ensure_ascii=False),encoding='utf-8')
    lines=['// Generated by tools/build_pet_scene.py; do not edit.','#pragma once','#include <stdint.h>',
        'enum class PetAction : uint8_t { Walk, Run, Sit, Ball, Frisbee, Tug, Fetch, Pet, Leash, Eat, Sleep };',
        'struct PetFrame { int16_t x,y,pose,flip,girlX,girlPose,girlFlip,toyX,toyY,toy,leash; };',
        'struct PetSprite { uint8_t w,h; const char *pixels; };']
    for i,s in enumerate(sprites):
        lines.append(f'static const char kPetPixels{i}[] = "'+''.join(s)+'";')
    lines.append('static const PetSprite kPetSprites[] = {'+','.join('{%d,%d,kPetPixels%d}'%(len(s[0]),len(s),i) for i,s in enumerate(sprites))+'};')
    lines.append('static constexpr uint32_t kPetPalette[] = {'+','.join('0x'+c for c in palette)+'};')
    for i,seq in enumerate(frames):
        lines.append('static const PetFrame kPetFrames%d[] = {'%i+','.join('{'+','.join(map(str,f))+'}' for f in seq)+'};')
    lines.append('static const PetFrame *const kPetActions[] = {'+','.join('kPetFrames%d'%i for i in range(len(actions)))+'};')
    lines.append('static constexpr uint16_t kPetFrameCounts[] = {'+','.join(str(len(s)) for s in frames)+'};')
    (ROOT/'home_pet_data.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(f'{len(actions)} actions, {sum(map(len,frames))} shared frames')


if __name__=='__main__': generate()
