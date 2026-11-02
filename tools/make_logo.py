#!/usr/bin/env python3
"""Gera src/sprites/Logo.png a partir de src/art/logo.png: o logotipo PONTIFICE recortado
(sem as margens brancas) com fundo transparente, para o menu.

1. Lê o PNG (aceita paleta, cinza, RGB e RGBA; só biblioteca padrão).
2. Fundo branco ligado às bordas vira transparente (preenchimento pelos pixels quase brancos,
   com a borda das letras suavizada pela claridade).
3. Recorta à caixa dos pixels opacos, com MARGIN px de folga.

Uso: python3 tools/make_logo.py
"""

import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src", "art", "logo.png")
OUT = os.path.join(ROOT, "src", "sprites", "Logo.png")
WHITE = 236
MARGIN = 8

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_torch import write_png_rgba  # noqa: E402


def read_png_rgba(data):
    """PNG 8 bits sem interlace, tipos 0, 2, 3 (paleta + tRNS), 4 e 6. Devolve (w, h, RGBA)."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "nao e PNG"
    pos = 8
    idat = []
    palette = b""
    trns = b""
    width = height = color_type = 0
    while pos < len(data):
        length, ctype = struct.unpack_from(">I4s", data, pos)
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack_from(">IIBB", chunk, 0)
            assert bit_depth == 8 and chunk[12] == 0, "PNG precisa ser 8 bits sem interlace"
        elif ctype == b"PLTE":
            palette = chunk
        elif ctype == b"tRNS":
            trns = chunk
        elif ctype == b"IDAT":
            idat.append(chunk)
        elif ctype == b"IEND":
            break
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color_type]
    raw = zlib.decompress(b"".join(idat))
    stride = width * channels
    prev = bytearray(stride)
    src = 0
    rows = []
    for _ in range(height):
        f = raw[src]
        src += 1
        line = bytearray(raw[src:src + stride])
        src += stride
        if f == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 255
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 255
        rows.append(line)
        prev = line
    rgba = bytearray(width * height * 4)
    o = 0
    for line in rows:
        for x in range(width):
            i = x * channels
            if color_type == 3:
                idx = line[i]
                r, g, b = palette[idx * 3:idx * 3 + 3]
                a = trns[idx] if idx < len(trns) else 255
            elif color_type == 6:
                r, g, b, a = line[i:i + 4]
            elif color_type == 2:
                r, g, b = line[i:i + 3]
                a = 255
            elif color_type == 4:
                r = g = b = line[i]
                a = line[i + 1]
            else:
                r = g = b = line[i]
                a = 255
            rgba[o:o + 4] = bytes((r, g, b, a))
            o += 4
    return width, height, rgba


def remove_background(w, h, rgba):
    seen = bytearray(w * h)
    stack = []

    def bright(i):
        return min(rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2]) >= WHITE or rgba[i * 4 + 3] == 0

    def push(x, y):
        if 0 <= x < w and 0 <= y < h:
            i = y * w + x
            if not seen[i] and bright(i):
                seen[i] = 1
                stack.append(i)

    for x in range(w):
        push(x, 0)
        push(x, h - 1)
    for y in range(h):
        push(0, y)
        push(w - 1, y)
    while stack:
        i = stack.pop()
        x, y = i % w, i // w
        push(x + 1, y)
        push(x - 1, y)
        push(x, y + 1)
        push(x, y - 1)
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if seen[i]:
                rgba[i * 4 + 3] = 0
                continue
            edge = (x == 0 or seen[i - 1]) or (x == w - 1 or seen[i + 1]) or \
                   (y == 0 or seen[i - w]) or (y == h - 1 or seen[i + w])
            if edge:
                lum = (rgba[i * 4] + rgba[i * 4 + 1] + rgba[i * 4 + 2]) // 3
                rgba[i * 4 + 3] = max(40, min(255, 255 - (lum - 140) * 2))


def crop(w, h, rgba):
    xs = [x for y in range(h) for x in range(w) if rgba[(y * w + x) * 4 + 3] > 20]
    ys = [y for y in range(h) for x in range(w) if rgba[(y * w + x) * 4 + 3] > 20]
    x0, x1 = max(0, min(xs) - MARGIN), min(w - 1, max(xs) + MARGIN)
    y0, y1 = max(0, min(ys) - MARGIN), min(h - 1, max(ys) + MARGIN)
    cw, ch = x1 - x0 + 1, y1 - y0 + 1
    out = bytearray(cw * ch * 4)
    for y in range(ch):
        s = ((y0 + y) * w + x0) * 4
        out[y * cw * 4:(y + 1) * cw * 4] = rgba[s:s + cw * 4]
    return cw, ch, out


def main():
    with open(SRC, "rb") as f:
        w, h, rgba = read_png_rgba(f.read())
    remove_background(w, h, rgba)
    cw, ch, out = crop(w, h, rgba)
    write_png_rgba(OUT, cw, ch, out)
    print(f"gravado {OUT} ({cw}x{ch}, de {w}x{h})")


if __name__ == "__main__":
    main()
