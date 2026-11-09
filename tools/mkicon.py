#!/usr/bin/env python3
"""Make icon.bmp (the game icon shown by Twilight Menu++ / flashcart menus) from any PNG.

Usage: python3 tools/mkicon.py <input.png> [icon.bmp]
Needs Pillow. The DS banner icon is 32x32 pixels, 16 colours (4 bits per pixel); palette entry 0 is the transparent colour.
The Makefile passes icon.bmp to ndstool (GAME_ICON).
"""
import struct, sys
from PIL import Image

def make(src, dst, dither=False):
    im = Image.open(src).convert("RGBA")
    bbox = im.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
    if bbox:
        im = im.crop(bbox)
    # fit into 32x32 keeping the aspect ratio, centred
    w, h = im.size
    s = 32.0 / max(w, h)
    nw, nh = max(1, round(w * s)), max(1, round(h * s))
    small = im.convert("RGBa").resize((nw, nh), Image.LANCZOS).convert("RGBA")   # premultiplied resize: no dark fringes
    canvas = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    canvas.paste(small, ((32 - nw) // 2, (32 - nh) // 2))
    px = canvas.load()
    opaque = [(x, y) for y in range(32) for x in range(32) if px[x, y][3] >= 128]
    # quantise the opaque pixels to 15 colours (index 0 is kept for transparency)
    rgb = Image.new("RGB", (32, 32))
    avg = tuple(sum(px[p][i] for p in opaque) // max(1, len(opaque)) for i in range(3))
    rp = rgb.load()
    for y in range(32):
        for x in range(32):
            rp[x, y] = px[x, y][:3] if px[x, y][3] >= 128 else avg
    q = rgb.quantize(colors=15, method=Image.Quantize.MEDIANCUT,
                     dither=Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE)
    pal = q.getpalette()[:45]
    qp = q.load()
    idx = [[0] * 32 for _ in range(32)]
    for y in range(32):
        for x in range(32):
            idx[y][x] = (qp[x, y] + 1) if px[x, y][3] >= 128 else 0
    # write a 4-bit BMP (bottom-up rows, BGR0 palette)
    palette = [(255, 0, 255)] + [tuple(pal[i * 3:i * 3 + 3]) for i in range(15)]   # entry 0 = transparent (magenta in the file)
    rows = bytearray()
    for y in range(31, -1, -1):
        for x in range(0, 32, 2):
            rows.append((idx[y][x] << 4) | idx[y][x + 1])
    pal_bytes = b"".join(struct.pack("BBBB", b, g, r, 0) for r, g, b in palette)
    off = 14 + 40 + len(pal_bytes)
    hdr = b"BM" + struct.pack("<IHHI", off + len(rows), 0, 0, off)
    dib = struct.pack("<IiiHHIIiiII", 40, 32, 32, 1, 4, 0, len(rows), 2835, 2835, 16, 16)
    open(dst, "wb").write(hdr + dib + pal_bytes + bytes(rows))
    return idx, palette

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    make(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "icon.bmp")
    print("written", sys.argv[2] if len(sys.argv) > 2 else "icon.bmp")
