#!/usr/bin/env python3
"""Gera src/models/Arena.glb a partir de src/models/ArenaRaw.glb e src/art/Arena.png.

O modelo bruto não tem UV nem material. A arte é uma ilustração em perspectiva, não um
atlas, então a texturização é feita assim:

1. Os elementos vêm do sprite sheet plano src/art/ArenaAtlas3.png: ladrilhos claros e
   escuros, painel de madeira emoldurado, banda de metal rebitada, tábuas, cruz e placa
   com flor-de-lis. (A busca de amostras em Arena.png continua disponível como apoio.)
2. Um atlas 2048x2048 é montado com três regiões: topo (tabuleiro 5x2 por lado, faixa
   central para a canaleta, moldura rebitada e ornamentos), laterais (painel de
   madeira com cruz e painel liso) e fundo (tábuas).
3. Cada triângulo recebe UV conforme a orientação da face: topo em projeção planar XZ,
   laterais pela posição ao longo da borda e altura, fundo em projeção planar.
4. O resultado é gravado como GLB com o atlas PNG embutido.

Por padrão a geometria é construída proceduralmente (ver build_arena_mesh): duas plataformas
elevadas (lado do oponente atrás, lado do jogador na frente), cada uma com 5 colunas x 2
linhas de ladrilhos quadrados, separadas por uma canaleta central da largura de um ladrilho e
rebaixada 0.2 ladrilho;
o conjunto é um prisma trapezoidal (topo menor, base retangular maior, laterais com altura de um ladrilho
e base avançando ARENA_SIDE_RUN além do topo em cada lado) com painel de madeira nas laterais e cantoneiras nas quinas. Com --raw, usa a
malha bruta ArenaRaw.glb classificando as faces pela normal.

Só usa a biblioteca padrão do Python. Uso: python3 tools/texture_arena.py [--raw]
"""

import json
import math
import os
import random
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW_GLB = os.path.join(ROOT, "src", "models", "ArenaRaw.glb")
ART_PNG = os.path.join(ROOT, "src", "art", "Arena.png")
SHEET_PNG = os.path.join(ROOT, "src", "art", "ArenaAtlas3.png")  # sprite sheet plano
OUT_GLB = os.path.join(ROOT, "src", "models", "Arena.glb")
OUT_ATLAS = os.path.join(ROOT, "src", "art", "ArenaAtlas.png")

ATLAS = 2048
TOP_H = 1024      # linhas 0..1023: topo
SIDE_H = 512      # linhas 1024..1535: laterais
SIDE_CROSS_H = 256  # linhas 1024..1279: painel de madeira com cruz (frente e fundo)
SIDE_PLAIN_H = 256  # linhas 1280..1535: painel de madeira liso (laterais em duas metades)
BOTTOM_H = 512    # linhas 1536..2047: fundo

COLS, ROWS_PER_SIDE = 5, 2
RIDGE_FRACTION = 0.07
GROUT = 3

# Recortes em ArenaAtlas3.png (x0, y0, x1, y1), medidos por detecção de componentes.
SHEET_TILE_COLS = [(10, 149), (153, 292), (296, 434), (438, 576), (580, 718)]
SHEET_TILE_ROWS = [(10, 151), (156, 297), (301, 441), (446, 585), (589, 728)]
SHEET_DARK_PARITY = 0            # (coluna + linha) par = ladrilho escuro
SHEET_PANEL = (732, 12, 1244, 120)       # painel de madeira emoldurado
SHEET_BAND = (736, 632, 1448, 668)       # banda de metal rebitada
SHEET_PLANKS = (12, 736, 880, 1008)      # bloco de tábuas
SHEET_CROSS = (1264, 372, 1396, 504)     # cruz grande
SHEET_PLATE = (1396, 876, 1524, 1008)    # placa quadrada com flor-de-lis

# Regiões da ilustração onde procurar amostras (x0, y0, x1, y1 em pixels). As amostras em si
# são escolhidas automaticamente: janelas sem rejunte, pouca variação e cor esperada.
TILE_SEARCH = (420, 50, 1050, 330)   # superfície de pedra, longe das bordas
WOOD_SEARCH = (250, 360, 950, 620)   # lateral de madeira
STONE_WINDOW = (104, 52)             # ladrilhos são largos e achatados pela perspectiva
WOOD_WINDOW = (150, 90)
SWATCH = 256


# ----------------------------------------------------------------------------- PNG

def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "nao e PNG"
    pos = 8
    idat = []
    width = height = 0
    color_type = bit_depth = 0
    while pos < len(data):
        length, ctype = struct.unpack_from(">I4s", data, pos)
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack_from(">IIBB", chunk, 0)
            interlace = chunk[12]
            assert bit_depth == 8 and interlace == 0, "PNG precisa ser 8 bits sem interlace"
        elif ctype == b"IDAT":
            idat.append(chunk)
        elif ctype == b"IEND":
            break
    channels = {2: 3, 6: 4, 0: 1, 4: 2}[color_type]
    raw = zlib.decompress(b"".join(idat))
    stride = width * channels
    out = bytearray(width * height * 3)
    prev = bytearray(stride)
    src = 0
    for y in range(height):
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
        o = y * width * 3
        if channels >= 3:
            for x in range(width):
                s = x * channels
                out[o:o + 3] = line[s:s + 3]
                o += 3
        else:
            for x in range(width):
                v = line[x * channels]
                out[o:o + 3] = bytes((v, v, v))
                o += 3
        prev = line
    return width, height, out


def write_png(path, width, height, rgb):
    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        raw += rgb[y * stride:(y + 1) * stride]

    def chunk(ctype, body):
        c = struct.pack(">I", len(body)) + ctype + body
        return c + struct.pack(">I", zlib.crc32(ctype + body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 6))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)
    return png


# ----------------------------------------------------------------------------- imagem

