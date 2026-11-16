"""Pack approved 296x32 preview frames without changing their visible pixels."""
import argparse
import base64
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def decode(sprite):
    raw = base64.b64decode(sprite['rle'])
    assert len(raw) % 2 == 0
    pixels = ''.join(('.' if color == 16 else format(color, 'X')) * count
                     for count, color in zip(raw[::2], raw[1::2]))
    assert len(pixels) == sprite['w'] * sprite['h']
    return [pixels[y:y + sprite['w']] for y in range(0, len(pixels), sprite['w'])]


def pack(capture):
    sprites, actions, lookup = [], [], {}
    for action in capture['actions']:
        frames = []
        digest = hashlib.sha256()
        for t, pixels in enumerate(action['frames']):
            assert len(pixels) == 296 * 32
            digest.update(pixels.encode('ascii'))
            occupied = [i for i, c in enumerate(pixels) if c != '.']
            x0, x1 = min(i % 296 for i in occupied), max(i % 296 for i in occupied)
            y0, y1 = min(i // 296 for i in occupied), max(i // 296 for i in occupied)
            w, h = x1 - x0 + 1, y1 - y0 + 1
            crop = ''.join(pixels[y * 296 + x0:y * 296 + x1 + 1] for y in range(y0, y1 + 1))
            key = (w, h, crop)
            if key not in lookup:
                packed = bytearray()
                for c in crop:
                    color = 16 if c == '.' else int(c, 16)
                    if packed and packed[-1] == color and packed[-2] < 255:
                        packed[-2] += 1
                    else:
                        packed.extend((1, color))
                lookup[key] = len(sprites)
                sprites.append({'w': w, 'h': h, 'rle': base64.b64encode(packed).decode('ascii')})
            heart = 157
            if action['name'] == 'spin':
                heart = 164 + math.floor(17 * math.cos(min(t / 72, 1) * math.tau * 2) + .5)
            elif action['name'].startswith('bath'):
                heart = 172
            frames.append([x0, y0, lookup[key], heart])
        actions.append({'name': action['name'], 'frames': frames, 'sha256': digest.hexdigest()})
    return {'palette': capture['palette'], 'sprites': sprites, 'actions': actions}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args()
    capture = json.loads(args.capture.read_text(encoding='utf-8'))
    data = pack(capture)
    # Decode every packed image and compare the entire original frame, not only
    # its bounds or a screenshot. Transparent/background pixels count too.
    for source, action in zip(capture['actions'], data['actions']):
        for expected, (x, y, index, _) in zip(source['frames'], action['frames']):
            pixels = list('.' * (296 * 32))
            for yy, row in enumerate(decode(data['sprites'][index])):
                pixels[(y + yy) * 296 + x:(y + yy) * 296 + x + len(row)] = row
            assert ''.join(pixels) == expected, (action['name'], index)
    target = ROOT / 'tools/pet_daily_clips.json'
    target.write_text(json.dumps(data, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    print(f"PASS: {sum(len(a['frames']) for a in data['actions'])} approved frames, "
          f"{len(data['sprites'])} RLE sprites, pixel-exact roundtrip; {target.stat().st_size} bytes")
