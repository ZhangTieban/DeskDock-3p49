"""Four-character clips composed from the firmware's existing pixel sprites."""
import math


def encode(rows):
    packed=bytearray()
    for row in rows:
        for c in row:
            color=16 if c=='.' else int(c,16)
            if packed and packed[-1]==color and packed[-2]<255: packed[-2]+=1
            else: packed.extend((1,color))
    return packed


def build_visit(sprites):
    # Blue dress, lighter hair and a dark Shiba distinguish the visiting pair.
    def girl(index): return [row.translate(str.maketrans({'C':'7','A':'3'})) for row in sprites[index]]
    def dog(index): return [row.translate(str.maketrans({'1':'A','4':'3','5':'4'})) for row in sprites[index]]
    images=[];lookup={};clips=[]
    for action in ('arrive','play','rest','leave'):
        seq=[]
        for tick in range(300 if action in ('arrive','leave') else 240):
            frame=[list('.'*296) for _ in range(32)]
            def draw(rows,x,y=0,flip=False):
                for yy,row in enumerate(rows):
                    for xx,c in enumerate(row[::-1] if flip else row):
                        if c!='.' and 0<=x+xx<296 and 0<=y+yy<32:frame[y+yy][x+xx]=c
            def lead(x1,y1,x2,y2):
                n=max(abs(x2-x1),abs(y2-y1))
                for i in range(n+1):frame[round(y1+(y2-y1)*i/max(1,n))][round(x1+(x2-x1)*i/max(1,n))]='6'
            ax,bx=112,160;ap,bp=7,7;af,bf=False,True;gy=244;gp=9
            if action in ('arrive','leave'):
                # 4.8-second approach, a greeting pause, then a settled pose.
                progress=min(tick/60,1) if action=='arrive' else max(0,(tick-240)/60)
                if action=='arrive':
                    gy=round(270-26*progress);bx=round(236-76*progress)
                    walking=tick<60;gf=True
                else:
                    gy=round(244+26*progress);bx=round(160+76*progress)
                    walking=tick>=240;gf=False
                gp=10+(tick//3)%4 if walking else 9
                bp=(tick//3)%4 if walking else 7;bf=gf
                lead(gy+(3 if gf else 15),16,bx+(8 if bf else 24),21)
            elif action=='play':
                if tick<160:
                    phase=tick/160*math.tau;delta=round(34*math.sin(phase))
                    ax+=delta;bx+=delta;af=bf=math.cos(phase)<0
                    ap=(tick//2)%4;bp=((tick+2)//2)%4
                    draw(sprites[17],ax+(-6 if af else 35),25)
                else:
                    ap=8 if tick%24<12 else 7;bp=7 if tick%24<12 else 8
                    draw(sprites[17],round(146+5*math.sin((tick-160)/80*math.tau)),25)
            else:
                # Sit, blink and watch each other; play is interspersed with rest.
                ap=7;bp=7
            draw(sprites[9],44)
            draw(girl(gp),gy,flip=(gf if action in ('arrive','leave') else True))
            draw(sprites[ap],ax,11,af);draw(dog(bp),bx,11,bf)
            occupied=[(x,y) for y,row in enumerate(frame) for x,c in enumerate(row) if c!='.']
            x0=min(x for x,y in occupied);x1=max(x for x,y in occupied)
            y0=min(y for x,y in occupied);y1=max(y for x,y in occupied)
            rows=tuple(''.join(frame[y][x0:x1+1]) for y in range(y0,y1+1))
            assert x1-x0+1<256
            if rows not in lookup:lookup[rows]=len(images);images.append(list(rows))
            seq.append([x0,y0,lookup[rows],ax+12])
        clips.append(seq)
    return images,clips