class Image:
    def __init__(self, width, height, rgb=None, fill=(0, 0, 0)):
        self.w, self.h = width, height
        self.px = rgb if rgb is not None else bytearray(bytes(fill) * (width * height))

    def get(self, x, y):
        x = min(max(x, 0), self.w - 1)
        y = min(max(y, 0), self.h - 1)
        i = (y * self.w + x) * 3
        return self.px[i], self.px[i + 1], self.px[i + 2]

    def sample(self, fx, fy):
        x0, y0 = int(math.floor(fx)), int(math.floor(fy))
        tx, ty = fx - x0, fy - y0
        a, b, c, d = self.get(x0, y0), self.get(x0 + 1, y0), self.get(x0, y0 + 1), self.get(x0 + 1, y0 + 1)
        return tuple(int(
            a[k] * (1 - tx) * (1 - ty) + b[k] * tx * (1 - ty) + c[k] * (1 - tx) * ty + d[k] * tx * ty)
            for k in range(3))

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            i = (y * self.w + x) * 3
            self.px[i], self.px[i + 1], self.px[i + 2] = c

    def fill_rect(self, x0, y0, x1, y1, c):
        x0, y0 = max(x0, 0), max(y0, 0)
        x1, y1 = min(x1, self.w), min(y1, self.h)
        row = bytes(c) * (x1 - x0)
        for y in range(y0, y1):
            i = (y * self.w + x0) * 3
            self.px[i:i + len(row)] = row


class Integral:
    """Imagens integrais para média, desvio e contagens em janelas, em O(1) por janela."""

    def __init__(self, img, region):
        self.x0, self.y0, self.x1, self.y1 = region
        w, h = self.x1 - self.x0, self.y1 - self.y0
        self.w, self.h = w, h
        self.lum = [[0.0] * (w + 1) for _ in range(h + 1)]
        self.lum2 = [[0.0] * (w + 1) for _ in range(h + 1)]
        self.dark = [[0] * (w + 1) for _ in range(h + 1)]
        self.rb = [[0.0] * (w + 1) for _ in range(h + 1)]
        for j in range(h):
            row_l = self.lum[j + 1]
            row_l2 = self.lum2[j + 1]
            row_d = self.dark[j + 1]
            row_rb = self.rb[j + 1]
            acc_l = acc_l2 = acc_rb = 0.0
            acc_d = 0
            for i in range(w):
                r, g, b = img.get(self.x0 + i, self.y0 + j)
                lum = 0.299 * r + 0.587 * g + 0.114 * b
                acc_l += lum
                acc_l2 += lum * lum
                acc_d += 1 if lum < 55 else 0
                acc_rb += r - b
                row_l[i + 1] = self.lum[j][i + 1] + acc_l
                row_l2[i + 1] = self.lum2[j][i + 1] + acc_l2
                row_d[i + 1] = self.dark[j][i + 1] + acc_d
                row_rb[i + 1] = self.rb[j][i + 1] + acc_rb

    def _sum(self, table, x, y, w, h):
        return table[y + h][x + w] - table[y][x + w] - table[y + h][x] + table[y][x]

    def stats(self, x, y, w, h):
        n = float(w * h)
        mean = self._sum(self.lum, x, y, w, h) / n
        var = max(self._sum(self.lum2, x, y, w, h) / n - mean * mean, 0.0)
        dark = self._sum(self.dark, x, y, w, h) / n
        rb = self._sum(self.rb, x, y, w, h) / n
        return mean, math.sqrt(var), dark, rb


def find_swatch(img, region, window, score, step=6):
    """Devolve o quad (TL, TR, BR, BL) da janela com maior pontuação na região.

    Se nenhuma janela passar nos critérios, relaxa os limites e tenta de novo."""
    integ = Integral(img, region)
    ww, wh = window
    best, best_score = None, -1e9
    for relax in (1.0, 1.3, 1.7, 2.2, 3.0):
        for y in range(0, integ.h - wh, step):
            for x in range(0, integ.w - ww, step):
                sc = score(*integ.stats(x, y, ww, wh), relax)
                if sc > best_score:
                    best_score, best = sc, (x, y)
        if best is not None:
            break
        print("  nenhuma janela com relax=%.1f, relaxando" % relax)
    assert best is not None, "nenhuma amostra encontrada"
    x, y = best
    ax, ay = integ.x0 + x, integ.y0 + y
    print("  amostra em (%d,%d) %dx%d, pontuacao %.1f" % (ax, ay, ww, wh, best_score))
    return [(ax, ay), (ax + ww, ay), (ax + ww, ay + wh), (ax, ay + wh)]


def score_light_stone(mean, std, dark, rb, relax=1.0):
    if dark > 0.005 * relax or std > 24 * relax:
        return -1e9
    return mean - std * 0.5


def score_dark_stone(mean, std, dark, rb, relax=1.0):
    if dark > 0.02 * relax or std > 26 * relax or not 60 <= mean <= 118:
        return -1e9
    return 100 - std - abs(mean - 92) * 0.3


def score_wood(mean, std, dark, rb, relax=1.0):
    # Madeira escura: boa parte dos pixels fica abaixo do limiar de "rejunte", então o
    # critério é só cor (vermelho acima do azul) e pouca variação.
    if rb < 30 / relax:
        return -1e9
    return rb - std * 1.2


def rectify(src, quad, size):
    """Recorta um quadrilátero da imagem fonte para um quadrado, por interpolação bilinear."""
    (ax, ay), (bx, by), (cx, cy), (dx, dy) = quad
    out = Image(size, size)
    for j in range(size):
        t = (j + 0.5) / size
        for i in range(size):
            s = (i + 0.5) / size
            fx = (1 - s) * (1 - t) * ax + s * (1 - t) * bx + s * t * cx + (1 - s) * t * dx
            fy = (1 - s) * (1 - t) * ay + s * (1 - t) * by + s * t * cy + (1 - s) * t * dy
            out.set(i, j, src.sample(fx, fy))
    return out


def blit_scaled(dst, swatch, x0, y0, x1, y1, flip_x=False, flip_y=False, rot90=False, shade=1.0):
    w, h = x1 - x0, y1 - y0
    if w <= 0 or h <= 0:
        return
    for j in range(h):
        v = (j + 0.5) / h
        for i in range(w):
            u = (i + 0.5) / w
            su, sv = (v, u) if rot90 else (u, v)
            if flip_x:
                su = 1 - su
            if flip_y:
                sv = 1 - sv
            c = swatch.sample(su * (swatch.w - 1), sv * (swatch.h - 1))
            if shade != 1.0:
                c = tuple(min(255, int(k * shade)) for k in c)
            dst.set(x0 + i, y0 + j, c)


def crop(src, box):
    x0, y0, x1, y1 = box
    out = Image(x1 - x0, y1 - y0)
    for j in range(y1 - y0):
        for i in range(x1 - x0):
            out.set(i, j, src.get(x0 + i, y0 + j))
    return out


