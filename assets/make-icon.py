#!/usr/bin/env python3
"""Build ICNS with legacy RGB/masks for small sizes and PNG for Retina.

JUCE emits PNG inside icp4, which Apple's decoder interprets as ARGB data.
Use the documented legacy RGB channels plus alpha mask at 16/32/48px.
Only Python stdlib and macOS sips are needed.
"""
from pathlib import Path
import struct
import subprocess
import tempfile


def run(*args):
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL)


def bitmap(path, output):
    run('/usr/bin/sips', '-s', 'format', 'bmp', str(path), '--out', str(output))
    data = output.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height, planes, depth = struct.unpack_from('<iiHH', data, 18)
    assert planes == 1 and depth == 32
    rows = [data[offset + y * width * 4:offset + (y + 1) * width * 4] for y in range(abs(height))]
    if height > 0:
        rows.reverse()
    return b''.join(rows)


def chunk(kind, data):
    return kind.encode('ascii') + struct.pack('>I', len(data) + 8) + data


def channel(data):
    # Literal packets of up to 128 bytes in Apple's channel RLE format.
    return b''.join(bytes([len(data[i:i+128]) - 1]) + data[i:i+128] for i in range(0, len(data), 128))


def main():
    root = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix='vini-icon-') as folder:
        temp = Path(folder)
        images = {}
        for size in (16, 32, 48, 64, 128, 256, 512, 1024):
            images[size] = temp / f'{size}.png'
            run('/usr/bin/sips', '-z', str(size), str(size), str(root / 'ViniStage.png'), '--out', str(images[size]))
        parts = []
        for size, rgb, mask in ((16, 'is32', 's8mk'), (32, 'il32', 'l8mk'), (48, 'ih32', 'h8mk')):
            pixels = bitmap(images[size], temp / f'{size}.bmp')  # BGRA, top-down
            parts.append(chunk(rgb, b''.join(channel(pixels[c::4]) for c in (2, 1, 0))))
            parts.append(chunk(mask, pixels[3::4]))
        for kind, size in (('ic07',128), ('ic08',256), ('ic09',512), ('ic10',1024), ('ic11',32), ('ic12',64), ('ic13',256), ('ic14',512)):
            parts.append(chunk(kind, images[size].read_bytes()))
        body = b''.join(parts)
        (root / 'ViniStage.icns').write_bytes(b'icns' + struct.pack('>I', len(body)+8) + body)


if __name__ == '__main__':
    main()