def blit_keyed(dst, sprite, x0, y0, x1, y1, key=28, rot90=False):
    """Como blit_scaled, mas pixels quase pretos do sprite são transparentes."""
    w, h = x1 - x0, y1 - y0
    if w <= 0 or h <= 0:
        return
    for j in range(h):
        v = (j + 0.5) / h
        for i in range(w):
            u = (i + 0.5) / w
            su, sv = (v, 1 - u) if rot90 else (u, v)
            c = sprite.sample(su * (sprite.w - 1), sv * (sprite.h - 1))
            if c[0] + c[1] + c[2] < key * 3:
                continue
            dst.set(x0 + i, y0 + j, c)


def tile_horizontal(dst, sprite, x0, y0, x1, y1, piece_w, rot90=False):
    """Repete o sprite ao longo de X (ou de Y, com rot90). O número de pedaços é
    arredondado para que todos tenham o mesmo tamanho, sem sobra comprimida no fim."""
    length = (y1 - y0) if rot90 else (x1 - x0)
    if length <= 0:
        return
    pieces = max(1, round(length / piece_w))
    for k in range(pieces):
        a = int(length * k / pieces)
        b = int(length * (k + 1) / pieces)
        if not rot90:
            blit_scaled(dst, sprite, x0 + a, y0, x0 + b, y1)
        else:
            blit_scaled(dst, sprite, x0, y0 + a, x1, y0 + b, rot90=True)


def metal_fill(dst, x0, y0, x1, y1, rng, base=(74, 76, 82), rivets=False, rivet_step=64):
    for y in range(max(y0, 0), min(y1, dst.h)):
        for x in range(max(x0, 0), min(x1, dst.w)):
            n = rng.randint(-10, 10)
            # arranhões horizontais sutis
            streak = 6 if (x * 7 + y * 3) % 23 == 0 else 0
            dst.set(x, y, tuple(min(255, max(0, k + n + streak)) for k in base))
    if rivets:
        cy = (y0 + y1) // 2
        for cx in range(x0 + rivet_step // 2, x1, rivet_step):
            for dy in range(-7, 8):
                for dx in range(-7, 8):
                    d = math.hypot(dx, dy)
                    if d <= 7:
                        light = 1.35 - 0.6 * (dy + 7) / 14.0
                        if d > 5.5:
                            light *= 0.6
                        c = tuple(min(255, int(k * light)) for k in base)
                        dst.set(cx + dx, cy + dy, c)


def bevel_edge(dst, x0, y0, x1, y1, thickness, light=(1.25, 1.25, 1.25), dark=(0.55, 0.55, 0.55)):
    """Escurece a borda inferior/direita e clareia a superior/esquerda de um retângulo."""
    for t in range(thickness):
        f = 1.0 - t / thickness
        for x in range(x0, x1):
            for y, mul in ((y0 + t, light), (y1 - 1 - t, dark)):
                c = dst.get(x, y)
                dst.set(x, y, tuple(min(255, int(c[k] * (1 + (mul[k] - 1) * f))) for k in range(3)))
        for y in range(y0, y1):
            for x, mul in ((x0 + t, light), (x1 - 1 - t, dark)):
                c = dst.get(x, y)
                dst.set(x, y, tuple(min(255, int(c[k] * (1 + (mul[k] - 1) * f))) for k in range(3)))


# ----------------------------------------------------------------------------- GLB

def read_glb(path):
    data = open(path, "rb").read()
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    assert magic == b"glTF" and version == 2
    json_len, json_type = struct.unpack_from("<I4s", data, 12)
    gltf = json.loads(data[20:20 + json_len])
    bin_off = 20 + json_len
    bin_len, bin_type = struct.unpack_from("<I4s", data, bin_off)
    assert bin_type == b"BIN\x00"
    return gltf, data[bin_off + 8:bin_off + 8 + bin_len]


def read_accessor(gltf, bin_data, index):
    acc = gltf["accessors"][index]
    view = gltf["bufferViews"][acc["bufferView"]]
    offset = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3}[acc["type"]]
    fmt = {5126: "f", 5125: "I", 5123: "H"}[acc["componentType"]]
    size = struct.calcsize(fmt) * comps
    stride = view.get("byteStride", size)
    out = []
    for i in range(acc["count"]):
        out.append(struct.unpack_from("<" + fmt * comps, bin_data, offset + i * stride))
    return out


def pad4(b, fill=b"\x00"):
    return b + fill * ((4 - len(b) % 4) % 4)


def write_glb(path, positions, uvs, indices, png_bytes):
    pos_bytes = b"".join(struct.pack("<fff", *p) for p in positions)
    uv_bytes = b"".join(struct.pack("<ff", *t) for t in uvs)
    idx_bytes = b"".join(struct.pack("<I", i) for i in indices)
    views = []
    blob = bytearray()

    def add_view(b, target=None):
        start = len(blob)
        blob.extend(pad4(b))
        v = {"buffer": 0, "byteOffset": start, "byteLength": len(b)}
        if target:
            v["target"] = target
        views.append(v)
        return len(views) - 1

    v_idx = add_view(idx_bytes, 34963)
    v_pos = add_view(pos_bytes, 34962)
    v_uv = add_view(uv_bytes, 34962)
    v_img = add_view(png_bytes)

    mins = [min(p[k] for p in positions) for k in range(3)]
    maxs = [max(p[k] for p in positions) for k in range(3)]
    gltf = {
        "asset": {"version": "2.0", "generator": "PONTIFICE tools/texture_arena.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Arena", "mesh": 0}],
        "meshes": [{"name": "Arena", "primitives": [{
            "attributes": {"POSITION": 1, "TEXCOORD_0": 2}, "indices": 0, "mode": 4, "material": 0}]}],
        "materials": [{"name": "ArenaAtlas", "doubleSided": True,
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                "metallicFactor": 0.1, "roughnessFactor": 0.8}}],
        "textures": [{"source": 0, "sampler": 0}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 33071, "wrapT": 33071}],
        "images": [{"bufferView": v_img, "mimeType": "image/png", "name": "ArenaAtlas"}],
        "accessors": [
            {"bufferView": v_idx, "componentType": 5125, "count": len(indices), "type": "SCALAR",
             "min": [0], "max": [len(positions) - 1]},
            {"bufferView": v_pos, "componentType": 5126, "count": len(positions), "type": "VEC3",
             "min": mins, "max": maxs},
            {"bufferView": v_uv, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
        ],
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    json_bytes = pad4(json.dumps(gltf, separators=(",", ":")).encode("utf-8"), b" ")
    bin_bytes = bytes(blob)
    total = 12 + 8 + len(json_bytes) + 8 + len(bin_bytes)
    with open(path, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, total))
        f.write(struct.pack("<I4s", len(json_bytes), b"JSON"))
        f.write(json_bytes)
        f.write(struct.pack("<I4s", len(bin_bytes), b"BIN\x00"))
        f.write(bin_bytes)


# ----------------------------------------------------------------------------- atlas

def build_atlas(sheet, field_rect, ridge_rows, rng, wall_fraction=0.14):
    """Monta o atlas a partir do sprite sheet ArenaAtlas3.png.

    field_rect: retângulo do tabuleiro (os dois níveis) na região do topo, em pixels.
    ridge_rows: (y0, y1) em pixels da faixa central da canaleta (parede, piso, parede).
    wall_fraction: fração da faixa ocupada por cada parede."""
    atlas = Image(ATLAS, ATLAS, fill=(40, 36, 34))
    tiles = {0: [], 1: []}
    for j, (ty0, ty1) in enumerate(SHEET_TILE_ROWS):
        for i, (tx0, tx1) in enumerate(SHEET_TILE_COLS):
            parity = (i + j) % 2
            tiles[0 if parity == SHEET_DARK_PARITY else 1].append(crop(sheet, (tx0, ty0, tx1, ty1)))
    dark_tiles, light_tiles = tiles[0], tiles[1]
    panel = crop(sheet, SHEET_PANEL)
    band = crop(sheet, SHEET_BAND)
    planks = crop(sheet, SHEET_PLANKS)
    cross = crop(sheet, SHEET_CROSS)
    plate = crop(sheet, SHEET_PLATE)

    # --- topo: fundo metálico, moldura rebitada, ladrilhos, canal e ornamentos
    metal_fill(atlas, 0, 0, ATLAS, TOP_H, rng, base=(58, 60, 66))
    fx0, fy0, fx1, fy1 = field_rect
    ridge_y0, ridge_y1 = ridge_rows
    frame = 40
    band_len = int(band.w * (frame / band.h))  # mantém a proporção da banda
    tile_horizontal(atlas, band, fx0 - frame, fy0 - frame, fx1 + frame, fy0, band_len)
    tile_horizontal(atlas, band, fx0 - frame, fy1, fx1 + frame, fy1 + frame, band_len)
    tile_horizontal(atlas, band, fx0 - frame, fy0, fx0, fy1, band_len, rot90=True)
    tile_horizontal(atlas, band, fx1, fy0, fx1 + frame, fy1, band_len, rot90=True)

    col_w = (fx1 - fx0) / COLS
    for side in range(2):
        y_start = fy0 if side == 0 else ridge_y1
        y_end = ridge_y0 if side == 0 else fy1
        row_h = (y_end - y_start) / ROWS_PER_SIDE
        for r in range(ROWS_PER_SIDE):
            for c in range(COLS):
                x0 = int(fx0 + c * col_w)
                x1 = int(fx0 + (c + 1) * col_w)
                y0 = int(y_start + r * row_h)
                y1 = int(y_start + (r + 1) * row_h)
                pool = light_tiles if (c + r + side * ROWS_PER_SIDE) % 2 == 0 else dark_tiles
                blit_scaled(atlas, rng.choice(pool), x0, y0, x1, y1,
                            flip_x=rng.random() < 0.5, flip_y=rng.random() < 0.5)

    # faixa central (canaleta): paredes com ladrilhos escuros pequenos e banda de metal no
    # lábio superior; piso com ladrilhos escuros maiores e cruz ao centro
    ch_h = ridge_y1 - ridge_y0
    wall = max(int(ch_h * wall_fraction), 12)
    floor_y0, floor_y1 = ridge_y0 + wall, ridge_y1 - wall

    def dark_row(y0, y1, size, shade):
        x = fx0
        while x < fx1:
            blit_scaled(atlas, rng.choice(dark_tiles), x, y0, min(x + size, fx1), y1, shade=shade)
            x += size

    dark_row(ridge_y0, floor_y0, wall, 0.75)
    dark_row(floor_y1, ridge_y1, wall, 0.75)
    dark_row(floor_y0, floor_y1, floor_y1 - floor_y0, 0.85)
    lip = max(int(wall * 0.3), 10)
    lip_len = int(band.w * (lip / band.h))
    tile_horizontal(atlas, band, fx0, ridge_y0, fx1, ridge_y0 + lip, lip_len)
    tile_horizontal(atlas, band, fx0, ridge_y1 - lip, fx1, ridge_y1, lip_len)
    cross_h = int((floor_y1 - floor_y0) * 0.8)
    cross_w = int(cross_h * cross.w / cross.h)
    cx = (fx0 + fx1) // 2
    cy = (floor_y0 + floor_y1) // 2
    blit_keyed(atlas, cross, cx - cross_w // 2, cy - cross_h // 2, cx + cross_w // 2,
               cy + cross_h // 2)

    # placas com flor-de-lis nos quatro cantos da moldura
    ps = int(frame * 2.2)
    for (px, py) in ((fx0 - frame, fy0 - frame), (fx1 + frame - ps, fy0 - frame),
                     (fx0 - frame, fy1 + frame - ps), (fx1 + frame - ps, fy1 + frame - ps)):
        blit_scaled(atlas, plate, px, py, px + ps, py + ps)

    # --- laterais: painel de madeira emoldurado, uma faixa com cruz ao centro e outra lisa
    side_y0 = TOP_H
    cross_y1 = side_y0 + SIDE_CROSS_H
    blit_scaled(atlas, panel, 0, side_y0, ATLAS, cross_y1)
    blit_scaled(atlas, panel, 0, cross_y1, ATLAS, cross_y1 + SIDE_PLAIN_H)
    cross_h = int(SIDE_CROSS_H * 0.62)
    cross_w = int(cross_h * cross.w / cross.h)
    cx, cy = ATLAS // 2, (side_y0 + cross_y1) // 2
    blit_keyed(atlas, cross, cx - cross_w // 2, cy - cross_h // 2, cx + cross_w // 2,
               cy + cross_h // 2)

    # --- fundo: tábuas à esquerda; placa triangular das pirâmides no canto direito
    bottom_y0 = TOP_H + SIDE_H
    piece = int(planks.w * (BOTTOM_H / planks.h))
    tile_horizontal(atlas, planks, 0, bottom_y0, TRI_BOX[0], ATLAS, piece)
    triangle = crop(sheet, SHEET_TRIANGLE)
    atlas.fill_rect(TRI_BOX[0], TRI_BOX[1], TRI_BOX[2], TRI_BOX[3], (52, 54, 60))
    blit_keyed(atlas, triangle, TRI_BOX[0], TRI_BOX[1], TRI_BOX[2], TRI_BOX[3])
    return atlas


# ----------------------------------------------------------------------------- malha

# Dimensões da arena procedural (unidades do modelo; a cena aplica a escala). Proporções da
# especificação (base 4 x 1 x 3; duas plataformas elevadas de 1.5 x 0.4 separadas por uma
# canaleta central de 1.0) reduzidas à metade, com a largura alongada para 5 colunas.
# Low poly: duas plataformas (lado do oponente, z < 0, e lado do jogador, z > 0), cada uma
# com 5 colunas x 2 linhas de ladrilhos quadrados, separadas por uma canaleta central da largura
# de um ladrilho e rebaixada 0.2 ladrilho; prisma trapezoidal: laterais planas inclinadas do
# topo até a base retangular maior (avanço ARENA_SIDE_RUN por lado), com painel de madeira
# rebaixado e cantoneiras piramidais.
# Proporções do tabuleiro (visão de cima: b, a, canaleta, a, b): ladrilho quadrado de lado
# ARENA_TILE, canaleta com a mesma largura do ladrilho e rebaixada 0.2 * ARENA_TILE.
ARENA_FRAME = 0.06      # moldura metálica em volta do tabuleiro
ARENA_TILE = 0.356      # lado do ladrilho (quadrado), em unidades do modelo
ARENA_CHANNEL_DEPTH_FRACTION = 0.2  # profundidade da canaleta em frações do ladrilho
ARENA_TOP_W = COLS * ARENA_TILE + 2 * ARENA_FRAME          # largura em X (5 colunas) = 1.90
ARENA_PLATFORM_D = ROWS_PER_SIDE * ARENA_TILE + ARENA_FRAME  # profundidade de cada plataforma em Z
ARENA_CHANNEL_W = ARENA_TILE                               # largura da canaleta central em Z
ARENA_TOP_D = 2 * ARENA_PLATFORM_D + ARENA_CHANNEL_W  # profundidade total (= ARENA_TOP_W)
ARENA_HIGH_Y = 0.30     # topo das plataformas
ARENA_LOW_Y = ARENA_HIGH_Y - ARENA_CHANNEL_DEPTH_FRACTION * ARENA_TILE  # piso da canaleta
# Prisma trapezoidal: topo retangular menor (tabuleiro), base retangular maior embaixo e
# laterais planas com altura (vertical, do topo à base) de um ladrilho. A largura da base
# (ARENA_BASE_W) é o parâmetro; o avanço por lado e o ângulo da lateral decorrem dela.
ARENA_SIDE_H = ARENA_TILE                          # altura da lateral = um ladrilho
ARENA_BASE_Y = ARENA_HIGH_Y - ARENA_SIDE_H
ARENA_BASE_W = 2.50                                # largura da base (topo = 1.90)
ARENA_SIDE_RUN = (ARENA_BASE_W - ARENA_TOP_W) / 2  # quanto cada lado da base avança além do topo
ARENA_SIDE_ANGLE_DEG = math.degrees(math.atan(ARENA_SIDE_RUN / ARENA_SIDE_H))  # ~40 da vertical
ARENA_BASE_D = ARENA_TOP_D + 2 * ARENA_SIDE_RUN
ARENA_PANEL_FRAME = 0.04   # espessura (unidades) da moldura horizontal dos painéis
ARENA_PANEL_CAP = 0.06     # largura (unidades) da moldura nas pontas dos painéis
ARENA_PANEL_TEX_FRAME = 0.09  # fração do sprite do painel ocupada pela moldura (cada lado)
ARENA_PANEL_DEPTH = 0.045  # quanto a madeira afunda em relação à moldura (vinco da lateral)
ARENA_PYRAMID_HEIGHT = 0.03
ARENA_PYRAMID_LEG = 0.18   # cateto (unidades) da placa triangular de canto

# Região do atlas reservada para a placa triangular das pirâmides (canto inferior direito).
TRI_BOX = (ATLAS - 512, ATLAS - 512, ATLAS, ATLAS)
SHEET_TRIANGLE = (1260, 12, 1378, 150)  # placa triangular, ângulo reto no canto superior esquerdo


def channel_wall_fraction():
    """Fração da faixa central do atlas ocupada por cada parede da canaleta."""
    wall = ARENA_HIGH_Y - ARENA_LOW_Y
    return wall / (2 * wall + ARENA_CHANNEL_W)


def build_arena_mesh():
    """Devolve (positions, uvs, indices, field_rect, ridge_rows) da arena procedural.

    ridge_rows é a faixa do atlas (linhas) usada pela canaleta (parede, piso, parede); no topo
    do atlas as duas plataformas ficam acima e abaixo dessa faixa (projeção planar
    "desdobrada" ao longo de Z)."""
    positions, uvs, indices = [], [], []

    def tri(p0, p1, p2, t0, t1, t2):
        base = len(positions)
        positions.extend([p0, p1, p2])
        uvs.extend([t0, t1, t2])
        indices.extend([base, base + 1, base + 2])

    def quad(p0, p1, p2, p3, t0, t1, t2, t3, nu=16, nv=4):
        # Subdividido em grade bilinear: num quad trapezoidal (painéis laterais), dois
        # triângulos com interpolação afim deixam a textura quebrada na diagonal (a cruz do
        # painel frontal aparecia torta). A grade aproxima o mapeamento bilinear.
        def lerp2(a, b, t):
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)

        def at(u, v):
            return (lerp3(lerp3(p0, p1, u), lerp3(p3, p2, u), v),
                    lerp2(lerp2(t0, t1, u), lerp2(t3, t2, u), v))

        for i in range(nu):
            for j in range(nv):
                u0, u1 = i / nu, (i + 1) / nu
                v0, v1 = j / nv, (j + 1) / nv
                a, ta = at(u0, v0)
                b, tb = at(u1, v0)
                c_, tc = at(u1, v1)
                d, td = at(u0, v1)
                tri(a, b, c_, ta, tb, tc)
                tri(a, c_, d, ta, tc, td)

    def lerp3(a, b, t):
        return tuple(a[k] + (b[k] - a[k]) * t for k in range(3))

    def add3(a, b, s=1.0):
        return tuple(a[k] + b[k] * s for k in range(3))

    def cross3(a, b):
        return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])

    def norm3(v):
        l = math.sqrt(sum(c * c for c in v)) or 1.0
        return tuple(c / l for c in v)

    def dist3(a, b):
        return math.sqrt(sum((a[k] - b[k]) ** 2 for k in range(3)))

    hw, hd = ARENA_TOP_W / 2, ARENA_TOP_D / 2
    c = ARENA_CHANNEL_W / 2
    y_lo, y_hi = ARENA_LOW_Y, ARENA_HIGH_Y
    wall_h = y_hi - y_lo
    top_v = TOP_H / ATLAS
    unrolled = ARENA_TOP_D + 2 * wall_h  # plataforma + parede + piso + parede + plataforma

    # posições ao longo do topo desdobrado
    s_wall_a = hd - c                 # início da parede do fundo (z = -c, de cima para baixo)
    s_floor = s_wall_a + wall_h       # piso, z de -c a c
    s_wall_b = s_floor + 2 * c        # parede da frente (z = c, de baixo para cima)
    s_front = s_wall_b + wall_h       # plataforma da frente, z de c a hd

    def uv_s(x, s):
        return ((x + hw) / ARENA_TOP_W, s / unrolled * top_v)

    def rows(s):
        return int(s / unrolled * TOP_H)

    def px(x):
        return int((x + hw) / ARENA_TOP_W * ATLAS)

    ridge_rows = (rows(s_wall_a), rows(s_front))

    # --- plataformas: fundo (oponente, z<-c) e frente (jogador, z>c)
    quad((-hw, y_hi, -hd), (-hw, y_hi, -c), (hw, y_hi, -c), (hw, y_hi, -hd),
         uv_s(-hw, 0), uv_s(-hw, s_wall_a), uv_s(hw, s_wall_a), uv_s(hw, 0))
    quad((-hw, y_hi, c), (-hw, y_hi, hd), (hw, y_hi, hd), (hw, y_hi, c),
         uv_s(-hw, s_front), uv_s(-hw, unrolled), uv_s(hw, unrolled), uv_s(hw, s_front))

    # --- canaleta: parede do fundo (vira para o jogador), piso, parede da frente
    quad((-hw, y_hi, -c), (-hw, y_lo, -c), (hw, y_lo, -c), (hw, y_hi, -c),
         uv_s(-hw, s_wall_a), uv_s(-hw, s_floor), uv_s(hw, s_floor), uv_s(hw, s_wall_a))
    quad((-hw, y_lo, -c), (-hw, y_lo, c), (hw, y_lo, c), (hw, y_lo, -c),
         uv_s(-hw, s_floor), uv_s(-hw, s_wall_b), uv_s(hw, s_wall_b), uv_s(hw, s_floor))
    quad((-hw, y_lo, c), (-hw, y_hi, c), (hw, y_hi, c), (hw, y_lo, c),
         uv_s(-hw, s_wall_b), uv_s(-hw, s_front), uv_s(hw, s_front), uv_s(hw, s_wall_b))

    # --- laterais
    side_v0 = TOP_H / ATLAS
    cross_v0, cross_v1 = side_v0, side_v0 + SIDE_CROSS_H / ATLAS
    plain_v0, plain_v1 = cross_v1, cross_v1 + SIDE_PLAIN_H / ATLAS
    # As laterais vão direto do topo (hw, hd) à base (bw, bd): prisma trapezoidal.
    bw, bd = ARENA_BASE_W / 2, ARENA_BASE_D / 2
    rw, rd, ry = bw, bd, ARENA_BASE_Y
    tri_u0, tri_v0 = TRI_BOX[0] / ATLAS, TRI_BOX[1] / ATLAS
    tri_u1, tri_v1 = TRI_BOX[2] / ATLAS, TRI_BOX[3] / ATLAS
    kt = ARENA_PANEL_TEX_FRAME

    # Cada painel: (topo-esq, topo-dir, base-esq, base-dir, faixa do atlas, cantoneiras),
    # visto de fora. Lados são divididos em três: plataforma, canaleta (baixa), plataforma.
    panels = [
        # fundo (oponente) e frente (jogador): painéis altos, com cruz
        ((hw, y_hi, -hd), (-hw, y_hi, -hd), (rw, ry, -rd), (-rw, ry, -rd), (cross_v0, cross_v1), True),
        ((-hw, y_hi, hd), (hw, y_hi, hd), (-rw, ry, rd), (rw, ry, rd), (cross_v0, cross_v1), True),
        # direita (vista de +X, a esquerda do observador é +Z)
        ((hw, y_hi, hd), (hw, y_hi, c), (rw, ry, rd), (rw, ry, c), (plain_v0, plain_v1), True),
        ((hw, y_lo, c), (hw, y_lo, -c), (rw, ry, c), (rw, ry, -c), (plain_v0, plain_v1), False),
        ((hw, y_hi, -c), (hw, y_hi, -hd), (rw, ry, -c), (rw, ry, -rd), (plain_v0, plain_v1), True),
        # esquerda (vista de -X, a esquerda do observador é -Z)
        ((-hw, y_hi, -hd), (-hw, y_hi, -c), (-rw, ry, -rd), (-rw, ry, -c), (plain_v0, plain_v1), True),
        ((-hw, y_lo, -c), (-hw, y_lo, c), (-rw, ry, -c), (-rw, ry, c), (plain_v0, plain_v1), False),
        ((-hw, y_hi, c), (-hw, y_hi, hd), (-rw, ry, c), (-rw, ry, rd), (plain_v0, plain_v1), True),
    ]

    for P00, P10, P01, P11, (tv0, tv1), pyramids in panels:
        length = dist3(P00, P10)
        height = dist3(P00, P01)
        k = min(ARENA_PANEL_FRAME / height, 0.3)   # moldura horizontal, espessura constante
        kx = min(ARENA_PANEL_CAP / length, 0.3)    # moldura das pontas, largura constante

        def P(u, v, P00=P00, P10=P10, P01=P01, P11=P11):
            return lerp3(lerp3(P00, P10, u), lerp3(P01, P11, u), v)

        def remap(t, k):
            # casa a moldura geométrica (k) com a moldura do sprite (kt)
            if t <= k:
                return t / k * kt
            if t >= 1 - k:
                return 1 - (1 - t) / k * kt
            return kt + (t - k) / (1 - 2 * k) * (1 - 2 * kt)

        def T(u, v, tv0=tv0, tv1=tv1, k=k, kx=kx):
            return (remap(u, kx), tv0 + (tv1 - tv0) * remap(v, k))

        normal = norm3(cross3(add3(P10, P00, -1.0), add3(P01, P00, -1.0)))
        centre = P(0.5, 0.5)
        if normal[0] * centre[0] + normal[2] * centre[2] < 0:  # deve apontar para fora
            normal = tuple(-c_ for c_ in normal)
        inward = tuple(-c_ * ARENA_PANEL_DEPTH for c_ in normal)

        # moldura (plano do painel): 4 tiras entre o retângulo externo e o interno
        rects = [((0, 0), (1, k)), ((0, 1 - k), (1, 1)), ((0, k), (kx, 1 - k)),
                 ((1 - kx, k), (1, 1 - k))]
        for (u0, v0), (u1, v1) in rects:
            quad(P(u0, v0), P(u0, v1), P(u1, v1), P(u1, v0),
                 T(u0, v0), T(u0, v1), T(u1, v1), T(u1, v0))
        # madeira rebaixada e as 4 paredes do rebaixo
        I00, I10 = P(kx, k), P(1 - kx, k)
        I01, I11 = P(kx, 1 - k), P(1 - kx, 1 - k)
        R00, R10 = add3(I00, inward), add3(I10, inward)
        R01, R11 = add3(I01, inward), add3(I11, inward)
        quad(R00, R01, R11, R10, T(kx, k), T(kx, 1 - k), T(1 - kx, 1 - k), T(1 - kx, k))
        walls = [(I00, I10, R10, R00, (kx, k), (1 - kx, k)),
                 (I01, R01, R11, I11, (kx, 1 - k), (1 - kx, 1 - k)),
                 (I00, R00, R01, I01, (kx, k), (kx, 1 - k)),
                 (I10, I11, R11, R10, (1 - kx, k), (1 - kx, 1 - k))]
        for p0, p1, p2, p3, ta, tb in walls:
            quad(p0, p1, p2, p3, T(*ta), T(*tb), T(*tb), T(*ta))

        if not pyramids:
            continue
        # cantoneiras: pirâmides triangulares pequenas nas duas pontas do painel, apex para
        # fora. A base é um triângulo retângulo isósceles no plano do painel com o ângulo
        # reto no canto superior: um cateto desce ao longo da aresta da quina (inclinada
        # pelo trapézio, logo a placa fica angulada) e o outro é perpendicular a ele, para
        # dentro do painel. Mesma forma da placa do atlas, sem distorção.
        top_dir = norm3(add3(P10, P00, -1.0))
        for end in (0, 1):
            A = P00 if end == 0 else P10
            edge = norm3(add3(P01 if end == 0 else P11, A, -1.0))  # aresta da quina, descendo
            inward = cross3(normal, edge)                            # perpendicular no plano
            toward = 1.0 if end == 0 else -1.0
            if sum(inward[k_] * top_dir[k_] for k_ in range(3)) * toward < 0:
                inward = tuple(-c_ for c_ in inward)
            leg = min(ARENA_PYRAMID_LEG, length * 0.45, height)
            B = add3(A, edge, leg)
            C = add3(A, inward, leg)
            pts = [A, B, C] if end == 0 else [A, C, B]
            tex = [(tri_u0, tri_v0), (tri_u0, tri_v1), (tri_u1, tri_v0)]
            if end == 1:
                tex = [tex[0], tex[2], tex[1]]
            apex = add3(tuple(sum(p_[k_] for p_ in pts) / 3 for k_ in range(3)), normal,
                        ARENA_PYRAMID_HEIGHT)
            apex_t = tuple(sum(t[k_] for t in tex) / 3 for k_ in range(2))
            for e in range(3):
                tri(pts[e], pts[(e + 1) % 3], apex, tex[e], tex[(e + 1) % 3], apex_t)

    # --- fechamento da canaleta nas laterais: triângulos finos entre o painel alto e o baixo,
    # nos planos z = ±c, texturizados com a moldura metálica do painel
    band_v = (plain_v0, plain_v0 + (plain_v1 - plain_v0) * kt)
    for sx in (1, -1):
        for sz_ in (1, -1):
            a = (sx * hw, y_hi, sz_ * c)
            b = (sx * hw, y_lo, sz_ * c)
            d = (sx * rw, ry, sz_ * c)
            tri(a, b, d, (0.0, band_v[0]), (0.0, band_v[1]), (1.0, band_v[1]))

    # --- fundo (tábuas ocupam a parte esquerda da região inferior do atlas)
    bottom_v0 = (TOP_H + SIDE_H) / ATLAS
    bottom_u1 = TRI_BOX[0] / ATLAS
    quad((-bw, ARENA_BASE_Y, -bd), (bw, ARENA_BASE_Y, -bd), (bw, ARENA_BASE_Y, bd),
         (-bw, ARENA_BASE_Y, bd), (0.0, bottom_v0), (bottom_u1, bottom_v0), (bottom_u1, 1.0),
         (0.0, 1.0))

    field_rect = (px(-hw + ARENA_FRAME), rows(ARENA_FRAME), px(hw - ARENA_FRAME),
                  rows(unrolled - ARENA_FRAME))
    return positions, uvs, indices, field_rect, ridge_rows


# ----------------------------------------------------------------------------- UV

def main():
    rng = random.Random(7)
    if "--raw" not in sys.argv:
        return main_procedural(rng)
    print("lendo", RAW_GLB)
    gltf, bin_data = read_glb(RAW_GLB)
    prim = gltf["meshes"][0]["primitives"][0]
    positions = read_accessor(gltf, bin_data, prim["attributes"]["POSITION"])
    indices = [i[0] for i in read_accessor(gltf, bin_data, prim["indices"])]

    mins = [min(p[k] for p in positions) for k in range(3)]
    maxs = [max(p[k] for p in positions) for k in range(3)]
    size = [maxs[k] - mins[k] for k in range(3)]
    print("bounds", [round(v, 3) for v in mins], [round(v, 3) for v in maxs])

    # Plateau do topo: faces voltadas para cima na altura máxima.
    top_y = maxs[1]
    plateau = [p for p in positions if p[1] > top_y - 0.02]
    px0, px1 = min(p[0] for p in plateau), max(p[0] for p in plateau)
    pz0, pz1 = min(p[2] for p in plateau), max(p[2] for p in plateau)
    print("plateau x[%.3f %.3f] z[%.3f %.3f]" % (px0, px1, pz0, pz1))

    def top_uv(p):
        return ((p[0] - mins[0]) / size[0], (p[2] - mins[2]) / size[2] * (TOP_H / ATLAS))

    def z_to_row(z):
        return int((z - mins[2]) / size[2] * TOP_H)

    # Canal central: faixa do topo mais baixa que o platô, no interior do tabuleiro.
    inner_x0, inner_x1 = px0 + 0.08 * (px1 - px0), px1 - 0.08 * (px1 - px0)
    inner_z0, inner_z1 = pz0 + 0.08 * (pz1 - pz0), pz1 - 0.08 * (pz1 - pz0)
    channel = [p for p in positions
               if inner_x0 < p[0] < inner_x1 and inner_z0 < p[2] < inner_z1
               and top_y - 0.07 < p[1] < top_y - 0.02]
    if len(channel) > 50:
        zs = sorted(p[2] for p in channel)
        cz0, cz1 = zs[int(len(zs) * 0.03)], zs[int(len(zs) * 0.97)]
        print("canal central em z[%.3f %.3f]" % (cz0, cz1))
    else:
        mid = (pz0 + pz1) / 2
        half = (pz1 - pz0) * RIDGE_FRACTION / 2
        cz0, cz1 = mid - half, mid + half
        print("canal central nao detectado; usando faixa no centro")
    ridge_rows = (z_to_row(cz0), z_to_row(cz1))

    field_rect = (int((px0 - mins[0]) / size[0] * ATLAS), int((pz0 - mins[2]) / size[2] * TOP_H),
                  int((px1 - mins[0]) / size[0] * ATLAS), int((pz1 - mins[2]) / size[2] * TOP_H))

    print("lendo", SHEET_PNG)
    sw, sh, srgb = read_png(SHEET_PNG)
    sheet = Image(sw, sh, srgb)
    print("montando atlas", ATLAS, "x", ATLAS)
    atlas = build_atlas(sheet, field_rect, ridge_rows, rng)
    png_bytes = write_png(OUT_ATLAS, atlas.w, atlas.h, atlas.px)
    print("atlas salvo em", OUT_ATLAS, len(png_bytes), "bytes")

    side_v0 = TOP_H / ATLAS
    side_v1 = (TOP_H + SIDE_H) / ATLAS
    bottom_v0 = side_v1
    inset = 0.004  # evita amostrar a região vizinha nas bordas

    def side_uv(p, axis):
        u = (p[axis] - mins[axis]) / size[axis]
        h = (p[1] - mins[1]) / size[1]
        v = side_v1 - (side_v1 - side_v0) * h
        return (min(max(u, inset), 1 - inset), min(max(v, side_v0 + inset), side_v1 - inset))

    def bottom_uv(p):
        return ((p[0] - mins[0]) / size[0],
                bottom_v0 + (p[2] - mins[2]) / size[2] * (BOTTOM_H / ATLAS))

    out_pos, out_uv, out_idx = [], [], []
    counts = {"top": 0, "side": 0, "bottom": 0}
    for t in range(0, len(indices), 3):
        tri = [positions[indices[t + k]] for k in range(3)]
        a, b, c = tri
        ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
        nx, ny, nz = nx / length, ny / length, nz / length
        cx = (a[0] + b[0] + c[0]) / 3
        cy = (a[1] + b[1] + c[1]) / 3
        cz = (a[2] + b[2] + c[2]) / 3
        inside_top = (inner_x0 < cx < inner_x1 and inner_z0 < cz < inner_z1
                      and cy > top_y - 0.1)
        if ny > 0.6 or inside_top:
            # Degraus internos do tabuleiro (bordas do canal) seguem a projeção do topo.
            uvs = [top_uv(p) for p in tri]
            counts["top"] += 1
        elif ny < -0.6:
            uvs = [bottom_uv(p) for p in tri]
            counts["bottom"] += 1
        else:
            axis = 2 if abs(nx) >= abs(nz) else 0  # face em X varia ao longo de Z e vice-versa
            uvs = [side_uv(p, axis) for p in tri]
            counts["side"] += 1
        base = len(out_pos)
        out_pos.extend(tri)
        out_uv.extend(uvs)
        out_idx.extend([base, base + 1, base + 2])
    print("faces por regiao", counts)

    write_glb(OUT_GLB, out_pos, out_uv, out_idx, png_bytes)
    print("gravado", OUT_GLB, os.path.getsize(OUT_GLB), "bytes")


def main_procedural(rng):
    positions, uvs, indices, field_rect, ridge_rows = build_arena_mesh()
    print("malha procedural: %d vertices, %d triangulos" % (len(positions), len(indices) // 3))
    print("lendo", SHEET_PNG)
    sw, sh, srgb = read_png(SHEET_PNG)
    sheet = Image(sw, sh, srgb)
    print("montando atlas", ATLAS, "x", ATLAS)
    atlas = build_atlas(sheet, field_rect, ridge_rows, rng, channel_wall_fraction())
    png_bytes = write_png(OUT_ATLAS, atlas.w, atlas.h, atlas.px)
    print("atlas salvo em", OUT_ATLAS, len(png_bytes), "bytes")
    write_glb(OUT_GLB, positions, uvs, indices, png_bytes)
    print("gravado", OUT_GLB, os.path.getsize(OUT_GLB), "bytes")
    import normalmap
    normalmap.add_to_glb(OUT_GLB, strength=1.2)  # ladrilhos: relevo mais discreto


if __name__ == "__main__":
    sys.exit(main())
